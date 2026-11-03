#!/bin/bash
# traytest/build_traytest.sh - the tray test (traytest/README.md)
#   bash traytest/build_traytest.sh <out dir> [NAME]
#     <out dir>/<NAME>.cue + .bin     the disc: Sega CD + 32X, no cartridge needed (T3)
#     <out dir>/<NAME>.32X            the same test as a 32X cartridge ROM for the Mega EverDrive PRO: no disc
#                                     needed (T5; the BootCamp's EDCD launcher, edboot/, docs/12)
#   (default NAME: CD32X_TRAYTEST_T5)
# Built in a temporary COPY of the BootCamp's stack/, tools/, edboot/ and edtest/src/:
#   1. commands 84 / 90 and the no-cart switch into the copy's stack (traytest/merge_tray.py)
#   2. the test's SH2 program (main.c, sdram.c, hello's cdapi.c) built like CDLOADER, with the copy's
#      stack/sh2/cdl_crt0.s and cdloader.ld (unchanged), and put in CDLOADER's place: it runs from SDRAM
#   3. the copy's Sub-CPU program and disc image with TRAYTEST=1 TRAY_NOCART=1 (as build.sh step 5)
#   4. the test tone as track 2, the burnable image (as build.sh step 6)
#   5. the cartridge ROM: the SH2 program again (TRAY_FROM_CART=1: the screen says CART), the Sub-CPU program
#      with MODE1=1 as well (as build.sh EDCD=1 step 7) and TRAY_NODISC=1 (no start-up disc read: the tray may
#      hold a music CD or nothing), and the copy's edboot/build_edcd.sh around them
# Nothing in the BootCamp itself changes (check_boot.py before and after).
# The toolchain and genisoimage as for build.sh: ROOTDIR= / SEGA_TOOLCHAIN=; REGION=US|EU|JP (default US).
set -e
[ $# -ge 1 ] && [ $# -le 2 ] || { echo "usage: bash traytest/build_traytest.sh <out dir> [NAME]"; exit 1; }
BC="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$1"; OUT="$(cd "$1" && pwd)"
NAME="${2:-CD32X_TRAYTEST_T5}"
if [ -n "${SEGA_TOOLCHAIN:-}" ]; then ROOTDIR="$SEGA_TOOLCHAIN"; export PATH="$PATH:$SEGA_TOOLCHAIN/usr/bin"; fi
ROOTDIR="${ROOTDIR:-/opt/toolchains/sega}"
[ -x "$ROOTDIR/sh-elf/bin/sh-elf-gcc" ] || { echo "*** no toolchain at $ROOTDIR (set ROOTDIR= or SEGA_TOOLCHAIN=)"; exit 1; }
command -v genisoimage >/dev/null || { echo "*** genisoimage missing (sudo apt install genisoimage)"; exit 1; }
case "${REGION:-US}" in US|EU|JP) ;; *) echo "*** REGION must be US, EU or JP"; exit 1;; esac
python3 "$BC/tools/check_boot.py" > /dev/null || { echo "*** the BootCamp's stack does not pass check_boot.py"; exit 1; }
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
( cd "$BC" && tar --exclude=__pycache__ --exclude=edboot/build -cf - stack tools edboot edtest/src ) | ( cd "$T" && tar -xf - )

echo "==> 1. commands 84 / 90 and the no-cart switch into the copy's stack"
python3 "$BC/traytest/merge_tray.py" "$T" "$BC"

SH="$ROOTDIR/sh-elf/bin/sh-elf-"
S2="$T/stack/sh2"
# the test program for SDRAM, in CDLOADER's place (the copy's stack/sh2/CDLOADER.32x); $1 = TRAY_FROM_CART
sh2_build() {
  local B="$T/traytest_sh2_$1"; mkdir "$B"
  local CF="-c -m2 -mb -O2 -Wall -D__32X__ -DMARS"
  "${SH}gcc" $CF -std=c11 -ffunction-sections -fdata-sections "$S2/cdl_crt0.s" -o "$B/cdl_crt0.o"   # as Makefile.cdloader
  # main.c's main() becomes tray_main(): sdram.c's main() (the start-up's _main) does the handshake first
  "${SH}gcc" $CF -std=gnu99 -fomit-frame-pointer -I"$BC/hello/src" -Dmain=tray_main -DTRAY_FROM_CART=$1 "$BC/traytest/main.c" -o "$B/main.o"
  for f in "$BC/traytest/sdram.c" "$BC/hello/src/cdapi.c"; do
    "${SH}gcc" $CF -std=gnu99 -fomit-frame-pointer -I"$BC/hello/src" "$f" -o "$B/$(basename "${f%.c}").o"
  done
  local LIBGCC; LIBGCC="$("${SH}gcc" -m2 -mb -print-libgcc-file-name)"           # the divisions in main.c
  "${SH}ld" -EB -T "$S2/cdloader.ld" -nostdlib -Map="$B/traytest.map" \
    "$B/cdl_crt0.o" "$B/main.o" "$B/sdram.o" "$B/cdapi.o" "$LIBGCC" -o "$B/traytest.elf"
  "${SH}objcopy" -O binary "$B/traytest.elf" "$S2/CDLOADER.32x"
  "${SH}nm" "$B/traytest.elf" | grep -q " T _tray_main$" && "${SH}nm" "$B/traytest.elf" | grep -q " T _main$" \
    || { echo "*** the SH2 program has no main() / tray_main()"; exit 1; }
  [ "$(head -c 16 "$S2/CDLOADER.32x")" = "CD BOOT TABLE   " ] || { echo "*** the SH2 program does not start with the CD boot table"; exit 1; }
  local END; END=$("${SH}nm" "$B/traytest.elf" | sed -n 's/^\([0-9a-fA-F]*\) . __bss_end$/\1/p')
  [ -n "$END" ] && [ $((0x$END)) -lt $((0x0603C000)) ] || { echo "*** the SH2 program reaches the stacks (end 0x$END)"; exit 1; }
  echo "    $(stat -c %s "$S2/CDLOADER.32x") bytes, SDRAM 0x06000000-0x$END"
}
# an unanswered command looks like a hang (ledger 7a): the switches must have reached main.c
switches() {
  grep -E "gcc .*-DTRAYTEST=1 .*-DTRAY_NOCART=1 .*-DTRAY_MODE1=$2 .*main\.c" "$1" > /dev/null \
    || { echo "*** TRAYTEST=1 TRAY_NOCART=1 (MODE1 $2) did not reach main.c"; exit 1; }
}

echo "==> 2. the test program, for SDRAM, in CDLOADER's place"
sh2_build 0

echo "==> 3. the Sub-CPU program and the disc image (TRAYTEST=1 TRAY_NOCART=1)"
( cd "$T/stack/subcpu" && rm -rf ROMS && make clean > /dev/null && make cd ROOTDIR="$ROOTDIR" \
    BOOTBLOCK="$ROOTDIR/bootblocks/${REGION:-US}_BOOT.BIN" TRAYTEST=1 TRAY_NOCART=1 > "$T/make.log" 2>&1 ) \
  || { tail -30 "$T/make.log"; echo "*** the tray test did not build"; exit 1; }
switches "$T/make.log" 0
ISO="$T/stack/subcpu/CDROMPlayer.iso"
grep -a -q "SH2-TRAYTEST-SDRAM" "$ISO" || { echo "*** the test program is not on the disc"; exit 1; }
! grep -a -q "SH2-OPENLARA-CD-OL1-LOADER" "$ISO" || { echo "*** CDLOADER is still on the disc"; exit 1; }
grep -a -q "TRAY-NOCART: NO START-UP CART CHECK" "$ISO" || { echo "*** the start-up cart check is still in the Sub-CPU program"; exit 1; }

echo "==> 4. the test tone (track 2) and the burnable image"
python3 "$BC/tools/make_test_track.py" "$T/Track02.wav"
# a CD track must be at least 4 s (300 sectors): pad the small data track with empty sectors (as build.sh)
python3 -c "import os,sys; p=sys.argv[1]; n=os.path.getsize(p)//2048; open(p,'ab').write(bytes(2048*(600-n))) if n<600 else None" "$ISO"
python3 "$T/stack/subcpu/make_mixed_cd2.py" "$ISO" "$T/Track02.wav" --out "$OUT/$NAME"

echo "==> 5. the cartridge ROM for the Mega EverDrive PRO: $NAME.32X (no disc needed)"
sh2_build 1
( cd "$T/stack/subcpu" && make clean > /dev/null && make CDROMPlayer.bin ROOTDIR="$ROOTDIR" MODE1=1 M1CTRL0=0x8000 TRAY_NODISC=1 \
    TRAYTEST=1 TRAY_NOCART=1 > "$T/mode1.log" 2>&1 ) \
  || { tail -30 "$T/mode1.log"; echo "*** the cartridge-boot Sub-CPU program did not build"; exit 1; }
switches "$T/mode1.log" 1
grep -E "gcc .*-DTRAY_NODISC=1 .*main\.c" "$T/mode1.log" > /dev/null || { echo "*** TRAY_NODISC=1 did not reach main.c"; exit 1; }
ROOTDIR="$ROOTDIR" bash "$T/edboot/build_edcd.sh" "$T/stack/subcpu/CDROMPlayer.bin" "$S2/CDLOADER.32x" "$OUT/$NAME.32X" \
  > "$T/edcd.log" 2>&1 || { cat "$T/edcd.log"; echo "*** the cartridge ROM did not build"; exit 1; }
grep -a -q "SH2-TRAYTEST-SDRAM" "$OUT/$NAME.32X" || { echo "*** the test program is not in the cartridge ROM"; exit 1; }
grep -a -q "TRAY-NOCART: NO START-UP CART CHECK" "$OUT/$NAME.32X" || { echo "*** the start-up cart check is still in the cartridge ROM"; exit 1; }
grep -a -q "TRAY-NODISC: NO START-UP DISC READ" "$OUT/$NAME.32X" || { echo "*** the start-up disc read is still in the cartridge ROM"; exit 1; }
grep -a -q "SEGA SSF" "$OUT/$NAME.32X" || { echo "*** the cartridge ROM has no EverDrive header"; exit 1; }

python3 "$BC/tools/check_boot.py" > /dev/null || { echo "*** the BootCamp's stack changed during the build"; exit 1; }
( cd "$OUT" && md5sum "$NAME.cue" "$NAME.bin" "$NAME.32X" )
echo "==> done: $OUT/$NAME.cue (with its .bin): Sega CD + 32X, no cartridge needed"
echo "          $OUT/$NAME.32X: Sega CD + 32X + Mega EverDrive PRO, no disc needed"
