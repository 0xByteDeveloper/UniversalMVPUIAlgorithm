// ============================================================================
// umvp/report.h — Analiz raporu çıktıları (konsol metni + JSON + .asm dosyaları)
// ============================================================================
#pragma once
#include "types.h"
#include <string>
#include <vector>

namespace umvp {

// Konsol / dosya metin raporu
std::string RenderTextReport(const AnalysisReport& rep);

// JSON raporu (tam veri modeli)
std::string RenderJsonReport(const AnalysisReport& rep);

// Handler başına .asm dosyaları yaz → dönen: yazılan dosya listesi
std::vector<std::string> WriteAsmDumps(const AnalysisReport& rep, const std::string& out_dir);

} // namespace umvp
