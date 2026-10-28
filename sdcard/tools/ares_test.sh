#!/bin/bash
# sdcard/tools/ares_test.sh - the demo's three ares runs (README_AI.md section 6; docs/13 for ares-headless)
#   bash sdcard/tools/ares_test.sh <ares-headless> <us BIOS> <dir with the demo's disc + launcher> <run dir> [--interpreter]
# 1. launch from the launcher (EverDrive board): BOOTS 1, SD BANK YES, the record in bank 31
# 2. relaunch with banks 0-30 changed as the EverDrive menu wipes them, bank 31 kept: BOOTS 2, LD 0;
#    START pressed at frame 1200 saves again
# 3. disc boot (no launcher, the RAM cart): SD BANK NO, LD 1 SV 1, no hang
# Writes <run dir>/sheet.png (the four pictures: send it to the owner) and prints the records found.
# With the RV probe build and ARES_RV_GARBAGE=1 in the environment, add --interpreter: the last line
# counts RV-CART lines (must be 0).  ares is a filter, not a pass: the console has the last word.
set -e
[ $# -ge 4 ] || { echo "usage: bash sdcard/tools/ares_test.sh <ares-headless> <us BIOS> <demo dir> <run dir> [--interpreter]"; exit 1; }
B="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"; BIOS="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
D="$3"; mkdir -p "$4"; R="$(cd "$4" && pwd)"; X="${5:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$R/l1" "$R/l2" "$R/d"
cd "$D"
"$B" $X --game SDSAVE_DEMO.cue --machine cd32x --bios-us "$BIOS" --ramcart-type everdrive \
  --ramcart-boot EDCD_SDSAVEDEMO.32X --ramcart "$R/l1/ed.ram" --home "$R/l1/home" --frames 1500 \
  --screenshot 1450:"$R/l1/a.ppm" > "$R/l1/out.txt" 2>&1
python3 - "$R/l1/ed.ram" "$R/l2/ed.ram" <<'EOF'
import sys
d = bytearray(open(sys.argv[1], 'rb').read())
for i in range(0, 31 * 0x80000, 4096):          # banks 0-30: what the EverDrive menu wipes
    d[i:i + 4096] = (b'\xff' if (i >> 12) & 1 else b'\x00') * 4096
open(sys.argv[2], 'wb').write(d)
EOF
"$B" $X --game SDSAVE_DEMO.cue --machine cd32x --bios-us "$BIOS" --ramcart-type everdrive \
  --ramcart-boot EDCD_SDSAVEDEMO.32X --ramcart "$R/l2/ed.ram" --home "$R/l2/home" --frames 1500 \
  --press 1200:S --press 1220: --screenshot 1150:"$R/l2/a.ppm" --screenshot 1490:"$R/l2/b.ppm" > "$R/l2/out.txt" 2>&1
"$B" $X --game SDSAVE_DEMO.cue --machine cd32x --bios-us "$BIOS" --ramcart "$R/d/cart.ram" --home "$R/d/home" \
  --frames 2700 --screenshot 2690:"$R/d/a.ppm" > "$R/d/out.txt" 2>&1
python3 - "$R" <<'EOF'
import sys
from PIL import Image
R = sys.argv[1]
fs = [R + '/l1/a.ppm', R + '/l2/a.ppm', R + '/l2/b.ppm', R + '/d/a.ppm']
sheet = Image.new('RGB', (1280, 600))
for i, f in enumerate(fs):
    sheet.paste(Image.open(f).resize((640, 300)), ((i % 2) * 640, (i // 2) * 300))
sheet.save(R + '/sheet.png')
EOF
for f in l1 l2; do echo "launch ${f#l}:"; python3 "$HERE/srm_dump.py" --ares "$R/$f/ed.ram" | grep -E 'record'; done
echo "RV-CART lines: $(cat "$R"/*/out.txt | grep -c RV-CART || true)"
echo "contact sheet: $R/sheet.png"
