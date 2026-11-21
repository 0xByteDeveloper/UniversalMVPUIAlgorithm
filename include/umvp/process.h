// ============================================================================
// umvp/process.h — Süreç erişim soyutlaması (Windows: Toolhelp + RPM)
// Linux native test build'leri için MemoryMap doğrudan lokal tamponla beslenir.
// ============================================================================
#pragma once
#include "types.h"
#include <functional>

namespace umvp {

// Soyut okuyucu — testlerde lokal tampon, hedefte ReadProcessMemory
class IProcessReader {
public:
    virtual ~IProcessReader() = default;
    // [addr, addr+size) → buf; okunan bayt sayısı (0 = okunamadı)
    virtual size_t Read(u64 addr, void* buf, size_t size) = 0;
    virtual std::vector<MemRegion> QueryRegions() = 0;
    virtual Arch arch() const = 0;
    virtual u32 pid() const = 0;
    virtual std::string name() const { return "?"; }
};

// Süreç listesi
std::vector<ProcessEntry> ListProcesses();

// Windows hedef süreci
class WinProcess : public IProcessReader {
public:
    WinProcess() = default;
    ~WinProcess();

    bool Open(u32 pid);
    bool OpenByName(const std::string& exe_name);  // kısmi eşleşme, ilk bulgu

    size_t Read(u64 addr, void* buf, size_t size) override;
    std::vector<MemRegion> QueryRegions() override;
    Arch arch() const override { return arch_; }
    u32 pid() const override { return pid_; }
    std::string name() const override { return name_; }

    const std::vector<ModuleEntry>& modules() const { return modules_; }
    // modül tabanlı RVA→VA çözümü
    const ModuleEntry* FindModule(u64 addr) const;
    const ModuleEntry* FindModuleByName(const std::string& name_lower) const;


    // PE analizi: modül başlığını okuyup kesit/dizin çıkarır (ModuleEntry doldurur)
    void AnalyzeModulePE(ModuleEntry& m);

private:
    void* handle_ = nullptr;      // HANDLE
    u32 pid_ = 0;
    std::string name_;
    Arch arch_ = Arch::Unknown;
    std::vector<ModuleEntry> modules_;
};

} // namespace umvp
