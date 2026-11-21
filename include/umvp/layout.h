// ============================================================================
// umvp/layout.h — ImGui layout DB çalışma zamanı (imgui_layout_db.h üstünde)
// Versiyon hipotez sistemi: her aday layout, canlı bellek invariant'ları ile
// doğrulanır; doğrulanan versiyon "confirmed" olur, hiçbiri tutmazsa jenerik
// hipotezler (yapı keşfi) devreye girer.
// ============================================================================
#pragma once
#include "types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace umvp {

class LayoutSet {
public:
    std::string version;
    int version_num = 0;
    std::unordered_map<std::string, int> off;      // "ImGuiWindow.Name" -> 8
    std::unordered_map<u32, std::string> winflags; // bit -> isim
    std::unordered_map<u32, std::string> childflags;
    std::unordered_map<u32, std::string> condflags;

    int get(const char* key, int defval = -1) const {
        auto it = off.find(key);
        return it == off.end() ? defval : it->second;
    }
    bool has(const char* key) const { return off.count(key) > 0; }
    int need(const char* key) const { return get(key, -1); }

    std::string DecodeWinFlags(u32 flags) const;
};

class LayoutDB {
public:
    static const std::vector<LayoutSet>& All();          // üretilmiş DB (imgui_layout_db.h)
    static const LayoutSet* ByVersionNum(int num);       // en yakın alt sürüm
    static const LayoutSet* Best();                      // varsayılan: en yeni
};

// Pencere yapı hipotezi — versiyon DB'si olmayan hedefler için keşif modu
struct WindowLayoutGuess {
    int off_ctx = -1;       // ImGuiContext* Ctx (1.89.0+)
    int off_name;           // char* Name
    int off_id;             // ImGuiID ID
    int off_flags;          // ImGuiWindowFlags Flags
    int off_childflags;     // ImGuiChildFlags ChildFlags (1.90.1+)
    int off_pos, off_size, off_sizefull;
    const char* why;        // hipotez açıklaması
};

// Bilinen aileler (x64):
//  A: Name@0 ID@8 Flags@12 [no Ctx/ChildFlags] Pos@16 Size@24 SizeFull@32   (<=1.88)
//  B: Ctx@0 Name@8 ID@16 Flags@20 [no ChildFlags] Pos@32 Size@40 SizeFull@48 (1.89.x)
//  C: Ctx@0 Name@8 ID@16 Flags@20 ChildFlags@24 Pos@40 Size@48 SizeFull@56   (1.90.1+)
// x86 karşılıkları otomatik üretilir (imleçler 4 bayt).
std::vector<WindowLayoutGuess> WindowLayoutGuesses(bool x64);

} // namespace umvp
