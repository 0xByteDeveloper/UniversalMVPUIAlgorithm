// ============================================================================
// src/report.cpp — Rapor üretimi
// ============================================================================
#include "umvp/report.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace umvp {

static std::string H(u64 v) {
    char b[32];
    snprintf(b, sizeof(b), "0x%llX", (unsigned long long)v);
    return b;
}
static std::string F2(Vec2 v) {
    char b[64];
    snprintf(b, sizeof(b), "(%.1f, %.1f)", v.x, v.y);
    return v.valid ? b : "(?)";
}
static std::string JsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char b[8];
                    snprintf(b, sizeof(b), "\\u%04X", c);
                    o += b;
                } else o += c;
        }
    }
    return o;
}

std::string RenderTextReport(const AnalysisReport& rep) {
    std::string o;
    char line[512];
    auto P = [&](const std::string& s) { o += s; o += "\n"; };

    P("===============================================================================");
    P("  UMVP // UniversalMVPUIAlgorithm — MVP GOD STATE UI ANALYSIS");
    P("===============================================================================");
    snprintf(line, sizeof(line), "Hedef        : %s (PID %u) [%s]", rep.process_name.c_str(), rep.pid,
             ArchName(rep.arch));
    P(line);
    snprintf(line, sizeof(line), "Renderer     : %s (%s)", rep.renderer.api.c_str(), rep.renderer.api_detail.c_str());
    P(line);
    if (!rep.renderer.graphics_modules.empty()) {
        std::string m = "Grafik mod.  : ";
        for (size_t i = 0; i < rep.renderer.graphics_modules.size(); i++)
            m += rep.renderer.graphics_modules[i] + (i + 1 < rep.renderer.graphics_modules.size() ? ", " : "");
        P(m);
    }
    snprintf(line, sizeof(line), "Bellek       : %zu bolge, %.1f MB okunabilir, %.1f MB anlik goruntu",
             rep.region_count, rep.readable_bytes / 1048576.0, rep.cached_bytes / 1048576.0);
    P(line);
    P("");

    P("--- ImGui ---------------------------------------------------------------------");
    if (rep.imgui.found) {
        snprintf(line, sizeof(line), "Bulundu      : EVET (%s)", rep.imgui.linkage.c_str());
        P(line);
        snprintf(line, sizeof(line), "Surum        : %s%s", rep.imgui.version_str.c_str(),
                 rep.imgui.version_confirmed ? "  [canli bellekten DOGRULANDI]" : "  [string taramasi]");
        P(line);
        snprintf(line, sizeof(line), "Hash donemi  : %s", rep.imgui.hash_variant.c_str());
        P(line);
        snprintf(line, sizeof(line), "Context      : %s", H(rep.imgui.context_addr).c_str());
        P(line);
        snprintf(line, sizeof(line), "CRC32 tablosu: %s", H(rep.imgui.crc32_table_addr).c_str());
        P(line);
        snprintf(line, sizeof(line), "ImHashStr    : %s", H(rep.imgui.imhashstr_func).c_str());
        P(line);
        if (rep.imgui.display_size.valid) {
            snprintf(line, sizeof(line), "DisplaySize  : %.0f x %.0f | Framerate: %.1f FPS | Aktif pencere: %d",
                     rep.imgui.display_size.x, rep.imgui.display_size.y, rep.imgui.framerate,
                     rep.imgui.windows_active_count);
            P(line);
        }
    } else {
        P("Bulundu      : HAYIR (hedefte ImGui izi yok — menu acik mi?)");
    }
    P("");

    snprintf(line, sizeof(line), "--- FORMLAR: %d ----------------------------------------------------------------", rep.form_count);
    P(line);
    if (rep.forms.empty()) {
        P("  (form bulunamadi)");
    }
    for (size_t fi = 0; fi < rep.forms.size(); fi++) {
        auto& f = rep.forms[fi];
        snprintf(line, sizeof(line), "  [%d] \"%s\"  id=0x%08X  %s%s", (int)fi + 1, f.name.c_str(), f.id,
                 f.live_found ? "[CANLI]" : "[KOD]", f.code_found ? "[KOD-ANALIZ]" : "");
        P(line);
        snprintf(line, sizeof(line), "      nesne=%s  pos=%s  size=%s", H(f.window_obj).c_str(),
                 F2(f.pos).c_str(), F2(f.size).c_str());
        P(line);
        if (!f.flags_decoded.empty())
            { std::string s = "      flags: " + f.flags_decoded; P(s); }
        if (f.static_size.valid)
            { snprintf(line, sizeof(line), "      SetNextWindowSize (kod): %s", F2(f.static_size).c_str()); P(line); }
        if (f.begin_call)
            { snprintf(line, sizeof(line), "      Begin cagrisi: %s | form kod blogu: %s..%s",
                       H(f.begin_call).c_str(), H(f.menu_func_start).c_str(), H(f.menu_func_end).c_str()); P(line); }
        if (f.widgets.empty()) continue;
        snprintf(line, sizeof(line), "      widget'lar (%zu):", f.widgets.size());
        P(line);
        for (size_t wi = 0; wi < f.widgets.size(); wi++) {
            auto& w = f.widgets[wi];
            snprintf(line, sizeof(line), "        %2zu. %-12s \"%s\"  guven=%d%%", wi + 1, w.type.c_str(),
                     w.label.c_str(), w.confidence);
            P(line);
            snprintf(line, sizeof(line), "            cagri=%s  imgui_fn=%s  id=0x%08X", H(w.call_site).c_str(),
                     H(w.callee).c_str(), w.id_hash);
            P(line);
            if (w.size_arg.valid || w.pos.valid || w.size.valid) {
                snprintf(line, sizeof(line), "            boyut=%s [%s]  konum=%s [%s]", F2(w.size).c_str(),
                         ConfName(w.size_conf), F2(w.pos).c_str(), ConfName(w.pos_conf));
                P(line);
            }
            if (!w.float_args.empty()) {
                snprintf(line, sizeof(line), "            arg float: min=%.3f max=%.3f", w.float_args[0],
                         w.float_args.size() > 1 ? w.float_args[1] : 0.0f);
                P(line);
            }
            if (!w.format_arg.empty()) {
                std::string s = "            format: \"" + w.format_arg + "\""; P(s);
            }
            if (!w.combo_items.empty()) {
                std::string s = "            ogeler: " + w.combo_items; P(s);
            }
            if (w.handler_start) {
                snprintf(line, sizeof(line), "            handler kod blogu: %s..%s (%zu komut)",
                         H(w.handler_start).c_str(), H(w.handler_end).c_str(), w.handler_disasm.size());
                P(line);
                for (auto& dl : w.handler_disasm) {
                    std::string s = "              ";
                    s += dl;
                    P(s);
                }
            }
            if (!w.live_validation.empty()) {
                std::string s = "            DOGRULAMA: " + w.live_validation; P(s);
            }
        }
    }
    P("");
    snprintf(line, sizeof(line), "Toplam widget: %d | Menu fonksiyonu: %s..%s | analiz: %.0f ms",
             rep.total_widgets, H(rep.menu_func_start).c_str(), H(rep.menu_func_end).c_str(), rep.analysis_ms);
    P(line);
    P("===============================================================================");
    return o;
}

// ---------------------------------------------------------------------------
static std::string Indent(int n) { return std::string(n, ' '); }

std::string RenderJsonReport(const AnalysisReport& rep) {
    std::string o = "{\n";
    o += "  \"tool\": \"UniversalMVPUIAlgorithm\",\n";
    o += "  \"pid\": " + std::to_string(rep.pid) + ",\n";
    o += "  \"process\": \"" + JsonEscape(rep.process_name) + "\",\n";
    o += "  \"arch\": \"" + std::string(ArchName(rep.arch)) + "\",\n";
    o += "  \"renderer\": {\n    \"api\": \"" + rep.renderer.api + "\",\n    \"detail\": \"" +
         JsonEscape(rep.renderer.api_detail) + "\",\n    \"modules\": [";
    for (size_t i = 0; i < rep.renderer.graphics_modules.size(); i++)
        o += "\"" + JsonEscape(rep.renderer.graphics_modules[i]) + "\"" + (i + 1 < rep.renderer.graphics_modules.size() ? "," : "");
    o += "]\n  },\n";
    o += "  \"memory\": { \"regions\": " + std::to_string(rep.region_count) +
         ", \"readable_bytes\": " + std::to_string(rep.readable_bytes) + " },\n";
    o += "  \"imgui\": {\n";
    o += "    \"found\": " + std::string(rep.imgui.found ? "true" : "false") + ",\n";
    o += "    \"version\": \"" + JsonEscape(rep.imgui.version_str) + "\",\n";
    o += "    \"version_num\": " + std::to_string(rep.imgui.version_num) + ",\n";
    o += "    \"version_confirmed\": " + std::string(rep.imgui.version_confirmed ? "true" : "false") + ",\n";
    o += "    \"hash_era\": \"" + JsonEscape(rep.imgui.hash_variant) + "\",\n";
    o += "    \"linkage\": \"" + JsonEscape(rep.imgui.linkage) + "\",\n";
    o += "    \"context_addr\": " + std::to_string(rep.imgui.context_addr) + ",\n";
    o += "    \"crc32_table\": " + std::to_string(rep.imgui.crc32_table_addr) + ",\n";
    o += "    \"imhashstr_func\": " + std::to_string(rep.imgui.imhashstr_func) + ",\n";
    o += "    \"display_size\": [" + (rep.imgui.display_size.valid ? std::to_string(rep.imgui.display_size.x) + "," + std::to_string(rep.imgui.display_size.y) : "null") + "],\n";
    o += "    \"framerate\": " + std::to_string(rep.imgui.framerate) + ",\n";
    o += "    \"windows_active_count\": " + std::to_string(rep.imgui.windows_active_count) + "\n";
    o += "  },\n";
    o += "  \"menu_function\": { \"start\": " + std::to_string(rep.menu_func_start) +
         ", \"end\": " + std::to_string(rep.menu_func_end) + " },\n";
    o += "  \"form_count\": " + std::to_string(rep.form_count) + ",\n";
    o += "  \"total_widgets\": " + std::to_string(rep.total_widgets) + ",\n";
    o += "  \"forms\": [";
    for (size_t fi = 0; fi < rep.forms.size(); fi++) {
        auto& f = rep.forms[fi];
        o += "\n  {\n";
        o += "    \"name\": \"" + JsonEscape(f.name) + "\",\n";
        o += "    \"id\": " + std::to_string(f.id) + ",\n";
        o += "    \"window_obj\": " + std::to_string(f.window_obj) + ",\n";
        o += "    \"live_found\": " + std::string(f.live_found ? "true" : "false") + ",\n";
        o += "    \"code_found\": " + std::string(f.code_found ? "true" : "false") + ",\n";
        o += "    \"pos\": " + (f.pos.valid ? "[" + std::to_string(f.pos.x) + "," + std::to_string(f.pos.y) + "]" : "null") + ",\n";
        o += "    \"size\": " + (f.size.valid ? "[" + std::to_string(f.size.x) + "," + std::to_string(f.size.y) + "]" : "null") + ",\n";
        o += "    \"flags\": " + std::to_string(f.flags) + ",\n";
        o += "    \"flags_decoded\": \"" + JsonEscape(f.flags_decoded) + "\",\n";
        o += "    \"child\": " + std::string(f.child ? "true" : "false") + ",\n";
        o += "    \"begin_call\": " + std::to_string(f.begin_call) + ",\n";
        o += "    \"code_range\": [" + std::to_string(f.menu_func_start) + "," + std::to_string(f.menu_func_end) + "],\n";
        o += "    \"widgets\": [";
        for (size_t wi = 0; wi < f.widgets.size(); wi++) {
            auto& w = f.widgets[wi];
            o += "\n      {\n";
            o += "        \"type\": \"" + JsonEscape(w.type) + "\",\n";
            o += "        \"label\": \"" + JsonEscape(w.label) + "\",\n";
            o += "        \"display_text\": \"" + JsonEscape(w.display_text) + "\",\n";
            o += "        \"id_hash\": " + std::to_string(w.id_hash) + ",\n";
            o += "        \"call_site\": " + std::to_string(w.call_site) + ",\n";
            o += "        \"callee\": " + std::to_string(w.callee) + ",\n";
            o += "        \"confidence\": " + std::to_string(w.confidence) + ",\n";
            o += "        \"evidence\": \"" + JsonEscape(w.evidence) + "\",\n";
            o += "        \"size_arg\": " + (w.size_arg.valid ? "[" + std::to_string(w.size_arg.x) + "," + std::to_string(w.size_arg.y) + "]" : std::string("null")) + ",\n";
            o += "        \"pos\": " + (w.pos.valid ? "[" + std::to_string(w.pos.x) + "," + std::to_string(w.pos.y) + "]" : std::string("null")) + ",\n";
            o += "        \"pos_conf\": \"" + std::string(ConfName(w.pos_conf)) + "\",\n";
            o += "        \"size\": " + (w.size.valid ? "[" + std::to_string(w.size.x) + "," + std::to_string(w.size.y) + "]" : std::string("null")) + ",\n";
            o += "        \"size_conf\": \"" + std::string(ConfName(w.size_conf)) + "\",\n";
            if (!w.float_args.empty()) {
                o += "        \"float_args\": [";
                for (size_t k = 0; k < w.float_args.size(); k++)
                    o += std::to_string(w.float_args[k]) + (k + 1 < w.float_args.size() ? "," : "");
                o += "],\n";
            }
            if (!w.format_arg.empty()) o += "        \"format\": \"" + JsonEscape(w.format_arg) + "\",\n";
            if (!w.combo_items.empty()) o += "        \"items\": \"" + JsonEscape(w.combo_items) + "\",\n";
            o += "        \"handler\": { \"start\": " + std::to_string(w.handler_start) +
                 ", \"end\": " + std::to_string(w.handler_end) + ", \"disasm\": [";
            for (size_t k = 0; k < w.handler_disasm.size(); k++)
                o += "\"" + JsonEscape(w.handler_disasm[k]) + "\"" + (k + 1 < w.handler_disasm.size() ? "," : "");
            o += "] },\n";
            if (!w.live_validation.empty()) o += "        \"live_validation\": \"" + JsonEscape(w.live_validation) + "\",\n";
            o += "        \"_e\": 0\n      }" + std::string(wi + 1 < f.widgets.size() ? "," : "");
        }
        o += "\n    ],\n    \"_e\": 0\n  }" + std::string(fi + 1 < rep.forms.size() ? "," : "");
    }
    o += "\n  ],\n";
    o += "  \"log\": [";
    for (size_t i = 0; i < rep.log.size(); i++)
        o += "\"" + JsonEscape(rep.log[i]) + "\"" + (i + 1 < rep.log.size() ? "," : "");
    o += "]\n}\n";
    return o;
}

std::vector<std::string> WriteAsmDumps(const AnalysisReport& rep, const std::string& out_dir) {
    std::vector<std::string> files;
#ifdef _WIN32
    _mkdir(out_dir.c_str());
#else
    mkdir(out_dir.c_str(), 0755);
#endif
    int n = 0;
    for (auto& f : rep.forms) {
        for (auto& w : f.widgets) {
            if (!w.handler_start || w.handler_disasm.empty()) continue;
            char name[256];
            snprintf(name, sizeof(name), "%s/form%d_%03d_%s_%s.asm", out_dir.c_str(),
                     (int)(&f - &rep.forms[0]) + 1, ++n,
                     w.type.c_str(), std::to_string(w.call_site & 0xFFFF).c_str());
            FILE* fp = fopen(name, "wb");
            if (!fp) continue;
            fprintf(fp, "; UMVP handler dump\n; form   : %s\n; widget : %s \"%s\"\n; call   : 0x%llX\n; range  : 0x%llX..0x%llX\n",
                    f.name.c_str(), w.type.c_str(), w.label.c_str(),
                    (unsigned long long)w.call_site, (unsigned long long)w.handler_start,
                    (unsigned long long)w.handler_end);
            for (auto& l : w.handler_disasm) fprintf(fp, "%s\n", l.c_str());
            fclose(fp);
            files.push_back(name);
        }
    }
    return files;
}

} // namespace umvp
