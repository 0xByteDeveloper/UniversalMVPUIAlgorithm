// ============================================================================
// umvp/imgui_scan.h — ImGui canlı bellek taraması (GOD STATE katmanı)
//   * CRC32 tablo parmak izleriyle ImGui varlığı + hash dönemi tespiti
//   * Heap taraması: ID == ImHashStr(Name) invariant'ı ile ImGuiWindow
//     nesnelerinin keşfi (versiyon-bağımsız yapı hipotezleriyle)
//   * ImGuiContext keşfi: Windows vektörü → çoklu-hipotez doğrulaması
//   * D3D9/10/11/12/OpenGL/Vulkan modül tespiti (render-agnostik kanıt)
// ============================================================================
#pragma once
#include "types.h"
#include "process.h"
#include "memory_map.h"
#include "imgui_hash.h"
#include "layout.h"

namespace umvp {

// ---- D3D / renderer tespiti -----------------------------------------------
RendererInfo DetectRenderer(const std::vector<ModuleEntry>& modules);

// ---- ImGui tespiti ---------------------------------------------------------
struct CrcTableHit {
    u64 addr = 0;
    HashAlgo algo = HashAlgo::Adler;  // tabloya göre dönem
    const ModuleEntry* module = nullptr;
};

// Bellekte ImGui CRC tablolarını ara (hem adler hem crc32c)
// modules verilirse tablolar modüllere atanır; ana (exe) modül önceliklidir
std::vector<CrcTableHit> FindCrcTables(MemoryMap& mm, const std::vector<ModuleEntry>* modules = nullptr);

// "Dear ImGui" / sürüm dizisi taraması (rdata)
struct VersionStringHit {
    u64 addr = 0;
    std::string version;   // "1.91.8"
    const ModuleEntry* module = nullptr;
};
std::vector<VersionStringHit> FindVersionStrings(MemoryMap& mm, const std::vector<ModuleEntry>* modules = nullptr);

// ---- Canlı pencere (form) taraması ----------------------------------------
struct LiveWindow {
    u64 obj = 0;             // ImGuiWindow* (yığında)
    u64 name_ptr = 0;
    std::string name;
    u32 id = 0;
    HashProfile hash;        // doğrulayan hash profili
    int layout_family = -1;  // eşleşen WindowLayoutGuess indeksi (0,1,2)

    // çözümlenmiş offset'ler (aile + DB)
    int off_pos = -1, off_size = -1, off_sizefull = -1, off_flags = -1;
    int off_childflags = -1, off_active = -1, off_collapsed = -1;
    int off_parent = -1, off_root = -1, off_isfallback = -1, off_ischild = -1;
    int off_idstack = -1, off_drawlist = -1;

    Vec2 pos, size, size_full;
    u32 flags = 0, childflags = 0;
    bool active = false, was_active = false, collapsed = false, fallback = false, child = false;
    u64 parent = 0, root = 0;
    int begin_order = -1;
    bool in_windows_vector = false;
    u64 draw_list = 0;

    // son widget (DC/LastItemData) — versiyon destekliyse
    bool last_item_known = false;
    u32 last_item_id = 0;
    Vec2 last_item_rect_min, last_item_rect_max;

    // font tabanlı metin çözümü (drawlist)
    struct TextRun { std::string text; Vec2 a, b; };
    std::vector<TextRun> text_runs;
};

struct ContextInfo {
    u64 addr = 0;
    const LayoutSet* layout = nullptr;   // doğrulanan versiyon layout'u
    int layout_index = -1;
    std::string version;
    int version_num = 0;
    bool confirmed = false;
    u64 io = 0;
    Vec2 display_size;
    float framerate = 0, delta_time = 0;
    u32 active_id = 0, hovered_id = 0;
    int windows_active_count = -1;
    u64 current_window = 0, hovered_window = 0, moving_window = 0;
    u64 windows_vec = 0;      // &g.Windows (ImVector)
    u64 windows_buf = 0;      // g.Windows.Data
    int windows_count = 0;
    std::vector<u64> window_objs;   // vektörden okunan pencere işaretçileri
    u64 last_item_data = 0;
    u32 last_item_id = 0;
    // style (layout simülasyonu için)
    Vec2 style_frame_padding, style_item_spacing, style_window_padding;
    float style_frame_height = 0, style_alpha = 0, style_indent_spacing = 0, style_scrollbar_size = 0;
    bool style_ok = false;
};

// Heap'te ImGuiWindow ara (tüm hash profilleri + yapı hipotezleri denenir)
std::vector<LiveWindow> ScanWindows(MemoryMap& mm, const std::vector<HashProfile>& try_profiles,
                                    const std::function<void(int,int,const char*)>& progress = nullptr);

// Pencere listesinden ImGuiContext keşfi (çoklu versiyon hipotezli doğrulama)
bool FindContext(MemoryMap& mm, std::vector<LiveWindow>& windows, ContextInfo* out);

// Doğrulanan context'i kullanarak pencereleri zenginleştir (aktiflik, sıra, son widget)
void EnrichWindows(MemoryMap& mm, std::vector<LiveWindow>& windows, ContextInfo& ctx);

// Draw-list metin çözümü: font atlası glif tablosundan UV→karakter eşlemesi
// (widget etiketlerinin EKRANDAKİ gerçek konumlarını çıkarır — GOD STATE)
void DecodeDrawListTexts(MemoryMap& mm, LiveWindow& w, const ContextInfo& ctx);

} // namespace umvp
