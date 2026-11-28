#!/usr/bin/env python3
"""TotallyResearch v2 MEGA-HASAT — cheat/loader sahasi ONCE, sonra imgui evreni.

Strateji:
- Ayni state dosyasi (harvest_state.json) -> v1'in 13.006 reposuyla GLOBAL dedup.
- Tier sirasi: evren problari -> cheat dogrudan -> unlu base'ler -> loader/lisans
  -> topic'ler -> KOD ARAMASI -> oyun x terim matrisi -> imgui dilimleri
  -> GENIS EVREN (cheat/hack) derin dilimleme.
- total_count > 950 olan repo sorgulari icin otomatik REKURSIF dilimleme:
  yil -> ceyrek -> ay -> gun parcasi (tag'e gore derinlik siniri).
- Token: scripts/.ghtoken (yoksa GH_TOKEN env).
Kullanim: python3 harvest_v2.py [max_saniye]   (varsayilan 560)
"""
import calendar
import datetime
import json
import os
import re
import sys
import time

import requests

ROOT = "/home/z/my-project/scripts"
STATE = f"{ROOT}/harvest_state.json"


def load_token():
    p = f"{ROOT}/.ghtoken"
    if os.path.exists(p):
        t = open(p).read().strip()
        if t:
            return t
    return os.environ.get("GH_TOKEN")


TOKEN = load_token()
if not TOKEN:
    print("Token yok (.ghtoken / GH_TOKEN)")
    sys.exit(1)

API = "https://api.github.com"
SLEEP = 2.15  # ~28 istek/dk (< 30 siniri)

# ------------------------------------------------------------------ #
#  SORGU BANKASI
# ------------------------------------------------------------------ #
GAMES = [
    "csgo", "cs2", "counter strike", "cs source", "cs 1.6", "cs2d",
    "fivem", "gta 5", "gta", "gta san andreas", "rdr2", "red dead",
    "fortnite", "valorant", "apex legends", "apex", "apexlegends",
    "tarkov", "escape from tarkov", "eft",
    "rust", "pubg", "pubg mobile", "dayz", "arma 3", "arma",
    "roblox", "minecraft", "terraria",
    "r6", "rainbow six", "siege", "dead by daylight",
    "call of duty", "warzone", "modern warfare", "cold war", "black ops",
    "vanguard", "mw2", "mw3", "bo3",
    "battlefield", "battlefield 2042", "battlefield 5", "overwatch",
    "deadside", "the finals", "xdefiant", "marvel rivals", "delta force",
    "arena breakout", "hunt showdown", "dark and darker",
    "sons of the forest", "the forest", "project zomboid", "unturned",
    "gmod", "left 4 dead", "team fortress", "tf2",
    "crossfire", "warface", "point blank", "combat arms",
    "critical ops", "standoff 2", "free fire", "mobile legends", "pubg lite",
    "zula", "wolfteam", "metin2", "knight online", "silkroad", "sudden attack",
    "halo", "halo infinite", "quake", "insurgency", "squad",
    "among us", "fall guys", "apb", "planetside", "unreal tournament",
    "mta", "samp", "multi theft auto", "deadlock", "cs condition zero",
]

CHEAT_TERMS = [
    "cheat", "hack", "aimbot", "esp", "external", "internal", "mod menu",
    "cheat source", "hack source", "menu source", "cheat menu", "hack menu",
    "cheat base", "hack base", "cheat sdk", "hile", "menu", "trainer",
    "spoofer", "bypass", "injector", "loader", "offsets", "sdk",
    "paste", "pasted", "source", "base", "wallhack",
]

A1_DIRECT = [
    "imgui cheat", "imgui hack", "imgui menu", "imgui loader",
    "imgui keyauth", "imgui aimbot", "imgui esp", "imgui hack menu",
    "imgui cheat menu", "imgui paste", "imgui internal", "imgui external",
    "imgui base", "imgui trainer", "imgui spoofer", "imgui injector",
    "imgui bypass", "imgui offsets", "imgui sdk", "imgui mod menu",
    "cheat menu imgui", "hack menu imgui", "mod menu imgui", "menu imgui",
    "cheat menu c++", "hack menu c++", "cheat menu source", "cheat loader",
    "cheat panel", "cheat source", "hack source", "game hack c++",
    "game cheat c++", "pasteware", "pasted menu", "cheat dll", "hack dll",
    "dll injector imgui", "imgui dll", "menu base c++", "cheat ui",
    "cheat gui c++", "hack gui c++", "game overlay menu", "d3d menu c++",
    "dx11 menu", "dx9 menu", "d3d9 menu", "hook menu c++", "overlay cheat",
    "cheat interface", "menu designer cheat",
    "esp cheat", "esp hack", "external cheat", "internal cheat",
    "cheat engine table", "aimbot source", "game menu c++",
    "menu source c++", "cheat tool", "hack tool", "cheat framework",
]

A2_BASES = [
    "csgosimple", "csgo simple", "cs go simple", "osiris csgo", "fwHook",
    "fw hook fortnite", "whack fortnite", "fortnite whack",
    "apex dma", "tarkov dma", "eft dma", "dma cheat", "dma radar",
    "pcileech", "pubg dma", "rust dma", "dayz dma", "hunt dma",
    "apex legends external", "fortnite external", "fortnite internal",
    "valorant external", "valorant internal", "csgo paste", "pasted csgo",
    "hvh", "hvh base", "hvh cheat", "1.5 hvh", "neverlose", "onetap",
    "fatality", "gamesense", "skeet", "ev0 cheat", "primordial",
    "saturn cheat", "awareness cheat", "weave apex", "fitties", "hpp",
    "eulen", "lynx menu", "redengine", "dopamine fivem", "brutan",
    "stand menu", "tierra fivem", "2take1", "kiddion", "impulse gta",
    "north menu", "astral menu", "recovery menu", "subzero menu",
    "unknowncheats", "guided hacking", "cfx lua", "qbcore menu",
    "esx menu", "fivem lua menu", "fivem script menu", "lua imgui gta",
    "samp cheat", "mta cheat", "samp imgui", "mta imgui", "cs16 cheat",
]

A4_LOADER = [
    "keyauth", "keyauth c++", "keyauth loader", "keyauth source",
    "keyauth cpp", "keyauth menu", "key system", "key-system", "keysystem",
    "key system cheat", "hwid lock", "hwid spoofer", "hwid checker",
    "hwid generator", "hwid ban", "cheat loader", "hack loader",
    "loader source c++", "loader c++", "web loader", "loader panel",
    "auth panel", "seller panel", "cheat shop", "license key",
    "license system", "license c++", "lisans", "lisans sistemi",
    "subscription system", "stripe loader", "sellix", "shoppy",
    "dashact", "billing panel", "loader database", "mysql loader",
    "cheat database", "loader api", "discord auth", "hwid system",
    "keygen c++", "license key system", "hwid checker c++",
]

A5_TOPICS = [
    "topic:aimbot", "topic:esp", "topic:wallhack", "topic:cheat",
    "topic:cheats", "topic:game-hack", "topic:hvh", "topic:spoofer",
    "topic:hwid-spoofer", "topic:bypass", "topic:anticheat",
    "topic:anticheat-bypass", "topic:cheat-menu", "topic:mod-menu",
    "topic:external", "topic:internal", "topic:counter-strike",
    "topic:cs2", "topic:csgo", "topic:valorant", "topic:fortnite",
    "topic:apex-legends", "topic:fivem", "topic:game-modding",
    "topic:game-hacking", "topic:cheat-development",
]

B_CODE = [
    # kod uzayi — metadata'da imgui/cheat gecmeyen menu depolari bulur
    ('"imgui.h" language:C++', 10), ('"imgui.h" language:C', 8),
    ('"imgui.h" language:Lua', 8), ('"imgui.h" language:C#', 6),
    ('"imgui.h" aimbot', 10), ('"imgui.h" esp', 10),
    ('"imgui.h" wallhack', 6), ('"imgui.h" keyauth', 10),
    ('"imgui.h" hwid', 6), ('"imgui.h" loader', 8),
    ('"imgui.h" license', 6), ('"imgui.h" injector', 8),
    ('"imgui.h" spoofer', 6), ('"imgui.h" bypass', 6),
    ('"imgui.h" cheat', 10), ('"imgui.h" offsets', 8),
    ('"imgui.h" esp external', 6), ('"imgui.h" aimbot external', 6),
    ('"ImGui::Begin" language:C++', 10), ('"ImGui::Begin" language:C', 8),
    ('"ImGui::Begin" aimbot', 10), ('"ImGui::Begin" esp', 10),
    ('"ImGui::Begin" wallhack', 6), ('"ImGui::Begin" cheat', 8),
    ('"ImGui::Begin" keyauth', 8), ('"ImGui::Begin" loader', 8),
    ('"ImGui::Begin" hwid', 6), ('"ImGui::Begin" bypass', 6),
    ('"ImGui_ImplDX11" language:C++', 10), ('"ImGui_ImplDX9" language:C++', 10),
    ('"ImGui_ImplDX12" language:C++', 8), ('"ImGui_ImplWin32" language:C++', 10),
    ('"imgui_impl_vulkan" language:C++', 6), ('"imgui_impl_opengl3" language:C++', 8),
    ('"imgui_impl_sdl2" language:C++', 8), ('"imgui_impl_glfw" language:C++', 8),
    ('kiero imgui', 6), ('imgui d3d11 hook', 8), ('imgui dx11 hook', 6),
    ('imgui present hook', 8), ('"ImDrawList" language:C++ aimbot', 8),
    ('imgui wndproc hook', 6), ('imgui swapchain hook', 6),
]

LANGS_BIG = ["C++", "C", "C#", "Python", "Lua", "Rust", "JavaScript", "HTML"]
LANGS_SMALL = ["Go", "Java", "Zig", "Nim", "Dart", "Kotlin", "TypeScript",
               "Ruby", "PHP", "Objective-C", "Swift", "GDScript", "Haskell"]

PROBES = [
    # (sorgu, sayfa) — sayfa=0: sadece evren buyuklugu; sayfa=1: 1 sayfa hasat
    ("imgui", 1), ("imgui fork:true", 0),
    ("cheat", 1), ("cheat fork:true", 0),
    ("hack", 1), ("hack fork:true", 0),
    ("aimbot", 1), ("aimbot fork:true", 0),
    ("mod menu", 1), ("mod menu fork:true", 0),
    ("keyauth", 1), ("keyauth fork:true", 0),
    ("cheat loader", 1), ("hwid spoofer", 1),
    ("csgo cheat", 1), ("fivem menu", 1), ("esp cheat", 1),
    ("game hack", 1), ("cheat engine", 1), "injector", "spoofer",
    ("trainer", 0), ("external", 0), ("menu", 0), ("gui", 0),
    ("imgui topic", 1),
]

G_GENERIC = [
    # genis evren — hacim katmani (derin dilimleme ile 1M+ yolu)
    "cheat", "hack", "mod menu", "aimbot", "keyauth", "esp cheat",
    "game hack", "cheat engine", "cheat loader", "hwid spoofer",
    "bypass", "injector", "spoofer", "trainer", "csgo cheat", "fivem menu",
]

# FAZ 2.16 — KULLANICININ HAZIRLADIGI CHEAT SAHASI SORGUSU (birebir + dilimler)
# GitHub arayuz sorgusu:
#   "imgui" AND ("Cheat" OR "Farmbot" OR "Aimbot" OR "Bypass" OR "Multihack"
#               OR "Anticheat" OR "Hile" OR "Executor" OR "Clientless") language:C++
# NOT: REST /search/code boolean AND/OR'yi desteklemeyebilir (422 -> zararsiz
# atlanir); o yuzden ayni sahalarin kelimeli dilimleri de bankada.
USER_CODE = [
    ('"imgui" AND ("Cheat" OR "Farmbot" OR "Aimbot" OR "Bypass" OR "Multihack" OR "Anticheat" OR "Hile" OR "Executor" OR "Clientless") language:C++', 10),
    ('"imgui.h" farmbot', 8), ('"imgui.h" multihack', 8),
    ('"imgui.h" anticheat', 8), ('"imgui.h" clientless', 8),
    ('"imgui.h" executor', 8),
    ('imgui farmbot', 8), ('imgui multihack', 8), ('imgui anticheat', 8),
    ('imgui clientless', 8), ('imgui executor', 8), ('imgui hile', 8),
]
USER_REPO = [
    "imgui farmbot", "imgui multihack", "imgui anticheat", "imgui clientless",
    "imgui executor", "imgui hile", "farmbot", "multihack", "clientless",
    "anticheat bypass", "anticheat c++", "cheat executor", "executor c++",
    "csgo multihack", "rust multihack", "game executor", "bot farm",
]


def build_bank():
    bank = []  # (sorgu, sort, maxp, tag, kind)
    # FAZ 2.16: kullanicinin cheat-sahasi sorgulari EN ONECE (en yuksek oncelik)
    for q, maxp in USER_CODE:            # kod uzayi (tag b: dilimlenemez)
        bank.append((q, "", maxp, "b", "code"))
    for q in USER_REPO:                  # repo uzayi (tag c6: ay-dilkimeli)
        bank.append((q, "stars", 6, "c6", "repo"))
    for item in PROBES:
        q, p = (item, 1) if isinstance(item, str) else item
        bank.append((q, "stars", p, "u", "repo"))
    for q in A1_DIRECT:
        bank.append((q, "stars", 6, "c1", "repo"))
    for q in A2_BASES:
        bank.append((q, "stars", 5, "c2", "repo"))
    for q in A4_LOADER:
        bank.append((q, "stars", 6, "c4", "repo"))
    for q in A5_TOPICS:
        bank.append((q, "stars", 6, "c5", "repo"))
    for q, maxp in B_CODE:
        bank.append((q, "", maxp, "b", "code"))
    for term in CHEAT_TERMS:
        for g in GAMES:
            bank.append((f"{g} {term}", "stars", 5, "c3", "repo"))
    for lang in LANGS_BIG:
        bank.append((f"imgui language:{lang}", "stars", 10, "v", "repo"))
        for y in range(2014, 2027):
            bank.append((f"imgui language:{lang} created:{y}-01-01..{y}-12-31", "stars", 10, "v", "repo"))
    for lang in LANGS_SMALL:
        bank.append((f"imgui language:{lang}", "stars", 8, "v", "repo"))
    for extra in ["imgui pushed:>2025-01-01", "imgui pushed:>2024-01-01",
                  "imgui created:>2025-01-01", "imgui created:>2024-06-01",
                  "imgui stars:100..2000", "imgui stars:10..100",
                  "imgui stars:1..10", "dear imgui", "dear-imgui",
                  "imgui demo", "imgui widget", "imgui fork:true"]:
        bank.append((extra, "stars", 6, "v", "repo"))
    for q in G_GENERIC:
        bank.append((q, "stars", 4, "g", "repo"))
    # tekrarlari at — v2.18 DUZELTME: anahtar TAG de icerir. Eskiden
    # kind|q|srt anahtari u-probe'larla G_GENERIC g-katmanini yenig
    # dusuruyordu (16 hacim teriminin 15'i HIC kayda girmiyordu — 1M
    # hacim katmani sessizce olu idi).
    seen, uniq = set(), []
    for q, srt, m, tag, kind in bank:
        k = f"{kind}|{q}|{srt}|{tag}"
        if k not in seen:
            seen.add(k)
            uniq.append((q, srt, m, tag, kind))
    return uniq


# ---------------- rekurif dilimleme (1000 sonuc sinirini kir) ---------------- #
CRE_RE = re.compile(r"created:(\d{4})-(\d{2})-(\d{2})\.\.(\d{4})-(\d{2})-(\d{2})")


def span_days(q):
    """sorgudaki created: araliginin gun sayisi; yoksa None."""
    m = CRE_RE.search(q)
    if not m:
        return None
    y1, m1, d1, y2, m2, d2 = map(int, m.groups())
    try:
        t1 = datetime.date(y1, m1, d1).toordinal()
        t2 = datetime.date(y2, m2, d2).toordinal()
    except ValueError:
        return None
    return t2 - t1 + 1


def allowed_depth(tag):
    """u:0, b:0 (kod aramasi dilimlenemez), c*:3 (ay), v:2 (ceyrek), g:4 (gun parcasi)"""
    if tag == "u" or tag == "b":
        return 0
    if tag.startswith("c"):
        return 3
    if tag == "v":
        return 2
    return 4  # g


def children(q, tag):
    """dilim cocuklari uret: taban->yillar, yil->ceyrek, ceyrek->ay, ay->gun parcasi."""
    out = []
    span = span_days(q)
    if span is None:
        for y in range(2011, 2027):
            last = calendar.monthrange(y, 12)[1]
            out.append(f"{q} created:{y}-01-01..{y}-12-{last}")
    elif span > 300:  # yil -> ceyrek
        m = CRE_RE.search(q)
        y = int(m.group(1))
        for m1, m2 in ((1, 3), (4, 6), (7, 9), (10, 12)):
            l2 = calendar.monthrange(y, m2)[1]
            out.append(f"{q[:m.start()]}created:{y}-{m1:02d}-01..{y}-{m2:02d}-{l2:02d}")
    elif span > 60:  # ceyrek -> ay
        m = CRE_RE.search(q)
        y, m1, _ = int(m.group(1)), int(m.group(2)), int(m.group(3))
        for mm in range(m1, m1 + 3):
            if mm > 12:
                break
            last = calendar.monthrange(y, mm)[1]
            out.append(f"{q[:m.start()]}created:{y}-{mm:02d}-01..{y}-{mm:02d}-{last:02d}")
    elif span > 10:  # ay -> gun parcalari (5 parcа)
        m = CRE_RE.search(q)
        y, mm = int(m.group(1)), int(m.group(2))
        for d1, d2 in ((1, 7), (8, 14), (15, 21), (22, 28), (29, 31)):
            last = calendar.monthrange(y, mm)[1]
            dd2 = min(d2, last)
            if d1 > last:
                continue
            out.append(f"{q[:m.start()]}created:{y}-{mm:02d}-{d1:02d}..{y}-{mm:02d}-{dd2:02d}")
    else:
        return []  # gun parcasi — derinlik siniri
    return out


# ------------------------------------------------------------------ #
#  STATE
# ------------------------------------------------------------------ #
def load_state():
    if os.path.exists(STATE):
        with open(STATE, encoding="utf-8") as f:
            st = json.load(f)
    else:
        st = {"repos": {}, "queries": {}, "totals": {}}
    # v2.18 gecis: eski 'kind|q|srt' kayit anahtarlarini uclari TAG'li
    # yeni formata tasi (kaydin kendi tag alani kullanilir). Baska turlu
    # ayni sorgunun u-probe kaydi, g-katman calismasini 'done' sanip atlar.
    q = st.get("queries", {})
    for k in list(q.keys()):
        rec = q[k]
        tag = rec.get("tag")
        if tag and not k.endswith("|" + tag):
            nk = k + "|" + tag
            if nk not in q:
                q[nk] = rec
            del q[k]
    return st


def save_state(st):
    tmp = STATE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(st, f, ensure_ascii=False)
    os.replace(tmp, STATE)


class GH:
    def __init__(self, token):
        self.s = requests.Session()
        self.s.headers.update({
            "Authorization": f"Bearer {token}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "imgui-god-research-harvester-v2",
        })

    def search(self, kind, q, sort, page, per_page=100):
        url = API + ("/search/repositories" if kind == "repo" else "/search/code")
        params = {"q": q, "per_page": per_page, "page": page}
        if sort and kind == "repo":
            params["sort"] = sort
        for _ in range(7):
            try:
                r = self.s.get(url, params=params, timeout=40)
            except requests.RequestException as e:
                print(f"  [net] {e} — 5s", flush=True)
                time.sleep(5)
                continue
            if r.status_code in (403, 429):
                if "secondary rate limit" in (r.text or "")[:400].lower():
                    ra = int(r.headers.get("retry-after", 0) or 0)
                    wait = ra if ra > 0 else 60
                    print(f"  [secondary-limit] {wait}s uyku", flush=True)
                    time.sleep(wait + 1)
                    continue
                reset = int(r.headers.get("x-ratelimit-reset", time.time() + 65))
                wait = max(2.0, min(reset - time.time() + 2.0, 65.0))
                print(f"  [rate-limit] {wait:.0f}s uyku", flush=True)
                time.sleep(wait)
                continue
            if r.status_code in (422, 451):
                return None
            if r.status_code >= 500:
                # GitHub tarafında geçici sunucu hatası (5xx) — döngüyü
                # öldürmek yerine aynı deneme bütçesi içinde geri çekilerek tekrar dene
                print(f"  [srv-{r.status_code}] sunucu hatası — 5s", flush=True)
                time.sleep(5)
                continue
            r.raise_for_status()
            time.sleep(SLEEP)
            return r.json()
        return None


def extract(kind, it):
    r = it if kind == "repo" else (it.get("repository") or {})
    fn = r.get("full_name")
    if not fn:
        return None
    return fn, {
        "full_name": fn,
        "url": r.get("html_url") or f"https://github.com/{fn}",
        "desc": r.get("description"),
        "lang": r.get("language"),
        "stars": r.get("stargazers_count", 0) or 0,
        "topics": r.get("topics") or [],
        "fork": bool(r.get("fork")),
        "archived": bool(r.get("archived")),
        "created": (r.get("created_at") or "")[:10],
        "pushed": (r.get("pushed_at") or "")[:10],
    }


def main():
    max_runtime = float(sys.argv[1]) if len(sys.argv) > 1 else 560
    deadline = time.time() + max_runtime
    st = load_state()
    gh = GH(TOKEN)
    bank = build_bank()
    hot = []
    hot_set = set()
    # v2.18: kalici hot kuyrugu — onceki turun islenemeyen cocuk dilimleri
    # kaybolmasin (parent zaten done oldugu icin bir daha uretilmiyorlardi;
    # son turda 43 dilim boylece cope gitti).
    for h in st.get("hotqueue", []):
        try:
            q, srt, m2, tag2, kind2 = h
        except Exception:
            continue
        if st["queries"].get(f"{kind2}|{q}|{srt}|{tag2}", {}).get("done"):
            continue
        sk = f"{kind2}|{q}|{srt}|{tag2}"
        if sk in hot_set:
            continue
        hot_set.add(sk)
        hot.append((q, srt, int(m2), tag2, kind2))
    # v2.18: g-katman (hacim yolu) hot kuyrugunda DA her zaman onde —
    # fork/v dilimleri bitmeden 1M hedefinin motoru calismaya baslasin.
    hot.sort(key=lambda x: x[3] != "g")
    i = 0
    new_total = 0
    q_new = 0
    last_save = time.time()
    stats_done0 = sum(1 for v in st["queries"].values() if v.get("done"))

    # bozuk probe kayitlarini temizle (v2.0 bug: maxp=0 hic calismadi)
    fixed = 0
    for k in list(st["queries"].keys()):
        rec = st["queries"][k]
        if rec.get("tag") == "u" and rec.get("total", 0) == 0 and not rec.get("pages_done"):
            del st["queries"][k]
            fixed += 1
    if fixed:
        print(f"[onarim] {fixed} bos probe kaydi temizlendi", flush=True)

    def nonfork():
        return sum(1 for r in st["repos"].values() if not r.get("fork"))

    print(f"BASLANGIC: {len(st['repos'])} repo ({nonfork()} non-fork) | "
          f"banka={len(bank)} | sure={max_runtime:.0f}s", flush=True)

    def next_item():
        nonlocal i
        if hot:
            return hot.pop(0)
        if i < len(bank):
            item = bank[i]
            i += 1
            return item
        return None

    while True:
        if time.time() > deadline - 15:
            break
        nxt = next_item()
        if nxt is None:
            break
        q, srt, maxp, tag, kind = nxt
        qkey = f"{kind}|{q}|{srt}|{tag}"
        rec = st["queries"].get(qkey)
        if rec and rec.get("done"):
            continue
        total = rec["total"] if rec else None
        start_page = (rec["pages_done"] + 1) if rec else 1
        new = rec["new"] if rec else 0
        page = start_page
        data, items = None, []

        while page <= (min(maxp, 10) if maxp > 0 else 1):
            if time.time() > deadline - 8:
                break
            data = gh.search(kind, q, srt, page, 100 if maxp > 0 else 1)
            if data is None:
                break
            if total is None:
                total = data.get("total_count", 0)
                st.setdefault("totals", {})[f"{kind}:{q}"] = total
                if total and total > 950 and maxp > 0 and rec is None:
                    if allowed_depth(tag) > 0:
                        for cq in children(q, tag):
                            sk = f"repo|{cq}|{srt}|{tag}"
                            if sk not in st["queries"] and sk not in hot_set:
                                hot_set.add(sk)
                                if tag == "g":
                                    # g-cocuklari (cheat/hack hacim dilimleri) kuyrugun
                                    # BASINA girer — oncelik hicbir zaman kaybolmaz
                                    hot.insert(0, (cq, srt, 10, tag, "repo"))
                                else:
                                    hot.append((cq, srt, 10, tag, "repo"))
            items = data.get("items", [])
            if not items:
                break
            for it in items:
                got = extract(kind, it)
                if not got:
                    continue
                fn, rec2 = got
                if fn not in st["repos"]:
                    rec2["via"] = tag
                    st["repos"][fn] = rec2
                    new += 1
                    new_total += 1
            if maxp <= 0 or len(items) < 100:
                break
            page += 1

        done = (page > (min(maxp, 10) if maxp > 0 else 1)) or \
               (data is not None and (len(items) < 100 or maxp <= 0)) or \
               (total == 0)
        if total is not None:
            st.setdefault("totals", {})[f"{kind}:{q}"] = total
        st["queries"][qkey] = {
            "total": total if total is not None else (rec["total"] if rec else 0),
            "new": new, "pages_done": page - 1,
            "done": bool(done), "tag": tag, "kind": kind,
        }
        q_new += 1
        if new > 0 or tag == "u" or q_new % 25 == 0:
            print(f"[{tag}] {q[:58]:58s} yeni={new:<4d} t={total if total is not None else '?':>6} "
                  f"| KORPUS={len(st['repos'])}", flush=True)
        if time.time() - last_save > 40:
            st["hotqueue"] = [list(x) for x in hot[:800]]
            save_state(st)
            last_save = time.time()

    st["hotqueue"] = [list(x) for x in hot[:800]]
    save_state(st)
    q_done = sum(1 for v in st["queries"].values() if v.get("done"))
    print(f"DURUM: bu turda +{new_total} yeni | toplam={len(st['repos'])} "
          f"({nonfork()} non-fork) | statik={i}/{len(bank)} | hot={len(hot)} "
          f"| sorgu kayit={len(st['queries'])} (bitti={q_done}, +{q_done - stats_done0})", flush=True)


if __name__ == "__main__":
    main()
