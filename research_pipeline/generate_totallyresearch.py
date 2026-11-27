#!/usr/bin/env python3
"""TotallyResearch.md v3 uretici — cheat-sahasi-oncelikli kurallar.

- Her satir = 1 proje (link + aciklama + dil + yildiz).
- CHEAT sahasi oyun oyun alt basliklara bolunur (rekbet haritasi).
- "cheat sheet" yanlis pozitifleri cheat kategorisine GIREMEZ.
- Genis evren (tag g) repolari ayri ek kategoride toplanir.
- 100K satir uzerinde dosya ciltlere bolunur (Vol01, Vol02, ...).
"""
import json
import os
import re
from collections import Counter

STATE = "/home/z/my-project/scripts/harvest_state.json"
OUTDIR = "/home/z/my-project/download"

st = json.load(open(STATE, encoding="utf-8"))
repos = dict(st["repos"])
queries = st.get("queries", {})
totals = st.get("totals", {})

# ---------------- bayrak projeler ---------------- #
FLAG = {
    "0xByteDeveloper/MVP_GOD_SSS_TIER_IMGUI_UI_EDITOR": {
        "full_name": "0xByteDeveloper/MVP_GOD_SSS_TIER_IMGUI_UI_EDITOR",
        "url": "https://github.com/0xByteDeveloper/MVP_GOD_SSS_TIER_IMGUI_UI_EDITOR",
        "desc": "VS2019 C++'a bağlanan ImGui menü YAPICI + kod DERLEYİCİ. Bu araştırmanın BESLENECEĞİ editör: GlobalInfos dosyasını okuyup hedef ImGui menüyü birebir yeniden inşa edecek GOD TIER araç.",
        "lang": "C", "stars": 0, "topics": ["imgui", "ui-editor", "codegen", "visual-studio-2019"],
        "archived": False, "via": "flag", "fork": False,
    },
    "0xByteDeveloper/UniversalMVPUIAlgorithm": {
        "full_name": "0xByteDeveloper/UniversalMVPUIAlgorithm",
        "url": "https://github.com/0xByteDeveloper/UniversalMVPUIAlgorithm",
        "desc": "ImGui EXE/DLL yapı-çözücü: formlar, widget listesi + bilgileri, handler kod blokları (event), sürüm doğrulama (canlı bellek + layout DB + hash dönemi), draw-list metin çözümü — PDB'siz derin analiz. GlobalInfos üretecek taraf.",
        "lang": "C++", "stars": 0, "topics": ["imgui", "reverse-engineering", "disassembly"],
        "archived": False, "via": "flag", "fork": False,
    },
}
for fn in FLAG:
    repos.pop(fn, None)

CORE_EXACT = {"ocornut/imgui", "cimgui/cimgui", "cimgui/cimplot", "cimgui/cimgui_backends",
              "ocornut/imgui_test_engine", "ocornut/imgui_club", "ocornut/imgui_docking"}
CORE_NAMES = {"imgui", "cimgui", "cimplot"}

R = lambda p: re.compile(p, re.I)

# YANLIS POZITIF KALKANI — cheat sahasi degil!
CHEATSHEET = R(r"cheat[ -]?sheet|cheatsheet")
EXAM = R(r"\b(exam|quiz|midterm|final ?(exam)?|sinav|sınav|vize|final)\b.*(cheat)|cheat.*(exam|quiz|sinav)")

CHEAT_W = R(r"\b(cheats?|cheating|hacks?|hacking|aimbot|wallhack|esp|bypass(es)?|pasted|paste|leak(ed|s)?|crack(ed)?|trainer|mod.?menu|hvh|external|internal|multihack|farm.?bots?|anti.?cheat|clientless|hile(si|leri)?|script.?execut(or|ors))\b")
GAMES_W = R(r"\b(csgo|cs2|cs:go|counter.?strike|valorant|fortnite|apex|pubg|gta|fivem|tarkov|eft|dayz|roblox|minecraft|warzone|call.?of.?duty|battlefield|halo|gmod|dota|deadlock|deadside|rust|escape from|r6|rainbow|dead by daylight|xdefiant|the finals|marvel rivals|delta force|arena breakout|hunt showdown|dark and darker|crossfire|warface|point blank|combat arms|critical ops|standoff 2|free fire|mobile legends|zula|wolfteam|metin2|knight online|silkroad|sudden attack|arma|overwatch|unturned|terraria|left 4 dead|team fortress|tf2|quake|insurgency|squad|planetside|apb|fall guys|among us)\b")
MENULIKE = R(r"\b(menu|menus|cheat|hack|esp|loader|internal|external|base|mod.?menu|offsets?|injector|spoofer|bypass|radar|dma|executor)\b")
LOADER_W = R(r"\b(loader|loaders|keyauth|key.?auth|license|licence|licensing|lisans|hwid|injector|inject(ing|ion)?|subscription|key.?system|keysystem|auth|login|account|seller|sellix|shoppy|dashact|billing|stripe|panel)\b")
THEME_W = R(r"\b(themes?|styles?|skins?|palettes?|colour|color(s|ing)?|theme)\b")
ANIM_W = R(r"\b(animations?|animated|anim|easing|ease|lerp|transition(s)?|tween|motion)\b")
VISUAL_W = R(r"\b(node.?edit(or|ors)?|node.?graph(s)?|graph(s)?|plots?|charts?|canvas(es)?|blueprints?|diagrams?|timelines?|visuali[sz]ation|visuali[sz]ing)\b")
EDITOR_W = R(r"\b(editors?|designer(s)?|builder(s)?|ui.?maker|form.?designer|scene.?editor|code.?gen|code generator)\b")
BINDING_W = R(r"\b(bindings?|wrappers?|lua|python|rust|csharp|c#|\.net|javascript|typescript|golang|\bgo\b|zig|perl|ruby|php|haskell|nim|dlang|crystal|elixir|swift|kotlin|jai|odin|fsharp|vb\.net)\b")
OVERLAY_W = R(r"\b(d3d\d*|dx9|dx10|dx11|dx12|directx|direct3d|opengl|vulkan|metal|hooks?|hooking|overlay(s)?|swapchain|swap.?chain|present|wndproc|win32|kiero|dwm|detour(s)?|trampoline|backend(s)?|imgui_impl)\b")
ENGINE_W = R(r"\b(unity|unreal|godot|sdl\d?|glfw|sfml|bgfx|engine(s)?|wasm|emscripten|webgl|android|ios|electron|qt|wx|hazel|monogame|libgdx)\b")
TOOL_W = R(r"\b(toolkits?|widgets?|utils?|utility|utilities|helpers?|extensions?|extras?|components?|addons?|plugins?|library|libraries|lib|framework)\b")
EXAMPLE_W = R(r"\b(examples?|samples?|demos?|templates?|starters?|boilerplate|tutorials?|learn(ing)?|sandbox|playground|tests?|testing|hello.?world|getting started)\b")
IMGUI_W = R(r"imgui")

# oyun grubu haritasi (cheat alt basliklari)
GAME_GROUPS = [
    ("CS / CS2 / CSGO", r"\b(csgo|cs2|cs:go|cs go|counter.?strike|csgo2|cs source|cs 1\.6|cs2d|condition zero)\b"),
    ("FiveM / GTA / RDR", r"\b(fivem|five-?m|gta|grand theft|gtav|gta5|rdr|red dead|2take1|kiddion|mta|samp|sa-mp|multi theft)\b"),
    ("Fortnite", r"\bfortnite\b|\bfwhook\b|\bwhack\b"),
    ("Valorant", r"\bvalorant\b"),
    ("Apex Legends", r"\bapex ?legends?\b"),
    ("Tarkov / EFT", r"\b(tarkov|eft|escape from tarkov|arena breakout)\b"),
    ("Rust (oyun)", r"\brust\b.*(cheat|hack|menu|esp|aimbot|external|internal|offsets|base|raid|pvp)"),
    ("PUBG", r"\bpubg\b"),
    ("DayZ / Arma", r"\b(dayz|arma)\b"),
    ("R6 / Siege / DbD", r"\b(r6|rainbow six|siege|dead by daylight|dbd)\b"),
    ("CoD / Warzone / BF / Overwatch", r"\b(call of duty|callofduty|cod|warzone|modern warfare|black ops|mw2|mw3|bo3|vanguard|battlefield|overwatch)\b"),
    ("Roblox / Minecraft / Terraria", r"\b(roblox|minecraft|terraria)\b"),
    ("Türk sahnesi (Zula / Wolfteam / Metin2 / KO / Silkroad)", r"\b(zula|wolf ?team|metin2|knight online|silkroad|sudden attack|mt2)\b"),
    ("DMA / Donanım sahnesi", r"\b(dma|pcileech|fpga|radar|leechcore|squirrel)\b"),
    ("HvH / paste sahnesi (neverlose / onetap / fatality / skeet / gamesense)", r"\b(hvh|neverlose|onetap|fatality|gamesense|skeet|ev0|primordial|awareness|weave|fitties)\b"),
]
GG = [(name, re.compile(p, re.I)) for name, p in GAME_GROUPS]

# tablo + bolum siralamasi: gercek gruplar + iki fallback grubu
ALL_GROUPS = GG + [
    ("Diğer oyunlar (Hunt / TF2 / L4D / Quake / Halo / mobil sahneler)", None),
    ("Evrensel cheat menü / base / paste (oyun belirtmeyen)", None),
]


def text_of(r):
    return " ".join([r["full_name"], r.get("desc") or "", " ".join(r.get("topics") or [])])


def imgui_word(r):
    return bool(IMGUI_W.search(text_of(r)))


def game_group(r):
    s = text_of(r)
    for name, rx in GG:
        if name.startswith("Rust (oyun)"):
            continue
        if rx.search(s):
            return name
    # rust ozel: kelime var VE cheat kelimesi (rust-lang projeleriyle karismasin)
    if re.search(r"\brust\b", s, re.I) and CHEAT_W.search(s):
        return "Rust (oyun)"
    # liste disi oyunlar: bilinmeyen oyun adı kelimesi varsa ayir (gun listemizde olmayanlar)
    if GAMES_W.search(s):
        return "Diğer oyunlar (Hunt / TF2 / L4D / Quake / Halo / mobil sahneler)"
    return "Evrensel cheat menü / base / paste (oyun belirtmeyen)"


def categorize(r):
    fn = r["full_name"].lower()
    s = text_of(r)
    if r.get("via") == "flag":
        return "flag"
    if fn in CORE_EXACT or fn.split("/")[1] in CORE_NAMES:
        return "core"
    has_cheat_word = bool(CHEAT_W.search(s))
    has_game = bool(GAMES_W.search(s))
    if CHEATSHEET.search(s):
        return None  # cheat sheet = YANLIS POZITIF, korpus disina atiliyor mu? hayir -> diger kategorilere
    if (has_cheat_word or (has_game and MENULIKE.search(s))) and not CHEATSHEET.search(s):
        return "cheat"
    if LOADER_W.search(s):
        return "loader"
    if THEME_W.search(s):
        return "theme"
    if ANIM_W.search(s):
        return "anim"
    if VISUAL_W.search(s):
        return "visual"
    if EDITOR_W.search(s):
        return "editor"
    if BINDING_W.search(s):
        return "binding"
    if OVERLAY_W.search(s):
        return "overlay"
    if ENGINE_W.search(s):
        return "engine"
    if TOOL_W.search(s):
        return "tool"
    if EXAMPLE_W.search(s):
        return "example"
    if r.get("via") in ("p5", "c1", "c2", "c3", "c4", "c5", "c6", "b"):
        return "other"
    return "genis"  # genis evren (g/u) — ilgililik sinamasi bekliyor


CHEAT_VIA = ("p5", "c1", "c2", "c3", "c4", "c5", "c6", "b", "g", "u")

# ---------------- P5 v1 filtresi ---------------- #
kept, dropped_p5 = {}, 0
for fn, r in repos.items():
    if r.get("via") == "p5":
        s = text_of(r)
        mentions = "imgui" in s
        cfamily = r.get("lang") in ("C", "C++", "C#")
        worth = bool(r.get("desc")) or (r.get("stars") or 0) >= 1
        if not (mentions or (cfamily and worth)):
            dropped_p5 += 1
            continue
    kept[fn] = r
kept.update(FLAG)

cat_map = {}
for fn, r in kept.items():
    c = categorize(r)
    if c is None:
        c = "other"
    cat_map.setdefault(c, []).append(r)

for k in cat_map:
    cat_map[k].sort(key=lambda r: (-r.get("stars", 0), r["full_name"].lower()))

cheat_groups = {}
for r in cat_map.get("cheat", []):
    cheat_groups.setdefault(game_group(r), []).append(r)
for k in cheat_groups:
    cheat_groups[k].sort(key=lambda r: (-r.get("stars", 0), r["full_name"].lower()))


def esc(s):
    s = re.sub(r"[\r\n\t|]+", " ", s or "")
    s = re.sub(r"\s+", " ", s).strip()
    return s


def trunc(s, n=130):
    if len(s) <= n:
        return s
    cut = s[:n].rsplit(" ", 1)[0]
    return cut + "…"


def desc_of(r):
    d = esc(r.get("desc"))
    if d:
        return trunc(d)
    topics = [esc(t) for t in (r.get("topics") or []) if esc(t)]
    if topics:
        return "konular: " + ", ".join(topics[:6])
    return "(açıklama yok)"


total = sum(len(v) for v in cat_map.values())
langs = Counter((r.get("lang") or "—") for r in kept.values())

CATS = [
    ("flag", "0. Bayrak Projelerimiz — Bu Hattın Kalbi"),
    ("core", "1. ImGui Çekirdeği & Resmi Ekosistem"),
    ("cheat", "2. Oyun Cheat / Hile Menüleri — REKABET SAHASI (oyun oyun döküldü)"),
    ("loader", "3. Loader, KeyAuth, HWID & Lisans Sistemleri"),
    ("editor", "4. UI Editörleri, Tasarımcılar & Builder'lar"),
    ("theme", "5. Temalar, Stiller & Görsel Paketler"),
    ("anim", "6. Animasyon & Efekt Sistemleri"),
    ("visual", "7. Node Editörler, Plotlar & Görselleştirme"),
    ("overlay", "8. Overlay, Hook & Render Backend Altyapıları (D3D / OpenGL / Vulkan)"),
    ("binding", "9. Dil Bağlayıcıları (Bindings & Wrappers)"),
    ("engine", "10. Motor & Platform Entegrasyonları (SDL / GLFW / Unity / Unreal / Web)"),
    ("tool", "11. Widget, Araç & Yardımcı Kütüphaneler"),
    ("example", "12. Örnek, Şablon & Öğrenme Projeleri"),
    ("genis", "13. Geniş Evren (hack/cheat evreni — derin ilgililik taraması bekliyor)"),
    ("other", "14. Diğer ImGui / Cheat-Saha Projeleri"),
]

# ---------------- evren buyuklukleri ---------------- #
def tot(key):
    v = totals.get(key)
    return f"{v:,}".replace(",", ".") if v is not None else "—"

head = []
A = head.append
A("# TotallyResearch — Global ImGui & Cheat-UI Repository Research Corpus")
A("")
A("> **Bu liste ne için var?** `UniversalMVPUIAlgorithm` (ImGui yapı-çözücü) ve")
A("> `MVP_GOD_SSS_TIER_IMGUI_UI_EDITOR` (ImGui menü yapıcı + kod derleyici) adlı iki GOD-TIER")
A("> aracı **durdurulamaz** yapmak için: on binlerce ImGui / cheat-UI projesini tek tek analiz edip")
A("> `GlobalInfos` dosyalarına dönüştüreceğiz; böylece obje (button, checkbox, slider...),")
A("> event/handler, tema, animasyon ve yerleşim **olasılık evrenini** öğreneceğiz.")
A("> Editör başka bir ImGui menüsünün kopyasını GlobalInfos ile 1:1 inşa edecek,")
A("> çözücü ise hiçbir objeyi `unknown_objectxN` olarak bırakmayacak kadar derin bilecek.")
A("")
A("> **REKABET GERÇEĞİ:** asıl savaş cheat UI'ları ve cheat loader/lisans ekosisteminde.")
A("> Geliştiriciler menülerini satılabilir yapmak için animasyonları ve objeleri")
A("> sürekli güzelleştiriyor — bu korpus o sahneyi oyun oyun dökerek tarar.")
A("")
A("---")
A("")
A(f"## Sayaçlar (v3.0 — 2026-11-27)")
A("")
A(f"- **Toplam benzersiz proje: {total:,}** (fork'lar hariç; v1.0 = 9.515 listelenmiş / 13.006 hasat)")
A(f"- Atılan düşük olasılıklı P5 kaydı (v1 kuralı): {dropped_p5}")
A(f"- Kullanılan sorgu varyasyonu (banka + dinamik yıl/çeyrek/ay dilimleri): {len(queries):,}")
A("- Kural: **her satır = 1 proje** (link + açıklama + dil + yıldız)")
A(f"- Dil dağılımı (ilk 12): " + ", ".join(f"{l} {c}" for l, c in langs.most_common(12)))
A("- Cheat-saha dökümü: aşağıdaki rekabet haritasına bakınız.")
A("")
A("### GitHub evreninin gerçek büyüklüğü (ölçülmüş `total_count`)")
A("")
A("| Anahtar | Evren (fork hariç) | Fork evreni dahil |")
A("|---|---:|---:|")
A(f"| `imgui` | {tot('repo:imgui')} | {tot('repo:imgui fork:true')} |")
A(f"| `cheat` | {tot('repo:cheat')} | {tot('repo:cheat fork:true')} |")
A(f"| `hack` | {tot('repo:hack')} | {tot('repo:hack fork:true')} |")
A(f"| `loader` | {tot('repo:loader')} | — |")
A(f"| `menu` | {tot('repo:menu')} | — |")
A(f"| `mod menu` | {tot('repo:mod menu')} | {tot('repo:mod menu fork:true')} |")
A(f"| `aimbot` | {tot('repo:aimbot')} | {tot('repo:aimbot fork:true')} |")
A(f"| `keyauth` | {tot('repo:keyauth')} | {tot('repo:keyauth fork:true')} |")
A(f"| `game hack` | {tot('repo:game hack')} | — |")
A(f"| `hack tool` | {tot('repo:hack tool')} | — |")
A("")
A("> **1M+ yolu matematiksel olarak açık:** `hack` evreni tek başına 1.000.000+ repo;")
A("> `cheat`+`hack`+`mod menu`+`aimbot`+`keyauth`+`imgui fork:true` evrenleri birleşince")
A("> fork hariç ~1,3M ve fork dahil ~2,5M+ repo. Otonom hasat hattı (cron) bu evreni")
A("> dilimleyerek (yıl → çeyrek → ay → gün) 1.000-sonuç sınırını aşıp tarar.")
A("")
A("## İçindekiler")
A("")
for key, title in CATS:
    n = len(cat_map.get(key, []))
    A(f"- {title} — **{n} repo**")
A("")
A("---")
A("")

# ---------------- cheat rekabet haritasi ---------------- #
cheat_all = cat_map.get("cheat", [])
A("## 2-A. Cheat-Sahası Rekabet Haritası (oyun × repo sayısı)")
A("")
A("| Saha | Repo sayısı |")
A("|---|---:|")
for name, _ in ALL_GROUPS:
    n = len(cheat_groups.get(name, []))
    A(f"| {name} | {n} |")
A(f"| **TOPLAM CHEAT SAHASI** | **{len(cheat_all)}** |")
A("")
A("---")
A("")

body_chunks = []  # (baslik_ici_mi, satirlar)


def emit_category(key, title):
    items = cat_map.get(key, [])
    if not items:
        return
    out = [f"## {title}", ""]
    if key == "cheat":
        for gname, _ in ALL_GROUPS:
            gitems = cheat_groups.get(gname, [])
            if not gitems:
                continue
            out.append(f"### {gname} — {len(gitems)} repo")
            out.append("")
            for r in gitems:
                out.append(line_of(r))
            out.append("")
    else:
        for r in items:
            out.append(line_of(r))
        out.append("")
    body_chunks.append((key, out))


def line_of(r):
    lang = r.get("lang") or "—"
    stars = r.get("stars", 0)
    extra = ""
    if r.get("archived"):
        extra = " · arşiv"
    if r.get("via") == "flag":
        extra += " · BAYRAK"
    return f"- [{r['full_name']}]({r['url']}) — {desc_of(r)} — {lang} · {stars} yıldız{extra}"


for key, title in CATS:
    if key == "cheat":
        emit_category(key, title)
        continue
    emit_category(key, title)

# ---------------- birlestir + cilt sistemi ---------------- #
MAX_LINES = 100000
full = head + [ln for _, chunk in body_chunks for ln in chunk]

A2 = full[:]
A2.append("---")
A2.append("")
A2.append("## Bu korpus nasıl 1.000.000+'a büyür (v3.0 yol haritası — AKTİF)")
A2.append("")
A2.append("1. **Otonom hasat hattı (çalışıyor):** `harvest_v2.py` her turda ~4-6K yeni repo ekler;")
A2.append("   cron ile sürekli döngüde. Sorgu bankası: cheat-loader öncelikli ~3.060 statik + sınırsız")
A2.append("   dinamik yıl/çeyrek/ay/gün dilimi (her sorgu 1.000-sonuç sınırını aşamaz ama dilimler aşar).")
A2.append("2. **Kod-uzayı araması:** `\"imgui.h\" aimbot`, `\"ImGui::Begin\" keyauth` gibi imzalar —")
A2.append("   açıklamasında imgui yazmayan menü depolarını yakalar (her imza ~1K repo).")
A2.append("3. **Derin dilimleme:** `hack` (1M+) ve `cheat` (124K) evrenleri yıl→çeyrek→ay→gün")
A2.append("   parçalarına bölünerek tam çekilir; 100K satır aşılınca cilt dosyaları açılır.")
A2.append("4. **Fork grafiği madenciliği:** `imgui fork:true` tek başına 46.087 depo.")
A2.append("5. **Sonraki faz — GlobalInfos:** bu korpus satır satır analiz edilip obje/event/tema/")
A2.append("   animasyon olasılık evrenine dönüşür; editör + çözücü beslenir.")
A2.append("6. Yeni satırlar aynı formatta eklenir: `- [owner/repo](url) — açıklama — dil · yıldız`.")
A2.append("")
A2.append(f"> Üretim: oto-hasat (GitHub Search API + kod araması + rekurif dilimleme) — 2026-11-27, v3.0.")
A2.append("> Analiz çıktıları (GlobalInfos) ayrı dosyalarda birikir; bu liste ham girdidir.")

if len(A2) <= MAX_LINES:
    path = os.path.join(OUTDIR, "TotallyResearch.md")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(A2) + "\n")
    print("YAZILDI:", path, "%.2f MB" % (os.path.getsize(path) / 1048576))
    print("Toplam:", total, "| atilan P5:", dropped_p5)
    for key, title in CATS:
        print(f"  {key:9s} {len(cat_map.get(key, [])):6d}  {title}")
    print("CHEAT SAHASI DÖKÜMÜ:")
    for name, _ in ALL_GROUPS:
        print(f"  {len(cheat_groups.get(name, [])):6d}  {name}")
else:
    # cilt sistemi: ana dosya = basliklar + ilk cilt; sonra Vol02, Vol03...
    i0 = 0
    vol = 1
    written = []
    while i0 < len(A2):
        part = A2[i0:i0 + MAX_LINES]
        name = "TotallyResearch.md" if vol == 1 else f"TotallyResearch_Vol{vol:02d}.md"
        path = os.path.join(OUTDIR, name)
        with open(path, "w", encoding="utf-8") as f:
            f.write("\n".join(part) + "\n")
        written.append(name)
        i0 += MAX_LINES
        vol += 1
    print("CILT YAZILDI:", written)
    print("Toplam:", total, "| satir:", len(A2))
