#!/usr/bin/env python3
"""
gen_extract.py — ImGui SDK Derin Tarama Aracı (1/2)
Bir imgui ağacının imgui.h + imgui_internal.h yapılarını parse eder,
yalnızca O VERSİYONDA VAR OLAN alanlar için offsetof yazan bir C++
extractor üretir, native derler, çalıştırır ve JSON döker.

Kullanım:
    python3 gen_extract.py <imgui-tree> <out.json>
"""
import re, subprocess, sys, os, tempfile, shutil

TARGET_STRUCTS = [
    "ImGuiWindow", "ImGuiWindowTempData", "ImGuiDrawContext", "ImGuiContext",
    "ImGuiIO", "ImGuiStyle", "ImDrawList", "ImDrawCmd", "ImDrawVert",
    "ImFontAtlas", "ImFont", "ImFontGlyph", "ImRect", "ImGuiLastItemData",
    "ImGuiNextItemData", "ImGuiNextWindowData",
]

def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text

def parse_structs(path):
    """{ 'StructName': ['field1','field2',...] } — basit süreç: struct blokları + üye satırları."""
    if not os.path.exists(path):
        return {}
    text = strip_comments(open(path, encoding="utf-8", errors="replace").read())
    structs = {}
    # struct [IMGUI_API] Name ... { ... };  (template'ler ve forward decl'ler hariç)
    for m in re.finditer(r"struct\s+(?:IMGUI_API\s+)?(\w+)\s*(?::[^{;]+)?\{", text):
        name = m.group(1)
        start = m.end() - 1  # '{' konumu
        depth, i = 1, start + 1
        while i < len(text) and depth > 0:
            c = text[i]
            if c == "{": depth += 1
            elif c == "}": depth -= 1
            i += 1
        if depth != 0:
            continue
        body = text[start + 1:i - 1]
        # satır satır üye çıkarımı — #if bloklarındaki üyeler ATILIR (varsayılan build'de olmayabilir)
        fields = []
        cond_depth = 0
        cont = ""  # '\' ile devam eden satırlar birleştirilir
        for raw in body.splitlines():
            line = (cont + " " + raw.strip()).strip() if cont else raw.strip()
            cont = raw.rstrip()[-1:] == "\\" if raw.rstrip() else False
            if cont:
                cont = raw.rstrip()[:-1]
                continue
            if line.startswith("#"):
                d = line.split()[0]
                if d in ("#if", "#ifdef", "#ifndef"):
                    cond_depth += 1
                elif d == "#endif":
                    cond_depth = max(0, cond_depth - 1)
                continue
            if cond_depth > 0:
                continue
            if not line or line.endswith((")", ",", "{")):
                continue
            if "(" in line or "=" in line or line.startswith(("typedef", "using", "struct", "enum", "static", "friend", "operator", "template")):
                continue
            if not line.endswith(";"):
                continue
            decl = line[:-1].strip()
            if ":" in decl.replace("::", ""):
                continue
            mm = re.search(r"(\w+)\s*(?:\[[^\]]*\])?\s*$", decl)
            if mm:
                fn = mm.group(1)
                if fn in ("const", "mutable", "sizeof", "if", "while"):
                    continue
                fields.append(fn)
        if fields:
            structs.setdefault(name, []).extend(fields)
    return structs

def gen_extractor(structs, tree):
    lines = []
    lines.append('#define IMGUI_DEFINE_MATH_OPERATORS')
    lines.append('#include "imgui.h"')
    lines.append('#include "imgui_internal.h"')
    lines.append('#include <cstdio>')
    lines.append('#include <cstddef>')
    lines.append('#define O(T, f) printf("  \\"%s.%s\\": %zu,\\n", #T, #f, offsetof(T, f))')
    lines.append('#define S(T)    printf("  \\"%s.sizeof\\": %zu,\\n", #T, sizeof(T))')
    lines.append('int main(){')
    lines.append('  printf("{\\n");')
    lines.append('  printf("  \\"version\\": \\"%s\\",\\n", IMGUI_VERSION);')
    lines.append('  printf("  \\"version_num\\": %d,\\n", IMGUI_VERSION_NUM);')
    # ImGuiDrawContext/ImGuiWindowTempData eş isim
    td = "ImGuiWindowTempData" if "ImGuiWindowTempData" in structs else "ImGuiDrawContext"
    for sname in TARGET_STRUCTS:
        if sname == "ImGuiDrawContext":
            continue
        if sname == "ImGuiWindowTempData":
            sname_use = td
        else:
            sname_use = sname
        if sname_use not in structs:
            continue
        lines.append(f'  printf("  \\"{sname}\\": {{\\n");')
        lines.append(f'  S({sname_use});')
        for f in structs[sname_use]:
            lines.append(f'  O({sname_use}, {f});')
        lines.append('  printf("  \\"_end\\": 0\\n}\\n");' if False else '  printf("  \\"_e\\": 0\\n},\\n");')
    lines.append('  printf("  \\"_e\\": 0\\n}\\n");')
    lines.append('  return 0; }')
    return "\n".join(lines)

def main():
    tree, outjson = sys.argv[1], sys.argv[2]
    structs = {}
    for h in ("imgui.h", "imgui_internal.h"):
        structs.update(parse_structs(os.path.join(tree, h)))
    src = gen_extractor(structs, tree)
    work = tempfile.mkdtemp(prefix="imgui_layout_")
    try:
        cpp = os.path.join(work, "x.cpp")
        open(cpp, "w").write(src)
        exe = os.path.join(work, "x")
        subprocess.run(["g++", "-std=c++17", "-O0", "-w", "-I", tree, cpp, "-o", exe], check=True)
        res = subprocess.run([exe], capture_output=True, text=True, check=True)
        open(outjson, "w").write(res.stdout)
        print(f"OK {outjson}: {res.stdout.count(chr(10))} satir")
    finally:
        shutil.rmtree(work, ignore_errors=True)

if __name__ == "__main__":
    main()
