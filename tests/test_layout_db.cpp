// ============================================================================
// tests/test_layout_db.cpp — BAĞIMSIZ TEST: layout DB bütünlüğü
// Üretilmiş imgui_layout_db.h'nin kritik offset'leri tutarlı mı?
// =============================================================================
#include "umvp/layout.h"
#include <cstdio>

using namespace umvp;

int main() {
    auto& all = LayoutDB::All();
    printf("layout DB: %zu versiyon\n", all.size());
    if (all.size() < 6) { printf("FAIL: versiyon sayisi az\n"); return 1; }

    // Bilinen gerçekler (x64 native derleme çıktılarıyla doğrulanmış):
    struct Expect { const char* ver; int vnum; int name_off; int id_off; };
    // 1.86: Name@0 ID@8 | 1.89.9+: Ctx@0 Name@8 ID@16 | 1.91.8+: ChildFlags@24
    Expect exp[] = {
        {"1.76", 17600, 0, 8},
        {"1.86", 18600, 0, 8},
        {"1.89.9", 18990, 8, 16},
        {"1.91.8", 19180, 8, 16},
        {"1.92.6", 19261, 8, 16},
    };
    for (auto& e : exp) {
        const LayoutSet* L = LayoutDB::ByVersionNum(e.vnum);
        if (!L) { printf("FAIL: %s bulunamadi\n", e.ver); return 1; }
        int name = L->get("ImGuiWindow.Name", -1);
        int id = L->get("ImGuiWindow.ID", -1);
        if (name != e.name_off || id != e.id_off) {
            printf("FAIL %s: Name@%d (beklenen %d), ID@%d (beklenen %d)\n",
                   e.ver, name, e.name_off, id, e.id_off);
            return 1;
        }
        printf("OK  %-8s Name@%d ID@%d Pos@%d Size@%d SizeFull@%d\n", e.ver,
               name, id, L->get("ImGuiWindow.Pos"), L->get("ImGuiWindow.Size"),
               L->get("ImGuiWindow.SizeFull"));
    }

    // WindowLayoutGuesses aileleri
    auto g64 = WindowLayoutGuesses(true);
    if (g64.size() != 3) { printf("FAIL: x64 hipotez sayisi %zu\n", g64.size()); return 1; }
    if (g64[0].off_name != 0 || g64[0].off_id != 8) { printf("FAIL: hipotez A yanlis\n"); return 1; }
    if (g64[2].off_name != 8 || g64[2].off_id != 16 || g64[2].off_childflags != 24) { printf("FAIL: hipotez C yanlis\n"); return 1; }
    auto g32 = WindowLayoutGuesses(false);
    if (g32[0].off_name != 0 || g32[0].off_id != 4 || g32[0].off_pos != 12) { printf("FAIL: x86 hipotez A yanlis\n"); return 1; }
    printf("OK  x64/x86 yapi hipotezleri\n");

    // flag decode
    auto* L = LayoutDB::ByVersionNum(19180);
    std::string dec = L->DecodeWinFlags(1 | 2 | (1 << 3));  // NoTitleBar|NoResize|NoCollapse
    printf("OK  flag decode (0xB): %s\n", dec.c_str());
    if (dec.find("NoTitleBar") == std::string::npos || dec.find("NoResize") == std::string::npos) {
        printf("FAIL: flag decode hatali\n"); return 1;
    }
    printf("LAYOUT DB TEST PASS\n");
    return 0;
}
