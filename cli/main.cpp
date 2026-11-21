// ============================================================================
// cli/main.cpp — UMVP komut satırı aracı
//   umvp_cli                → süreç listesi + interaktif PID seçimi
//   umvp_cli --list         → süreç listesi
//   umvp_cli --pid N        → PID'ye analiz
//   umvp_cli --name x.exe   → isimle analiz
//   umvp_cli --pid N --json out.json --asm-dir asm/ --no-live --no-code
// ============================================================================
#include "umvp/analyzer.h"
#include "umvp/report.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

using namespace umvp;

static void PrintBanner() {
    printf("=============================================================================\n");
    printf("   UMVP // UniversalMVPUIAlgorithm — MVP GOD STATE UI ANALYSIS SDK\n");
    printf("   ImGui menu/form/widget extractor + deep disassembly analysis\n");
    printf("   d3d9 | d3d10 | d3d11 | d3d12 | OpenGL | Vulkan — render agnostic\n");
    printf("=============================================================================\n");
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    PrintBanner();

    u32 pid = 0;
    std::string name, json_out, asm_dir, txt_out;
    bool opt_live = true, opt_code = true, opt_text_dcd = true;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&](const char* what) -> std::string {
            if (i + 1 >= argc) { fprintf(stderr, "HATA: %s icin deger eksik\n", what); exit(2); }
            return argv[++i];
        };
        if (a == "--list") {
            for (auto& p : ListProcesses())
                printf("%6u  %s\n", p.pid, p.name.c_str());
            return 0;
        } else if (a == "--pid") pid = (u32)atoi(next("--pid").c_str());
        else if (a == "--name") name = next("--name");
        else if (a == "--json") json_out = next("--json");
        else if (a == "--txt") txt_out = next("--txt");
        else if (a == "--asm-dir") asm_dir = next("--asm-dir");
        else if (a == "--no-live") opt_live = false;
        else if (a == "--no-code") opt_code = false;
        else if (a == "--no-text-decode") opt_text_dcd = false;
        else if (a == "--help" || a == "-h") {
            printf("Kullanim: umvp_cli [--list | --pid N | --name exe] [--json f] [--txt f] [--asm-dir d]\n"
                   "                 [--no-live] [--no-code] [--no-text-decode]\n");
            return 0;
        }
    }

    // ---- hedef seçimi ----
    if (pid == 0 && name.empty()) {
        printf("Surecler taraniyor...\n");
        auto procs = ListProcesses();
        if (procs.empty()) {
            fprintf(stderr, "HATA: surec listesi alinamadi\n");
            return 1;
        }
        printf("%6s  %s\n", "PID", "AD");
        printf("------  ------------------------------------------------\n");
        for (auto& p : procs)
            printf("%6u  %s\n", p.pid, p.name.c_str());
        printf("\nAnaliz icin PID girin (Enter = cikis): ");
        char buf[64] = {};
        if (!fgets(buf, sizeof(buf), stdin)) return 0;
        pid = (u32)strtoul(buf, nullptr, 10);
        if (pid == 0) { printf("Iptal.\n"); return 0; }
    }

    WinProcess proc;
    if (!name.empty()) {
        if (!proc.OpenByName(name)) {
            fprintf(stderr, "HATA: '%s' bulunamadi/acilamadi\n", name.c_str());
            return 1;
        }
    } else if (!proc.Open(pid)) {
        fprintf(stderr, "HATA: PID %u acilamadi (ProcessId dogru mu? yetki?)\n", pid);
        return 1;
    }
    printf("Hedef: %s (PID %u) [%s]\n\n", proc.name().c_str(), proc.pid(), ArchName(proc.arch()));

    AnalyzeOptions opts;
    opts.scan_live = opt_live;
    opts.scan_code = opt_code;
    opts.decode_draw_text = opt_text_dcd;

    Analyzer an(proc, opts);
    AnalysisReport rep = an.Run([](const char* msg) {
        printf("  [*] %s\n", msg);
        fflush(stdout);
    });

    std::string text = RenderTextReport(rep);
    printf("%s", text.c_str());

    if (!txt_out.empty()) {
        FILE* f = fopen(txt_out.c_str(), "wb");
        if (f) { fwrite(text.data(), 1, text.size(), f); fclose(f); printf("\nRapor: %s\n", txt_out.c_str()); }
    }
    if (!json_out.empty()) {
        std::string j = RenderJsonReport(rep);
        FILE* f = fopen(json_out.c_str(), "wb");
        if (f) { fwrite(j.data(), 1, j.size(), f); fclose(f); printf("JSON  : %s\n", json_out.c_str()); }
    }
    if (!asm_dir.empty()) {
        auto files = WriteAsmDumps(rep, asm_dir);
        printf("ASM   : %zu handler dosyasi -> %s\n", files.size(), asm_dir.c_str());
    }
    return rep.imgui.found ? 0 : 3;
}
