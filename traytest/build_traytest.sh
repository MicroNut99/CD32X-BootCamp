#!/bin/bash
# traytest/build_traytest.sh - the tray test disc (traytest/README.md)
#   bash traytest/build_traytest.sh <out dir> [NAME]
#     <out dir>/<NAME>.cue + .bin     the disc (default NAME: CD32X_TRAYTEST_T2)
# Built in a temporary COPY of the BootCamp: commands 84 and 90 are merged into the copy's stack
# (traytest/merge_tray.py), the game is hello/ with traytest/main.c in it, then the copy's own build.sh
# runs with TRAYTEST=1.  Nothing in the BootCamp itself changes (check_boot.py before and after).
# The toolchain and genisoimage as for build.sh (ROOTDIR= / SEGA_TOOLCHAIN= pass through).
set -e
[ $# -ge 1 ] && [ $# -le 2 ] || { echo "usage: bash traytest/build_traytest.sh <out dir> [NAME]"; exit 1; }
BC="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$1"; OUT="$(cd "$1" && pwd)"
NAME="${2:-CD32X_TRAYTEST_T2}"
python3 "$BC/tools/check_boot.py" > /dev/null || { echo "*** the BootCamp's stack does not pass check_boot.py"; exit 1; }
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
( cd "$BC" && tar --exclude=./.git --exclude=./disc -cf - . ) | ( cd "$T" && tar -xf - )
echo "==> commands 84 / 90 into the copy's stack"
python3 "$BC/traytest/merge_tray.py" "$T" "$BC"
echo "==> the game: hello + traytest/main.c"
D="$T/traytest_game"
mkdir "$D"          # hello's sources only: no data files, no CD audio of its own (build.sh adds a test tone)
( cd "$BC/hello" && tar --exclude=./music --exclude=./cd --exclude=./build --exclude='*.32x' --exclude='*.elf' \
    --exclude='*.o' --exclude='*.bin' --exclude=output.map -cf - . ) | ( cd "$D" && tar -xf - )
cp "$BC/traytest/main.c" "$D/src/main.c"
python3 - "$D/Makefile" <<'EOF'
import sys
p = sys.argv[1]; s = open(p).read()
assert s.count("TARGET  = hello") == 1, "hello/Makefile: the TARGET line moved"
open(p, "w").write(s.replace("TARGET  = hello", "TARGET  = traytest"))
EOF
( cd "$T" && GAME=traytest_game DISC_NAME="$NAME" TRAYTEST=1 bash build.sh > "$T/build.log" 2>&1 ) \
  || { tail -30 "$T/build.log"; echo "*** the tray test did not build"; exit 1; }
# an unanswered command looks like a hang (ledger 7a): the switch must have reached main.c
grep -E "gcc .*-DTRAYTEST=1 .*main\.c" "$T/build.log" > /dev/null || { echo "*** TRAYTEST=1 did not reach main.c"; exit 1; }
cp "$T/disc/$NAME.cue" "$T/disc/$NAME.bin" "$OUT/"
python3 "$BC/tools/check_boot.py" > /dev/null || { echo "*** the BootCamp's stack changed during the build"; exit 1; }
( cd "$OUT" && md5sum "$NAME.cue" "$NAME.bin" )
echo "==> done: $OUT/$NAME.cue (with its .bin).  Sega CD + 32X + the 4 MB RAM cart, as every BootCamp disc."
