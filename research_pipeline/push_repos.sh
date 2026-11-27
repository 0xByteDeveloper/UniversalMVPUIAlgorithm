#!/bin/bash
# TotallyResearch.md + DEV_MISSION_GOD_PIPELINE.md'i iki repoya git uzerinden push et.
# Clone/fetch: repo herkese acik (auth gerekmez). Push: credential helper ile token
# (token hicbir dosyaya / git config'e yazilmaz, sadece proses bellekte).
set -u
TOK=$(cat /home/z/my-project/scripts/.ghtoken)
OWNER="0xByteDeveloper"
CLONES="/home/z/my-project/scripts/clones"
DL="/home/z/my-project/download"

for REPO in MVP_GOD_SSS_TIER_IMGUI_UI_EDITOR UniversalMVPUIAlgorithm; do
  D="$CLONES/$REPO"
  if [ ! -d "$D/.git" ]; then
    git clone -q "https://github.com/$OWNER/$REPO.git" "$D" 2>/dev/null || { echo "$REPO: CLONE HATA"; continue; }
  fi
  cd "$D" || continue
  BR=$(git rev-parse --abbrev-ref HEAD)
  if ! git fetch -q origin "$BR" 2>/dev/null; then echo "$REPO: FETCH HATA"; continue; fi
  git reset -q --hard "origin/$BR" 2>/dev/null
  ADDED=0
  # FAZ 2.17: ciltler de gitsin — 100K satir uzerinde MD ciltlere bolunuyor.
  # NOT: glob DL (download/) tarafinda acilir — clone icinde Vol dosyasi henuz yok.
  for F in $(cd "$DL" && ls TotallyResearch.md TotallyResearch_Vol*.md DEV_MISSION_GOD_PIPELINE.md 2>/dev/null); do
    if [ ! -f "$DL/$F" ]; then continue; fi
    if cmp -s "$DL/$F" "$D/$F"; then continue; fi
    cp "$DL/$F" "$D/$F"
    git add "$F"
    ADDED=1
  done
  if [ "$ADDED" = "0" ]; then
    echo "$REPO: degisiklik yok"
    continue
  fi
  N=$(grep -c '^- \[' "$DL/TotallyResearch.md" 2>/dev/null || echo 0)
  T=$(date +"%Y-%m-%d %H:%M")
  if git -c user.name="0xByteDeveloper" -c user.email="0xByteDeveloper@users.noreply.github.com" \
      commit -q -m "TotallyResearch v3 oto-hasat: $N satir — $T (cheat-sahasi oncelikli genisleme)"; then
    HELPER="!f() { echo username=x-access-token; echo password=$TOK; }; f"
    if git -c "credential.helper=$HELPER" push -q origin "HEAD:$BR" 2>/dev/null; then
      echo "$REPO: PUSH OK ($N satir)"
    else
      echo "$REPO: PUSH HATA"
    fi
  else
    echo "$REPO: COMMIT HATA"
  fi
done
