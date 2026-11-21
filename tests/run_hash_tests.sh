#!/bin/bash
# =============================================================================
# run_hash_tests.sh — BAĞIMSIZ TEST: umvp hash portu vs GERÇEK ImGui (7 versiyon)
# =============================================================================
set -u
cd "$(dirname "$0")/.."
DEPS="$HOME/deps"
PASS=0; FAIL=0
for v in 1.76 1.80 1.86 1.89.9 1.91.8 1.92.6 master; do
    IMG="$DEPS/imgui-$v"
    [ -d "$IMG" ] || { echo "SKIP $v (kaynak yok)"; continue; }
    SRCS="$IMG/imgui.cpp $IMG/imgui_draw.cpp $IMG/imgui_widgets.cpp"
    [ -f "$IMG/imgui_tables.cpp" ] && SRCS="$SRCS $IMG/imgui_tables.cpp"
    if g++ -std=c++17 -O1 -w -I"$IMG" -Iinclude tests/test_hash_native.cpp src/imgui_hash.cpp \
        $SRCS -o build/hashtest_$v 2>/tmp/hashtest_err_$v.txt; then
        if build/hashtest_$v >/tmp/hashtest_out_$v.txt 2>&1; then
            echo "PASS  imgui $v — $(tail -1 /tmp/hashtest_out_$v.txt)"
            PASS=$((PASS+1))
        else
            echo "FAIL  imgui $v — $(tail -1 /tmp/hashtest_out_$v.txt)"
            FAIL=$((FAIL+1))
        fi
    else
        echo "FAIL  imgui $v — derleme hatasi"
        tail -2 /tmp/hashtest_err_$v.txt
        FAIL=$((FAIL+1))
    fi
done
echo "=========================================="
echo "HASH TEST SONUC: $PASS PASS / $FAIL FAIL"
[ $FAIL -eq 0 ] || exit 1
