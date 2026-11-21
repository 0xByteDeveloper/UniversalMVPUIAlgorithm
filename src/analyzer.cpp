// ============================================================================
// src/analyzer.cpp — GOD STATE analiz orkestratörü (tam uygulama)
// ============================================================================
#include "umvp/analyzer.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <set>
#include <chrono>

namespace umvp {

static std::string to_hex(u64 v) {
    char b[32];
    snprintf(b, sizeof(b), "0x%llX", (unsigned long long)v);
    return b;
}

Analyzer::Analyzer(WinProcess& proc, const AnalyzeOptions& opt)
    : proc_(proc), opt_(opt) {}

static std::string ExtractDisplayText(const std::string& label) {
    size_t p = label.find("##");
    return p == std::string::npos ? label : label.substr(0, p);
}

static std::string ComboItemsToString(u64 items_addr, MemoryMap* mm) {
    // "Head\0Neck\0Chest\0\0" → "Head|Neck|Chest"
    char buf[256] = {};
    if (!mm->FastRead(items_addr, buf, sizeof(buf))) return "?";
    std::string out;
    char* p = buf;
    char* end = buf + sizeof(buf);
    while (p < end) {
        size_t n = strnlen(p, (size_t)(end - p));
        if (n == 0) break;  // çift NUL
        if (!out.empty()) out += '|';
        out.append(p, n);
        p += n + 1;
    }
    return out;
}

AnalysisReport Analyzer::Run(const std::function<void(const char*)>& progress) {
    AnalysisReport rep;
    rep.pid = proc_.pid();
    rep.process_name = proc_.name();
    rep.arch = proc_.arch();

    auto t0 = std::chrono::steady_clock::now();

    mm_ = std::make_unique<MemoryMap>(proc_);
    de_ = std::make_unique<DisasmEngine>(proc_.arch() == Arch::X86 ? Arch::X86 : Arch::X64);
    xrefs_ = std::make_unique<XrefMap>();
    fbr_ = std::make_unique<FuncBoundsResolver>(*mm_, *de_, proc_.arch());

    if (progress) progress("Bellek haritasi ciziliyor (VirtualQueryEx + anlik goruntu)");
    mm_->Build();
    rep.region_count = mm_->regions().size();
    rep.readable_bytes = mm_->readable_bytes_total();
    rep.cached_bytes = mm_->cached_bytes();
    rep.modules = proc_.modules();

    PhaseRenderer(rep);
    PhaseImGuiDetect(rep, progress);
    if (opt_.scan_live) PhaseLive(rep, progress);
    if (opt_.scan_code) PhaseCode(rep, progress);
    AssignLivePositions(rep);

    auto t1 = std::chrono::steady_clock::now();
    rep.analysis_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    rep.form_count = (int)rep.forms.size();
    rep.total_widgets = 0;
    for (auto& f : rep.forms) rep.total_widgets += (int)f.widgets.size();
    return rep;
}

// ---------------------------------------------------------------------------
bool Analyzer::FindImGuiExports(AnalysisReport& rep) {
    auto mods = proc_.modules();  // kopya: imgui_exports doldurulmayacak (const sorununu atlar)
    for (auto& m : mods) {
        bool found_any = false;
        if (m.export_rva == 0 || m.export_size == 0) continue;
        u8 dir[40];
        if (proc_.Read(m.base + m.export_rva, dir, 40) != 40) continue;
        u32 namesRva, namesCount, funcsRva, ordsRva;
        memcpy(&namesCount, dir + 24, 4);
        memcpy(&funcsRva, dir + 28, 4);
        memcpy(&namesRva, dir + 32, 4);
        memcpy(&ordsRva, dir + 36, 4);
        if (namesCount == 0 || namesCount > 200000) continue;
        std::vector<u8> names((size_t)namesCount * 4);
        if (proc_.Read(m.base + namesRva, names.data(), names.size()) != names.size()) continue;
        std::vector<u8> funcs((size_t)namesCount * 4), ords((size_t)namesCount * 2);
        if (proc_.Read(m.base + funcsRva, funcs.data(), funcs.size()) != funcs.size()) continue;
        if (proc_.Read(m.base + ordsRva, ords.data(), ords.size()) != ords.size()) continue;
        for (u32 i = 0; i < namesCount; i++) {
            u32 nrva;
            memcpy(&nrva, &names[i * 4], 4);
            char nb[300] = {};
            if (proc_.Read(m.base + nrva, nb, sizeof(nb) - 1) == 0) continue;
            std::string name = nb;
            std::string clean;
            if (name.rfind("_ZN5ImGui", 0) == 0) {  // MinGW Itanium: _ZN5ImGui6ButtonEPKc...
                size_t p = 8;
                if (p < name.size() && isdigit((unsigned char)name[p])) {
                    int n = atoi(name.c_str() + p);
                    size_t ds = p;
                    while (ds < name.size() && isdigit((unsigned char)name[ds])) ds++;
                    if ((int)(name.size() - ds) >= n && n > 0) clean = name.substr(ds, n);
                }
            } else if (name.find("@ImGui@@") != std::string::npos && name[0] == '?') {
                size_t a = 1, b = name.find('@', 1);
                if (b != std::string::npos && b > a) clean = name.substr(a, b - a);
            }
            if (clean.empty()) continue;
            u16 ord;
            memcpy(&ord, &ords[i * 2], 2);
            u32 frva;
            memcpy(&frva, &funcs[(size_t)ord * 4], 4);
            export_names_[m.base + frva] = clean;
            found_any = true;
        }
        if (found_any) {
            rep.imgui.linkage = "dll-exports (" + m.name + ")";
            return true;
        }
    }
    return false;
}

std::vector<u64> Analyzer::FindHashFuncs(AnalysisReport& rep) {
    std::vector<u64> out;
    auto hits = FindCrcTables(*mm_, &proc_.modules());
    // ana moduldeki tabloyu kullan (wine dll'lerindeki zlib tablolari yaniltici)
    CrcTableHit hit{};
    bool have = false;
    for (auto& hh : hits)
        if (hh.module && !hh.module->is_dll) { hit = hh; have = true; break; }
    if (!have && !hits.empty()) { hit = hits[0]; have = true; }
    if (!have) return out;
    rep.imgui.crc32_table_addr = hit.addr;
    auto* sites = xrefs_->SitesTo(hit.addr);
    if (!sites) return out;
    std::unordered_map<u64, bool> seen;
    for (auto& s : *sites) {
        FuncBounds* fb = GetFuncBounds(s.site);
        if (fb && fb->start && !seen.count(fb->start)) {
            seen[fb->start] = true;
            if (fb->end - fb->start < 0x800) out.push_back(fb->start);
        }
    }
    if (!out.empty()) rep.imgui.imhashstr_func = out[0];
    return out;
}

void Analyzer::PhaseRenderer(AnalysisReport& rep) {
    rep.renderer = DetectRenderer(proc_.modules());
}

void Analyzer::PhaseImGuiDetect(AnalysisReport& rep, const std::function<void(const char*)>& progress) {
    if (progress) progress("ImGui izleri: CRC32 tablolari + surum dizileri");
    bool dll = FindImGuiExports(rep);
    auto tables = FindCrcTables(*mm_, &proc_.modules());
    // ana moduldeki tablo = ImGui tablosu (diger modullerdeki zlib tablolari atilir)
    const CrcTableHit* main_tbl = nullptr;
    for (auto& t : tables)
        if (t.module && !t.module->is_dll) { main_tbl = &t; break; }
    auto vstrs = FindVersionStrings(*mm_, &proc_.modules());
    if (main_tbl) {
        rep.imgui.found = true;
        rep.imgui.crc32_table_addr = main_tbl->addr;
        rep.imgui.hash_variant = main_tbl->algo == HashAlgo::Adler ? "A (crc32-adler, <=1.91.5)"
                                                                   : "B/C (crc32c, >=1.91.6)";
    }
    if (!vstrs.empty()) {
        rep.imgui.found = true;
        rep.imgui.version_str = vstrs[0].version;
        int vmaj = 0, vmid = 0, vpat = 0;
        if (sscanf(vstrs[0].version.c_str(), "%d.%d.%d", &vmaj, &vmid, &vpat) == 3)
            rep.imgui.version_num = vmaj * 10000 + vmid * 100 + vpat;
    }
    if (!dll) rep.imgui.linkage = "static (derin analiz modu)";
}

void Analyzer::PhaseLive(AnalysisReport& rep, const std::function<void(const char*)>& progress) {
    if (progress) progress("Heap taramasi: ImGuiWindow (ID==ImHashStr(Name))");
    std::vector<HashProfile> profiles;
    profiles.push_back(HashProfileForVersion(rep.imgui.version_num));
    for (auto& p : {HashProfile{HashAlgo::Adler, HashVariant::Legacy},
                    HashProfile{HashAlgo::Castagnoli, HashVariant::Legacy},
                    HashProfile{HashAlgo::Castagnoli, HashVariant::Modern}}) {
        bool have = false;
        for (auto& q : profiles)
            if (q.algo == p.algo && q.variant == p.variant) have = true;
        if (!have) profiles.push_back(p);
    }

    live_windows_ = ScanWindows(*mm_, profiles);
    if (progress) progress(("Heap taramasi sonucu: " + std::to_string(live_windows_.size()) + " ImGuiWindow").c_str());
    ctx_ = ContextInfo{};
    if (!live_windows_.empty())
        FindContext(*mm_, live_windows_, &ctx_);
    if (progress) progress(ctx_.confirmed ? ("Context DOGRULANDI: ImGui v" + ctx_.version).c_str()
                                          : "Context bulunamadi (kod analizi devam edecek)");

    if (ctx_.confirmed) {
        rep.imgui.found = true;
        rep.imgui.context_addr = ctx_.addr;
        rep.imgui.version_str = ctx_.version;
        rep.imgui.version_num = ctx_.version_num;
        rep.imgui.version_confirmed = true;
        rep.imgui.hash_variant = HashProfileForVersion(ctx_.version_num).era_name();
        rep.imgui.windows_vector_buf = ctx_.windows_buf;
        rep.imgui.display_size = ctx_.display_size;
        rep.imgui.framerate = ctx_.framerate;
        rep.imgui.windows_active_count = ctx_.windows_active_count;
        EnrichWindows(*mm_, live_windows_, ctx_);
    }

    for (auto& w : live_windows_) {
        if (w.fallback) continue;
        if (ctx_.confirmed && !w.in_windows_vector) continue;  // ini-handler sahteleri (Window/Table)
        FormInfo f;
        f.name = w.name;
        f.id = w.id;
        f.window_obj = w.obj;
        f.pos = w.pos; f.size = w.size; f.size_full = w.size_full;
        f.flags = w.flags;
        f.child = w.child;
        f.active = w.active;
        f.collapsed = w.collapsed;
        f.live_found = true;
        if (ctx_.layout) f.flags_decoded = ctx_.layout->DecodeWinFlags(w.flags);
        rep.forms.push_back(std::move(f));

        if (opt_.decode_draw_text && ctx_.confirmed) {
            if (progress) progress(("Draw-list metin cozumu: " + w.name).c_str());
            DecodeDrawListTexts(*mm_, w, ctx_);
            for (auto& tr : w.text_runs)
                text_runs_all_.push_back({tr.text, tr.a, tr.b, w.name});
        }
    }
    rep.log.push_back("canli pencere: " + std::to_string(live_windows_.size()) +
                      (ctx_.confirmed ? " | context OK v" + ctx_.version : " | context bulunamadi"));
}

// Thunk çözümü: küçük pdata girdisi (≤16B) = jmp köprüsü → gerçek işlevi takip et
u64 Analyzer::ResolveThunk(u64 callee) {
    for (int i = 0; i < 4; i++) {
        FuncBounds* fb = GetFuncBounds(callee);
        if (!fb || !fb->start) return callee;
        if (fb->end - fb->start > 16) return callee;  // gerçek işlev
        MemoryMap::RegionData rd;
        if (!mm_->GetRegionDataForRange(fb->start, (size_t)(fb->end - fb->start), &rd)) return callee;
        auto insns = de_->Disassemble(rd.data + (fb->start - rd.base), (size_t)(fb->end - fb->start), fb->start);
        bool followed = false;
        for (auto& I : insns)
            if (I.is_jmp && I.branch_target && I.branch_target != callee) {
                callee = I.branch_target; followed = true; break;
            }
        if (!followed) return callee;
    }
    return callee;
}

FuncBounds* Analyzer::GetFuncBounds(u64 addr) {
    auto it = func_cache_.find(addr);
    if (it != func_cache_.end()) return &it->second;
    FuncBounds fb = fbr_->Resolve(addr);
    func_cache_[addr] = fb;
    return &func_cache_[addr];
}

// Çağrılan işlev hash işlevine ULAŞIYOR mu? (derinlik-2: Button→GetID→ImHashStr)
static bool FuncCallsHashDirect(MemoryMap& mm, DisasmEngine& de, FuncBoundsResolver& fbr,
                                u64 func, const std::vector<u64>& hash_funcs,
                                std::vector<u64>* callees_out) {
    FuncBounds fb;
    if (!fbr.FromPData(func, &fb)) {
        // pdata'da yok — sezgisel (pahalı olmasın diye küçük pencere)
        if (!fbr.Heuristic(func, &fb)) return false;
    }
    if (fb.end <= fb.start || fb.end - fb.start > 0x40000) return false;
    MemoryMap::RegionData rd;
    if (!mm.GetRegionDataForRange(fb.start, (size_t)(fb.end - fb.start), &rd)) return false;
    u64 off = fb.start - rd.base;
    auto insns = de.Disassemble(rd.data + off, (size_t)(fb.end - fb.start), fb.start);
    bool res = false;
    for (auto& I : insns) {
        if (I.is_call && I.branch_target) {
            for (auto h : hash_funcs)
                if (I.branch_target == h) return true;
            if (callees_out) callees_out->push_back(I.branch_target);
            res = false;
        }
    }
    return res;
}

bool Analyzer::CalleeCallsHash(u64 callee) {
    auto it = callee_hash_cache_.find(callee);
    if (it != callee_hash_cache_.end()) return it->second;
    bool res = FuncCallsHashDirect(*mm_, *de_, *fbr_, callee, hash_funcs_, nullptr);
    if (!res) {
        // derinlik-2: ara çağrılanlar hash'i çağırıyor mu? (Button→GetID→ImHashStr)
        std::vector<u64> callees;
        FuncCallsHashDirect(*mm_, *de_, *fbr_, callee, hash_funcs_, &callees);
        std::unordered_map<u64, bool> seen;
        for (u64 c : callees) {
            if (seen.count(c)) continue;
            seen[c] = true;
            // küçük işlevler only (GetID gibi sarmalayıcılar)
            FuncBounds fb;
            if (fbr_->FromPData(c, &fb) && fb.end - fb.start < 0x400) {
                if (FuncCallsHashDirect(*mm_, *de_, *fbr_, c, hash_funcs_, nullptr)) { res = true; break; }
            }
        }
    }
    callee_hash_cache_[callee] = res;
    return res;
}

void Analyzer::PhaseCode(AnalysisReport& rep, const std::function<void(const char*)>& progress) {
    if (progress) progress("Kod analizi: string xref haritasi");
    mm_->BuildStringIndex();
    if (progress) progress((std::string("string indeksi: ") + std::to_string(mm_->string_index().size()) + " aday").c_str());
    xrefs_->Build(*mm_, *de_);
    if (progress) progress((std::string("xref: ") + std::to_string(xrefs_->total_sites()) + " referans").c_str());

    for (auto& m : proc_.modules()) fbr_->LoadPData(m);
    hash_funcs_ = FindHashFuncs(rep);
    ClassifyAndExtract(rep, progress);
}

// ---------------------------------------------------------------------------
// Sınıflandırma + widget çıkarımı
// ---------------------------------------------------------------------------
void Analyzer::ClassifyAndExtract(AnalysisReport& rep, const std::function<void(const char*)>& progress) {
    if (progress) progress("Menu fonksiyonu tespiti (etiket xref kumesi)");

    // 1) canlı form isimlerinin string adresleri (İÇ pencereler hariç)
    std::unordered_map<std::string, u64> form_name_straddr;
    for (auto& s : mm_->string_index()) {
        std::string sv(s.data, s.len);
        if (sv.empty() || sv[0] == '#') continue;
        for (auto& w : live_windows_) {
            if (w.fallback) continue;                 // "Debug##Default" iç pencere
            if (ctx_.confirmed && !w.in_windows_vector) continue;  // ini-handler sahteleri
            if (sv == w.name) { form_name_straddr[sv] = s.addr; break; }
        }
    }
    if (progress) progress(("Form adi xref: " + std::to_string(form_name_straddr.size()) + " aday").c_str());

    // 2) aday menü fonksiyonları
    std::unordered_map<u64, int> func_label_count;
    for (auto& [name, addr] : form_name_straddr)
        if (auto* sites = xrefs_->SitesTo(addr))
            for (auto& s : *sites) {
                FuncBounds* fb = GetFuncBounds(s.site);
                if (fb && fb->start) func_label_count[fb->start]++;
            }
    if (func_label_count.empty()) {
        // fallback: .pdata ile en çok string xref'i içeren fonksiyon
        if (fbr_->pdata_loaded()) {
            std::unordered_map<u64, std::set<u64>> func_strings;
            int budget = 30000;
            for (auto& s : mm_->string_index()) {
                if (budget <= 0) break;
                if (s.len < 3 || s.len > 64) continue;
                if (auto* sites = xrefs_->SitesTo(s.addr))
                    for (auto& st : *sites) {
                        if (budget-- <= 0) break;
                        FuncBounds fb;
                        if (!fbr_->FromPData(st.site, &fb)) continue;
                        if (fb.end - fb.start < 0x40000) func_strings[fb.start].insert(s.addr);
                    }
            }
            u64 best = 0; size_t bestn = 0;
            for (auto& [f, set] : func_strings)
                if (set.size() > bestn) { bestn = set.size(); best = f; }
            if (best) func_label_count[best] = (int)bestn;
        }
    }
    std::vector<std::pair<u64, int>> ranked(func_label_count.begin(), func_label_count.end());
    std::sort(ranked.begin(), ranked.end(), [](auto& a, auto& b) { return a.second > b.second; });
    if (ranked.size() > 3) ranked.resize(3);

    std::unordered_map<std::string, size_t> form_by_name;
    for (size_t i = 0; i < rep.forms.size(); i++) form_by_name[rep.forms[i].name] = i;

    for (auto& [fstart, cnt] : ranked) {
        FuncBounds* fb = GetFuncBounds(fstart);
        if (!fb || !fb->start) continue;
        if (rep.menu_func_start == 0) {
            rep.menu_func_start = fb->start;
            rep.menu_func_end = fb->end;
        }
        if (progress) progress(("Menu fonksiyonu: " + to_hex(fb->start) + ".." + to_hex(fb->end) +
                                " (" + std::to_string(cnt) + " form xref, " + fb->how + ")").c_str());

        MemoryMap::RegionData rd;
        if (!mm_->GetRegionDataForRange(fb->start, (size_t)(fb->end - fb->start), &rd)) continue;
        u64 off = fb->start - rd.base;
        auto insns = de_->Disassemble(rd.data + off, (size_t)(fb->end - fb->start), fb->start);
        if (progress) progress(("  " + std::to_string(insns.size()) + " komut taraniyor").c_str());

        std::vector<size_t> call_idx_list;
        for (size_t i = 0; i < insns.size(); i++)
            if (insns[i].is_call && insns[i].branch_target) call_idx_list.push_back(i);

        // ---- PASS 1: Begin çağrıları + form içerik blokları -------------------
        struct FormBlock { int form_idx; u64 lo, hi; };
        std::vector<FormBlock> blocks;
        std::vector<size_t> begin_call_idx(call_idx_list.size(), (size_t)-1);
        Vec2 pending_next_size;
        for (size_t ci = 0; ci < call_idx_list.size(); ci++) {
            size_t i = call_idx_list[ci];
            Insn& I = insns[i];
            u64 callee = ResolveThunk(I.branch_target);
            CallSiteArgs ca = ExtractCallArgs(*mm_, *de_, insns, i);
            if (!ca.args[0].str_valid) {
                if (ca.args[0].v2_valid) pending_next_size = ca.args[0].v2;  // SetNextWindowSize
                continue;
            }
            const std::string& label = ca.args[0].str;
            bool live_form = form_name_straddr.count(label) > 0;
            bool known_begin = false;
            {
                auto it = export_names_.find(callee);
                if (it != export_names_.end() &&
                    (it->second == "Begin" || it->second == "BeginChild" || it->second == "BeginTabItem" ||
                     it->second == "BeginPopup" || it->second == "BeginTooltip"))
                    known_begin = true;
            }
            if (!live_form && !known_begin) continue;

            // form aç/oluştur
            std::string fname = label;
            auto it = form_by_name.find(fname);
            int fidx;
            if (it != form_by_name.end()) {
                fidx = (int)it->second;
            } else {
                FormInfo nf;
                nf.name = fname;
                nf.code_found = true;
                nf.live_found = false;
                rep.forms.push_back(nf);
                fidx = (int)rep.forms.size() - 1;
                form_by_name[fname] = fidx;
            }
            rep.forms[fidx].code_found = true;
            rep.forms[fidx].begin_call = I.addr;
            rep.forms[fidx].menu_func_start = fb->start;
            rep.forms[fidx].menu_func_end = fb->end;
            if (pending_next_size.valid) {
                rep.forms[fidx].static_size = pending_next_size;
                pending_next_size = Vec2();
            }
            begin_call_idx[ci] = (size_t)fidx;

            // içerik bloğu: test al,al; jne/jcc → hedef blok
            HandlerBlock hb = ExtractHandlerBlock(insns, i);
            if (hb.found && hb.end > hb.start && hb.end - hb.start >= 4) {
                blocks.push_back({fidx, hb.start, hb.end});
                rep.forms[fidx].menu_func_start = hb.start;  // formun kod aralığı = içerik bloğu
                rep.forms[fidx].menu_func_end = hb.end;
            }
        }
        if (progress) progress(("  " + std::to_string(blocks.size()) + " form kod blogu (Begin test+jcc)").c_str());

        // ---- PASS 2: widget çağrıları — blok içerme ile form ataması ----------
        struct WCall {
            int form_idx;
            size_t insn_idx;
            HandlerBlock hb;
        };
        std::vector<WCall> wcalls;
        for (size_t ci = 0; ci < call_idx_list.size(); ci++) {
            size_t i = call_idx_list[ci];
            Insn& I = insns[i];
            u64 callee = ResolveThunk(I.branch_target);
            if (begin_call_idx[ci] != (size_t)-1) continue;  // Begin'ler zaten işlendi

            CallSiteArgs ca = ExtractCallArgs(*mm_, *de_, insns, i);
            ArgValue& a0 = ca.args[0];
            ArgValue& a1 = ca.args[1];
            ArgValue& a2 = ca.args[2];
            ArgValue& a3 = ca.args[3];

            std::string known;
            {
                auto it = export_names_.find(callee);
                if (it != export_names_.end()) known = it->second;
            }
            // hash işlevinin kendisi mi?
            bool is_hashfn = false;
            for (auto h : hash_funcs_) if (callee == h) { is_hashfn = true; break; }
            if (is_hashfn) continue;

            bool hashcaller = CalleeCallsHash(callee);
            FuncBounds* cfb = GetFuncBounds(callee);
            u64 callee_size = cfb ? (cfb->end - cfb->start) : 0;

            std::string type;
            int conf = 0;
            std::string ev;
            Vec2 size_arg;
            float fmin = 0, fmax = 0;
            std::string fmt, items;

            if (!known.empty()) {
                type = known; conf = 99; ev = "export tablosu";
                if (type == "Button" && (a1.v2_valid || (a1.ival_valid && a1.ival == 0)))
                    size_arg = a1.v2_valid ? a1.v2 : Vec2();
                if (type.rfind("Slider", 0) == 0) {
                    fmin = a2.f; fmax = a3.f; fmt = ca.stack_args[0].str;
                }
            } else if (a0.str_valid) {
                const std::string& label = a0.str;
                bool a1_ptr = a1.ival_valid && a1.ival != 0;
                if (a1_ptr && a2.str_valid) {
                    type = "Combo"; conf = 85; ev = "(str,int*,items_str)";
                    items = ComboItemsToString(a2.ival, mm_.get());
                } else if (a1_ptr && a2.f_valid && a3.f_valid) {
                    type = "SliderFloat"; conf = 88; ev = "(str,f*,min,max)";
                    fmin = a2.f; fmax = a3.f; fmt = ca.stack_args[0].str;
                    if (fmt.empty()) fmt = "%.3f (varsayilan)";
                } else if (a1_ptr && a2.f_valid && hashcaller) {
                    type = "DragFloat"; conf = 65; ev = "(str,f*,f)+hash";
                    fmin = a2.f;
                } else if (a1_ptr && a2.ival_valid && a2.ival >= 4 && a2.ival <= 4096) {
                    type = "InputText"; conf = 78; ev = "(str,char*,size)";
                } else if (a1_ptr && a1.ptr_floats_read && hashcaller && callee_size > 1800) {
                    type = "ColorEdit4"; conf = 70; ev = "(str,float[4])+hash+buyuk";
                } else if (a1_ptr && hashcaller && callee_size && callee_size < 1600) {
                    u8 bv = 0;
                    bool boolish = mm_->FastRead(a1.ival, &bv, 1) && bv <= 1;
                    if (boolish) { type = "Checkbox"; conf = 82; ev = "(str,bool*)+hash"; }
                    else { type = "Combo/Radio"; conf = 55; ev = "(str,int*)+hash"; }
                } else if ((a1.v2_valid || (a1.ival_valid && a1.ival == 0)) && hashcaller) {
                    type = "Button"; conf = 85; ev = "(str,[ImVec2])+hash";
                    size_arg = a1.v2_valid ? a1.v2 : Vec2();
                } else if (!hashcaller && callee_size && callee_size < 400) {
                    type = "Text"; conf = 72; ev = "(str), hash YOK, kucuk callee";
                } else if (!hashcaller && callee_size >= 400) {
                    type = "Text/Label"; conf = 45; ev = "(str), hash YOK, orta callee";
                } else {
                    type = "Unknown"; conf = 30; ev = "belirsiz";
                }
            } else {
                continue;  // string arg yok → widget değil (End/SameLine vb.)
            }

            // form ataması: blok içermesi
            int fidx = -1;
            for (auto& b : blocks)
                if (I.addr >= b.lo && I.addr < b.hi) { fidx = b.form_idx; break; }

            WCall wc;
            wc.form_idx = fidx;
            wc.insn_idx = i;
            wc.hb = ExtractHandlerBlock(insns, i);
            wcalls.push_back(std::move(wc));

            if (fidx < 0) continue;  // hiçbir forma ait değil → şimdilik bekle

            // (gecikmeli doldurma: atama netleştikten sonra aşağıda yapılır)
        }

        // ---- PASS 3: handler-blok yayılımı (iç içe koşullu widget'lar) --------
        for (int pass = 0; pass < 3; pass++) {
            bool changed = false;
            for (auto& wc : wcalls) {
                if (wc.form_idx >= 0) continue;  // hb şartı YOK — koşulsuz widget'lar da yayılım alır
                for (auto& wc2 : wcalls) {
                    if (wc2.form_idx < 0 || !wc2.hb.found) continue;
                    // wc'nin çağrısı, wc2'nin handler bloğu içinde mi?
                    size_t call_addr_idx = wc.insn_idx;
                    u64 ca_addr = insns[call_addr_idx].addr;
                    if (ca_addr >= wc2.hb.start && ca_addr < wc2.hb.end) {
                        wc.form_idx = wc2.form_idx;
                        changed = true;
                        break;
                    }
                }
                if (wc.form_idx < 0 && blocks.size() == 1) wc.form_idx = blocks[0].form_idx, changed = true;
            }
            if (!changed) break;
        }

        // ---- PASS 4: form bazında widget kayıtları ----------------------------
        for (size_t ci = 0, wi = 0; ci < call_idx_list.size() && wi < wcalls.size(); ci++) {
            // wcalls sırası call_idx_list ile hizalı DEĞİL (Begin'ler atlandı) —
            // yeniden eşle: wcalls[i].insn_idx üzerinden ilerle
            (void)ci;
            auto& wc = wcalls[wi];
            size_t i = wc.insn_idx;
            Insn& I = insns[i];
            u64 callee = ResolveThunk(I.branch_target);
            wi++;
            if (wc.form_idx < 0) continue;
            if (rep.forms[wc.form_idx].widgets.size() >= (size_t)opt_.max_widgets_per_form) continue;

            CallSiteArgs ca = ExtractCallArgs(*mm_, *de_, insns, i);
            ArgValue& a0 = ca.args[0];
            ArgValue& a1 = ca.args[1];
            ArgValue& a2 = ca.args[2];
            ArgValue& a3 = ca.args[3];
            bool hashcaller = CalleeCallsHash(callee);
            FuncBounds* cfb = GetFuncBounds(callee);
            u64 callee_size = cfb ? (cfb->end - cfb->start) : 0;

            std::string known;
            {
                auto it = export_names_.find(callee);
                if (it != export_names_.end()) known = it->second;
            }
            std::string type;
            int conf = 0;
            std::string ev;
            Vec2 size_arg;
            float fmin = 0, fmax = 0;
            std::string fmt, items;
            if (!known.empty()) {
                type = known; conf = 99; ev = "export tablosu";
                if (type == "Button" && (a1.v2_valid || (a1.ival_valid && a1.ival == 0)))
                    size_arg = a1.v2_valid ? a1.v2 : Vec2();
                if (type.rfind("Slider", 0) == 0) { fmin = a2.f; fmax = a3.f; fmt = ca.stack_args[0].str; }
            } else if (a0.str_valid && !a0.str.empty() && a0.str[0] != '#') {
                const std::string& label = a0.str;
                bool a1_ptr = a1.ival_valid && a1.ival != 0;
                bool a1_writable = a1_ptr && a1.is_writable_ptr;  // .data/global → state işaretçisi
                if (a1_ptr && a2.str_valid) {
                    type = "Combo"; conf = 85; ev = "(str,int*,items_str)";
                    items = ComboItemsToString(a2.ival, mm_.get());
                } else if (a1_ptr && a2.f_valid && a3.f_valid) {
                    type = "SliderFloat"; conf = 88; ev = "(str,f*,min,max)";
                    fmin = a2.f; fmax = a3.f; fmt = ca.stack_args[0].str;
                    if (fmt.empty()) fmt = "%.3f (varsayilan)";
                } else if (a1_ptr && a2.f_valid) {
                    type = "DragFloat"; conf = 65; ev = "(str,f*,f)"; fmin = a2.f;
                } else if (a1_ptr && a2.ival_valid && a2.ival >= 4 && a2.ival <= 4096) {
                    type = "InputText"; conf = 78; ev = "(str,char*,size)";
                } else if (a1_writable && callee_size > 2500 && a1.ptr_floats_read) {
                    type = "ColorEdit4"; conf = 70; ev = "(str,float[4]) yazilabilir+buyuk callee";
                } else if (a1_writable && callee_size && callee_size < 2500) {
                    u8 bv = 0;
                    bool boolish = mm_->FastRead(a1.ival, &bv, 1) && bv <= 1;
                    if (boolish) { type = "Checkbox"; conf = 82; ev = "(str,bool*) yazilabilir ptr"; }
                    else { type = "Combo/Radio"; conf = 55; ev = "(str,int*) yazilabilir ptr"; }
                } else if (a1.v2_valid && !a1.is_writable_ptr && callee_size >= 1500) {
                    type = "BeginChild"; conf = 70; ev = "(str, ImVec2)+buyuk callee";
                    size_arg = a1.v2;
                } else if (a1.v2_valid && !a1.is_writable_ptr) {
                    type = "Button"; conf = 85; ev = "(str, ImVec2 sabiti)";
                    size_arg = a1.v2;
                } else if (a1.ival_valid && a1.ival == 0 && callee_size >= 400) {
                    type = "Button"; conf = 78; ev = "(str, 0) + buyuk callee";
                    size_arg = Vec2();
                } else if (a1.ival_valid && a1.ival == 0) {
                    type = "Button/Small"; conf = 60; ev = "(str, 0)";
                    size_arg = Vec2();
                } else if (!hashcaller && callee_size && callee_size < 400) {
                    type = "Text"; conf = 72; ev = "(str), hash YOK, kucuk callee";
                } else {
                    type = "Text/Label"; conf = 45; ev = "(str), belirsiz";
                }
            } else continue;

            WidgetInfo winfo;
            winfo.type = type;
            winfo.label = a0.str_valid ? a0.str : "?";
            winfo.display_text = ExtractDisplayText(winfo.label);
            winfo.call_site = I.addr;
            winfo.callee = callee;
            winfo.size_arg = size_arg;
            winfo.confidence = conf;
            winfo.evidence = ev + (callee_size ? (" | callee=" + to_hex(callee_size) + "B" +
                          (hashcaller ? ", ImHashStr cagirir" : ", hash YOK")) : "");
            if (fmin != 0 || fmax != 0) { winfo.float_args.push_back(fmin); winfo.float_args.push_back(fmax); }
            winfo.format_arg = fmt;
            winfo.combo_items = items;
            winfo.id_hash = ImHashStr(winfo.label.c_str(), rep.forms[wc.form_idx].id,
                                      HashProfileForVersion(rep.imgui.version_num));
            winfo.handler_start = wc.hb.start;
            winfo.handler_end = wc.hb.end;
            winfo.handler_disasm = FormatInsns(insns, wc.hb.start, wc.hb.end);
            rep.forms[wc.form_idx].widgets.push_back(std::move(winfo));
        }
    }

    // menü fonksiyonu disassembly dökümü
    if (opt_.dump_menu_asm && rep.menu_func_start) {
        MemoryMap::RegionData rd;
        if (mm_->GetRegionDataForRange(rep.menu_func_start, (size_t)(rep.menu_func_end - rep.menu_func_start), &rd)) {
            u64 off = rep.menu_func_start - rd.base;
            auto insns = de_->Disassemble(rd.data + off, (size_t)(rep.menu_func_end - rep.menu_func_start), rep.menu_func_start);
            rep.menu_disasm = FormatInsns(insns, rep.menu_func_start, rep.menu_func_end);
        }
    }
}

// ---------------------------------------------------------------------------
// Canlı konum eşleme (draw-list metinleri + LastItemData)
// ---------------------------------------------------------------------------
void Analyzer::AssignLivePositions(AnalysisReport& rep) {
    // g.LastItemData ile kesin doğrulama
    u32 last_id = ctx_.last_item_id;
    LayoutSet* L = (LayoutSet*)ctx_.layout;
    Vec2 last_rect_min, last_rect_max;
    bool have_last_rect = false;
    if (L && ctx_.last_item_data) {
        int offRect = L->get("ImGuiLastItemData.Rect", -1);
        int offMin = L->get("ImRect.Min", -1);
        if (offRect >= 0 && offMin >= 0) {
            float f[4];
            if (mm_->FastRead(ctx_.last_item_data + offRect, f, 16)) {
                last_rect_min = Vec2(f[0], f[1]);
                last_rect_max = Vec2(f[2], f[3]);
                have_last_rect = true;
            }
        }
    }

    for (auto& f : rep.forms) {
        for (auto& w : f.widgets) {
            // 1) LastItemData kesin eşleşme
            if (have_last_rect && last_id == w.id_hash && last_id != 0) {
                w.pos = Vec2(last_rect_min.x, last_rect_min.y);
                w.size = Vec2(last_rect_max.x - last_rect_min.x, last_rect_max.y - last_rect_min.y);
                w.pos_conf = Conf::LiveExact;
                w.size_conf = Conf::LiveExact;
                w.live_validation = "g.LastItemData.ID == ImHashStr(label, window.ID) — KESIN";
                continue;
            }
            // 2) draw-list metin konumu
            bool found = false;
            for (auto& tr : text_runs_all_) {
                if (tr.window != f.name) continue;
                if (tr.text == w.display_text ||
                    (w.display_text.size() >= 2 && tr.text.find(w.display_text) != std::string::npos)) {
                    // metin dikdörtgeni → widget çerçevesi kestirimi
                    float padx = ctx_.style_ok ? ctx_.style_frame_padding.x : 4.0f;
                    float pady = ctx_.style_ok ? ctx_.style_frame_padding.y : 3.0f;
                    w.pos = Vec2(tr.a.x - padx, tr.a.y - pady);
                    if (w.size_arg.valid)
                        w.size = w.size_arg;  // koddan kesin
                    else
                        w.size = Vec2((tr.b.x - tr.a.x) + padx * 2, (tr.b.y - tr.a.y) + pady * 2);
                    w.pos_conf = Conf::Estimated;
                    w.size_conf = w.size_arg.valid ? Conf::CodeStatic : Conf::Estimated;
                    found = true;
                    break;
                }
            }
            if (found) continue;
            // 3) sadece kod verisi
            if (w.size_arg.valid) {
                w.size = w.size_arg;
                w.size_conf = Conf::CodeStatic;
            }
            // konum: form başlangıcı + satır kestirimi (kaba)
            // (burada kesin kaynak yok — est olarak işaretle)
        }
    }
}

} // namespace umvp
