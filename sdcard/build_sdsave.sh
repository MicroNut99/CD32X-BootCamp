#!/bin/bash
# sdcard/build_sdsave.sh - the two Sub-CPU programs and the launcher for SD-card saves (README.md)
#   bash sdcard/build_sdsave.sh <out dir> <NAME>
#     <out dir>/CDROMPlayer_SDSAVE.bin   the disc's Sub-CPU program: has the save commands, answers "no bank"
#     <out dir>/app_mode1_SDSAVE.bin     the launcher's Sub-CPU program: MODE1=1 M1SAVE=1 SDSAVE=1
#     <out dir>/EDCD_<NAME>.32X          the launcher, header with save RAM (EDCD_SAVE=1): the EverDrive
#                                        keeps its bank 31 as MEGA/gamedata/EDCD_<NAME>/bram.srm
# All of it is built in a temporary COPY of the BootCamp; stack/ is not touched (check_boot.py, before
# and after).  ROOTDIR= / SEGA_TOOLCHAIN= as for build.sh.  M1WORDS=1 M1W0BANK=<n> pass through.
set -e
[ $# = 2 ] || { echo "usage: bash sdcard/build_sdsave.sh <out dir> <NAME>   (NAME: the launcher's unique tag)"; exit 1; }
BC="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$1"; OUT="$(cd "$1" && pwd)"; NAME="$2"
case "$NAME" in *[!A-Za-z0-9_]*|"") echo "*** NAME: letters, digits and _ only"; exit 1;; esac
if [ -n "${SEGA_TOOLCHAIN:-}" ]; then ROOTDIR="$SEGA_TOOLCHAIN"; fi
ROOTDIR="${ROOTDIR:-/opt/toolchains/sega}"
python3 "$BC/tools/check_boot.py" > /dev/null || { echo "*** the BootCamp's stack does not pass check_boot.py"; exit 1; }
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
cp -a "$BC/stack" "$BC/tools" "$BC/edboot" "$BC/edtest" "$T/"   # edboot uses ../edtest/src/crt0.s
echo "==> 1. the save commands into a copy of the stack"
python3 "$BC/sdcard/subcpu/merge_sdsave.py" "$T" "$BC"
S="$T/stack/subcpu"
echo "==> 2. the disc's Sub-CPU program (SDSAVE=1)"
( cd "$S" && make clean > /dev/null && make CDROMPlayer.bin ROOTDIR="$ROOTDIR" SDSAVE=1 > "$T/disc.log" 2>&1 ) \
  || { tail -20 "$T/disc.log"; echo "*** the disc's program did not build"; exit 1; }
cp "$S/CDROMPlayer.bin" "$OUT/CDROMPlayer_SDSAVE.bin"
echo "==> 3. the launcher's Sub-CPU program (MODE1=1 M1SAVE=1 SDSAVE=1)"
( cd "$S" && make clean > /dev/null && make CDROMPlayer.bin ROOTDIR="$ROOTDIR" MODE1=1 M1CTRL0=0x8000 M1SAVE=1 SDSAVE=1 \
    ${M1WORDS:+M1WORDS=$M1WORDS} ${M1W0BANK:+M1W0BANK=$M1W0BANK} > "$T/mode1.log" 2>&1 ) \
  || { tail -20 "$T/mode1.log"; echo "*** the launcher's program did not build"; exit 1; }
cp "$S/CDROMPlayer.bin" "$OUT/app_mode1_SDSAVE.bin"
echo "==> 4. EDCD_$NAME.32X (EDCD_SAVE=1)"
EDCD_SAVE=1 ROOTDIR="$ROOTDIR" bash "$T/edboot/build_edcd.sh" "$OUT/app_mode1_SDSAVE.bin" "$BC/stack/sh2/CDLOADER.32x" \
  "$OUT/EDCD_$NAME.32X"
python3 "$BC/tools/check_boot.py" > /dev/null || { echo "*** the BootCamp's stack changed during the build"; exit 1; }
( cd "$OUT" && md5sum CDROMPlayer_SDSAVE.bin app_mode1_SDSAVE.bin "EDCD_$NAME.32X" )
echo "==> done: the stack is untouched.  The game's SH2 code: sdcard/sh2/sdsave.c (README.md)"
