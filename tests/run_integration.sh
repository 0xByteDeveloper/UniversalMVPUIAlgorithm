#!/bin/bash
# =============================================================================
# run_integration.sh — UMVP ENTEGRASYON TESTLERİ (Wine altında, x64)
# Hedefler: d3d11+imgui-master, d3d9+imgui-master, d3d11+imgui-1.89.9
# =============================================================================
set -u
cd "$(dirname "$0")/.."
ROOT=$(pwd)
WINE=$HOME/.local/bin/wine64
export WINEPREFIX=$HOME/.wine64 WINEDEBUG=-all DISPLAY=:99
PASS=0; FAIL=0

# Xvfb kontrolü
if ! xdpyinfo >/dev/null 2>&1; then
    mkdir -p /tmp/.X11-unix && chmod 1777 /tmp/.X11-unix
    (Xvfb :99 -screen 0 1600x1000x24 >/tmp/xvfb.log 2>&1 &)
    sleep 1.5
fi

run_case() {
    local name="$1" exe="$2" expect_forms="$3" expect_widgets="$4"
    echo ""
    echo "==================================================================="
    echo " TEST: $name"
    echo "==================================================================="
    cp "build/$exe" /tmp/umvptest/ 2>/dev/null || { echo "FAIL: $exe yok"; FAIL=$((FAIL+1)); return; }
    # wine ile başlat
    (cd /tmp/umvptest && rm -f umvp_target.log && $WINE "$exe" > target_out.log 2>&1 &)
    sleep 8
    local pid=$($WINE build/umvp_cli_x64.exe --list 2>/dev/null | tr -d '\r' | grep "$exe" | awk '{print $1}' | head -1)
    if [ -z "$pid" ]; then echo "FAIL: surec bulunamadi"; FAIL=$((FAIL+1)); return; fi
    echo "PID=$pid"
    cd /tmp/umvptest
    timeout 240 $WINE "$ROOT/build/umvp_cli_x64.exe" --pid "$pid" --txt "report_$name.txt" --json "report_$name.json" --asm-dir "asm_$name" >/dev/null 2>&1
    cd "$ROOT"
    # dogrulama
    python3 tests/verify_report.py "/tmp/umvptest/report_$name.json" "$expect_forms" "$expect_widgets"
    if [ $? -eq 0 ]; then echo ">>> $name: PASS"; PASS=$((PASS+1)); else echo ">>> $name: FAIL"; FAIL=$((FAIL+1)); fi
    # hedefi kapat
    $WINE taskkill /IM "$exe" /F >/dev/null 2>&1 || true
    sleep 2
}

echo "Bina: analyzer + hedefler hazir mi?"
ls build/umvp_cli_x64.exe build/target_*.exe >/dev/null 2>&1 || { echo "once: make cli64 targets"; exit 1; }
mkdir -p /tmp/umvptest

# Beklenen widget'lar (label listeleri)
W1="Aimbot Enabled:FOV:Smoothing:Hitbox:Player Color:Config Name:Save Config:Load Config:Panic:Radar"
W2="ESP:Box ESP:Health Bar:ESP Alpha:Unload"
W3="Info:Tools:Refresh:Apply Settings:child icerik"

run_case "dx11_master" "target_dx11_master.exe" "XXGOD MVP | Aimbot:Visuals:Misc" "$W1:$W2:$W3"
run_case "dx9_master"  "target_dx9_master.exe"  "XXGOD MVP | Aimbot:Visuals:Misc" "$W1:$W2:$W3"
run_case "dx11_1899"   "target_dx11_1899.exe"   "XXGOD MVP | Aimbot:Visuals:Misc" "$W1:$W2:$W3"

echo ""
echo "==================================================================="
echo " ENTEGRASYON SONUC: $PASS PASS / $FAIL FAIL"
echo "==================================================================="
[ $FAIL -eq 0 ] || exit 1
