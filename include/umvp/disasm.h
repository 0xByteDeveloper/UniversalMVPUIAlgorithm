// ============================================================================
// umvp/disasm.h — Derin disassembly motoru (Capstone tabanlı)
//   * x86/x64 doğrusal disassembly
//   * Cross-reference toplama (RIP-relative lea/mov, call/jmp rel32, push imm,
//     movabs imm64) → string/datasheet referans çözümü
//   * Fonksiyon sınırları: PE .pdata (RUNTIME_FUNCTION — kesin) + sezgisel
//     (prolog/padding) yedek
//   * Çağrı argümanı kurtarma (backward-slice): string ptr, ImVec2 sabitleri,
//     float/int immediate'lar, 5.+ stack argümanları
//   * Handler blok çıkarma: if (ImGui::Button(...)) { ... } gövde sınırları
// ============================================================================
#pragma once
#include "types.h"
#include "memory_map.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace umvp {

// ---------------------------------------------------------------------------
struct Insn {
    u64 addr = 0;
    u32 size = 0;
    std::string mnemonic;
    std::string op_str;
    u64 branch_target = 0;   // doğrudan call/jmp hedefi (varsa)
    u64 rip_target = 0;      // RIP-relative bellek erişim hedefi (varsa)
    bool is_call = false, is_jmp = false, is_jcc = false, is_ret = false;
    bool is_push_imm = false;
    u64 imm = 0;             // push imm / movabs imm değeri
    // backward-slice ham verisi
    int reg_written = -1;    // yazılan GP register (x86 reg id) — basit izleme
};

class DisasmEngine {
public:
    explicit DisasmEngine(Arch arch);
    ~DisasmEngine();
    DisasmEngine(const DisasmEngine&) = delete;
    DisasmEngine& operator=(const DisasmEngine&) = delete;

    // [data, data+size) disassemble; base = sanal adres
    std::vector<Insn> Disassemble(const u8* data, size_t size, u64 base);

    // Bellek haritasından aralık disassemble (önbellek/uzak okuma otomatik)
    std::vector<Insn> DisassembleRange(u64 start, size_t size);

    Arch arch() const { return arch_; }

private:
    Arch arch_;
    void* handle_ = nullptr;  // csh
    friend struct DisasmDetailBridge;
};

// ---------------------------------------------------------------------------
// Xref haritası: hedef adres → referans veren kod adresleri
enum XrefKind : int {
    XR_LEA = 0,      // lea reg, [rip+X] / lea reg, [imm32] (x86)
    XR_PUSH_IMM,     // push imm32 (x86 string arg)
    XR_MOV_IMM,      // mov reg/mem, imm32/imm64
    XR_CALL,         // call rel32 hedefi
    XR_JMP,          // jmp rel32 hedefi
    XR_MEM_DISP,     // mov eax, dword [imm32] vb (x86 mutlak)
};

struct XrefSite {
    u64 site = 0;      // referans veren komut adresi
    u64 target = 0;    // işaret edilen adres
    XrefKind kind = XR_LEA;
};

class XrefMap {
public:
    // tüm çalıştırılabilir IMAGE bölgelerini süpürür
    void Build(MemoryMap& mm, DisasmEngine& de, const std::function<void(int, int)>& progress = nullptr);

    const std::vector<XrefSite>* SitesTo(u64 target) const;
    std::vector<XrefSite> SitesInRange(u64 lo, u64 hi) const;  // site adresi aralıkta
    // bir adrese işaret eden herhangi bir xref var mı (string için)
    bool HasRefTo(u64 target) const { return SitesTo(target) != nullptr; }

    // kod → veri: verilen KOD adresini içeren fonksiyonun xref'leri
    std::vector<XrefSite> CodeRefsFrom(u64 insn_addr) const;

    size_t total_sites() const { return total_; }
    u64 min_code() const { return min_code_; }
    u64 max_code() const { return max_code_; }

private:
    std::unordered_map<u64, std::vector<XrefSite>> by_target_;
    size_t total_ = 0;
    u64 min_code_ = ~0ull, max_code_ = 0;
};

// ---------------------------------------------------------------------------
// Fonksiyon sınırları
struct FuncBounds {
    u64 start = 0, end = 0;
    bool exact = false;   // .pdata'dan mı (kesin) yoksa sezgisel mi
    std::string how;
};

class FuncBoundsResolver {
public:
    FuncBoundsResolver(MemoryMap& mm, DisasmEngine& de, Arch arch) : mm_(mm), de_(de), arch_(arch) {}

    // Modül .pdata tablosunu yükle (x64)
    void LoadPData(const ModuleEntry& mod);

    // Kesin çözüm: .pdata
    bool FromPData(u64 addr, FuncBounds* out) const;

    // Sezgisel çözüm: geriye padding/prolog taraması + ileri ret/padding
    bool Heuristic(u64 inside_addr, FuncBounds* out, int max_back = 0x6000, int max_fwd = 0x20000) const;

    // Birleşik: pdata önce, sonra sezgisel
    FuncBounds Resolve(u64 inside_addr) const;

    bool pdata_loaded() const { return !pdata_.empty(); }
    size_t pdata_count() const { return pdata_.size(); }

private:
    MemoryMap& mm_;
    DisasmEngine& de_;
    Arch arch_;
    struct PDataEntry { u64 beg, end; };  // mutlak adres
    std::vector<PDataEntry> pdata_;       // TÜM modüller, başlangıca göre sıralı
};

// ---------------------------------------------------------------------------
// Çağrı argümanı kurtarma (backward slice)
struct ArgValue {
    // genel: ham işaretçi / tam sayı
    u64 ival = 0;
    bool ival_valid = false;
    // işaretçi string'e mi çözümleniyor
    std::string str;
    bool str_valid = false;
    // ImVec2 (float çifti)
    Vec2 v2;
    bool v2_valid = false;
    // tek float
    float f = 0;
    bool f_valid = false;
    // bool* (yazılabilir işaretçi) — Checkbox vb.
    bool is_writable_ptr = false;
    // argümanın işaret ettiği bellek: 2 float okundu mu (ImVec2&)
    bool ptr_floats_read = false;
    float ptr_f0 = 0, ptr_f1 = 0;
};

struct CallSiteArgs {
    ArgValue args[4];      // x64: rcx,rdx,r8,r9 | x86: [esp+4],[esp+8],[esp+0xc],[esp+0x10]
    ArgValue stack_args[2];// x64: [rsp+0x20], [rsp+0x28] (5., 6. argüman)
    bool analyzed = false;
    std::string note;
};

// Çağrı noktası (call) argümanlarını kurtar.
// func_insns: çağrıyı içeren fonksiyonun komut listesi; call_idx: çağrı indeksi
CallSiteArgs ExtractCallArgs(MemoryMap& mm, DisasmEngine& de,
                             const std::vector<Insn>& func_insns, size_t call_idx);

// ---------------------------------------------------------------------------
// Handler blok çıkarma: call sonrası test+jcc deseni
struct HandlerBlock {
    u64 start = 0, end = 0;   // [start, end) — widget eylem gövdesi
    bool found = false;
    bool inverted = false;    // jcc hedefi gövde başlangıcıysa (jne do_work)
    std::string pattern;      // eşleşen desen açıklaması
    u64 jcc_addr = 0;
    u64 merge_addr = 0;       // gövde sonrası birleşim noktası
};
HandlerBlock ExtractHandlerBlock(const std::vector<Insn>& func_insns, size_t call_idx);

// Komut listesini metin satırlarına çevir (rapor için)
std::vector<std::string> FormatInsns(const std::vector<Insn>& insns, u64 lo, u64 hi,
                                     const char* addr_prefix = "");

} // namespace umvp
