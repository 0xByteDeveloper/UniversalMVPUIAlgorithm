# research_pipeline — TotallyResearch.md Otonom Hasat Makinesi

`TotallyResearch.md`'yi 1.000.000+ projeye büyüten hasat hattı. `harvest_v2.py`
GitHub Search API'yi **cheat-sahası-öncelikli** tarar; `generate_totallyresearch.py`
korpusu kategorize edip MD'yi yeniden üretir; `push_repos.sh` iki repoya push eder.

## Gereksinimler

- python3 + `requests` (`pip install requests`)
- git (push için)
- Token: `research_pipeline/.ghtoken` dosyası (satır başı boşluksuz token) veya `GH_TOKEN` env.
  (İnce taneli PAT, Contents: Read and write izniyle)

## Kullanım

```bash
# 1) hasat (9 dk'lık tur; durum kayıtlı, istediğin kadar tekrarla — kaldığı yerden devam eder)
bash run_cycle.sh harvest

# 2) MD yeniden üret + iki repoya push (değişiklik varsa)
bash run_cycle.sh publish

# 3) hepsi tek seferde (hasat + üret + push, ~12 dk)
bash run_cycle.sh full
```

24/7 çalıştırma örneği (VPS / cron):

```bash
# her 15 dakikada bir tur (GitHub arama limiti 30 istek/dk; makine 28/dk'da çalışır)
*/15 * * * * cd /yol/research_pipeline && bash run_cycle.sh full >> harvest.log 2>&1
```

## Makinenin çalışma prensibi

| Katman | Açıklama |
|---|---|
| Sorgu bankası | ~3.060 statik sorgu: cheat doğrudan → ünlü base'ler (neverlose/onetap/hvh/dma...) → loader/keyauth/hwid/lisans → topic'ler → **kod-uzayı araması** (`"imgui.h" aimbot`, `"ImGui::Begin" keyauth`...) → oyun×terim matrisi (94 oyun × 25 terim) → imgui dil/yıl dilimleri → geniş evren (cheat/hack) |
| Rekürsif dilimleme | `total_count > 950` olan her repo sorgusu otomatik yıl → çeyrek → ay → gün parçası dilimlerine bölünür; GitHub'ın 1.000-sonuç sayfa sınırı böyle kırılır |
| Global dedup | `harvest_state.json` tek gerçek kaynak; aynı repo bir daha asla eklenmez |
| Durum | Her sorgu kaydedilir (`done`, `pages_done`); makine her yeniden başlatmada kaldığı yerden devam eder |

## Ölçülmüş evrenler (2026-11-27)

| Anahtar | Evren (fork hariç) | Fork dahil |
|---|---:|---:|
| `hack` | 1.026.334 | 2.023.881 |
| `cheat` | 123.984 | 402.461 |
| `imgui` | 12.049 | 46.087 |
| `mod menu` | 13.325 | — |
| `aimbot` | 11.384 | 25.263 |

**1M+ matematiksel olarak garanti:** `hack`+`cheat`+`mod menu`+`aimbot`+`keyauth`+`imgui fork:true`
evrenlerinin birleşimi ~1,3M (fork hariç) / ~2,5M+ (fork dahil).

## Dosyalar

- `harvest_v2.py` — hasatçı (banka + dilimleme + dedup)
- `generate_totallyresearch.py` — MD üretici (kategoriler, oyun oyun cheat sahaları, cheat-sheet filtresi)
- `run_cycle.sh` — tur koordinatörü (harvest | publish | full)
- `push_repos.sh` — git push (token credential-helper üzerinden; config'e yazılmaz)

> Not: `harvest_state.json` büyüdükçe (500K+ repo) SQLite'a geçiş planlanmıştır.
