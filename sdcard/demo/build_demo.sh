#!/bin/bash
# sdcard/demo/build_demo.sh - the SD-card save demo: a disc and its launcher (sdcard/README.md, "The demo")
#   bash sdcard/demo/build_demo.sh <out dir>
#     <out dir>/SDSAVE_DEMO.cue + .bin   the disc (hello with its boot counter on the SD card)
#     <out dir>/EDCD_SDSAVEDEMO.32X       its launcher, save RAM in the header (EDCD_SAVE=1)
# Built in a temporary COPY of the BootCamp: the save commands are merged into the copy's stack
# (sdcard/subcpu/merge_sdsave.py), the demo folder is hello/ with sdcard/demo/main.c and
# sdcard/sh2/sdsave.c in it, then the copy's own build.sh runs with SDSAVE=1 EDCD=1 EDCD_SAVE=1.
# Nothing in the BootCamp itself changes (check_boot.py before and after).
set -e
[ $# = 1 ] || { echo "usage: bash sdcard/demo/build_demo.sh <out dir>"; exit 1; }
BC="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$1"; OUT="$(cd "$1" && pwd)"
python3 "$BC/tools/check_boot.py" > /dev/null || { echo "*** the BootCamp's stack does not pass check_boot.py"; exit 1; }
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
( cd "$BC" && tar --exclude=./.git --exclude=./disc -cf - . ) | ( cd "$T" && tar -xf - )
echo "==> the save commands into the copy's stack"
python3 "$BC/sdcard/subcpu/merge_sdsave.py" "$T" "$BC"
echo "==> the demo game: hello + sdcard/demo/main.c + sdcard/sh2/sdsave.c"
D="$T/sdsave_demo"
mkdir "$D"          # hello's sources only: no CD audio in the demo, no old build outputs
( cd "$BC/hello" && tar --exclude=./music --exclude=./build --exclude='*.32x' --exclude='*.elf' --exclude='*.o' \
    --exclude='*.bin' --exclude=output.map -cf - . ) | ( cd "$D" && tar -xf - )
cp "$BC/sdcard/demo/main.c" "$D/src/main.c"
cp "$BC/sdcard/sh2/sdsave.c" "$BC/sdcard/sh2/sdsave.h" "$D/src/"
python3 - "$D/Makefile" <<'EOF'
import sys
p = sys.argv[1]; s = open(p).read()
a = "OBJS = $(BUILD)/crt0.o $(BUILD)/main.o $(BUILD)/cdapi.o\n"
assert s.count(a) == 1, "hello/Makefile: the OBJS line moved"
s = s.replace(a, a.rstrip("\n") + " $(BUILD)/sdsave.o\n")
s = s.replace("$(BUILD)/%.o: src/%.c src/32x.h src/cdapi.h", "$(BUILD)/%.o: src/%.c src/32x.h src/cdapi.h src/sdsave.h")
s = s.replace("TARGET  = hello", "TARGET  = sdsave_demo")
open(p, "w").write(s)
EOF
( cd "$T" && GAME=sdsave_demo DISC_NAME=SDSAVE_DEMO SDSAVE=1 EDCD=1 EDCD_SAVE=1 bash build.sh > "$T/build.log" 2>&1 ) \
  || { tail -30 "$T/build.log"; echo "*** the demo did not build"; exit 1; }
cp "$T/disc/SDSAVE_DEMO.cue" "$T/disc/SDSAVE_DEMO.bin" "$OUT/"
cp "$T/edboot/EDCD.32X" "$OUT/EDCD_SDSAVEDEMO.32X"
grep -q 'save RAM declared in the header' "$T/build.log" || { echo "*** the launcher has no save RAM in its header"; exit 1; }
python3 "$BC/tools/check_boot.py" > /dev/null || { echo "*** the BootCamp's stack changed during the build"; exit 1; }
( cd "$OUT" && md5sum SDSAVE_DEMO.cue SDSAVE_DEMO.bin EDCD_SDSAVEDEMO.32X )
echo "==> done.  SD card: EDCD_SDSAVEDEMO.32X; Sega CD: the disc.  The bank: MEGA/gamedata/EDCD_SDSAVEDEMO/bram.srm"
