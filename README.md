# UniversalMVPUIAlgorithm

**MVP GOD STATE LEVEL UI ANALYSIS SDK** — ImGui tabanlı menülere sahip herhangi bir EXE'nin
süreç belleğini derin analizle tarayan, formları (Form1/Form2...), widget'ları (Button,
Checkbox, SliderFloat, Combo, ...) ve bunların **kod adreslerini (base/end) + handler
disassembly'sini** PDB OLMAKSIZIN, saf derin disassembly + canlı bellek analiziyle çıkaran
C++17 SDK ve CLI aracı.

```
=============================================================================
   UMVP // UniversalMVPUIAlgorithm — MVP GOD STATE UI ANALYSIS SDK
   d3d9 | d3d10 | d3d11 | d3d12 | OpenGL | Vulkan — render agnostic
=============================================================================
```

## Ne yapar?

1. **Süreç seçimi**: `--list` ile PID listesi, `--pid N` / `--name x.exe` ile hedef.
2. **Canlı bellek analizi (GOD STATE)**:
   - Heap'i `ID == ImHashStr(Name)` **invariant'ıyla** tarar → `ImGuiWindow` nesneleri
     (versiyon-bağımsız yapı hipotezleri: 1.76 → 1.93+ arası 3 layout ailesi).
   - `ImGuiContext` keşfi: pencerenin kendi `Ctx` işaretçisi + `g.Windows` vektörü geri-bağı
     doğrulaması → **sürüm canlı bellekten DOĞRULANIR** (layout DB hipotez testiyle).
   - Draw-list → font glif UV çözümü → widget **ekran konumları**.
3. **Derin disassembly analizi (PDB YOK)**:
   - CRC32 tablo parmak izleriyle ImGui dönemi tespiti (3 hash dönemi!).
   - String xref haritası (RIP-relative LEA + call/jmp rel32 + push imm + movabs).
   - `.pdata` (RUNTIME_FUNCTION) ile **kesin fonksiyon sınırları** + sezgisel yedek.
   - Backward-slice argüman kurtarma: `Button("Save", ImVec2(120,28))` boyutu, slider
     min/max, combo öğeleri, format string'leri **koddan okunur**.
   - Handler blok çıkarma: `if (Button(...)) { ... }` gövdesi [start,end) + disassembly.
4. **D3D tespiti**: d3d9/d3d10/d3d11/d3d12/dxgi/opengl32/vulkan modülleri raporlanır.
   Yöntem tamamen render-agnostiktir (hook yok, sadece bellek okuma) → **tüm sürümlerde çalışır**.

## Derin bulgular (ImGui SDK taramasından)

Bu SDK, Dear ImGui kaynağının 7 versiyonlu (1.76, 1.80, 1.86, 1.89.9, 1.91.8, 1.92.6,
1.93.1 WIP) derin taramasıyla üretilmiş bilgiyi kullanır:

| Dönem | Versiyonlar | CRC tablosu | `###` işlemi |
|---|---|---|---|
| A | ≤ 1.91.5 | CRC32-adler (zlib) | karakterler hash'lenir |
| B | 1.91.6 – 1.92.5 | CRC32C (SSE4.2 uyumlu) | karakterler hash'lenir |
| C | ≥ 1.92.6 | CRC32C | üç `#` atlanır |

- `tools/gen_layout/` her versiyonun **gerçek offsetof çıktısını** native derleyerek
  `src/imgui_layout_db.h` başlığını üretir (elle offset yazmak YOK).
- Hash portu (`src/imgui_hash.cpp`) 7 versiyonun GERÇEK `ImHashStr`'iyle birebir
  doğrulanmıştır: **7/7 PASS, 232 deneme/versiyon, 0 hata**.

## Derleme

Gereksinimler: `mingw-w64` (x86_64 + i686, posix-thread g++), `make`, imgui kaynakları
(test hedefleri için), Capstone (otomatik derlenir).

```bash
# ortam (root'suz sandbox —详见 scripts/setup_env.sh tarzı)
make cli64          # umvp_cli_x64.exe  (analyzer, x64)
make cli86          # umvp_cli_x86.exe  (analyzer, x86/32-bit)
make targets        # test hedefleri (gerçek ImGui menülü EXE'ler)
make test-hash      # bağımsız hash testi (7 imgui versiyonu vs SDK portu)
make test-layout    # layout DB bütünlük testi
make test-integration  # Wine altında uçtan uca entegrasyon (3 hedef)
```

Değişkenler: `IMGUI_MASTER=... IMGUI_1899=... CAPSTONE_SRC=...` (varsayılan `~/deps`).

## Kullanım

```
umvp_cli_x64.exe                       # interaktif: süreç listesi + PID seç
umvp_cli_x64.exe --list
umvp_cli_x64.exe --pid 1234 --json r.json --txt r.txt --asm-dir asm/
umvp_cli_x64.exe --name game.exe --no-live      # yalnız kod analizi
                     --no-code                  # yalnız canlı tarama
                     --no-text-decode           # draw-list metin çözümü kapalı
```

Örnek çıktı (gerçek test hedefinden, Wine altında):

```
Hedef        : target_dx11_master.exe (PID 32) [x64 (64-bit)]
Renderer     : D3D11 (d3d11.dll yüklü + dxgi.dll)
Surum        : 1.93.1 WIP  [canli bellekten DOGRULANDI]
Hash donemi  : B/C (crc32c, >=1.91.6)
Context      : 0x7FFFFE8E4D50
DisplaySize  : 1272 x 698 | Framerate: 82.7 FPS | Aktif pencere: 4

--- FORMLAR: 3 ----------------------------------------------------------------
  [3] "XXGOD MVP | Aimbot"  id=0x4D5FF1B2  [CANLI][KOD-ANALIZ]
      nesne=0x7FFFFE8EEB10  pos=(60.0,60.0)  size=(430.0,460.0)
      Begin cagrisi: 0x140001894 | form kod blogu: 0x1400019C0..0x140001AE4
      widget'lar (10):
         1. Checkbox   "Aimbot Enabled"  guven=82%
         4. Button     "Save Config"     guven=85%   boyut=(120.0, 28.0) [code]
            handler kod blogu: 0x140001CB0..0x140001CE5 (9 komut)
              0x0000000140001CB0  mov      rcx, qword ptr [rip + 0x103399]
              ...
         7. SliderFloat "FOV"   guven=88%   arg float: min=1.000 max=180.000
         9. Combo       "Hitbox" guven=85%  ogeler: Head|Neck|Chest
```

## Mimari

```
include/umvp/          SDK başlıkları
  types.h              veri modeli (AnalysisReport/FormInfo/WidgetInfo)
  process.h            IProcessReader + WinProcess (Toolhelp+RPM)
  memory_map.h         bellek anlık görüntüsü + pattern/string taraması
  imgui_hash.h         ImHashStr portu (3 dönem × birebir doğrulanmış)
  layout.h             layout DB çalışma zamanı + yapı hipotezleri
  imgui_scan.h         pencere/context taraması, D3D tespiti, draw-list metni
  disasm.h             Capstone motoru, xref, .pdata, backward-slice, handler blokları
  analyzer.h           GOD STATE orkestratörü
  report.h             text + JSON + .asm çıktıları
src/                   uygulamalar (+ imgui_layout_db.h ÜRETİLMİŞ)
tools/gen_layout/      ImGui SDK derin taraması: offsetof DB üreteci (python)
cli/main.cpp           umvp_cli
tests/                 bağımsız + entegrasyon testleri, test hedefi (gerçek ImGui menü)
```

## Testler (bağımsız + entegrasyon)

| Test | Kapsam | Sonuç |
|---|---|---|
| `test-hash` | SDK hash portu vs GERÇEK imgui ImHashStr — 7 versiyon × 232 vaka | 7/7 PASS |
| `test-layout` | layout DB vs native offsetof doğrulamaları | PASS |
| `test-integration` | Wine+x64: d3d11+1.93, d3d9+1.93, d3d11+1.89.9 canlı süreçler | 3/3 PASS |

Entegrasyon testi gerçek bir ImGui menüsü olan EXE'yi (Form1/Form2/Form3, 23 widget)
başlatır, çözümleyiciyi çalıştırır ve JSON raporu otomatik doğrular:
form kümesi, widget etiketleri (≥%70), handler blokları, Button boyutları, slider aralıkları.

## Sınırlamalar / yol haritası

- Widget konumu: draw-list metin eşleşmesiyle kestirim; `g.LastItemData` ile son widget
  için kesin. (Layout simülasyonu yol haritasında.)
- MSVC-derlenmiş hedeflerde thunk/pdata kalıpları MinGW'dekinden farklılaşabilir;
  sezgisel fonksiyon sınırı yedek olarak devrede.
- 32-bit hedef desteği derlenir (umvp_cli_x86.exe, PE32) — bu sandbox yalnızca x64
  çalıştırabildiğinden canlı 32-bit test yapılmadı (yapı hipotezleri x86 tablolarıyla
  üretilmiş ve unit-test edilmiştir).
- SSE4.2 donanım-CRC ile derlenmiş imgui'de (tablo .rdata'da olmayabilir) CRC tablo
  tespiti devre dışı kalır; pencere taraması hash-probingle yine de çalışır.

## Lisans

MIT. Bağımlılıklar: Capstone (BSD-3), Dear ImGui (MIT, yalnız test hedeflerinde).
