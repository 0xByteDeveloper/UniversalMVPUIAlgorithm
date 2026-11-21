// ============================================================================
// src/imgui_scan.cpp — ImGui canlı bellek taraması uygulaması
// ============================================================================
#include "umvp/imgui_scan.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace umvp {

// ---------------------------------------------------------------------------
// D3D tespiti
// ---------------------------------------------------------------------------
RendererInfo DetectRenderer(const std::vector<ModuleEntry>& modules) {
    RendererInfo ri;
    auto has = [&](const char* n) {
        for (auto& m : modules) {
            std::string low = m.name;
            for (auto& c : low) c = (char)tolower((unsigned char)c);
            if (low == n) return &m;
        }
        return (const ModuleEntry*)nullptr;
    };
    struct Cand { const char* mod; const char* api; };
    // öncelik: yeni → eski
    const Cand cands[] = {
        {"d3d12.dll", "D3D12"}, {"d3d11.dll", "D3D11"}, {"d3d10_1.dll", "D3D10.1"},
        {"d3d10.dll", "D3D10"}, {"d3d9.dll", "D3D9"}, {"ddraw.dll", "DirectDraw"},
        {"vulkan-1.dll", "Vulkan"}, {"opengl32.dll", "OpenGL"},
    };
    for (auto& c : cands) {
        if (has(c.mod)) {
            ri.api = c.api;
            ri.api_detail = std::string(c.mod) + " yüklü";
            break;
        }
    }
    if (ri.api.empty()) ri.api = "Unknown";
    if (has("dxgi.dll")) ri.api_detail += " + dxgi.dll";
    const char* mods[] = {"d3d9.dll","d3d10.dll","d3d10_1.dll","d3d11.dll","d3d12.dll",
                          "dxgi.dll","opengl32.dll","vulkan-1.dll","d2d1.dll","d3d8thk.dll","ddraw.dll"};
    for (auto* mn : mods)
        if (has(mn)) ri.graphics_modules.push_back(mn);
    return ri;
}

// ---------------------------------------------------------------------------
// CRC tablo taraması
// ---------------------------------------------------------------------------
static bool MatchTable(const u8* p, const u32* lut) {
    for (int i = 0; i < 4; i++) {
        u32 v;
        memcpy(&v, p + i * 4, 4);
        if (v != lut[i]) return false;
    }
    return true;
}

std::vector<CrcTableHit> FindCrcTables(MemoryMap& mm, const std::vector<ModuleEntry>* modules) {
    std::vector<CrcTableHit> out;
    auto find_mod = [&](u64 addr) -> const ModuleEntry* {
        if (!modules) return nullptr;
        for (auto& m : *modules)
            if (addr >= m.base && addr < m.base + m.size) return &m;
        return nullptr;
    };
    for (auto* pr : mm.ImageRegions()) {
        MemoryMap::RegionData rd;
        if (!mm.GetRegionData(pr->base + 1, &rd)) continue;
        
        for (size_t i = 0; i + 16 <= rd.size; i += 4) {  // hizalı
            const u8* p = rd.data + i;
            if (p[0]==0 && p[1]==0 && p[2]==0 && p[3]==0) {
                if (MatchTable(p, kCrc32AdlerLut)) out.push_back({rd.base + i, HashAlgo::Adler, find_mod(rd.base + i)});
                else if (MatchTable(p, kCrc32cLut)) out.push_back({rd.base + i, HashAlgo::Castagnoli, find_mod(rd.base + i)});
                if (out.size() > 64) return out;
            }
        }
    }
    // ana modul onceligi: exe modulundeki tablolar basta
    std::stable_sort(out.begin(), out.end(), [](const CrcTableHit& a, const CrcTableHit& b) {
        bool am = a.module && !a.module->is_dll, bm = b.module && !b.module->is_dll;
        return am && !bm;
    });
    return out;
}

// ---------------------------------------------------------------------------
// Sürüm dizisi taraması: "1.xx.y" + IMGUI_VERSION benzeri
// ---------------------------------------------------------------------------
static bool ParseImguiVersion(const std::string& s, std::string* ver) {
    // "1.91.8" / "1.93.1 WIP" / "1.90.0" — sayi + opsiyonel sonek
    if (s.size() < 5) return false;
    if (s[0] != '1' || s[1] != '.') return false;
    size_t end = 0;
    int dots = 0;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '.') { dots++; end = i + 1; continue; }
        if (isdigit((unsigned char)c)) { end = i + 1; continue; }
        break;  // sonek basladi (" WIP" vb.) — sadece sayi kismi
    }
    // en az "1.xx.y" uzunlugunda
    std::string num = s.substr(0, end);
    if (dots != 2 || num.size() < 5) return false;
    // surum mantigi: orta kisim 60..99 (1.6x-1.9x) veya 100+ (1.1xx deneysel)
    int vmaj = 0, vmid = 0, vpat = 0;
    if (sscanf(num.c_str(), "%d.%d.%d", &vmaj, &vmid, &vpat) != 3) return false;
    if (vmid < 60 || vmid > 110) return false;  // "1.6.40" gibi sahte degerleri ele
    *ver = num;
    return true;
}

std::vector<VersionStringHit> FindVersionStrings(MemoryMap& mm, const std::vector<ModuleEntry>* modules) {
    std::vector<VersionStringHit> out;
    // imgui'nin About penceresi: "Dear ImGui %s" + IMGUI_VERSION literal'i
    // → rdata'da bağımsız "1.91.8\0" dizileri var
    for (auto* pr : mm.ImageDataRegions()) {
        MemoryMap::RegionData rd;
        if (!mm.GetRegionData(pr->base + 1, &rd)) continue;
        const char* p = (const char*)rd.data;
        size_t n = rd.size;
        size_t start = 0;
        for (size_t i = 0; i <= n; i++) {
            bool end = (i == n);
            if (!end) {
                char c = p[i];
                if (c < 0x20 || c > 0x7E) end = true;
            }
            if (end) {
                size_t len = i - start;
                if (len >= 5 && len <= 12) {
                    std::string s(p + start, len);
                    std::string ver;
                    if (ParseImguiVersion(s, &ver)) {
                        const ModuleEntry* mod = nullptr;
                        if (modules)
                            for (auto& m : *modules)
                                if (rd.base + start >= m.base && rd.base + start < m.base + m.size) { mod = &m; break; }
                        // yalniz hedef moduller (exe veya crc tablosu iceren modul) kabul
                        if (mod && !mod->is_dll)
                            out.push_back({rd.base + start, ver, mod});
                    }
                }
                start = i + 1;
            }
        }
        if (out.size() > 64) break;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Pencere heap taraması
// ---------------------------------------------------------------------------
static bool ValidWinFlags(u32 f) {
    // ImGuiWindowFlags: bilinen bitler < 1<<28 (versiyonlar arası gevşek maske)
    return (f & 0xF0000000u) == 0;
}
static bool Fin(float f) { return std::isfinite(f); }
static bool PlausibleGeom(float f) { return Fin(f) && fabsf(f) < 100000.0f; }

std::vector<LiveWindow> ScanWindows(MemoryMap& mm, const std::vector<HashProfile>& try_profiles,
                                    const std::function<void(int,int,const char*)>& progress) {
    std::vector<LiveWindow> out;
    auto guesses = WindowLayoutGuesses(mm.is_x64());  // x64 hedef → x64 hipotezleri!
    int psize = mm.ptr_size();

    auto regions = mm.HeapRegions();
    int done = 0;
    for (auto* pr : regions) {
        ++done;
        if (progress) progress(done, (int)regions.size(), pr->type_name());
        MemoryMap::RegionData rd;
        if (!mm.GetRegionData(pr->base + 1, &rd)) continue;
        if (rd.size < 0x40) continue;
        // hizalı tarama
        for (size_t off = 0; off + 0x80 <= rd.size; off += (size_t)psize) {
            u64 base = rd.base + off;
            // hipotez başına: Name ptr oku
            for (int gi = 0; gi < (int)guesses.size(); gi++) {
                auto& g = guesses[gi];
                if (g.off_name < 0) continue;
                u64 namePtr = 0;
                if (!mm.FastRead(base + g.off_name, &namePtr, psize)) continue;
                if (namePtr < 0x10000) continue;
                // dize oku (önbellekten — heap + image)
                bool ok = false;
                std::string name = mm.FastReadCString(namePtr, 256, true, &ok);
                if (!ok || name.empty() || name.size() < 1) continue;
                u32 id = 0;
                if (!mm.FastRead(base + g.off_id, &id, 4)) continue;
                if (id == 0) continue;
                // hash doğrulaması — üç profil
                HashProfile matched;
                bool any = false;
                for (auto& prof : try_profiles) {
                    if (ImHashStr(name.c_str(), 0, prof) == id) { matched = prof; any = true; break; }
                }
                if (!any) continue;
                // ikincil doğrulama: flags + geometri
                u32 flags = 0;
                if (!mm.FastRead(base + g.off_flags, &flags, 4) || !ValidWinFlags(flags)) continue;
                float pos[2], size[2], sizefull[2];
                if (!mm.FastRead(base + g.off_pos, pos, 8)) continue;
                if (!mm.FastRead(base + g.off_size, size, 8)) continue;
                if (!mm.FastRead(base + g.off_sizefull, sizefull, 8)) continue;
                if (!PlausibleGeom(pos[0]) || !PlausibleGeom(pos[1])) continue;
                if (!PlausibleGeom(size[0]) || !PlausibleGeom(size[1])) continue;
                if (size[0] < 0 || size[1] < 0) continue;

                LiveWindow w;
                w.obj = base;
                w.name_ptr = namePtr;
                w.name = name;
                w.id = id;
                w.hash = matched;
                w.layout_family = gi;
                w.pos = Vec2(pos[0], pos[1]);
                w.size = Vec2(size[0], size[1]);
                w.size_full = Vec2(sizefull[0], sizefull[1]);
                w.flags = flags;
                w.off_pos = g.off_pos; w.off_size = g.off_size; w.off_sizefull = g.off_sizefull;
                w.off_flags = g.off_flags;
                out.push_back(std::move(w));
                break;  // bu adres için ilk eşleşen hipotez yeter
            }
        }
    }
    // aynı isim + aynı id → tekrarları ele (eski kopyalar olabilir)
    std::vector<LiveWindow> dedup;
    for (auto& w : out) {
        bool dup = false;
        for (auto& d : dedup) {
            if (d.id == w.id && d.name == w.name) { dup = true; break; }
        }
        if (!dup) dedup.push_back(w);
    }
    return dedup;
}

// ---------------------------------------------------------------------------
// Context keşfi
// ---------------------------------------------------------------------------
bool FindContext(MemoryMap& mm, std::vector<LiveWindow>& windows, ContextInfo* out) {
    if (windows.empty()) return false;
    int psize = mm.ptr_size();

    // pencere işaretçi seti
    std::unordered_map<u64, int> winset;
    for (int i = 0; i < (int)windows.size(); i++) winset[windows[i].obj] = i;

    // 1) ardışık pencere işaretçisi KOŞULARI (tümü toplanır — en uzun olan
    //    bir kopya olabilir; gerçek g.Windows.Data farklı koşuyu gösteriyor olabilir)
    struct Run { u64 start; int len; };
    std::vector<Run> runs;
    for (auto* pr : mm.HeapRegions()) {
        MemoryMap::RegionData rd;
        if (!mm.GetRegionData(pr->base + 1, &rd)) continue;
        if (rd.size < 0x20) continue;
        int cur = 0;
        u64 cur_start = 0;
        for (size_t off = 0; off + (size_t)psize <= rd.size; off += (size_t)psize) {
            u64 v = 0;
            memcpy(&v, rd.data + off, psize);
            if (winset.count(v)) {
                if (cur == 0) cur_start = rd.base + off;
                cur++;
            } else {
                if (cur >= 2) runs.push_back({cur_start, cur});
                cur = 0;
            }
        }
        if (cur >= 2) runs.push_back({cur_start, cur});
    }
    if (runs.empty()) return false;
    std::stable_sort(runs.begin(), runs.end(), [](auto& a, auto& b) { return a.len > b.len; });

    // 2) her koşu adayı için: Data == run.start olan ImVector ara
    u64 vec_addr = 0, buf_addr = 0;
    int vec_size = 0, run_len = 0;
    for (auto& run : runs) {
        for (auto* pr : mm.HeapRegions()) {
            MemoryMap::RegionData rd;
            if (!mm.GetRegionData(pr->base + 1, &rd)) continue;
            for (size_t off = 0; off + (size_t)psize <= rd.size; off += 4) {
                u64 v = 0;
                memcpy(&v, rd.data + off, psize);
                if (v == run.start && off >= 8) {
                    u32 sz = 0, cap = 0;
                    memcpy(&sz, rd.data + off - 8, 4);
                    memcpy(&cap, rd.data + off - 4, 4);
                    if (sz >= 1 && sz <= 1024 && cap >= sz && cap < 8192) {
                        vec_addr = rd.base + off - 8;
                        vec_size = (int)sz;
                        buf_addr = run.start;
                        run_len = run.len;
                        break;
                    }
                }
            }
            if (vec_addr) break;
        }
        if (vec_addr) break;
    }
    if (!vec_addr) {
        // Fallback: koşu biliniyor; vektör aranamadı → vektörsüz devam
        buf_addr = runs[0].start;
        run_len = runs[0].len;
        out->windows_buf = buf_addr;
        out->windows_count = run_len;
    }

    // 3) BİRİNCİL YÖNTEM: pencere nesnesinin kendi Ctx işaretçisi (family B/C:
    //    ImGuiWindow.Ctx@0) — en sağlam kaynak. Aday context'leri topla.
    std::vector<u64> ctx_candidates;
    {
        std::unordered_map<u64, bool> seen;
        for (auto& w : windows) {
            if (w.layout_family <= 0) continue;  // A ailesinde Ctx alanı yok
            u64 c = 0;
            if (mm.FastRead(w.obj, &c, psize) && c > 0x10000 && mm.IsReadable(c) && !seen.count(c)) {
                seen[c] = true;
                ctx_candidates.push_back(c);
            }
        }
    }
    // vektör tabanlı aday (varsa) da ekle
    std::vector<std::pair<u64, u64>> all_candidates;  // (ctx, vec_addr)
    if (vec_addr) {
        auto& layouts0 = LayoutDB::All();
        for (auto& L : layouts0) {
            int offWin = L.get("ImGuiContext.Windows", -1);
            if (offWin < 0) continue;
            all_candidates.push_back({vec_addr - offWin, vec_addr});
        }
    }
    for (u64 c : ctx_candidates)
        all_candidates.push_back({c, vec_addr});

    // 4) her aday × her versiyon layout'u: doğrula
    auto& layouts = LayoutDB::All();
    std::unordered_map<u64, bool> tried;
    for (auto& [ctx, vec_used] : all_candidates) {
    if (tried.count(ctx)) continue;
    tried[ctx] = true;
    for (int li = 0; li < (int)layouts.size(); li++) {
        auto& L = layouts[li];
        int offWin = L.get("ImGuiContext.Windows", -1);
        if (offWin < 0) continue;
        u64 vec_here = ctx + offWin;
        // bu ctx'in Windows vektörü: Size/Cap/Data oku ve geri-bağ doğrula
        {
            u32 sz = 0, cap = 0; u64 data = 0;
            if (!mm.FastRead(vec_here, &sz, 4)) continue;
            if (!mm.FastRead(vec_here + 4, &cap, 4)) continue;
            if (!mm.FastRead(vec_here + 8, &data, psize)) continue;
            if (sz == 0 || sz > 4096 || cap < sz || cap > 8192 || data < 0x10000) continue;
            // geri-bağ: data buffer'ında bilinen pencere var mı?
            int known = 0;
            for (u32 i = 0; i < sz && i < 4096; i++) {
                u64 wp = 0;
                if (mm.FastRead(data + (u64)i * psize, &wp, psize) && winset.count(wp)) known++;
            }
            if (known < 1) continue;
        }
        int offIO = L.get("ImGuiContext.IO", -1);
        if (offIO < 0) continue;
        u64 io = ctx + offIO;
        // IO.DisplaySize doğrulaması
        int offDS = L.get("ImGuiIO.DisplaySize", -1);
        if (offDS < 0) continue;
        float ds[2] = {0, 0};
        if (!mm.FastRead(io + offDS, ds, 8)) continue;
        if (!Fin(ds[0]) || !Fin(ds[1]) || ds[0] <= 0 || ds[1] <= 0 || ds[0] > 16384 || ds[1] > 16384)
            continue;
        // IO.Fonts doğrulaması
        int offFonts = L.get("ImGuiIO.Fonts", -1);
        if (offFonts >= 0) {
            u64 fonts = 0;
            if (!mm.FastRead(io + offFonts, &fonts, psize) || fonts < 0x10000) continue;
            if (!mm.IsReadable(fonts)) continue;
        }
        // IO.Framerate
        int offFR = L.get("ImGuiIO.Framerate", -1);
        float fr = 0;
        if (offFR >= 0 && mm.FastRead(io + offFR, &fr, 4)) {
            if (!Fin(fr) || fr < 0 || fr > 5000) continue;
        }
        // WindowsActiveCount
        int offWAC = L.get("ImGuiContext.WindowsActiveCount", -1);
        int wac = -1;
        if (offWAC >= 0) mm.FastRead(ctx + offWAC, &wac, 4);
        if (wac > (int)windows.size() + 8 || wac < 0) wac = -1;

        // BAŞARDI — bu layout doğrulandı
        out->addr = ctx;
        out->windows_vec = vec_here;
        out->layout = &L;
        out->layout_index = li;
        out->version = L.version;
        out->version_num = L.version_num;
        out->confirmed = true;
        out->io = io;
        out->display_size = Vec2(ds[0], ds[1]);
        out->framerate = fr;
        out->windows_vec = vec_addr;
        out->windows_buf = buf_addr;
        out->windows_count = vec_size > 0 ? vec_size : run_len;
        if (wac >= 0) out->windows_active_count = wac;
        int off;
        if ((off = L.get("ImGuiContext.ActiveId", -1)) >= 0) mm.FastRead(ctx + off, &out->active_id, 4);
        if ((off = L.get("ImGuiContext.HoveredId", -1)) >= 0) mm.FastRead(ctx + off, &out->hovered_id, 4);
        if ((off = L.get("ImGuiContext.CurrentWindow", -1)) >= 0) mm.FastRead(ctx + off, &out->current_window, psize);
        if ((off = L.get("ImGuiContext.HoveredWindow", -1)) >= 0) mm.FastRead(ctx + off, &out->hovered_window, psize);
        if ((off = L.get("ImGuiContext.MovingWindow", -1)) >= 0) mm.FastRead(ctx + off, &out->moving_window, psize);
        if ((off = L.get("ImGuiContext.LastItemData", -1)) >= 0) {
            out->last_item_data = ctx + off;
            int offID = L.get("ImGuiLastItemData.ID", -1);
            if (offID >= 0) mm.FastRead(out->last_item_data + offID, &out->last_item_id, 4);
        }
        // vektör içeriğini oku
        if (out->windows_buf) {
            out->window_objs.clear();
            for (int i = 0; i < out->windows_count; i++) {
                u64 wp = 0;
                if (mm.FastRead(out->windows_buf + (u64)i * psize, &wp, psize) && wp)
                    out->window_objs.push_back(wp);
            }
        }
        // style
        int offStyle = L.get("ImGuiContext.Style", -1);
        if (offStyle >= 0) {
            u64 st = ctx + offStyle;
            auto rd2 = [&](const char* k, float* f, int n) {
                int o = L.get(k, -1);
                if (o >= 0) mm.FastRead(st + o, f, 4 * n);
            };
            float fp[2], is[2], wp2[2];
            rd2("ImGuiStyle.FramePadding", fp, 2);
            rd2("ImGuiStyle.ItemSpacing", is, 2);
            rd2("ImGuiStyle.WindowPadding", wp2, 2);
            out->style_frame_padding = Vec2(fp[0], fp[1]);
            out->style_item_spacing = Vec2(is[0], is[1]);
            out->style_window_padding = Vec2(wp2[0], wp2[1]);
            float fh = 0;
            rd2("ImGuiStyle.FramePadding", &fh, 1);
            out->style_frame_height = fp[1] * 2.0f;
            rd2("ImGuiStyle.IndentSpacing", &out->style_indent_spacing, 1);
            rd2("ImGuiStyle.ScrollbarSize", &out->style_scrollbar_size, 1);
            rd2("ImGuiStyle.Alpha", &out->style_alpha, 1);
            out->style_ok = true;
        }
        return true;
    }
    }  // ctx aday dongusu
    return false;
}

// ---------------------------------------------------------------------------
// Pencere zenginleştirme
// ---------------------------------------------------------------------------
void EnrichWindows(MemoryMap& mm, std::vector<LiveWindow>& windows, ContextInfo& ctx) {
    if (!ctx.layout) return;
    auto& L = *ctx.layout;
    int psize = mm.ptr_size();
    // vektör üyeliği + sıra
    std::unordered_map<u64, int> order;
    for (int i = 0; i < (int)ctx.window_objs.size(); i++) order[ctx.window_objs[i]] = i;

    for (auto& w : windows) {
        // DB offset'leriyle zengin okuma (layout ailesi DB ile uyuşmalı)
        auto rd_off = [&](const char* key, void* dst, size_t n) -> bool {
            int o = L.get(key, -1);
            if (o < 0) return false;
            return mm.FastRead(w.obj + o, dst, n);
        };
        u8 act = 0, was = 0, col = 0, fb = 0, isch = 0;
        if (rd_off("ImGuiWindow.Active", &act, 1)) w.active = act != 0;
        if (rd_off("ImGuiWindow.WasActive", &was, 1)) w.was_active = was != 0;
        if (rd_off("ImGuiWindow.Collapsed", &col, 1)) w.collapsed = col != 0;
        if (rd_off("ImGuiWindow.IsFallbackWindow", &fb, 1)) w.fallback = fb != 0;
        if (rd_off("ImGuiWindow.IsExplicitChild", &isch, 1)) w.child = isch != 0;
        if (!w.child) {
            u32 cf = 0;
            if (rd_off("ImGuiWindow.ChildFlags", &cf, 4)) w.childflags = cf;
        }
        rd_off("ImGuiWindow.ParentWindow", &w.parent, psize);
        rd_off("ImGuiWindow.RootWindow", &w.root, psize);
        rd_off("ImGuiWindow.DrawList", &w.draw_list, psize);
        short bo = 0;
        if (rd_off("ImGuiWindow.BeginOrderWithinContext", &bo, 2)) w.begin_order = bo;
        auto it = order.find(w.obj);
        if (it != order.end()) w.in_windows_vector = true;
    }
}

// ---------------------------------------------------------------------------
// Draw-list metin çözümü (font glif UV → karakter)
// ---------------------------------------------------------------------------
void DecodeDrawListTexts(MemoryMap& mm, LiveWindow& w, const ContextInfo& ctx) {
    if (!ctx.layout || !w.draw_list) return;
    auto& L = *ctx.layout;
    int psize = mm.ptr_size();

    // ImDrawList alanları
    int offCmd = L.get("ImDrawList.CmdBuffer", -1);
    int offVtx = L.get("ImDrawList.VtxBuffer", -1);
    int offOwner = L.get("ImDrawList._OwnerName", -1);
    if (offVtx < 0 || offCmd < 0) return;
    // sahiplik kontrolü: _OwnerName == w.name_ptr
    if (offOwner >= 0) {
        u64 owner = 0;
        if (mm.FastRead(w.draw_list + offOwner, &owner, psize) && owner && owner != w.name_ptr) {
            // farklı pencerenin listesi olabilir (child'lar parent listesini kullanır) — yine de dene
        }
    }

    // vektör oku: {Size, Cap, Data}
    auto rdvec = [&](u64 vecAddr, u64* dataOut, int* sizeOut) -> bool {
        int sz = 0;
        u64 data = 0;
        if (!mm.FastRead(vecAddr, &sz, 4)) return false;
        if (!mm.FastRead(vecAddr + 8, &data, psize)) return false;
        if (sz < 0 || sz > 8 * 1024 * 1024) return false;
        *sizeOut = sz;
        *dataOut = data;
        return true;
    };
    u64 vtxData = 0, cmdData = 0;
    int vtxSize = 0, cmdSize = 0;
    if (!rdvec(w.draw_list + offVtx, &vtxData, &vtxSize)) return;
    if (!rdvec(w.draw_list + offCmd, &cmdData, &cmdSize)) return;
    if (!vtxData || !cmdData) return;

    // font atlası: IO.Fonts → atlas.Fonts → ilk font → Glyphs + IndexLookup
    int offFonts = L.get("ImGuiIO.Fonts", -1);
    if (offFonts < 0) return;
    u64 atlas = 0;
    if (!mm.FastRead(ctx.io + offFonts, &atlas, psize) || !atlas) return;
    int offAtlasFonts = L.get("ImFontAtlas.Fonts", -1);
    if (offAtlasFonts < 0) return;
    u64 fontsData = 0;
    int fontsSize = 0;
    if (!rdvec(atlas + offAtlasFonts, &fontsData, &fontsSize) || fontsSize < 1 || !fontsData) return;
    u64 font0 = 0;
    if (!mm.FastRead(fontsData, &font0, psize) || !font0) return;

    // glif tablosu
    int offGlyphs = L.get("ImFont.Glyphs", -1);
    int offGlyphSz = -1;
    if (offGlyphs < 0) return;
    u64 glyphsData = 0;
    int glyphCount = 0;
    if (!rdvec(font0 + offGlyphs, &glyphsData, &glyphCount) || glyphCount <= 0 || !glyphsData) return;
    int glyphSize = L.get("ImFontGlyph.sizeof", 20);
    if (glyphSize < 20) return;

    // glif alan offset'leri (ImFontGlyph: Codepoint, X0,Y0,X1,Y1, XAdvance, U0,V0,U1,V1)
    // imzadan çöz: sizeof=20 → klasik düzen; 1.92+ font yenilemesi da aynı alan sırası
    struct Glyph { u32 cp; float u0, v0, u1, v1, x1; };
    std::vector<Glyph> glyphs;
    glyphs.reserve(glyphCount);
    for (int i = 0; i < glyphCount; i++) {
        u8 buf[40];
        if (glyphSize > 40 || !mm.FastRead(glyphsData + (u64)i * glyphSize, buf, glyphSize)) return;
        u32 cp;
        float x0, y0, x1, y1, adv, u0, v0, u1, v1;
        if (glyphSize == 20) {
            memcpy(&cp, buf, 4);
            memcpy(&x0, buf + 4, 4); memcpy(&y0, buf + 8, 4); memcpy(&x1, buf + 12, 4);
            memcpy(&adv, buf + 16, 4);
            u0 = v0 = u1 = v1 = 0;  // 1.76-1.89: UV'ler ayrı yapıda yok — ImFontGlyph'te UV VAR mı?
            // Gerçekte: 1.76+: Codepoint, X0..Y1(4 float), XAdvance → 20 bayt, UV YOK!
            u1 = x1; (void)u0; (void)v0; (void)v1; (void)adv; (void)x0; (void)y0;
        } else {
            // 1.90+: Codepoint(4) + X0,Y0,X1,Y1(16) + XAdvance(4) + U0,V0,U1,V1(16) = 40
            memcpy(&cp, buf, 4);
            memcpy(&x0, buf + 4, 4); memcpy(&y0, buf + 8, 4); memcpy(&x1, buf + 12, 4);
            memcpy(&adv, buf + 16, 4);
            memcpy(&u0, buf + 20, 4); memcpy(&v0, buf + 24, 4);
            memcpy(&u1, buf + 28, 4); memcpy(&v1, buf + 32, 4);
            (void)x0; (void)y0; (void)adv;
        }
        if (glyphSize >= 40)
            glyphs.push_back({cp, u0, v0, u1, v1, x1});
        else {
            // UV'siz eski glifler: metin çözümü atlanır (texel konumundan hesap gerekir)
        }
    }
    if (glyphs.empty()) return;

    // draw komutlarını gez: ClipRect + ElemCount; vtx offset'li glif dörtgenlerini çöz
    int offCmdClip = L.get("ImDrawCmd.ClipRect", -1);
    int offCmdElem = L.get("ImDrawCmd.ElemCount", -1);
    int offCmdVtxOff = L.get("ImDrawCmd.VtxOffset", -1);
    int offCmdIdxOff = L.get("ImDrawCmd.IdxOffset", -1);
    int offVertPos = L.get("ImDrawVert.pos", 0);
    int offVertUv = L.get("ImDrawVert.uv", 8);
    int vertSize = L.get("ImDrawVert.sizeof", 20);
    if (offCmdClip < 0 || offCmdElem < 0 || vertSize <= 0) return;
    int cmdSizeof = L.get("ImDrawCmd.sizeof", 0);
    if (cmdSizeof <= 0) return;

    u64 vtxRead = 0;  // okunan maksimum vertex (VtxOffset dahil)
    struct PendingRun { std::string text; float y0 = 0, y1 = 0, x0 = 1e9f, x1 = -1e9f; };
    std::vector<PendingRun> runs;
    u64 elemBase = 0;
    for (int ci = 0; ci < cmdSize; ci++) {
        u8 cmdbuf[64];
        if (!mm.FastRead(cmdData + (u64)ci * cmdSizeof, cmdbuf, std::min(cmdSizeof, 64))) break;
        u32 elem = 0;
        memcpy(&elem, cmdbuf + offCmdElem, 4);
        if (elem == 0) continue;
        u32 vtxOff = 0;
        if (offCmdVtxOff >= 0) memcpy(&vtxOff, cmdbuf + offCmdVtxOff, 4);
        (void)elemBase;
        // glif dörtgenlerini tara (4 vertex/quad varsayımı — idx buffer kullanmadan kaba tarama:
        // metin dörtgenleri vtx bufferında ardışık 4'lü gruplar halinde; font dokusuna ait UV'ler)
        // Not: kesin çözüm idx buffer gerektirir; kaba mod yeterli (test hedefinde doğrulandı)
        for (int q = 0; q + 4 <= (int)elem / 6 + 4 && q < 4096; q += 4) {
            u8 vbuf[4][40];
            u64 vbase = vtxData + (u64)(vtxOff + (u32)q) * vertSize;
            bool all = true;
            for (int k = 0; k < 4; k++)
                if (!mm.FastRead(vbase + (u64)k * vertSize, vbuf[k], vertSize)) { all = false; break; }
            if (!all) break;
            float pos[4][2], uv[4][2];
            for (int k = 0; k < 4; k++) {
                memcpy(pos[k], vbuf[k] + offVertPos, 8);
                memcpy(uv[k], vbuf[k] + offVertUv, 8);
            }
            // glif eşleştir (uv merkez)
            float uc = (uv[0][0] + uv[2][0]) * 0.5f, vc = (uv[0][1] + uv[2][1]) * 0.5f;
            const Glyph* g = nullptr;
            for (auto& gg : glyphs) {
                if (uc >= gg.u0 && uc < gg.u1 && vc >= gg.v0 && vc < gg.v1) { g = &gg; break; }
            }
            if (!g || g->cp < 0x20) continue;
            PendingRun pr;
            pr.text.push_back((char)g->cp < 0x20 ? '?' : (char)g->cp);
            pr.x0 = std::min(std::min(pos[0][0], pos[1][0]), std::min(pos[2][0], pos[3][0]));
            pr.x1 = std::max(std::max(pos[0][0], pos[1][0]), std::max(pos[2][0], pos[3][0]));
            pr.y0 = std::min(std::min(pos[0][1], pos[1][1]), std::min(pos[2][1], pos[3][1]));
            pr.y1 = std::max(std::max(pos[0][1], pos[1][1]), std::max(pos[2][1], pos[3][1]));
            // aynı satırda önceki run'a ekle (x yakın & y aynı)
            bool merged = false;
            for (auto it = runs.rbegin(); it != runs.rend(); ++it) {
                if (fabsf(it->y0 - pr.y0) < 2.0f && pr.x0 >= it->x0 - 2.0f && pr.x0 <= it->x1 + 14.0f) {
                    it->text += pr.text;
                    it->x1 = std::max(it->x1, pr.x1);
                    it->x0 = std::min(it->x0, pr.x0);
                    merged = true;
                    break;
                }
            }
            if (!merged) runs.push_back(pr);
        }
    }
    for (auto& r : runs) {
        if (r.text.empty()) continue;
        LiveWindow::TextRun tr;
        tr.text = r.text;
        tr.a = Vec2(r.x0, r.y0);
        tr.b = Vec2(r.x1, r.y1);
        w.text_runs.push_back(tr);
    }
}

} // namespace umvp
