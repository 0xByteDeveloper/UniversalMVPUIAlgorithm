# DEV MISSION — GOD-TIER TOTAL PIPELINE
## TotallyResearch → GlobalInfos → Olasılık Evreni → 1:1 Rekonstrüksiyon (v2.0)

> Bu belge, `UniversalMVPUIAlgorithm` (yapı-çözücü) ve `MVP_GOD_SSS_TIER_IMGUI_UI_EDITOR`
> (menü yapıcı + kod derleyici) adlı iki aracı **durdurulamaz** yapan resmî görev tanımıdır.
> İki repo da bu belgeyi referans alır; hasat çıktısı `TotallyResearch.md` ile birlikte yaşar.

- **Tarih:** 2026-11-27
- **Durum:** Faz 0 tamamlandı — **13.006 repo hasat edildi, 9.515 proje listelendi** (104 sorgu)
- **Nihai hedef:** 1.000.000+ imgui projesi, tam bilgi çıkarımı, **algoritma dışında neredeyse hiçbir şey kalmayacak**

---

## 0. Vizyon — Durdurulamaz Sistem

Binlerce ImGui projesini tek tek analiz edip şunları öğreniyoruz:

1. **Obje evreni** — button, checkbox, slider, combo, tab, child, color picker, keybind, spinner, notification... hepsi; her varyantıyla.
2. **Event/handler evreni** — ImGui immediate-mode olduğundan "button1_click" yoktur; `if (ImGui::Button("Aimbot")) { ... }` biçimindedir. Event = true-branch gövdesidir (çıkarıcı bu dalı yakalar).
3. **Tema evreni** — StyleColors/StyleVars/renk paletleri, kümelenebilir tema aileleri.
4. **Animasyon evreni** — hover geçişleri, açılış tween'leri, easing fonksiyonları, süre dağılımları.
5. **Yerleşim evreni** — pencere/child/colon/SameLine düzenleri, boyut ve pozisyon dağılımları.
6. **Sürüm evreni** — 1.76 → 1.93+ layout aileleri, hash dönemleri (A/B/C), CRC tabloları.

Bu altı evren **olasılık dağılımları** olarak modellenir (Olasılık Evreni, Bölüm 7).
Sonuç: UniversalMVPUIAlgorithm hedef ImGui menüsünün **her objesini, her animasyonu, her temayı**
tanır; MVP_GOD_SSS_TIER_IMGUI_UI_EDITOR aynı menüyü **birebir (1:1)** VS2019 C++ projesi olarak inşa eder.

```
[GitHub evreni] ──hasat──> TotallyResearch.md (9.515+ proje, her satır = 1 proje)
      │
      ▼
[Toplu klonlama + triyaj] (Faz 1)
      │
      ▼
[Analiz: kaynak kod + UMVP canlı/bellek analizi]  ──üretir──>  GlobalInfos/*.json (Faz 2)
      │                                                          │
      ▼                                                          ▼
[Olasılık Evreni: tema kümeleri, animasyon grameri,          [GlobalInfos →
 obje priors, boyut/pos dağılımları] (Faz 3)                  MVP_GOD editör importu]
      │                                                          │
      ▼                                                          ▼
[UMVP unknown_objectxN motoru: bilinmeyeni       [1:1 REKONSTRÜKSİYON →
 numaralandır, özellik çıkar, terfi ettir]        VS2019 C++ kod üretimi] (Faz 5)
```

---

## 1. FAZ 0 — TotallyResearch.md (Araştırma Korpusu)

**Kurallar:**

- Her satır = 1 proje: `- [owner/repo](url) — açıklama — dil · yıldız`
- Fork'lar hariç (özelleştirilmiş fork'lar ayrı maden olarak Faz 0.5'te işlenir).
- Kategoriler: bayrak, çekirdek, cheat menüleri, loader/auth, editör, tema, animasyon,
  node/plot, overlay/hook, bağlayıcılar, motor entegrasyonları, araçlar, örnekler,
  yüksek olasılıklı saha (imgui kelimesi geçmeyen ama C/C++/C# cheat/loader projeleri), diğer.
- Arşivlenmiş repolar `· arşiv` ile işaretlenir (DMCA izi = popülerlik sinyali).

**Büyüme stratejisi (1.000 → 1.000.000+):**

| Kademe | Yöntem | Ölçek |
|---|---|---|
| 1 | GitHub Search API (104 sorgu varyasyonu — bitti) | 13.006 hasat / 9.515 listeli |
| 2 | Sorgu uzayı genişletme: dil × oyun × tema × tarih katmanları | ~100.000 |
| 3 | **BigQuery GH Archive + `github_repos` dataset** (`imgui.h` / `#include "imgui.h"` içeren tüm depolar SQL ile) | ~1.000.000+ |
| 4 | **Sourcegraph / grep.app** kod-uzayı araması (`ImGui::Begin(`, `imgui.h` imzaları) | milyon+ referans |
| 5 | **Fork grafiği madenciliği** (ocornut/imgui + cheat base'lerinin fork ağaçları) | yüzbinlerce özelleştirme |
| 6 | **Software Heritage arşivi** (silinmiş/DMCA'lı repoların kayıtları) | geri kazanım |

**Kalite filtresi (P5 sahası):** imgui kelimesi geçmeyen projeler yalnızca
`dil ∈ {C, C++, C#}` VE (açıklama VAR veya yıldız ≥ 1) ise listelenir; aksi atılır
(3.492 kayıt bu şekilde elendi). Yüzde emin olunamayanlar "yüksek olasılık" kategorisinde
şeffaf biçimde tutulur — asla gizli karışıklık yok.

---

## 2. FAZ 1 — Toplu Klonlama & Triyaj

1. `git clone --depth 1 --filter=blob:none` + sparse-checkout (yalnız `.cpp/.h/.sln/.vcxproj`).
2. ImGui tespiti: `imgui.h`, `IMGUI_VERSION "1.xx.x"` stringi, `#include "imgui.h"`, cimgui.
3. Derleme ortamı tespiti: `.sln` (v160 = VS2019), CMake, MinGW, vcpkg/submodule/vendor.
4. Öncelik sırası (analiz kuyruğu): yıldız → kategori çeşitliliği → tema/animasyon zenginliği
   (renk/animasyon anahtar kelimesi yoğunluğu) → güncellik.
5. Her repo için GlobalInfos üretildikçe `analyzed: true` işaretlenir; çift analizi önler.

---

## 3. FAZ 2 — GlobalInfos Dosya Formatı (İKİ REPO ARASINDAKİ KÖPRÜ)

GlobalInfos, her analiz edilen projeden çıkan **zengin bilgi dosyasıdır**. UniversalMVPUIAlgorithm
üretir/yazar, MVP_GOD_SSS_TIER_IMGUI_UI_EDITOR okur/uygular. Sürüm: **2.0**.

```json
{
  "magic": "GLOBALINFOS",
  "version": "2.0",
  "source": {
    "repo": "owner/name", "commit": "abc1234", "license": "MIT",
    "analyzed_at": "2026-11-27T00:00:00Z",
    "corpus_line": "TotallyResearch.md satır referansı"
  },
  "binary": {
    "type": "exe|dll", "arch": "x64|x86",
    "protection": "none|vmprotect|themida|upx",
    "mode": "internal|external",
    "modules": ["d3d11", "dxgi"], "hook": "present|wndproc|swapchain|kiero|minhook"
  },
  "imgui": {
    "version": "1.89.9", "hash_period": "A|B|C",
    "evidence": ["IMGUI_VERSION string @ 0x...", "layout DB hipotez 2/3 PASS", "CRC32C tablosu .rdata"],
    "binding": "cpp|cimgui|lua|python|csharp"
  },
  "menu": {
    "toggle_key": "INSERT", "toggle_xref": "GetAsyncKeyState(0x2D) @ 0x...",
    "structure": "tabs-left|tabs-top|sidebar|child-sections|single-window"
  },
  "forms": [
    { "id": "main", "label": "XXGOD MVP", "pos": [60,60], "size": [430,460],
      "flags": ["ImGuiWindowFlags_NoCollapse"], "children": ["tab_legit", "tab_visuals"] }
  ],
  "objects": [
    { "id": "obj_7", "type": "ImGui::SliderFloat", "label": "FOV",
      "form": "main/tab_legit", "pos": [x,y], "size": [w,h],
      "args": { "min": 1.0, "max": 180.0, "format": "%.0f" },
      "colors": { "hover": "#7a28c9", "grab": "#9b59d0" },
      "id_hash": "0x4D5FF1B2", "z": 3 }
  ],
  "events": [
    { "object_id": "obj_5", "trigger": "click",
      "handler_block": "0x140001CB0..0x140001CE5",
      "behavior": "config.aimbot.enabled ^= true", "side_effects": ["config save"] }
  ],
  "themes": [
    { "name": "purple-god", "style_vars": { "FrameRounding": 4.0, "WindowRounding": 6.0,
      "GrabRounding": 3.0, "Alpha": 0.98 },
      "colors": { "WindowBg": "#111111ee", "Button": "#333333ff",
                  "ButtonHovered": "#7a28c9ff", "CheckMark": "#9b59d0ff" },
      "fonts": [ { "hint": "Verdana 15px", "atlas": "512x512 R8" } ] }
  ],
  "animations": [
    { "target": "obj_5", "trigger": "hover", "type": "color_lerp",
      "from": "#333333", "to": "#7a28c9", "duration_ms": 150, "easing": "in_out_quad" },
    { "target": "form:main", "trigger": "menu_open", "type": "alpha_fade+pos_slide",
      "duration_ms": 250, "easing": "out_cubic", "slide_from": [0,-30] }
  ],
  "textures": [ { "kind": "atlas|logo|background", "size": [512,512], "embedded": true } ],
  "custom_draw": [ { "kind": "gradient_header", "rect": [0,0,430,42] } ],
  "config_io": { "format": "json|ini", "path_hint": "%appdata%/xxgod/config.json", "keys": 37 },
  "probability": { "theme_cluster": "csgo-purple", "similarity": 0.92, "confidence": 0.87 },
  "unknown_objects": [
    { "id": "unknown_objectx1", "form": "main/tab_visuals",
      "raw_features": { "bbox": [12,120,96,24], "colors": ["#222", "#7a28c9"],
        "child_count": 0, "behavior": "toggle+popup", "text_hits": ["Radar"] },
      "candidates": [ {"type": "custom_toggle", "score": 0.61}, {"type": "ImGui::Checkbox", "score": 0.44} ] }
  ]
}
```

**Üretim yolları (ikisi birleşir):**
- **Statik:** kaynak koddan (Faz 1) Begin/End blokları, widget çağrıları, renk tabloları, animasyon yardımcıları.
- **Dinamik:** UMVP'nin mevcut GOD STATE motoru — canlı bellek `ImHashStr` taraması, layout DB
  hipotez testleri, `.pdata` fonksiyon sınırları, backward-slice argüman kurtarma, draw-list metin
  çözümü, handler blokları. (Wine altında uçtan uca test altyapısı zaten var: `make test-integration`.)

---

## 4. UniversalMVPUIAlgorithm — GÜNCELLENMİŞ Tam Çıkarım Hedef Listesi

Mevcut 6 madde korunur; eksik olanlar **[YENİ]** ile eklenir:

1. **Yapı** — pencere/child hiyerarşisi (mevcut).
2. **Sürüm** — layout DB + hash dönemi + CRC tablosu + canlı bellek doğrulaması (mevcut).
3. **Formlar** — Form1/Form2/tabs (mevcut).
4. **Obje listesi** — tüm widget'lar (mevcut).
5. **Obje bilgileri** — boyut, argümanlar (slider min/max, combo öğeleri) (mevcut).
6. **Event bilgileri** — handler kod blokları [start,end) + disassembly (mevcut).
7. **[YENİ] Tema/stil** — StyleColors + StyleVars + global alpha (renk değerleri bellek + koddan).
8. **[YENİ] Fontlar** — atlas boyutu, font ipuçları, glyph ölçüleri.
9. **[YENİ] Animasyonlar** — hover/aktif renk lerp'leri, menü açılış tween'leri, easing kimlikleri, süreler.
10. **[YENİ] Yerleşim simülasyonu** — SameLine/columns/child düzeni, tab yönü, margin'ler.
11. **[YENİ] Render backend + hook tipi** — d3d9/10/11/12, opengl, vulkan; present/wndproc/swapchain.
12. **[YENİ] Menü toggle tuşu** — GetAsyncKeyState xref'i (INSERT/HOME/END/MOUSE4...).
13. **[YENİ] Popup/tooltip/toast katmanı** — bildirim sistemleri.
14. **[YENİ] Custom draw-list çizimleri** — gradient'ler, gölgeler, watermark, rotate.
15. **[YENİ] Texture/resim** — gömülü logo/arka plan, atlas kullanımı.
16. **[YENİ] Konfigürasyon persist'i** — ini/json yolu + anahtar şeması (string xref).
17. **[YENİ] Keybind tabloları** — keybind widget'ları ve saklanan tuşlar.
18. **[YENİ] ID hash kaydı** — her objenin ImHashStr değeri (birebir eşleme anahtarı).
19. **[YENİ] Olasılık skorları** — tema kümesi benzerliği, güven yüzdesi.
20. **[YENİ] unknown_objectxN** — aşağıdaki motorla hiçbir obje atlanmaz.

Bu 20 madde, GlobalInfos şemasının 2.0'daki tüm alanlarını kapsar; UMVP çıktı raporları
(JSON) bu şemaya genişletilir.

---

## 5. unknown_objectxN Motoru — "Algoritma Dışında Hiçbir Şey Kalmayacak"

**Problem:** özel/modifye widget'lar (custom toggle, radar widget, custom palette...) standart ImGui
imzalarına uymaz. **Çözüm:** bilinmeyeni atlamak yerine numaralandırıp sahiplenmek.

1. **Tespit:** draw-list anomali bölgeleri (standart olmayan çizim kalıpları) + kod deseni
   eşleşmeyen interaktif bölgeler + LastItemData dışı hit-testler.
2. **Özellik çıkarımı (raw feature vector):** bbox, renk kümesi, alt obje sayısı, metin isabetleri,
   davranış imzası (tıklanabilir mi, toggle mı, popup açıyor mu), animasyon varlığı.
3. **En yakın sınıf eşleşmesi:** olasılık evrenindeki obje sınıflarına karşı benzerlik skoru.
4. **Karar:** skor ≥ 0.75 → bilinen sınıf olarak etiketle (güven skoruyla).
   Skor < 0.75 → `unknown_objectxN` (N artan sayaç) + aday listesi + raw özellikler GlobalInfos'a yazılır.
5. **Terfi (promotion):** aynı unknown imzası ≥ 3 farklı projede görülürse → yeni sınıf
   olarak obje veritabanına girer (`custom_toggle_v1` gibi), bundan sonra bilinendir.
6. **Metrik:** unknown oranı = unknown_objectxN / toplam obje. Hedef: sürekli azalış → ~%0.

Editör tarafı unknown_objectxN'i en yakın adayla taslak olarak render eder, farkı işaretler;
böylece 1:1 hedefi hiçbir koşulda bloke olmaz.

---

## 6. FAZ 3 — Olasılık Evreni (ProbabilityUniverse)

GlobalInfos yığınından sürekli güncellenen merkezi istatistik dosyası:

- **Obje tipi priors:** P(tip | saha) — cheat menüsünde checkbox oranı, loader'da login formu.
- **Tema kümeleri:** renk paleti kümelemesi (k-means) → "csgo-purple", "dark-neon", "pastel-clean" aileleri.
- **Animasyon grameri:** geçiş türleri (color_lerp, alpha_fade, pos_slide, size_pulse), easing
  fonksiyonları (linear, in_out_quad, out_cubic...), süre dağılımları (150/200/250 ms modları).
- **Yerleşim istatistikleri:** pencere boyutu modları (430×460 gibi), buton boyutları (120×28),
  margin/padding dağılımları.
- **Event isimlendirme desenleri:** `enabled`, `_toggle`, `save config`, tab adları...
- **Sürüm × saha matrisi:** hangi dönem hangi oyunlarda.

Kullanımı: UMVP belirsiz objeyi en olası tiple çözümler; editör eksik bilgiyi en olası değerle
tamamlar; 1:1 skor güven aralığıyla raporlanır.

---

## 7. FAZ 4-5 — Editör Besleme & 1:1 Rekonstrüksiyon

1. MVP_GOD_SSS_TIER_IMGUI_UI_EDITOR **GlobalInfos import** ekranı: form ağacı, objeler, tema,
   animasyonlar tek tıkla yüklenir (editördeki ~20-30 obje tipi bu korpusla 100+'a büyür).
2. Tema/animasyon paleti: olasılık evreninden "tema aileleri" editöre kütüphane olarak gelir.
3. **Kod üretimi:** VS2019 C++ çözümü + ImGui entegrasyonlu kaynak (mevcut codegen altyapısı).
4. **1:1 skor:** piksel diff (render karşılaştırma) + yapı diff + event kapsama; skor ≥ %95 = birebir.

---

## 8. Otomasyon, CI & Metrikler

- **Haftalık hasat cron'u** (GitHub Actions): TotallyResearch.md güncellemesi otomatik PR.
- **Analiz kuyruğu:** yeni repo → triyaj → GlobalInfos → olasılık evreni yeniden hesap.
- **KPI panosu:** toplam repo / GlobalInfos sayısı / bilinen obje tipi sayısı / unknown oranı /
  tema ailesi sayısı / 1:1 ortalama skor / sürüm parmak izi kapsamı.
- **Sürümleme:** TotallyResearch.md vN.N; GlobalInfos şeması kendi semver'iyle (2.0) ilerler.

## 9. Hukuk & Lisans Notları

- Dear ImGui MIT lisanslıdır: analiz serbesttir; kod taşınırken lisans/atribusyon korunmalıdır.
- Korpus yalnızca **kamuoyuna açık** GitHub repolarının link + açıklama listesidir; kod dağıtımı yoktur.
- Analiz; birlikte çalışabilirlik, güvenlik araştırması ve eğitim amaçlıdır. Başkalarının
  lisanslı/ticari UI kodunu olduğu gibi dağıtmak yasaklanmıştır (liste kaydı ≠ kod paylaşımı).

## 10. Yol Haritası & Durum

- [x] **Faz 0** — 13.006 repo hasat, 9.515 proje listelendi (104 sorgu, 2026-11-27)
- [ ] **Faz 0.5** — BigQuery + fork grafiği ile korpusu 100.000+'ye büyüt
- [ ] **Faz 1** — klonlayıcı + triyaj otomasyonu (öncelik kuyruğu)
- [ ] **Faz 2** — GlobalInfos v2.0 statik üretici + UMVP JSON çıktısının şemaya genişletilmesi
- [ ] **Faz 3** — olasılık evreni v1 (tema kümeleme + animasyon grameri + priors)
- [ ] **Faz 4** — unknown_objectxN motoru + terfi sistemi
- [ ] **Faz 5** — editör GlobalInfos import + 1:1 skor + VS2019 kod üretimi
- [ ] **Faz 6** — 1.000.000+ korpus, sürekli CI, KPI panosu

> **Slogan:** Hasat → Analiz → Olasılık Evreni → Tanıma → Birebir İnce. Algoritma dışında hiçbir şey kalmayacak.
