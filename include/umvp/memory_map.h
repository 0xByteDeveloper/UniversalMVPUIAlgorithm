// ============================================================================
// umvp/memory_map.h — Hedef bellek anlık görüntüsü (lazy bölge önbelleği)
// Tüm tarayıcılar (heap scan, string scan, pattern scan) bu katman üzerinden
// çalışır: ReadProcessMemory maliyetini tek sefere indirir.
// ============================================================================
#pragma once
#include "types.h"
#include "process.h"
#include <unordered_map>
#include <mutex>

namespace umvp {

class MemoryMap {
public:
    explicit MemoryMap(IProcessReader& rd) : rd_(rd) {}

    // Bölge listesini al ve okunabilir bölge önbelleğini kur (toplam üst sınırı ile)
    void Build(size_t max_region_bytes = 64 * 1024 * 1024, u64 max_total_bytes = 1536ull * 1024 * 1024);

    const std::vector<MemRegion>& regions() const { return regions_; }
    size_t cached_region_count() const { return cache_.size(); }
    u64 cached_bytes() const { return cached_bytes_; }
    u64 readable_bytes_total() const { return readable_total_; }

    // Okuma: öncelikle önbellekten; yoksa hedeften (ve önbelleğe değilse atlar)
    size_t Read(u64 addr, void* buf, size_t size);
    bool  IsReadable(u64 addr) const;
    bool  IsReadableRange(u64 addr, size_t size) const;

    template <typename T>
    bool ReadT(u64 addr, T* out) { return Read(addr, out, sizeof(T)) == sizeof(T); }

    u8  ReadU8(u64 a, bool* ok = nullptr);
    u16 ReadU16(u64 a, bool* ok = nullptr);
    u32 ReadU32(u64 a, bool* ok = nullptr);
    u64 ReadU64(u64 a, bool* ok = nullptr);
    u32 ReadPtrAsU32(u64 a, bool* ok = nullptr);  // mimariye göre
    u64 ReadPtr(u64 a, bool* ok = nullptr);       // x64: 8 bayt, x86: 4 bayt
    int ptr_size() const { return is_x64_ ? 8 : 4; }

    // C dizesi oku (maks len; yazdırılabilirlik kontrolü opsiyonel)
    std::string ReadCString(u64 addr, size_t max_len = 512, bool require_printable = false, bool* ok = nullptr);

    // Bölge içi raw işaretçi (önbellekteyse doğrudan, yoksa geçici okuma)
    // pattern taramaları için verimli yol
    struct RegionData {
        u64 base = 0, size = 0;
        const u8* data = nullptr;  // önbellek sahipliği MemoryMap'te
    };
    bool GetRegionData(u64 containing_addr, RegionData* out);
    bool GetRegionDataForRange(u64 addr, size_t size, RegionData* out);

    // Yalnızca önbellekten okuma (uzak erişim YOK — hızlı taramalar için)
    bool FastRead(u64 addr, void* buf, size_t len) const;
    std::string FastReadCString(u64 addr, size_t max_len, bool require_printable, bool* ok) const;
    bool FastIsReadable(u64 addr) const { return IsReadable(addr); }
    bool IsWritable(u64 addr) const;   // bölge korumasına göre

    // Pattern (bayt + maske, ?? = joker)
    std::vector<u64> FindPattern(const char* pattern, const char* mask, u64 range_start, u64 range_end, int max_hits = 1);
    // Önceden alınmış bölge verisinde pattern
    static std::vector<u64> FindPatternIn(const u8* data, size_t size, u64 data_base,
                                          const char* pattern, const char* mask, int max_hits);

    // Bölge filtreleri
    std::vector<const MemRegion*> HeapRegions() const;       // PRIVATE + WRITE (heap adayı)
    std::vector<const MemRegion*> ImageRegions() const;      // MEM_IMAGE
    std::vector<const MemRegion*> ImageDataRegions() const;  // IMAGE + read (rdata/data)

    bool is_x64() const { return is_x64_; }

    // String indeksi (ImageDataRegions üzerinde ASCII string toplama)
    struct StrRef { u64 addr; const char* data; size_t len; };
    void BuildStringIndex(size_t min_len = 2, size_t max_len = 512);
    const std::vector<StrRef>& string_index() const { return strings_; }

private:
    IProcessReader& rd_;
    std::vector<MemRegion> regions_;
    struct CacheEntry { std::vector<u8> data; };
    std::unordered_map<u64, std::shared_ptr<CacheEntry>> cache_;
    u64 cached_bytes_ = 0;
    u64 readable_total_ = 0;
    bool is_x64_ = true;
    std::vector<StrRef> strings_;
    std::vector<std::shared_ptr<CacheEntry>> cache_keepalive_;
};

} // namespace umvp
