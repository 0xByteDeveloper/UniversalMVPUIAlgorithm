// ============================================================================
// src/memory_map.cpp — Bellek anlık görüntüsü uygulaması
// ============================================================================
#include "umvp/memory_map.h"
#include <algorithm>
#include <cstring>

namespace umvp {

void MemoryMap::Build(size_t max_region_bytes, u64 max_total_bytes) {
    regions_ = rd_.QueryRegions();
    is_x64_ = rd_.arch() != Arch::X86;

    cache_.clear();
    cache_keepalive_.clear();
    cached_bytes_ = 0;
    readable_total_ = 0;

    // Öncelik: küçük-orta bölgeyi tam önbelleğe al; büyük IMAGE bölge parçalı okunur
    for (auto& r : regions_) {
        if (!r.readable()) continue;
        readable_total_ += r.size;
    }

    for (auto& r : regions_) {
        if (!r.readable()) continue;
        if (r.size > max_region_bytes) continue;
        if (cached_bytes_ + r.size > max_total_bytes) continue;
        auto e = std::make_shared<CacheEntry>();
        e->data.resize((size_t)r.size);
        size_t got = rd_.Read(r.base, e->data.data(), e->data.size());
        if (got != e->data.size()) {
            // kısmi okuma: bölgeyi ikiye böl, okunabilen kısmı sakla
            if (got == 0) continue;
            e->data.resize(got);
            // bölge kaydını daralt
            MemRegion r2 = r;
            r2.size = got;
            // orijinali kaldır yerine daraltılmış ekle
            // (bölge vektöründe değişiklik yapmayalım: cache lookup'u addr aralığıyla yapacağız)
            // → cache'e daraltılmış aralık kaydet
            r2.base = r.base;
            r2.size = got;
            // regions_ içinde r'yi r2 ile değiştir
            for (auto& rr : regions_) {
                if (rr.base == r.base && rr.size == r.size) { rr.size = got; break; }
            }
        }
        cache_[r.base] = e;
        cache_keepalive_.push_back(e);
        cached_bytes_ += e->data.size();
    }
}

bool MemoryMap::IsReadable(u64 addr) const {
    for (auto& r : regions_)
        if (addr >= r.base && addr < r.base + r.size) return r.readable();
    return false;
}

bool MemoryMap::IsReadableRange(u64 addr, size_t size) const {
    if (size == 0) return IsReadable(addr);
    for (auto& r : regions_) {
        if (addr >= r.base && addr < r.base + r.size) {
            u64 off = addr - r.base;
            return (off + size <= r.size) && r.readable();
        }
    }
    return false;
}

size_t MemoryMap::Read(u64 addr, void* buf, size_t size) {
    // önbellekte mi?
    for (auto& r : regions_) {
        if (addr >= r.base && addr < r.base + r.size && r.readable()) {
            u64 off = addr - r.base;
            u64 avail = r.size - off;
            size_t take = (size_t)std::min<u64>(size, avail);
            auto it = cache_.find(r.base);
            if (it != cache_.end() && off + take <= it->second->data.size()) {
                memcpy(buf, it->second->data.data() + off, take);
                return take;
            }
            return rd_.Read(addr, buf, take);
        }
    }
    return 0;
}

u8 MemoryMap::ReadU8(u64 a, bool* ok) {
    u8 v = 0; bool o = Read(a, &v, 1) == 1; if (ok) *ok = o; return v;
}
u16 MemoryMap::ReadU16(u64 a, bool* ok) {
    u16 v = 0; bool o = Read(a, &v, 2) == 2; if (ok) *ok = o; return v;
}
u32 MemoryMap::ReadU32(u64 a, bool* ok) {
    u32 v = 0; bool o = Read(a, &v, 4) == 4; if (ok) *ok = o; return v;
}
u64 MemoryMap::ReadU64(u64 a, bool* ok) {
    u64 v = 0; bool o = Read(a, &v, 8) == 8; if (ok) *ok = o; return v;
}
u64 MemoryMap::ReadPtr(u64 a, bool* ok) {
    if (is_x64_) return ReadU64(a, ok);
    u32 v = ReadU32(a, ok);
    return v;
}

std::string MemoryMap::ReadCString(u64 addr, size_t max_len, bool require_printable, bool* ok) {
    std::string out;
    if (ok) *ok = false;
    if (addr == 0) return out;
    RegionData rd;
    if (GetRegionDataForRange(addr, 1, &rd)) {
        u64 off = addr - rd.base;
        size_t avail = (size_t)(rd.size - off);
        size_t n = std::min(max_len, avail);
        const char* p = (const char*)(rd.data + off);
        size_t i = 0;
        for (; i < n; i++) {
            char c = p[i];
            if (c == 0) {
                if (ok) *ok = true;
                if (require_printable && i == 0) { *ok = false; return {}; }
                return out;
            }
            if (require_printable) {
                unsigned char uc = (unsigned char)c;
                if (uc < 0x20 || uc > 0x7E) return {};  // yazdırılabilir değil
            }
            out.push_back(c);
        }
        // max_len doldu ama NUL yok → string değil (çok uzun)
        if (i == max_len) return {};
        if (ok) *ok = true;
        return out;
    }
    // yavaş yol
    char buf[64];
    size_t got = 0;
    while (out.size() < max_len) {
        got = Read(addr + out.size(), buf, sizeof(buf));
        if (got == 0) return {};
        for (size_t i = 0; i < got; i++) {
            if (buf[i] == 0) { if (ok) *ok = true; return out; }
            if (require_printable) {
                unsigned char uc = (unsigned char)buf[i];
                if (uc < 0x20 || uc > 0x7E) return {};
            }
            out.push_back(buf[i]);
            if (out.size() >= max_len) return {};
        }
    }
    return {};
}

bool MemoryMap::GetRegionData(u64 containing_addr, RegionData* out) {
    for (auto& r : regions_) {
        if (containing_addr >= r.base && containing_addr < r.base + r.size && r.readable()) {
            auto it = cache_.find(r.base);
            if (it != cache_.end()) {
                out->base = r.base;
                out->size = it->second->data.size();
                out->data = it->second->data.data();
                return true;
            }
            // önbellekte değil → oku ve cache'e ekle (küçükse)
            if (r.size <= 16 * 1024 * 1024) {
                auto e = std::make_shared<CacheEntry>();
                e->data.resize((size_t)r.size);
                size_t got = rd_.Read(r.base, e->data.data(), e->data.size());
                if (got == 0) return false;
                e->data.resize(got);
                cache_[r.base] = e;
                cache_keepalive_.push_back(e);
                cached_bytes_ += got;
                for (auto& rr : regions_)
                    if (rr.base == r.base && got < rr.size) rr.size = got;
                out->base = r.base;
                out->size = got;
                out->data = e->data.data();
                return true;
            }
            return false;
        }
    }
    return false;
}

bool MemoryMap::GetRegionDataForRange(u64 addr, size_t size, RegionData* out) {
    for (auto& r : regions_) {
        if (addr >= r.base && addr < r.base + r.size && r.readable()) {
            u64 off = addr - r.base;
            if (off + size > r.size) return false;
            return GetRegionData(addr, out);
        }
    }
    return false;
}

std::vector<u64> MemoryMap::FindPatternIn(const u8* data, size_t size, u64 data_base,
                                          const char* pattern, const char* mask, int max_hits) {
    std::vector<u64> hits;
    size_t plen = strlen(pattern);
    for (size_t i = 0; i + plen <= size; i++) {
        bool m = true;
        for (size_t j = 0; j < plen; j++) {
            if (mask[j] == 'x' && data[i + j] != (u8)pattern[j]) { m = false; break; }
        }
        if (m) {
            hits.push_back(data_base + i);
            if (max_hits > 0 && (int)hits.size() >= max_hits) break;
        }
    }
    return hits;
}

std::vector<u64> MemoryMap::FindPattern(const char* pattern, const char* mask, u64 range_start, u64 range_end, int max_hits) {
    std::vector<u64> out;
    for (auto& r : regions_) {
        if (!r.readable()) continue;
        if (r.base + r.size <= range_start || r.base >= range_end) continue;
        RegionData rd;
        if (!GetRegionData(r.base + 1, &rd)) continue;
        u64 lo = std::max<u64>(r.base, range_start);
        u64 hi = std::min<u64>(r.base + r.size, range_end);
        if (lo >= rd.base && hi <= rd.base + rd.size) {
            auto hits = FindPatternIn(rd.data + (lo - rd.base), (size_t)(hi - lo), lo, pattern, mask, max_hits);
            for (auto h : hits) {
                out.push_back(h);
                if (max_hits > 0 && (int)out.size() >= max_hits) return out;
            }
        }
    }
    return out;
}

std::vector<const MemRegion*> MemoryMap::HeapRegions() const {
    std::vector<const MemRegion*> out;
    for (auto& r : regions_)
        if (r.readable() && r.writable() && r.is_private()) out.push_back(&r);
    return out;
}
std::vector<const MemRegion*> MemoryMap::ImageRegions() const {
    std::vector<const MemRegion*> out;
    for (auto& r : regions_)
        if (r.readable() && r.is_image()) out.push_back(&r);
    return out;
}
std::vector<const MemRegion*> MemoryMap::ImageDataRegions() const {
    std::vector<const MemRegion*> out;
    for (auto& r : regions_)
        if (r.readable() && r.is_image() && !r.exec()) out.push_back(&r);
    return out;
}

bool MemoryMap::IsWritable(u64 addr) const {
    for (auto& r : regions_)
        if (addr >= r.base && addr < r.base + r.size) return r.writable();
    return false;
}

bool MemoryMap::FastRead(u64 addr, void* buf, size_t len) const {
    // regions_ sıralı değil → önce sıralanmış indeks kur (bir kez)
    // basitlik: doğrusal arama bölge sayısı az olduğunda yeterli;
    // performans için sıralı vektör önbelleği
    static thread_local std::vector<const MemRegion*> sorted;
    static thread_local const MemoryMap* owner = nullptr;
    if (owner != this) {
        sorted.clear();
        for (auto& r : regions_) sorted.push_back(&r);
        std::sort(sorted.begin(), sorted.end(), [](auto* a, auto* b) { return a->base < b->base; });
        owner = this;
    }
    // ikili arama
    size_t lo = 0, hi = sorted.size();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (sorted[mid]->base + sorted[mid]->size <= addr) lo = mid + 1;
        else hi = mid;
    }
    if (lo >= sorted.size()) return false;
    const MemRegion* r = sorted[lo];
    if (!(addr >= r->base && addr < r->base + r->size) || !r->readable()) return false;
    u64 off = addr - r->base;
    if (off + len > r->size) return false;
    auto it = cache_.find(r->base);
    if (it != cache_.end()) {
        if (off + len > it->second->data.size()) return false;
        memcpy(buf, it->second->data.data() + off, len);
        return true;
    }
    // onbellekte yok → dogrudan hedeften oku (taramalarin dogrulugu icin kritik)
    return const_cast<MemoryMap*>(this)->rd_.Read(addr, buf, len) == len;
}

std::string MemoryMap::FastReadCString(u64 addr, size_t max_len, bool require_printable, bool* ok) const {
    if (ok) *ok = false;
    if (addr == 0) return {};
    char chunk[64];
    std::string out;
    while (out.size() < max_len) {
        size_t want = std::min(sizeof(chunk), max_len - out.size());
        if (!FastRead(addr + out.size(), chunk, want)) return {};
        for (size_t i = 0; i < want; i++) {
            if (chunk[i] == 0) {
                if (ok) *ok = true;
                return out;
            }
            if (require_printable) {
                u8 c = (u8)chunk[i];
                if (c < 0x20 || c > 0x7E) return {};
            }
            out.push_back(chunk[i]);
        }
    }
    return {};
}

void MemoryMap::BuildStringIndex(size_t min_len, size_t max_len) {
    strings_.clear();
    for (auto* pr : ImageDataRegions()) {
        RegionData rd;
        if (!GetRegionData(pr->base + 1, &rd)) continue;
        size_t start = 0;
        for (size_t i = 0; i <= rd.size; i++) {
            bool end = (i == rd.size);
            if (!end) {
                u8 c = rd.data[i];
                if (c < 0x20 || c > 0x7E) end = true;  // yazdırılabilir ASCII dışı
            }
            if (end) {
                size_t len = i - start;
                if (len >= min_len && len <= max_len) {
                    StrRef s;
                    s.addr = rd.base + start;
                    s.data = (const char*)(rd.data + start);
                    s.len = len;
                    strings_.push_back(s);
                }
                start = i + 1;
            }
        }
    }
}

} // namespace umvp
