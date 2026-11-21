// ============================================================================
// src/layout.cpp — Layout DB çalışma zamanı
// ============================================================================
#include "umvp/layout.h"
#include "imgui_layout_db.h"
#include <cstring>
#include <algorithm>
#include <cstdlib>

namespace umvp {

static void ParsePacked(const char* s, std::unordered_map<std::string, int>& out) {
    if (!s) return;
    const char* p = s;
    while (*p) {
        const char* eq = strchr(p, '=');
        const char* semi = strchr(p, ';');
        if (!eq) break;
        size_t klen = (size_t)(eq - p);
        size_t vlen = semi ? (size_t)(semi - eq - 1) : strlen(eq + 1);
        if (klen && vlen) {
            std::string key(p, klen);
            std::string val(eq + 1, vlen);
            out[key] = atoi(val.c_str());
        }
        if (!semi) break;
        p = semi + 1;
    }
}

static void ParseFlagNames(const char* s, std::unordered_map<u32, std::string>& out) {
    if (!s) return;
    const char* p = s;
    while (*p) {
        const char* eq = strchr(p, '=');
        const char* semi = strchr(p, ';');
        if (!eq) break;
        size_t klen = (size_t)(eq - p);
        size_t vlen = semi ? (size_t)(semi - eq - 1) : strlen(eq + 1);
        if (klen && vlen) {
            std::string key(p, klen);
            std::string val(eq + 1, vlen);
            u32 v = (u32)strtoul(val.c_str(), nullptr, 0);
            // önde gelen "ImGuiWindowFlags_" önekini kısalt
            const char* prefixes[] = { "ImGuiWindowFlags_", "ImGuiChildFlags_", "ImGuiCond_" };
            for (auto* pre : prefixes) {
                size_t pl = strlen(pre);
                if (key.size() > pl && key.compare(0, pl, pre) == 0) { key = key.substr(pl); break; }
            }
            out[v] = key;
        }
        if (!semi) break;
        p = semi + 1;
    }
}

std::string LayoutSet::DecodeWinFlags(u32 flags) const {
    std::string out;
    u32 rest = flags;
    // önce büyük (composite) bitleri dene, sonra tek bitler
    std::vector<std::pair<u32, std::string>> items(winflags.begin(), winflags.end());
    std::sort(items.begin(), items.end(),
              [](auto& a, auto& b) { return a.first > b.first; });
    for (auto& [bit, name] : items) {
        if ((flags & bit) == bit && bit != 0) {
            if (!out.empty()) out += "|";
            out += name;
            rest &= ~bit;
        }
    }
    if (rest) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%08X", rest);
        if (!out.empty()) out += "|";
        out += buf;
    }
    return out;
}

static std::vector<LayoutSet> g_db;
static bool g_init = false;

static void InitDB() {
    if (g_init) return;
    g_init = true;
    for (int i = 0; i < imguidb::kVersionCount; i++) {
        const auto& v = imguidb::kVersions[i];
        LayoutSet ls;
        ls.version = v.version;
        ls.version_num = v.version_num;
        ParsePacked(v.offsets, ls.off);
        ParseFlagNames(v.winflags, ls.winflags);
        ParseFlagNames(v.childflags, ls.childflags);
        ParseFlagNames(v.condflags, ls.condflags);
        g_db.push_back(std::move(ls));
    }
}

const std::vector<LayoutSet>& LayoutDB::All() {
    InitDB();
    return g_db;
}

const LayoutSet* LayoutDB::ByVersionNum(int num) {
    InitDB();
    const LayoutSet* best = nullptr;
    for (auto& ls : g_db) {
        if (ls.version_num <= num) best = &ls;   // en yakın alt sürüm
    }
    if (!best && !g_db.empty()) best = &g_db.front();
    return best;
}

const LayoutSet* LayoutDB::Best() {
    InitDB();
    return &g_db.back();
}

std::vector<WindowLayoutGuess> WindowLayoutGuesses(bool x64) {
    std::vector<WindowLayoutGuess> g;
    if (x64) {
        g.push_back({ -1,  0,  8, 12, -1, 16, 24, 32, "legacy <=1.88 (Name@0)" });
        g.push_back({  0,  8, 16, 20, -1, 32, 40, 48, "1.89.x (Ctx eklendi)" });
        g.push_back({  0,  8, 16, 20, 24, 40, 48, 56, "1.90.1+ (ChildFlags eklendi)" });
    } else {
        g.push_back({ -1,  0,  4,  8, -1, 12, 20, 28, "legacy <=1.88 x86" });
        g.push_back({  0,  4,  8, 12, -1, 16, 24, 32, "1.89.x x86" });
        g.push_back({  0,  4,  8, 12, 16, 20, 28, 36, "1.90.1+ x86 (ChildFlags)" });
    }
    return g;
}

} // namespace umvp
