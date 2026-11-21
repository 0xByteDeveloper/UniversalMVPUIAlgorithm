// ============================================================================
// tests/test_hash_native.cpp — BAĞIMSIZ TEST
// umvp::ImHashStr portu, GERÇEK Dear ImGui ImHashStr ile karşılaştırılır.
// Her imgui versiyonu için native derlenir; uyumsuzluk = FAIL.
// Dönem farkları otomatik hesaplanır (A: adler/legacy, B: crc32c/legacy, C: crc32c/modern).
// ============================================================================
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "umvp/imgui_hash.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

using namespace umvp;

static int g_fail = 0, g_total = 0;

// Bu derlemenin gerçek SDK tablosu hangi algoritma? (kaynak içi sinyal)
#if defined(IMGUI_USE_LEGACY_CRC32_ADLER) || (IMGUI_VERSION_NUM < 19160)
  #define THIS_BUILD_ALGO HashAlgo::Adler
#else
  #define THIS_BUILD_ALGO HashAlgo::Castagnoli
#endif

static void CheckOne(const char* s, u32 seed, HashProfile p) {
    ImGuiID real = ImHashStr(s, 0, seed);
    u32 ours = umvp::ImHashStr(s, seed, p);
    ++g_total;
    if (real != ours) {
        ++g_fail;
        printf("MISMATCH s=[%s] seed=0x%08X real=0x%08X ours=0x%08X (%s)\n",
               s, seed, real, ours, p.era_name());
    }
}

int main() {
    printf("imgui %s (NUM=%d) — umvp hash dogrulamasi\n", IMGUI_VERSION, IMGUI_VERSION_NUM);

    HashProfile p;
    p.algo = THIS_BUILD_ALGO;
    p.variant = (IMGUI_VERSION_NUM >= 19206)  // 1.92.6: "###" atlamaya geçer
                    ? HashVariant::Modern : HashVariant::Legacy;
    printf("beklenen donem: %s\n", p.era_name());

    std::vector<std::string> corpus = {
        "", "a", "Aimbot", "ESP", "Form1", "Form2", "Debug##Default",
        "Aimbot###id1", "Hello###World", "###World", "World", "A##B", "A###B",
        "A##B###C", "##", "###", "####", "#", "###abc", "x###", "x####y",
        "The quick brown fox jumps over the lazy dog 0123456789",
        "unicode-\xc3\xa9\xc3\xa8\xe2\x82\xac", "tab\tnewline\n", "space ",
        "very-long-" + std::string(256, 'L'), "slider##float", "combo#1",
        "a###b###c", "###a###b", "#a##b", "trailing###", "%%%.3f",
    };
    std::vector<u32> seeds = { 0u, 1u, 0xDEADBEEFu, 0xFFFFFFFFu, 0x12345678u,
                               ImHashStr("Form1", 0, 0), ImHashStr("Window", 0, 0) };

    for (auto& s : corpus)
        for (u32 seed : seeds)
            CheckOne(s.c_str(), seed, p);

    // Varyant mantığı testi: "Hello###World" kimere eşit olmalı?
    ImGuiID real = ImHashStr("Hello###World", 0, 0);
    if (p.variant == HashVariant::Modern) {
        if (real != (ImGuiID)umvp::ImHashStr("World", 0, p))
            { ++g_fail; printf("MODERN varyant esitligi bozuk\n"); }
    } else {
        if (real != (ImGuiID)umvp::ImHashStr("###World", 0, p))
            { ++g_fail; printf("LEGACY varyant esitligi bozuk\n"); }
    }

    if (g_fail) {
        printf("HASH TEST FAIL: %d/%d\n", g_fail, g_total + 1);
        return 1;
    }
    printf("HASH TEST PASS: %d deneme, 0 hata (donem: %s)\n", g_total + 1, p.era_name());
    return 0;
}
