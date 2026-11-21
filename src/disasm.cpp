// ============================================================================
// src/disasm.cpp — Derin disassembly motoru uygulaması (Capstone 5/6 uyumlu)
// ============================================================================
#include "umvp/disasm.h"
#include <capstone/capstone.h>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace umvp {

// ---------------------------------------------------------------------------
// DisasmEngine
// ---------------------------------------------------------------------------
DisasmEngine::DisasmEngine(Arch arch) : arch_(arch) {
    cs_mode mode = (arch == Arch::X64) ? CS_MODE_64 : CS_MODE_32;
    csh h = 0;
    if (cs_open(CS_ARCH_X86, mode, &h) == CS_ERR_OK) {
        cs_option(h, CS_OPT_DETAIL, CS_OPT_ON);
        handle_ = (void*)h;
    }
}

DisasmEngine::~DisasmEngine() {
    if (handle_) cs_close((csh*)&handle_);
}

static bool GroupHas(cs_detail* d, cs_group_type g) {
    for (u8 k = 0; k < d->groups_count; k++)
        if (d->groups[k] == (u8)g) return true;
    return false;
}

std::vector<Insn> DisasmEngine::Disassemble(const u8* data, size_t size, u64 base) {
    std::vector<Insn> out;
    if (!handle_) return out;
    csh h = (csh)handle_;
    const u8* p = data;
    u64 addr = base;
    size_t remain = size;
    // doğrusal; hatalı baytta 1 bayt atla (kod içinde veri olabilir)
    while (remain > 0) {
        cs_insn* ci_arr = nullptr;
        size_t cnt = cs_disasm(h, p, remain, addr, 1, &ci_arr);
        if (cnt == 0 || !ci_arr) {
            p += 1; addr += 1; remain -= 1;
            continue;
        }
        cs_insn* ci = &ci_arr[0];
        Insn I;
        I.addr = ci->address;
        I.size = ci->size;
        I.mnemonic = ci->mnemonic;
        I.op_str = ci->op_str;
        I.is_call = GroupHas(ci->detail, CS_GRP_CALL);
        I.is_ret = strcmp(ci->mnemonic, "ret") == 0 || strcmp(ci->mnemonic, "retn") == 0;
        if (GroupHas(ci->detail, CS_GRP_JUMP)) {
            I.is_jmp = strcmp(ci->mnemonic, "jmp") == 0;
            I.is_jcc = !I.is_jmp;
        }
        // x86 detay analizi
        cs_x86* xd = &ci->detail->x86;
        for (u8 oi = 0; oi < xd->op_count; oi++) {
            cs_x86_op* op = &xd->operands[oi];
            if (op->type == X86_OP_MEM && op->mem.base == X86_REG_RIP) {
                u64 t = ci->address + ci->size + op->mem.disp;
                if (I.rip_target == 0) I.rip_target = t;
            }
            if (op->type == X86_OP_IMM) {
                // dallanma hedefi (capstone mutlak verir) veya immediate veri
                if (I.is_call || I.is_jmp || I.is_jcc) {
                    if (I.branch_target == 0) I.branch_target = (u64)op->imm;
                } else {
                    if (I.imm == 0 && op->imm != 0) I.imm = (u64)op->imm;
                    if (strncmp(ci->mnemonic, "push", 4) == 0) {
                        I.is_push_imm = true;
                        I.imm = (u64)op->imm;
                    }
                }
            }
        }
        out.push_back(std::move(I));
        p += ci->size;
        addr += ci->size;
        remain -= ci->size;
        cs_free(ci_arr, cnt);
    }
    return out;
}

std::vector<Insn> DisasmEngine::DisassembleRange(u64 start, size_t size) {
    // bellekten oku (MemoryMap erişimi için global değil — çağıran sağlamalı)
    // NOTE: bu yardımcı yalız ca çağıranlar için; bellek okuması çağıran tarafında yapılır
    (void)start; (void)size;
    return {};
}

// ---------------------------------------------------------------------------
// XrefMap
// ---------------------------------------------------------------------------
const std::vector<XrefSite>* XrefMap::SitesTo(u64 target) const {
    auto it = by_target_.find(target);
    return it == by_target_.end() ? nullptr : &it->second;
}

std::vector<XrefSite> XrefMap::SitesInRange(u64 lo, u64 hi) const {
    std::vector<XrefSite> out;
    for (auto& [tgt, sites] : by_target_) {
        (void)tgt;
        for (auto& s : sites)
            if (s.site >= lo && s.site < hi) out.push_back(s);
    }
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.site < b.site; });
    return out;
}

std::vector<XrefSite> XrefMap::CodeRefsFrom(u64 insn_addr) const {
    // site == insn_addr olan kayıtlar
    std::vector<XrefSite> out;
    for (auto& [tgt, sites] : by_target_) {
        (void)tgt;
        for (auto& s : sites)
            if (s.site == insn_addr) out.push_back(s);
    }
    return out;
}

void XrefMap::Build(MemoryMap& mm, DisasmEngine& de, const std::function<void(int, int)>& progress) {
    by_target_.clear();
    total_ = 0;
    auto regions = mm.ImageRegions();
    int done = 0;
    for (auto* pr : regions) {
        if (!pr->exec()) continue;
        ++done;
        if (progress) progress(done, (int)regions.size());
        MemoryMap::RegionData rd;
        if (!mm.GetRegionData(pr->base + 1, &rd)) continue;
        // disassemble tüm bölge (uzunsa parça parça)
        const size_t CHUNK = 4 * 1024 * 1024;
        for (size_t off = 0; off < rd.size; off += CHUNK) {
            size_t len = (size_t)std::min<u64>(CHUNK, rd.size - off);
            auto insns = de.Disassemble(rd.data + off, len, rd.base + off);
            for (auto& I : insns) {
                auto add = [&](u64 tgt, XrefKind k) {
                    if (tgt == 0) return;
                    XrefSite s{I.addr, tgt, k};
                    by_target_[tgt].push_back(s);
                    total_++;
                    if (I.addr < min_code_) min_code_ = I.addr;
                    if (I.addr > max_code_) max_code_ = I.addr;
                };
                if (I.is_call) add(I.branch_target, XR_CALL);
                else if (I.is_jmp) add(I.branch_target, XR_JMP);
                if (I.rip_target) add(I.rip_target, XR_LEA);
                if (I.is_push_imm) add(I.imm, XR_PUSH_IMM);
                if (I.imm && !I.is_push_imm && strncmp(I.mnemonic.c_str(), "mov", 3) == 0)
                    add(I.imm, XR_MOV_IMM);
                // x86 mutlak bellek erişimi [imm32]
                if (de.arch() == Arch::X86 && I.rip_target == 0) {
                    // capstone x86'da [imm] disp absolute'u op.mem.disp içinde
                    // (Disassemble içinde işlemedik — burada ayrıştıramayız; movabs/push yeterli)
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// FuncBoundsResolver
// ---------------------------------------------------------------------------
void FuncBoundsResolver::LoadPData(const ModuleEntry& mod) {
    // ÇOKLU MODÜL: tüm modüllerin RUNTIME_FUNCTION tabloları tek sıralı listede
    if (mod.pdata_rva == 0 || mod.pdata_size < 12) return;
    size_t n = mod.pdata_size / 12;
    if (n > 1'000'000) n = 1'000'000;
    std::vector<u8> buf(n * 12);
    size_t got = mm_.Read(mod.base + mod.pdata_rva, buf.data(), buf.size());
    if (got < 12) return;
    n = got / 12;
    size_t before = pdata_.size();
    for (size_t i = 0; i < n; i++) {
        u32 beg, end;
        memcpy(&beg, &buf[i * 12], 4);
        memcpy(&end, &buf[i * 12 + 4], 4);
        if (beg == 0 && end == 0) break;
        if (end <= beg) continue;
        pdata_.push_back({mod.base + beg, mod.base + end});
    }
    // yeniden sırala (modül eklemeleri sırayı bozabilir)
    if (pdata_.size() != before)
        std::sort(pdata_.begin(), pdata_.end(), [](auto& a, auto& b) { return a.beg < b.beg; });
}

bool FuncBoundsResolver::FromPData(u64 addr, FuncBounds* out) const {
    if (pdata_.empty()) return false;
    // binary search: ilk entry with end > addr
    size_t lo = 0, hi = pdata_.size();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (pdata_[mid].end <= addr) lo = mid + 1;
        else hi = mid;
    }
    if (lo < pdata_.size() && pdata_[lo].beg <= addr && addr < pdata_[lo].end) {
        out->start = pdata_[lo].beg;
        out->end = pdata_[lo].end;
        out->exact = true;
        out->how = ".pdata (RUNTIME_FUNCTION)";
        return true;
    }
    return false;
}

bool IsPaddingRun(const u8* p, size_t n) {
    // CC / 90 / 00 karışımı padding
    for (size_t i = 0; i < n; i++) {
        u8 c = p[i];
        if (c != 0xCC && c != 0x90 && c != 0x00) return false;
    }
    return true;
}

bool FuncBoundsResolver::Heuristic(u64 inside_addr, FuncBounds* out, int max_back, int max_fwd) const {
    MemoryMap::RegionData rd;
    if (!mm_.GetRegionDataForRange(inside_addr, 1, &rd)) return false;
    u64 off = inside_addr - rd.base;
    if (off >= rd.size) return false;

    // 1) geriye doğru padding ara (min 2 bayt CC/90 serisi)
    u64 scan_lo = (off > (u64)max_back) ? off - max_back : 0;
    u64 start_off = ~0ull;
    for (u64 i = off; i > scan_lo + 1; i--) {
        // i-1..i konumunda padding biter mi? → [i-2] ve [i-1] padding, [i] değil olmalı
        if (i + 1 < rd.size && i >= 2) {
            if (IsPaddingRun(rd.data + i - 2, 2) && rd.data[i] != 0xCC && rd.data[i] != 0x90) {
                start_off = i;
                break;
            }
        }
        if (rd.data[i - 1] == 0xCC && rd.data[i] != 0xCC && rd.data[i] != 0x90 && i >= 1) {
            start_off = i;
            break;
        }
    }
    if (start_off == ~0ull) {
        // 16 bayt hizalı prolog tara
        for (u64 i = off & ~0xFull; i > scan_lo; i -= 16) {
            u8 b = rd.data[i];
            bool pro = (b == 0x55 || b == 0x53 || b == 0x41 || b == 0x48 || b == 0x56 || b == 0x57);
            if (pro) { start_off = i; break; }
        }
    }
    if (start_off == ~0ull) return false;

    // 2) ileriye doğru disassemble; ret sonrası padding/sonraki prolog → bitiş
    u64 avail = std::min<u64>(rd.size - start_off, (u64)max_fwd);
    auto insns = de_.Disassemble(rd.data + start_off, (size_t)avail, rd.base + start_off);
    if (insns.empty()) return false;
    // inside_addr'e ulaşmalı (doğrusal)
    bool reached = false;
    for (auto& I : insns) {
        if (I.addr <= inside_addr && inside_addr < I.addr + I.size) { reached = true; break; }
        if (I.addr > inside_addr) break;
    }
    if (!reached) return false;

    // bitiş: ret + padding ya da jmp-out + padding
    for (size_t i = 0; i < insns.size(); i++) {
        auto& I = insns[i];
        if (I.is_ret && I.addr > inside_addr) {
            u64 nxt = I.addr + I.size;
            u64 no = nxt - rd.base;
            if (no + 2 <= rd.size) {
                if (IsPaddingRun(rd.data + no, 2)) {
                    out->start = rd.base + start_off;
                    out->end = nxt;
                    out->exact = false;
                    out->how = "sezgisel (ret+padding)";
                    return true;
                }
            }
        }
        if (I.is_jmp && I.addr > inside_addr) {
            // fonksiyon dışına kuyruk zıplaması + padding
            u64 nxt = I.addr + I.size;
            u64 no = nxt - rd.base;
            if (no + 2 <= rd.size && IsPaddingRun(rd.data + no, 2)) {
                out->start = rd.base + start_off;
                out->end = nxt;
                out->exact = false;
                out->how = "sezgisel (jmp+padding)";
                return true;
            }
        }
    }
    // son çare: son ret
    for (auto it = insns.rbegin(); it != insns.rend(); ++it) {
        if (it->is_ret) {
            out->start = rd.base + start_off;
            out->end = it->addr + it->size;
            out->exact = false;
            out->how = "sezgisel (son ret)";
            return true;
        }
    }
    return false;
}

FuncBounds FuncBoundsResolver::Resolve(u64 inside_addr) const {
    FuncBounds fb;
    if (FromPData(inside_addr, &fb)) return fb;
    if (Heuristic(inside_addr, &fb)) return fb;
    fb.start = fb.end = 0;
    return fb;
}

// ---------------------------------------------------------------------------
// Backward-slice argüman kurtarma
// ---------------------------------------------------------------------------
namespace {

inline bool IsFiniteF(float f) { return std::isfinite(f); }
inline bool PlausibleFloat(u32 bits) {
    float f;
    memcpy(&f, &bits, 4);
    return IsFiniteF(f) && fabsf(f) < 1e7f;
}

struct SlotState {
    int st = 0;          // 0=Unknown 1=Const 2=LeaRip 3=StackSlot
    u64 val = 0;
    bool is_float = false;
};

} // namespace

// not: Insn'de reg bilgisi detaylı değil; basit opcode ayrıştırması yapacağız
// (mov r64, imm64 / lea r64,[rip+x] / push imm vb. zaten Insn'de işlendi:
//  rip_target, imm, is_push_imm var). Yazılan register'ı op_str'den çıkarırız.
CallSiteArgs ExtractCallArgs(MemoryMap& mm, DisasmEngine& de,
                             const std::vector<Insn>& func, size_t call_idx) {
    // DOĞRU BACKWARD-SLICE SEMANTİĞİ:
    //   Yazmaç durumu, geriye yürüyüşte İLK atamayla DONAR (ileri sırada son yazma =
    //   çağrı anındaki değer). Sonraki (ileri-sırada daha eski) yazmalar ezmez.
    //   Slot yazmaları sembolik tutulur; kaynak yazmacın değeri yürüyüşte
    //   netleşince çözülür (pending).
    CallSiteArgs out;
    out.analyzed = true;
    bool x64 = de.arch() == Arch::X64;
    bool trace = getenv("UMVP_ARGTRACE2") && func[call_idx].addr == strtoull(getenv("UMVP_ARGTRACE2"), nullptr, 0);
    if (call_idx >= func.size()) return out;

    static const char* kGp64[4] = {"rcx", "rdx", "r8", "r9"};
    static const char* kGp64d[4] = {"ecx", "edx", "r8d", "r9d"};

    enum class RS { Unknown, Const, LeaRip, StackSlot };
    struct RegState { RS st = RS::Unknown; u64 val = 0; };

    RegState rs[4];
    bool rs_frozen[4] = {false, false, false, false};
    std::unordered_map<std::string, RegState> carrier;      // rax/rbx/rsi/rdi/r12-r15/rbp
    std::unordered_map<std::string, SlotState> xmm;          // xmm0-3
    std::unordered_map<u64, SlotState> slots;                // rsp+off → içerik
    std::unordered_map<u64, std::string> pending_slots;      // off → kaynak yazmaç adı
    std::unordered_map<u64, std::string> pending_slots_xmm;  // off → xmm adı

    auto reg_index_of = [&](const std::string& name) -> int {
        for (int i = 0; i < 4; i++)
            if (name == kGp64[i] || (x64 && name == kGp64d[i])) return i;
        return -1;
    };
    auto set_reg_once = [&](int ai, RS st, u64 val) {
        if (ai >= 0 && ai < 4 && !rs_frozen[ai]) {
            rs[ai].st = st; rs[ai].val = val; rs_frozen[ai] = true;
            if (trace) fprintf(stderr, "  SET arg%d st=%d val=0x%llX\n", ai, (int)st, (unsigned long long)val);
        }
    };
    auto set_slot = [&](u64 off, int st, u64 val, bool is_float) {
        if (slots.count(off)) return;  // donmuş
        SlotState ss; ss.st = st; ss.val = val; ss.is_float = is_float;
        slots[off] = ss;
        pending_slots.erase(off);
        pending_slots_xmm.erase(off);
    };
    auto flush_pending = [&](const std::string& regname) {
        for (auto it = pending_slots.begin(); it != pending_slots.end(); ) {
            if (it->second == regname && carrier.count(regname)) {
                SlotState ss; ss.st = (int)carrier[regname].st; ss.val = carrier[regname].val;
                slots[it->first] = ss;
                it = pending_slots.erase(it);
            } else ++it;
        }
        for (auto it = pending_slots_xmm.begin(); it != pending_slots_xmm.end(); ) {
            if (it->second == regname && xmm.count(regname)) {
                SlotState ss; ss.st = 1; ss.val = xmm[regname].val;
                ss.is_float = xmm[regname].is_float;
                slots[it->first] = ss;
                it = pending_slots_xmm.erase(it);
            } else ++it;
        }
    };

    if (trace) fprintf(stderr, "PROLOG carrier: %zu adet (r13=%d)\n", carrier.size(), (int)carrier.count("r13"));
    // PROLOG: callee-saved yazmaçlar (rbx/rbp/r12-r15) kalıcı stack işaretçisi kurar
    {
        bool t2 = getenv("UMVP_ARGTRACE2") && func[call_idx].addr == strtoull(getenv("UMVP_ARGTRACE2"), nullptr, 0);
        if (t2) {
            int n_lea = 0;
            for (size_t k = 0; k < (func.size() < 160 ? func.size() : 160); k++)
                if (func[k].mnemonic == "lea") n_lea++;
            fprintf(stderr, "  prolog lea sayisi=%d (ilk 160 islemde)\n", n_lea);
        }
        size_t pro_end = func.size() < 160 ? func.size() : 160;
        for (size_t k = 0; k < pro_end; k++) {
            const Insn& P = func[k];
            if (P.mnemonic != "lea") continue;
            std::string dn = P.op_str.substr(0, P.op_str.find(','));
            if (dn == "rbx" || dn == "rbp" || dn == "r12" || dn == "r13" || dn == "r14" || dn == "r15") {
                size_t p = P.op_str.find("rsp + ");
                if (p == std::string::npos) p = P.op_str.find("rsp+");
                if (p != std::string::npos) {
                    u64 soff = strtoull(P.op_str.c_str() + P.op_str.find('+', p) + 1, nullptr, 0);
                    RegState st; st.st = RS::StackSlot; st.val = soff;
                    if (!carrier.count(dn)) carrier[dn] = st;
                }
            }
        }
        if (t2) for (auto& [k2, v2] : carrier)
            fprintf(stderr, "  carrier[%s] st=%d val=0x%llX\n", k2.c_str(), (int)v2.st, (unsigned long long)v2.val);
    }

    // GERİYE YÜRÜYÜŞ — çağrı bariyerinde dur
    size_t begin = (call_idx > 64) ? call_idx - 64 : 0;
    for (size_t k = call_idx; k-- > begin;) {
        const Insn& I = func[k];
        const std::string& m = I.mnemonic;
        const std::string& o = I.op_str;
        if (trace) fprintf(stderr, "WALK 0x%llX %-7s %s\n", (unsigned long long)I.addr, m.c_str(), o.c_str());
        if (m == "call") break;
        if (m == "pop" || m == "leave") break;
        if ((m == "sub" || m == "add") && (o.rfind("rsp, ", 0) == 0 || o.rfind("esp, ", 0) == 0)) break;

        // pxor xmmN, xmmN → 0.0f
        if (m == "pxor") {
            size_t c = o.find(',');
            if (c != std::string::npos && o.substr(0, c) == o.substr(c + 2) &&
                o.substr(0, c).rfind("xmm", 0) == 0) {
                std::string xn = o.substr(0, c);
                if (!xmm.count(xn)) { SlotState ss; ss.st = 1; ss.val = 0; ss.is_float = true; xmm[xn] = ss; }
                flush_pending(xn);
                continue;
            }
        }

        // lea reg, [rip+X] → LeaRip
        if (m == "lea" && I.rip_target) {
            std::string dn = o.substr(0, o.find(','));
            int ai = reg_index_of(dn);
            if (ai >= 0) set_reg_once(ai, RS::LeaRip, I.rip_target);
            else if (!carrier.count(dn)) {
                RegState st; st.st = RS::LeaRip; st.val = I.rip_target;
                carrier[dn] = st;
                flush_pending(dn);
            }
            continue;
        }
        // lea reg, [rsp+off] → StackSlot (hem arg hem carrier)
        if (m == "lea") {
            std::string dn = o.substr(0, o.find(','));
            size_t p = o.find("rsp + ");
            if (p == std::string::npos) p = o.find("rsp+");
            if (p != std::string::npos) {
                u64 soff = strtoull(o.c_str() + o.find('+', p) + 1, nullptr, 0);
                int ai = reg_index_of(dn);
                if (ai >= 0) set_reg_once(ai, RS::StackSlot, soff);
                else if (!carrier.count(dn)) {
                    RegState st; st.st = RS::StackSlot; st.val = soff;
                    carrier[dn] = st;
                    flush_pending(dn);
                }
            } else {
                int ai = reg_index_of(dn);
                if (ai >= 0) set_reg_once(ai, RS::Unknown, 0);
            }
            continue;
        }
        // mov reg, imm
        if ((m == "mov" || m == "movabs") && I.imm) {
            std::string dn = o.substr(0, o.find(','));
            int ai = reg_index_of(dn);
            if (ai >= 0) set_reg_once(ai, RS::Const, I.imm);
            else if (!carrier.count(dn)) {
                RegState st; st.st = RS::Const; st.val = I.imm;
                carrier[dn] = st; flush_pending(dn);
            }
            continue;
        }
        // mov rax/rbx/rsi/rdi/r12-15, qword ptr [rip+X] (veri sabiti kalıbı)
        if (m == "mov" && I.rip_target &&
            (o.rfind("rax, qword ptr [rip", 0) == 0 || o.rfind("rbx, qword ptr [rip", 0) == 0 ||
             o.rfind("rsi, qword ptr [rip", 0) == 0 || o.rfind("rdi, qword ptr [rip", 0) == 0 ||
             o.rfind("r12, qword ptr [rip", 0) == 0 || o.rfind("r13, qword ptr [rip", 0) == 0 ||
             o.rfind("r14, qword ptr [rip", 0) == 0 || o.rfind("r15, qword ptr [rip", 0) == 0)) {
            std::string dn = o.substr(0, o.find(','));
            if (!carrier.count(dn)) {
                RegState st; st.st = RS::LeaRip; st.val = I.rip_target;
                carrier[dn] = st; flush_pending(dn);
            }
            continue;
        }
        // mov reg, reg (arg↔arg, arg↔carrier)
        if (m == "mov") {
            size_t c = o.find(',');
            if (c != std::string::npos) {
                std::string dst = o.substr(0, c), src2 = o.substr(c + 2);
                int di = reg_index_of(dst);
                int si = reg_index_of(src2);
                if (di >= 0 && si >= 0 && rs_frozen[si]) {
                    set_reg_once(di, rs[si].st, rs[si].val);
                    continue;
                }
                if (di >= 0 && si >= 0 && !rs_frozen[si]) {
                    // kaynak henüz bilinmiyor → pending arg (kaynak netleşince doldur)
                    // (basitlik: bilinmiyor say)
                    set_reg_once(di, RS::Unknown, 0);
                    continue;
                }
                if (di >= 0 && carrier.count(src2)) {
                    set_reg_once(di, carrier[src2].st, carrier[src2].val);
                    continue;
                }
                if (di >= 0 && !carrier.count(src2) && reg_index_of(src2) < 0) {
                    // carrier'dan geliyor olabilir → pending arg
                    // (kaynak carrier'da netleşirse) — basit yaklaşım: bekleme yok, Unknown
                    set_reg_once(di, RS::Unknown, 0);
                    continue;
                }
            }
        }
        // xor reg, reg → 0
        if (m == "xor") {
            size_t c = o.find(',');
            if (c != std::string::npos && o.substr(0, c) == o.substr(c + 2)) {
                int ai = reg_index_of(o.substr(0, c));
                if (ai >= 0) set_reg_once(ai, RS::Const, 0);
                continue;
            }
        }
        // xmm ← rip float / imm (movss xmm, [rip+X])
        if (m == "movss" || m == "movsd") {
            size_t c = o.find(',');
            if (c != std::string::npos) {
                std::string dst = o.substr(0, c), src2 = o.substr(c + 2);
                if (dst.rfind("xmm", 0) == 0 && !xmm.count(dst)) {
                    SlotState ss;
                    if (I.rip_target) {
                        float f = 0;
                        if (mm.Read(I.rip_target, &f, 4) == 4 && IsFiniteF(f)) {
                            ss.st = 1; memcpy(&ss.val, &f, 4); ss.is_float = true;
                        } else ss.st = 0;
                    } else if (src2.rfind("0x", 0) == 0) {
                        ss.st = 1; ss.val = strtoull(src2.c_str(), nullptr, 0); ss.is_float = true;
                    }
                    xmm[dst] = ss;
                    flush_pending(dst);
                    continue;
                }
            }
        }
        // [rsp+off] ← yazmaç / imm (movss/mov/movups/movq)
        if (m == "movss" || m == "movsd" || m == "mov" || m == "movups" || m == "movq") {
            size_t c = o.find(',');
            if (c != std::string::npos) {
                std::string dst = o.substr(0, c), src2 = o.substr(c + 2);
                if (dst.find("rsp") != std::string::npos || dst.find("esp") != std::string::npos) {
                    size_t plus = dst.find('+');
                    u64 off = plus != std::string::npos ? strtoull(dst.c_str() + plus + 1, nullptr, 0) : 0;
                    if (trace) fprintf(stderr, "  SLOT-YAZ dst=[%s] src=[%s] off=0x%llX frozen=%d carrier_rs=%d idx=%d\n",
                                       dst.c_str(), src2.c_str(), (unsigned long long)off, (int)slots.count(off),
                                       (int)carrier.count(src2), reg_index_of(src2));
                    if (slots.count(off)) continue;  // donmuş
                    if (src2.rfind("xmm", 0) == 0) {
                        if (xmm.count(src2)) set_slot(off, (int)xmm[src2].st, xmm[src2].val, xmm[src2].is_float);
                        else pending_slots_xmm[off] = src2;
                    } else if (src2.rfind("0x", 0) == 0) {
                        set_slot(off, 1 /*Const*/, strtoull(src2.c_str(), nullptr, 0),
                                 m == "movss" || m == "movsd");
                    } else if (carrier.count(src2)) {
                        set_slot(off, (int)carrier[src2].st, carrier[src2].val, false);
                    } else if (reg_index_of(src2) < 0) {
                        pending_slots[off] = src2;  // yazmaç değeri yürüyüşte netleşecek
                    }
                    continue;
                }
                // [rsp+0x20] 5.arg stack yazmaları (mov [rsp+0x20], rax/imm) yukarıda kapsanır
            }
        }
        // yazmaç zehirlenmesi YOK — dondurma semantiği: ilk atama kazandı, sonraki yazmalar
        // (ileri-sırada daha eski) bu dilimi etkilemez
    }

    // pending slot'ları son carrier durumlarıyla çöz
    for (auto& [off, regname] : pending_slots)
        if (carrier.count(regname))
            set_slot(off, (int)carrier[regname].st, carrier[regname].val, false);
    if (trace) {
        fprintf(stderr, "SLOTLAR: ");
        for (auto& [off, ss] : slots) fprintf(stderr, "[0x%llX]=st%d ", (unsigned long long)off, ss.st);
        fprintf(stderr, "| pending=%zu\n", pending_slots.size() + pending_slots_xmm.size());
    }

    // --- SONUÇ (x64) ---
    if (x64) {
        for (int i = 0; i < 4; i++) {
            ArgValue& A = out.args[i];
            if (rs[i].st == RS::Const) {
                A.ival = rs[i].val; A.ival_valid = true;
                u32 lo = (u32)(rs[i].val & 0xFFFFFFFF), hi = (u32)(rs[i].val >> 32);
                if (hi != 0 && PlausibleFloat(lo) && PlausibleFloat(hi)) {
                    float f0, f1; memcpy(&f0, &lo, 4); memcpy(&f1, &hi, 4);
                    if (fabsf(f0) < 8192 && fabsf(f1) < 8192) { A.v2 = Vec2(f0, f1); A.v2_valid = true; }
                }
            } else if (rs[i].st == RS::LeaRip) {
                A.ival = rs[i].val; A.ival_valid = true;
                A.is_writable_ptr = mm.IsWritable(rs[i].val);
                bool ok = false;
                auto s = mm.ReadCString(rs[i].val, 256, true, &ok);
                if (ok && !s.empty()) { A.str = s; A.str_valid = true; }
                else {
                    float f[2] = {0, 0};
                    if (mm.Read(rs[i].val, f, 8) == 8 && IsFiniteF(f[0]) && IsFiniteF(f[1])) {
                        A.ptr_floats_read = true; A.ptr_f0 = f[0]; A.ptr_f1 = f[1];
                        if (!A.is_writable_ptr && fabsf(f[0]) < 8192 && fabsf(f[1]) < 8192) {
                            A.v2 = Vec2(f[0], f[1]); A.v2_valid = true;
                        }
                    }
                }
            } else if (rs[i].st == RS::StackSlot) {
                auto it = slots.find(rs[i].val);
                if (it != slots.end()) {
                    if (it->second.st == 1) {
                        if (it->second.is_float) {
                            memcpy(&A.f, &it->second.val, 4); A.f_valid = true;
                            auto it2 = slots.find(rs[i].val + 4);
                            if (it2 != slots.end() && it2->second.st == 1 && it2->second.is_float) {
                                float f1; memcpy(&f1, &it2->second.val, 4);
                                if (PlausibleFloat((u32)it2->second.val)) { A.v2 = Vec2(A.f, f1); A.v2_valid = true; }
                            }
                        } else {
                            A.ival = it->second.val; A.ival_valid = true;
                            if (PlausibleFloat((u32)it->second.val)) {
                                float f; memcpy(&f, &it->second.val, 4);
                                A.f = f; A.f_valid = true;
                            }
                        }
                    } else if (it->second.st == 2 /*LeaRip*/) {
                        A.ival = it->second.val; A.ival_valid = true;
                        bool ok = false;
                        auto s = mm.ReadCString(it->second.val, 256, true, &ok);
                        if (ok && !s.empty()) { A.str = s; A.str_valid = true; }
                        else {
                            float f[2] = {0, 0};
                            if (mm.Read(it->second.val, f, 8) == 8 && IsFiniteF(f[0]) && IsFiniteF(f[1]) &&
                                fabsf(f[0]) < 8192 && fabsf(f[1]) < 8192) {
                                A.v2 = Vec2(f[0], f[1]); A.v2_valid = true;
                                A.ptr_floats_read = true; A.ptr_f0 = f[0]; A.ptr_f1 = f[1];
                            }
                        }
                    }
                }
            }
        }
        auto it5 = slots.find(0x20);
        if (it5 != slots.end() && it5->second.st == 2 /*LeaRip*/) {
            out.stack_args[0].ival = it5->second.val; out.stack_args[0].ival_valid = true;
            bool ok = false;
            auto s = mm.ReadCString(it5->second.val, 256, true, &ok);
            if (ok && !s.empty()) { out.stack_args[0].str = s; out.stack_args[0].str_valid = true; }
        } else if (it5 != slots.end() && it5->second.st == 1 /*Const*/) {
            out.stack_args[0].ival = it5->second.val; out.stack_args[0].ival_valid = true;
        }
        static const char* kXmm[4] = {"xmm0", "xmm1", "xmm2", "xmm3"};
        for (int i = 0; i < 4; i++) {
            auto it = xmm.find(kXmm[i]);
            if (it != xmm.end() && it->second.is_float &&
                (it->second.st == 1 || it->second.st == 2)) {
                memcpy(&out.args[i].f, &it->second.val, 4);
                out.args[i].f_valid = true;
            }
        }
    } else {
        // x86 cdecl push zinciri
        std::vector<std::pair<RS, u64>> pushes;
        for (size_t k = call_idx; k-- > begin;) {
            const Insn& I = func[k];
            if (I.mnemonic == "call") break;
            if (I.mnemonic == "push") {
                if (I.is_push_imm) pushes.push_back({RS::Const, I.imm});
                else {
                    std::string src = I.op_str;
                    if (src.rfind("offset ", 0) == 0)
                        pushes.push_back({RS::LeaRip, strtoull(src.c_str() + 7, nullptr, 0)});
                    else pushes.push_back({RS::Unknown, 0});
                }
                if (pushes.size() >= 6) break;
            }
        }
        for (size_t i = 0; i < pushes.size() && i < 4; i++) {
            auto& pr = pushes[i];
            ArgValue& A = out.args[i];
            if (pr.first == RS::Const) {
                A.ival = pr.second; A.ival_valid = true;
                if (PlausibleFloat((u32)pr.second)) { float f; memcpy(&f, &pr.second, 4); A.f = f; A.f_valid = true; }
            } else if (pr.first == RS::LeaRip) {
                A.ival = pr.second; A.ival_valid = true;
                A.is_writable_ptr = mm.IsWritable(pr.second);
                bool ok = false;
                auto s = mm.ReadCString(pr.second, 256, true, &ok);
                if (ok && !s.empty()) { A.str = s; A.str_valid = true; }
                else {
                    float f[2] = {0, 0};
                    if (mm.Read(pr.second, f, 8) == 8 && IsFiniteF(f[0]) && IsFiniteF(f[1]) &&
                        !A.is_writable_ptr && fabsf(f[0]) < 8192 && fabsf(f[1]) < 8192) {
                        A.v2 = Vec2(f[0], f[1]); A.v2_valid = true;
                    }
                }
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Handler blok çıkarma
// ---------------------------------------------------------------------------
HandlerBlock ExtractHandlerBlock(const std::vector<Insn>& func, size_t call_idx) {
    HandlerBlock hb;
    if (call_idx >= func.size()) return hb;
    // call'dan sonraki ~6 komutta test+jcc ara
    size_t i = call_idx + 1;
    int skip = 0;
    const Insn* test = nullptr;
    while (i < func.size() && skip < 4) {
        const Insn& I = func[i];
        if (I.mnemonic == "test" || I.mnemonic == "cmp") { test = &I; i++; break; }
        if (I.is_call) { // arada başka çağrı → bu call'ın sonucu değil
            return hb;
        }
        if (I.mnemonic == "movzx" || I.mnemonic == "mov" || I.mnemonic == "and") { skip++; i++; continue; }
        break;
    }
    if (!test || i >= func.size()) return hb;
    const Insn& jcc = func[i];
    if (!jcc.is_jcc) return hb;
    // test al,al; je X → gövde = (jcc sonrası, X)
    // test al,al; jne X → gövde = [X, ...] — bitiş: gövdeden ilk jmp-out/ret'e kadar
    if (jcc.mnemonic == "je" || jcc.mnemonic == "jz") {
        u64 body_start = jcc.addr + jcc.size;
        u64 body_end = jcc.branch_target;
        if (body_end > body_start) {
            hb.found = true;
            hb.inverted = false;
            hb.start = body_start;
            hb.end = body_end;
            hb.jcc_addr = jcc.addr;
            hb.merge_addr = body_end;
            hb.pattern = "test;jz → gövde = fallthrough..merge";
            return hb;
        }
    } else if (jcc.mnemonic == "jne" || jcc.mnemonic == "jnz") {
        u64 body_start = jcc.branch_target;
        // gövde bitişi: body_start'tan ileri, ilk jmp (merge zıplaması) veya ret
        for (size_t j = 0; j < func.size(); j++) {
            const Insn& I2 = func[j];
            if (I2.addr < body_start) continue;
            if (I2.is_jmp && I2.branch_target != 0) {
                hb.found = true;
                hb.inverted = true;
                hb.start = body_start;
                hb.end = I2.addr;
                hb.jcc_addr = jcc.addr;
                hb.merge_addr = jcc.addr + jcc.size;
                hb.pattern = "test;jnz → gövde = hedef..jmp";
                return hb;
            }
            if (I2.is_ret) {
                hb.found = true;
                hb.inverted = true;
                hb.start = body_start;
                hb.end = I2.addr;
                hb.jcc_addr = jcc.addr;
                hb.merge_addr = jcc.addr + jcc.size;
                hb.pattern = "test;jnz → gövde = hedef..ret";
                return hb;
            }
        }
    }
    return hb;
}

std::vector<std::string> FormatInsns(const std::vector<Insn>& insns, u64 lo, u64 hi, const char* addr_prefix) {
    std::vector<std::string> out;
    char line[512];
    for (auto& I : insns) {
        if (I.addr < lo || I.addr >= hi) continue;
        snprintf(line, sizeof(line), "%s0x%016llX  %-8s %s", addr_prefix,
                 (unsigned long long)I.addr, I.mnemonic.c_str(), I.op_str.c_str());
        out.push_back(line);
    }
    return out;
}

} // namespace umvp
