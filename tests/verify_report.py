#!/usr/bin/env python3
"""
verify_report.py — UMVP rapor doğrulayıcı (entegrasyon testlerinin kalbi)
Kullanım: verify_report.py <report.json> <formlar ':'-ayraçlı> <widget'lar ':'-ayraçlı (gruplar da ':' ile)>
Başarı ölçütleri:
  * JSON geçerli + imgui.found
  * tüm beklenen formlar bulundu (canlı VEYA kod)
  * beklenen widget etiketlerinin >= %70'i bulundu
  * en az bir handler kod bloğu çıkarıldı
  * en az bir Button size_arg kurtarıldı (120x28 / 200x30)
"""
import json, sys

def main():
    path, forms_s, widgets_s = sys.argv[1], sys.argv[2], sys.argv[3]
    d = json.load(open(path))
    errors = []

    if not d.get("imgui", {}).get("found"):
        errors.append("imgui bulunamadi")
    if not d.get("form_count"):
        errors.append("form sayisi 0")

    found_forms = {f["name"] for f in d.get("forms", [])}
    for want in forms_s.split(":"):
        if want not in found_forms:
            errors.append(f"form eksik: {want}")

    all_labels = set()
    handlers = 0
    button_sizes = 0
    sliders = 0
    for f in d.get("forms", []):
        for w in f.get("widgets", []):
            all_labels.add(w["label"])
            if w.get("handler", {}).get("start"):
                handlers += 1
            if w["type"] == "Button" and w.get("size_arg"):
                button_sizes += 1
            if w["type"] == "SliderFloat" and w.get("float_args"):
                sliders += 1

    want_labels = [x for x in widgets_s.split(":") if x]
    hit = sum(1 for x in want_labels if x in all_labels)
    ratio = hit / max(1, len(want_labels))
    if ratio < 0.7:
        missing = [x for x in want_labels if x not in all_labels]
        errors.append(f"widget orani %{ratio*100:.0f} (%d/%d) — eksik: {missing[:6]}")
    if handlers < 1:
        errors.append("handler kod blogu cikarilamadi")
    if button_sizes < 1:
        errors.append("Button size kurtarilamadi")
    if sliders < 1:
        errors.append("SliderFloat araliklari kurtarilamadi")

    print(f"  imgui={d['imgui'].get('version','?')} (confirmed={d['imgui']['version_confirmed']}) | "
          f"form={d['form_count']} | widget={d['total_widgets']} | handler={handlers} | "
          f"btn_size={button_sizes} | slider={sliders} | label_orani=%{ratio*100:.0f}")
    if errors:
        for e in errors:
            print(f"  HATA: {e}")
        return 1
    print("  DOGRULAMA OK")
    return 0

if __name__ == "__main__":
    sys.exit(main())
