#!/bin/bash
# Otonom hasat dongusu — iki asama (cron ve manuel kullanım):
#   run_cycle.sh harvest  -> hasat (~9 dk, 545s)
#   run_cycle.sh publish  -> MD yeniden uret + iki repoya push
#   run_cycle.sh full     -> harvest + publish (tek cagrida, ~12 dk)
set -u
LOCK=/home/z/my-project/scripts/.harvest.lock
S=/home/z/my-project/scripts

lock() { exec 9>"$LOCK"; flock -n 9 || { echo "LOCK BUSY — oncek dongu hala calisiyor"; exit 0; }; }

case "${1:-full}" in
  harvest)
    lock
    cd "$S" && timeout 570 python3 harvest_v2.py 545 2>&1 | tail -3
    ;;
  publish)
    lock
    cd "$S" && python3 generate_totallyresearch.py 2>&1 | head -4
    bash "$S/push_repos.sh"
    ;;
  full)
    lock
    cd "$S" && timeout 570 python3 harvest_v2.py 545 2>&1 | tail -3
    python3 "$S/generate_totallyresearch.py" 2>&1 | head -4
    bash "$S/push_repos.sh"
    ;;
esac
