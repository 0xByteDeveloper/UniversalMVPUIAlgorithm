// ============================================================================
// src/win_process.cpp — Windows süreç erişimi (Toolhelp32 + VirtualQueryEx + RPM)
// Wine altında da tam çalışır (wineserver süreçleri aynı prefix'te görür).
// ============================================================================
#include "umvp/process.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

namespace umvp {

bool MemRegion::readable() const {
    if (state != MEM_COMMIT) return false;
    return (protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                       PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}
bool MemRegion::writable() const {
    if (state != MEM_COMMIT) return false;
    return (protect & (PAGE_READWRITE | PAGE_WRITECOPY |
                       PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}
bool MemRegion::exec() const {
    if (state != MEM_COMMIT) return false;
    return (protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                       PAGE_EXECUTE_WRITECOPY)) != 0;
}

std::vector<ProcessEntry> ListProcesses() {
    std::vector<ProcessEntry> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        char buf[512];
        do {
            ProcessEntry e;
            e.pid = pe.th32ProcessID;
            int n = WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, buf, sizeof(buf), nullptr, nullptr);
            e.name = (n > 0) ? std::string(buf, n - 1) : "?";
            out.push_back(std::move(e));
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return out;
}

WinProcess::~WinProcess() {
    if (handle_) CloseHandle((HANDLE)handle_);
}

bool WinProcess::Open(u32 pid) {
    handle_ = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!handle_) {
        handle_ = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!handle_) return false;
    }
    pid_ = pid;

    // Süreç adı
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            char buf[512];
            do {
                if (pe.th32ProcessID == pid) {
                    int n = WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, buf, sizeof(buf), nullptr, nullptr);
                    name_ = (n > 0) ? std::string(buf, n - 1) : "?";
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }

    // Modüller
    HANDLE msnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (msnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        if (Module32FirstW(msnap, &me)) {
            do {
                ModuleEntry m;
                m.base = (u64)me.modBaseAddr;
                m.size = me.modBaseSize;
                char buf[1024];
                int n = WideCharToMultiByte(CP_UTF8, 0, me.szModule, -1, buf, sizeof(buf), nullptr, nullptr);
                m.name = (n > 0) ? std::string(buf, n - 1) : "?";
                n = WideCharToMultiByte(CP_UTF8, 0, me.szExePath, -1, buf, sizeof(buf), nullptr, nullptr);
                m.path = (n > 0) ? std::string(buf, n - 1) : "";
                std::string low = m.name;
                for (auto& c : low) c = (char)tolower((unsigned char)c);
                m.is_dll = low.rfind(".exe") != low.size() - 4;
                modules_.push_back(std::move(m));
            } while (Module32NextW(msnap, &me));
        }
        CloseHandle(msnap);
    }

    // PE analizi (kesitler, .pdata, exports)
    for (auto& m : modules_) AnalyzeModulePE(m);

    // mimari: ana modülden
    for (auto& m : modules_) {
        if (!m.is_dll) { arch_ = m.arch; break; }
    }
    if (arch_ == Arch::Unknown && !modules_.empty()) arch_ = modules_[0].arch;
    return !modules_.empty();
}

bool WinProcess::OpenByName(const std::string& exe_name) {
    for (auto& p : ListProcesses()) {
        if (p.name == exe_name || p.name.find(exe_name) != std::string::npos) {
            return Open(p.pid);
        }
    }
    return false;
}

size_t WinProcess::Read(u64 addr, void* buf, size_t size) {
    if (!handle_) return 0;
    size_t done = 0;
    u8* p = (u8*)buf;
    while (done < size) {
        SIZE_T got = 0;
        SIZE_T chunk = (SIZE_T)std::min<size_t>(size - done, 0x1000000);
        if (ReadProcessMemory((HANDLE)handle_, (LPCVOID)(addr + done), p + done, chunk, &got) && got > 0) {
            done += got;
            continue;
        }
        // büyük okuma başarısız/kısmi → sayfa sayfa (indeksleme: done sabit, i ilerler)
        bool any = false;
        for (SIZE_T i = 0; i < chunk; i += 0x1000) {
            SIZE_T sub = 0;
            SIZE_T len = std::min<SIZE_T>(0x1000, chunk - i);
            if (ReadProcessMemory((HANDLE)handle_, (LPCVOID)(addr + done + i), p + done + i, len, &sub) && sub > 0) {
                any = true;
                if (sub < len) { done += i + sub; return done; }
            } else {
                if (any) { done += i; return done; }
                // baştaki okunamayan sayfaları atla (sonraki sayfaları dene)
            }
        }
        if (!any) return done;
        done += chunk;
    }
    return done;
}

const ModuleEntry* WinProcess::FindModule(u64 addr) const {
    for (auto& m : modules_)
        if (addr >= m.base && addr < m.base + m.size) return &m;
    return nullptr;
}
const ModuleEntry* WinProcess::FindModuleByName(const std::string& name_lower) const {
    for (auto& m : modules_) {
        std::string low = m.name;
        for (auto& c : low) c = (char)tolower((unsigned char)c);
        if (low == name_lower) return &m;
    }
    return nullptr;
}

std::vector<MemRegion> WinProcess::QueryRegions() {
    std::vector<MemRegion> out;
    if (!handle_) return out;
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    u64 addr = (u64)si.lpMinimumApplicationAddress;
    u64 maxa = (u64)si.lpMaximumApplicationAddress;
    MEMORY_BASIC_INFORMATION mbi{};
    while (addr < maxa) {
        if (VirtualQueryEx((HANDLE)handle_, (LPCVOID)addr, &mbi, sizeof(mbi)) == 0) break;
        MemRegion r;
        r.base = (u64)mbi.BaseAddress;
        r.size = mbi.RegionSize;
        r.state = mbi.State;
        r.protect = mbi.Protect;
        r.type = mbi.Type;
        out.push_back(r);
        addr = r.base + r.size;
        if (mbi.RegionSize == 0) break;
    }
    return out;
}

// ---- PE analizi ------------------------------------------------------------
void WinProcess::AnalyzeModulePE(ModuleEntry& m) {
    if (!handle_ || m.base == 0) return;
    u8 hdr[0x1000];
    if (Read(m.base, hdr, sizeof(hdr)) != sizeof(hdr)) return;
    auto* dos = (IMAGE_DOS_HEADER*)hdr;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    if (dos->e_lfanew <= 0 || (u32)dos->e_lfanew + 0x400 > sizeof(hdr)) return;
    auto* nt = (IMAGE_NT_HEADERS64*)(hdr + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;
    WORD machine = nt->FileHeader.Machine;
    m.arch = (machine == IMAGE_FILE_MACHINE_AMD64) ? Arch::X64
             : (machine == IMAGE_FILE_MACHINE_I386) ? Arch::X86 : Arch::Unknown;

    WORD nsec = nt->FileHeader.NumberOfSections;
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nsec && i < 96; i++, sec++) {
        ModuleEntry::Section s;
        char nb[9] = {};
        memcpy(nb, sec->Name, 8);
        s.name = nb;
        s.rva = sec->VirtualAddress;
        s.vsize = sec->Misc.VirtualSize;
        s.raw_size = sec->SizeOfRawData;
        s.chars = sec->Characteristics;
        s.exec = (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        s.write = (sec->Characteristics & IMAGE_SCN_MEM_WRITE) != 0;
        s.read = (sec->Characteristics & IMAGE_SCN_MEM_READ) != 0;
        m.sections.push_back(std::move(s));
    }
    auto& dd = nt->OptionalHeader.DataDirectory;
    if (nt->OptionalHeader.NumberOfRvaAndSizes > 3) {
        m.pdata_rva = dd[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress;
        m.pdata_size = dd[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size;
    }
    if (nt->OptionalHeader.NumberOfRvaAndSizes > 0) {
        m.export_rva = dd[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        m.export_size = dd[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
    }
}

} // namespace umvp

#else  // !_WIN32 — native test build'i için iskelet
namespace umvp {
bool MemRegion::readable() const { return false; }
bool MemRegion::writable() const { return false; }
bool MemRegion::exec() const { return false; }
std::vector<ProcessEntry> ListProcesses() { return {}; }
WinProcess::~WinProcess() {}
bool WinProcess::Open(u32) { return false; }
bool WinProcess::OpenByName(const std::string&) { return false; }
size_t WinProcess::Read(u64, void*, size_t) { return 0; }
std::vector<MemRegion> WinProcess::QueryRegions() { return {}; }
const ModuleEntry* WinProcess::FindModule(u64) const { return nullptr; }
const ModuleEntry* WinProcess::FindModuleByName(const std::string&) const { return nullptr; }
void WinProcess::AnalyzeModulePE(ModuleEntry&) {}
} // namespace umvp
#endif
