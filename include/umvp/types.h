// ============================================================================
// umvp/types.h — UniversalMVPUIAlgorithm ortak tipleri
// MVP GOD STATE LEVEL UI ANALYSIS SDK
// ============================================================================
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace umvp {

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i32 = int32_t;
using i64 = int64_t;

// ---------------------------------------------------------------------------
// Hedef mimarisi
// ---------------------------------------------------------------------------
enum class Arch : int { X86, X64, Unknown };

inline const char* ArchName(Arch a) {
    switch (a) {
        case Arch::X86: return "x86 (32-bit)";
        case Arch::X64: return "x64 (64-bit)";
        default:        return "unknown";
    }
}

// ---------------------------------------------------------------------------
// Temel veri yapıları (ImGui karşılıkları)
// ---------------------------------------------------------------------------
struct Vec2 {
    float x = 0.0f, y = 0.0f;
    bool valid = false;
    Vec2() = default;
    Vec2(float X, float Y) : x(X), y(Y), valid(true) {}
};

struct Rect {  // ImRect karşılığı
    Vec2 min, max;
    bool valid = false;
};

// ---------------------------------------------------------------------------
// Süreç / modül
// ---------------------------------------------------------------------------
struct ProcessEntry {
    u32 pid = 0;
    std::string name;     // exe adı
    std::string path;     // tam yol (varsa)
};

struct ModuleEntry {
    u64 base = 0;
    u64 size = 0;
    std::string name;         // "app.exe", "d3d11.dll" ...
    std::string path;
    bool is_dll = false;
    Arch arch = Arch::Unknown;
    // PE kesitleri (RVA tabanlı)
    struct Section {
        u32 rva = 0, vsize = 0, raw_size = 0, chars = 0;
        std::string name;
        bool exec = false, write = false, read = true;
    };
    std::vector<Section> sections;
    // PE dizinleri
    u64 pdata_rva = 0;   // exception directory (RUNTIME_FUNCTION tablosu)
    u32 pdata_size = 0;
    u64 export_rva = 0;  // export directory
    u32 export_size = 0;
    // çözülmüş ImGui export'ları (DLL linkajında): isim -> VA
    std::vector<std::pair<std::string, u64>> imgui_exports;
};

// ---------------------------------------------------------------------------
// Bellek bölgesi
// ---------------------------------------------------------------------------
struct MemRegion {
    u64 base = 0, size = 0;
    u32 state = 0;    // MEM_COMMIT / MEM_RESERVE / MEM_FREE
    u32 protect = 0;  // PAGE_*
    u32 type = 0;     // MEM_IMAGE / MEM_MAPPED / MEM_PRIVATE
    bool readable() const;
    bool writable() const;
    bool exec() const;
    bool is_image() const { return type == 0x1000000; }
    bool is_mapped() const { return type == 0x40000; }
    bool is_private() const { return type == 0x20000; }
    const char* type_name() const {
        if (is_image()) return "IMAGE";
        if (is_mapped()) return "MAPPED";
        if (is_private()) return "PRIVATE";
        return "FREE/OTHER";
    }
};

// ---------------------------------------------------------------------------
// ImGui analizi sonuçları
// ---------------------------------------------------------------------------

// Veri kaynağı güveni
enum class Conf : int {
    Unknown = 0,   // bulunamadı
    Estimated,     // kestirim (layout simülasyonu vb.)
    CodeStatic,    // disassembly'den kesin (statik)
    LiveExact      // canlı bellekten kesin
};

inline const char* ConfName(Conf c) {
    switch (c) {
        case Conf::Estimated:  return "est";
        case Conf::CodeStatic: return "code";
        case Conf::LiveExact:  return "live";
        default:               return "?";
    }
}

struct WidgetInfo {
    std::string type;         // "Button", "Checkbox", "SliderFloat", "Combo", ...
    std::string label;        // "Aimbot" (tam literal)
    std::string display_text; // "##" öncesi gösterilen metin
    // kod tarafı (statik disassembly)
    u64 call_site = 0;        // ImGui::<Type>(...) çağrı adresi
    u64 callee = 0;           // çağrılan ImGui işlevinin adresi
    Vec2 size_arg;            // Button(str, ImVec2(w,h)) — koddan kurtarıldı
    std::vector<float> float_args;  // slider min/max vb.
    std::vector<u64> int_args;
    std::string format_arg;   // "%.3f" vb.
    std::string combo_items;  // Combo öğeleri ("Head|Neck|Chest")
    // handler kod bloğu (if (Button(...)) { ... } gövdesi)
    u64 handler_start = 0, handler_end = 0;
    std::vector<std::string> handler_disasm; // satır satır listeleme
    std::string handler_calls;               // handler içinden çağrılan işlevler (özeti)
    // canlı taraf
    u32 id_hash = 0;          // ImHashStr(label) — beklenen widget ID'si
    Vec2 pos, size;           // canlı/kestirilmiş konum ve boyut
    Conf pos_conf = Conf::Unknown, size_conf = Conf::Unknown;
    std::string live_validation; // "LastItemId eşleşti" vb.
    // sınıflandırma güveni 0..100
    int confidence = 0;
    std::string evidence;     // sınıflandırma kanıtı (kısa metin)
};

struct FormInfo {
    std::string name;          // "Form1"
    u32 id = 0;                // ImHashStr(name)
    // canlı (heap) veriler
    u64 window_obj = 0;        // ImGuiWindow nesne adresi
    Vec2 pos, size, size_full;
    u32 flags = 0;
    std::string flags_decoded; // "NoTitleBar|NoResize|..."
    bool child = false;
    bool active = false, collapsed = false;
    int widget_count_live = -1; // -1: bilinmiyor
    // kod (statik) veriler
    u64 begin_call = 0;        // Begin("name") çağrı adresi
    u64 menu_func_start = 0, menu_func_end = 0;  // içeren fonksiyon
    Vec2 static_size;          // SetNextWindowSize sabiti
    std::vector<WidgetInfo> widgets;
    bool live_found = false;
    bool code_found = false;
};

struct RendererInfo {
    std::vector<std::string> graphics_modules; // d3d9.dll, d3d11.dll, dxgi.dll...
    std::string api;           // "D3D11", "D3D9", "D3D12", "D3D10", "OpenGL", "Vulkan", "Unknown"
    std::string api_detail;    // ek ayrıntı
};

struct ImGuiDetection {
    bool found = false;
    std::string version_str;   // "1.91.8"
    int version_num = 0;       // 19180
    std::string hash_variant;  // "legacy (<=1.92.5)" / "modern (>=1.92.6)"
    bool version_confirmed = false;
    std::string linkage;       // "static" / "dll-exports"
    u64 crc32_table_addr = 0;
    u64 imhashstr_func = 0;    // xref ile bulunan ImHashStr işlevi
    u64 context_addr = 0;      // ImGuiContext
    u64 windows_vector_buf = 0;
    Vec2 display_size;
    float framerate = 0.0f;
    int windows_active_count = -1;
    int total_windows_created = -1;
    std::string notes;
};

struct AnalysisReport {
    // hedef
    u32 pid = 0;
    std::string process_name;
    Arch arch = Arch::Unknown;
    std::vector<ModuleEntry> modules;
    RendererInfo renderer;
    // bellek
    size_t region_count = 0;
    u64 readable_bytes = 0;
    u64 cached_bytes = 0;
    // imgui
    ImGuiDetection imgui;
    // formlar
    int form_count = 0;
    std::vector<FormInfo> forms;
    // derin analiz
    u64 menu_func_start = 0, menu_func_end = 0;
    int total_widgets = 0;
    std::vector<std::string> menu_disasm;  // menü fonksiyonunun tam listesi
    // tanılama
    std::vector<std::string> log;
    double analysis_ms = 0.0;
};

} // namespace umvp
