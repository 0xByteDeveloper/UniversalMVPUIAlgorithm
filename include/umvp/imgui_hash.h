// ============================================================================
// umvp/imgui_hash.h — ImGui::ImHashStr birebir portu (SDK derin taraması ürünü)
//
// DERİN BULGU — ImGui hash'i ÜÇ DÖNEME AYRILIR (imgui.cpp + CHANGELOG doğrulamalı):
//   Dönem A  (≤ 1.91.5):  CRC32-ADLER tablosu (zlib tablosu), "###" karakterleri
//                         reset sonrası hash'lenir (legacy varyant)
//   Dönem B  (1.91.6–1.92.5): CRC32C tablosu (SSE4.2 uyumlu), "###" legacy
//   Dönem C  (≥ 1.92.6):  CRC32C tablosu, "###" atlanır (modern varyant)
//   + Her dönemde IMGUI_ENABLE_SSE4_2_CRC ile donanım CRC32C kullanılabilir;
//     sonuç, CRC32C tablosu ile birebir aynıdır (tek implementasyon yeter).
//
// Bellek taramasında GCrc32LookupTable baytları dönem parmak izidir:
//   A: 00 00 00 00 | 96 30 07 77 | 2C 61 0E EE | BA 51 09 99
//   B/C: 00 00 00 00 | 03 83 6B F2 | F7 70 3B E1 | F4 F3 50 13
// ============================================================================
#pragma once
#include <cstdint>
#include <cstddef>
#include "types.h"

namespace umvp {

// Standart CRC-32 (poly 0xEDB88320) — Dönem A tablosu
extern const u32 kCrc32AdlerLut[256];
// CRC-32C (poly 0x82F63B78) — Dönem B/C tablosu (SSE4.2 _mm_crc32 ile özdeş)
extern const u32 kCrc32cLut[256];

enum class HashAlgo : int { Adler = 0, Castagnoli = 1 };
enum class HashVariant : int { Legacy = 0, Modern = 1 }; // "###" işlemi

struct HashProfile {
    HashAlgo algo = HashAlgo::Adler;
    HashVariant variant = HashVariant::Legacy;
    const char* era_name() const {
        if (algo == HashAlgo::Adler) return "A (crc32-adler, <=1.91.5)";
        return variant == HashVariant::Modern ? "C (crc32c, >=1.92.6)" : "B (crc32c, 1.91.6..1.92.5)";
    }
};

// Versiyon numarasından profil (version_num = IMGUI_VERSION_NUM)
inline HashProfile HashProfileForVersion(int version_num) {
    HashProfile p;
    if (version_num >= 19206) { p.algo = HashAlgo::Castagnoli; p.variant = HashVariant::Modern; }
    else if (version_num >= 19160) { p.algo = HashAlgo::Castagnoli; p.variant = HashVariant::Legacy; }
    else { p.algo = HashAlgo::Adler; p.variant = HashVariant::Legacy; }
    return p;
}

// Tüm profiller (invariant taraması için)
inline constexpr int kHashProfileCount = 3;

// ImHashStr(data, 0, seed) — zero-terminated yol
u32 ImHashStr(const char* s, u32 seed, HashProfile p);

// Uzunluk belirtilmiş yol (ImHashData)
u32 ImHashData(const void* data, size_t size, u32 seed, HashProfile p);

// Tablo parmak izleri (bellek taraması desenleri) — LE dword dizisi
inline u32 TableMagic(const u32* lut, int i) { return lut[i]; }

} // namespace umvp
