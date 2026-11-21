#!/usr/bin/env python3
"""
make_db.py — ImGui SDK Derin Tarama Aracı (2/2)
Tüm versiyon layout JSON'larını + imgui.h flag enum'larını
tek bir üretilmiş C++ başlığına çevirir: src/imgui_layout_db.h
"""
import json, re, sys, os, glob

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "src", "imgui_layout_db.h")

def cxx_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

def parse_flags(imgui_h_path):
    """ImGuiWindowFlags_ / ImGuiChildFlags_ / ImGuiCond_ enum'larını (isim=değer) çıkarır."""
    text = open(imgui_h_path, encoding="utf-8", errors="replace").read()
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    out = {}
    for ename in ("ImGuiWindowFlags_", "ImGuiChildFlags_", "ImGuiCond_"):
        m = re.search(r"enum\s+(?:int\s+)?" + ename + r"\s*\{(.*?)\}", text, flags=re.S)
        if not m:
            continue
        body = m.group(1)
        vals, known = {}, {}
        for line in body.splitlines():
            line = line.strip()
            mm = re.match(r"([A-Za-z_]\w*)\s*=\s*([0-9xXa-fA-F<|()+ ~ ]+?)\s*,?\s*(?://.*)?$", line)
            if not mm:
                continue
            name, expr = mm.group(1), mm.group(2).strip()
            try:
                v = eval(expr, {"__builtins__": {}}, dict(known, **{"1": 1}))
                # sadece tek-bit flag'ler (None=0 hariç) ve makul aralık
                if v != 0 and 0 < v < (1 << 31):
                    known[name] = v
                    vals[name] = v
            except Exception:
                pass
        out[ename] = vals
    return out

def flatten(d, prefix=""):
    out = {}
    if isinstance(d, dict):
        for k, v in d.items():
            if isinstance(v, dict):
                out.update(flatten(v, prefix))
            elif isinstance(v, int) and not k.startswith("_"):
                out[k] = v
    return out

def main():
    layouts = []
    for f in sorted(glob.glob(os.path.join(HERE, "out", "layout_*.json"))):
        d = json.load(open(f))
        ver = d.get("version", "?")
        vnum = d.get("version_num", 0)
        flat = flatten(d)
        # imgui.h flag'leri
        imgui_h = os.path.join(HERE, "out", "imgui_h", os.path.basename(f).replace("layout_", "").replace(".json", ""))
        flags = {}
        m = re.search(r"layout_(.+)\.json", os.path.basename(f))
        verdir = os.path.join(os.path.expanduser("~/deps"), "imgui-" + m.group(1))
        ih = os.path.join(verdir, "imgui.h")
        if os.path.exists(ih):
            flags = parse_flags(ih)
        layouts.append((ver, vnum, flat, flags))

    buf = []
    buf.append("// ============================================================================")
    buf.append("// imgui_layout_db.h — OTOMATİK ÜRETİLDİ (tools/gen_layout). ELLE DEĞİŞTİRME!")
    buf.append("// Kaynak: Dear ImGui SDK çoklu versiyon derin taraması (native offsetof çıktıları)")
    buf.append("// ============================================================================")
    buf.append("#pragma once")
    buf.append("#include <cstddef>")
    buf.append("namespace umvp { namespace imguidb {")
    buf.append("")
    buf.append("struct VersionLayout {")
    buf.append("    const char* version;")
    buf.append("    int version_num;")
    buf.append("    const char* offsets;   // 'Struct.field=value;' biçiminde paketli")
    buf.append("    const char* winflags;  // 'Name=value;' paketli")
    buf.append("    const char* childflags;")
    buf.append("    const char* condflags;")
    buf.append("};")
    buf.append("")
    buf.append("// Not: flag tabloları tek-bit değerleridir; composite'ler (ör. FlagsFromINI)")
    buf.append("// decode edilirken birden fazla ada bölünebilir.")
    buf.append("static const VersionLayout kVersions[] = {")
    for ver, vnum, flat, flags in layouts:
        offs = ";".join(f"{k}={v}" for k, v in sorted(flat.items()) if not k.startswith("_"))
        wf = ";".join(f"{k}={v}" for k, v in sorted(flags.get("ImGuiWindowFlags_", {}).items()))
        cf = ";".join(f"{k}={v}" for k, v in sorted(flags.get("ImGuiChildFlags_", {}).items()))
        cd = ";".join(f"{k}={v}" for k, v in sorted(flags.get("ImGuiCond_", {}).items()))
        buf.append(f"    {{ {cxx_str(ver)}, {vnum},")
        buf.append(f"      {cxx_str(offs + ';') if offs else cxx_str('')},")
        buf.append(f"      {cxx_str(wf + ';') if wf else cxx_str('')},")
        buf.append(f"      {cxx_str(cf + ';') if cf else cxx_str('')},")
        buf.append(f"      {cxx_str(cd + ';') if cd else cxx_str('')} }},")
    buf.append("};")
    buf.append("static const int kVersionCount = sizeof(kVersions) / sizeof(kVersions[0]);")
    buf.append("")
    buf.append("}} // namespace")
    open(OUT, "w").write("\n".join(buf) + "\n")
    print(f"OK {OUT}: {len(layouts)} versiyon")
    for ver, vnum, flat, flags in layouts:
        n_fl = len([1 for k in flat if not k.startswith('_')])
        print(f"  {ver:<10} vnum={vnum:<6} {n_fl} offset, {len(flags.get('ImGuiWindowFlags_', {}))} winflag")

if __name__ == "__main__":
    main()
