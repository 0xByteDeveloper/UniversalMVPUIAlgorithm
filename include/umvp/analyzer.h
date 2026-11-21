// ============================================================================
// umvp/analyzer.h — GOD STATE analiz orkestratörü
// Canlı bellek taraması + derin disassembly analizini birleştirir:
// formlar, widget'lar, kod adresleri (base/end), handler gövdeleri.
// ============================================================================
#pragma once
#include "types.h"
#include "process.h"
#include "memory_map.h"
#include "disasm.h"
#include "imgui_scan.h"
#include "imgui_hash.h"
#include "layout.h"
#include <functional>

namespace umvp {

struct AnalyzeOptions {
    bool scan_live = true;       // heap taraması (pencere/context)
    bool scan_code = true;       // disassembly analizi
    bool decode_draw_text = true;// draw-list metin çözümü (konumlar)
    int  max_widgets_per_form = 512;
    bool dump_menu_asm = true;   // menü fonksiyonunun tam disassembly'si rapora
};

class Analyzer {
public:
    Analyzer(WinProcess& proc, const AnalyzeOptions& opt = {});

    // Tüm pipeline'ı çalıştır
    AnalysisReport Run(const std::function<void(const char*)>& progress = nullptr);

private:
    WinProcess& proc_;
    AnalyzeOptions opt_;
    std::unique_ptr<MemoryMap> mm_;
    std::unique_ptr<DisasmEngine> de_;
    std::unique_ptr<XrefMap> xrefs_;
    std::unique_ptr<FuncBoundsResolver> fbr_;

    // draw-list'ten çözülen metin koordinatları (etiket → ekran konumu)
    struct TextRunRec { std::string text; Vec2 a, b; std::string window; };
    std::vector<TextRunRec> text_runs_all_;
    ContextInfo ctx_;                     // doğrulanan context
    std::vector<LiveWindow> live_windows_;
    std::vector<u64> hash_funcs_;         // ImHashStr/ImHashData adresleri
    std::unordered_map<u64, std::string> export_names_;  // addr → temiz ImGui adı
    std::unordered_map<u64, FuncBounds> func_cache_;
    std::unordered_map<u64, bool> callee_hash_cache_;    // callee → hash çağırıyor mu

    // aşamalar
    void PhaseRenderer(AnalysisReport& rep);
    void PhaseImGuiDetect(AnalysisReport& rep, const std::function<void(const char*)>& progress);
    void PhaseLive(AnalysisReport& rep, const std::function<void(const char*)>& progress);
    void PhaseCode(AnalysisReport& rep, const std::function<void(const char*)>& progress);

    // yardımcılar
    bool FindImGuiExports(AnalysisReport& rep);          // DLL linkajı → tam isimler
    std::vector<u64> FindHashFuncs(AnalysisReport& rep); // CRC tablo xref → ImHashStr
    void ClassifyAndExtract(AnalysisReport& rep, const std::function<void(const char*)>& progress);
    bool CalleeCallsHash(u64 callee);
    u64 ResolveThunk(u64 callee);  // jmp köprülerini gerçek işleve çöz
    FuncBounds* GetFuncBounds(u64 addr);
    void AssignLivePositions(AnalysisReport& rep);
};

} // namespace umvp
