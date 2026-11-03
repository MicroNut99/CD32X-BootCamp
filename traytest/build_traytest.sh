#!/bin/bash
# traytest/build_traytest.sh - the tray test disc (traytest/README.md)
#   bash traytest/build_traytest.sh <out dir> [NAME]
#     <out dir>/<NAME>.cue + .bin     the disc (default NAME: CD32X_TRAYTEST_T3)
# T3: no cartridge (Sega CD + 32X only).  Built in a temporary COPY of the BootCamp's stack/ and tools/:
#   1. commands 84 / 90 and the no-cart switch into the copy's stack (traytest/merge_tray.py)
#   2. the test's SH2 program (main.c, sdram.c, hello's cdapi.c) built like CDLOADER, with the copy's
#      stack/sh2/cdl_crt0.s and cdloader.ld (unchanged), and put in CDLOADER's place: it runs from SDRAM
#   3. the copy's Sub-CPU program and disc image with TRAYTEST=1 TRAY_NOCART=1 (as build.sh step 5)
#   4. the test tone as track 2, the burnable image (as build.sh step 6)
# Nothing in the BootCamp itself changes (check_boot.py before and after).
# The toolchain and genisoimage as for build.sh: ROOTDIR= / SEGA_TOOLCHAIN=; REGION=US|EU|JP (default US).
set -e
[ $# -ge 1 ] && [ $# -le 2 ] || { echo "usage: bash traytest/build_traytest.sh <out dir> [NAME]"; exit 1; }
BC="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$1"; OUT="$(cd "$1" && pwd)"
NAME="${2:-CD32X_TRAYTEST_T3}"
if [ -n "${SEGA_TOOLCHAIN:-}" ]; then ROOTDIR="$SEGA_TOOLCHAIN"; export PATH="$PATH:$SEGA_TOOLCHAIN/usr/bin"; fi
ROOTDIR="${ROOTDIR:-/opt/toolchains/sega}"
[ -x "$ROOTDIR/sh-elf/bin/sh-elf-gcc" ] || { echo "*** no toolchain at $ROOTDIR (set ROOTDIR= or SEGA_TOOLCHAIN=)"; exit 1; }
command -v genisoimage >/dev/null || { echo "*** genisoimage missing (sudo apt install genisoimage)"; exit 1; }
case "${REGION:-US}" in US|EU|JP) ;; *) echo "*** REGION must be US, EU or JP"; exit 1;; esac
python3 "$BC/tools/check_boot.py" > /dev/null || { echo "*** the BootCamp's stack does not pass check_boot.py"; exit 1; }
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
( cd "$BC" && tar --exclude=__pycache__ -cf - stack tools ) | ( cd "$T" && tar -xf - )

echo "==> 1. commands 84 / 90 and the no-cart switch into the copy's stack"
python3 "$BC/traytest/merge_tray.py" "$T" "$BC"

echo "==> 2. the test program, for SDRAM, in CDLOADER's place"
SH="$ROOTDIR/sh-elf/bin/sh-elf-"
S2="$T/stack/sh2"; B="$T/traytest_sh2"; mkdir "$B"
CF="-c -m2 -mb -O2 -Wall -D__32X__ -DMARS"
"${SH}gcc" $CF -std=c11 -ffunction-sections -fdata-sections "$S2/cdl_crt0.s" -o "$B/cdl_crt0.o"   # as Makefile.cdloader
# main.c's main() becomes tray_main(): sdram.c's main() (the start-up's _main) does the handshake first
"${SH}gcc" $CF -std=gnu99 -fomit-frame-pointer -I"$BC/hello/src" -Dmain=tray_main "$BC/traytest/main.c" -o "$B/main.o"
for f in "$BC/traytest/sdram.c" "$BC/hello/src/cdapi.c"; do
  "${SH}gcc" $CF -std=gnu99 -fomit-frame-pointer -I"$BC/hello/src" "$f" -o "$B/$(basename "${f%.c}").o"
done
LIBGCC="$("${SH}gcc" -m2 -mb -print-libgcc-file-name)"           # the divisions in main.c
"${SH}ld" -EB -T "$S2/cdloader.ld" -nostdlib -Map="$B/traytest.map" \
  "$B/cdl_crt0.o" "$B/main.o" "$B/sdram.o" "$B/cdapi.o" "$LIBGCC" -o "$B/traytest.elf"
"${SH}objcopy" -O binary "$B/traytest.elf" "$S2/CDLOADER.32x"
"${SH}nm" "$B/traytest.elf" | grep -q " T _tray_main$" && "${SH}nm" "$B/traytest.elf" | grep -q " T _main$" \
  || { echo "*** the SH2 program has no main() / tray_main()"; exit 1; }
[ "$(head -c 16 "$S2/CDLOADER.32x")" = "CD BOOT TABLE   " ] || { echo "*** the SH2 program does not start with the CD boot table"; exit 1; }
END=$("${SH}nm" "$B/traytest.elf" | sed -n 's/^\([0-9a-fA-F]*\) . __bss_end$/\1/p')
[ -n "$END" ] && [ $((0x$END)) -lt $((0x0603C000)) ] || { echo "*** the SH2 program reaches the stacks (end 0x$END)"; exit 1; }
echo "    $(stat -c %s "$S2/CDLOADER.32x") bytes, SDRAM 0x06000000-0x$END"

echo "==> 3. the Sub-CPU program and the disc image (TRAYTEST=1 TRAY_NOCART=1)"
( cd "$T/stack/subcpu" && rm -rf ROMS && make clean > /dev/null && make cd ROOTDIR="$ROOTDIR" \
    BOOTBLOCK="$ROOTDIR/bootblocks/${REGION:-US}_BOOT.BIN" TRAYTEST=1 TRAY_NOCART=1 > "$T/make.log" 2>&1 ) \
  || { tail -30 "$T/make.log"; echo "*** the tray test did not build"; exit 1; }
# an unanswered command looks like a hang (ledger 7a): both switches must have reached main.c
grep -E "gcc .*-DTRAYTEST=1 .*-DTRAY_NOCART=1 .*main\.c" "$T/make.log" > /dev/null || { echo "*** TRAYTEST=1 TRAY_NOCART=1 did not reach main.c"; exit 1; }
ISO="$T/stack/subcpu/CDROMPlayer.iso"
grep -a -q "SH2-TRAYTEST-T3-SDRAM" "$ISO" || { echo "*** the test program is not on the disc"; exit 1; }
! grep -a -q "SH2-OPENLARA-CD-OL1-LOADER" "$ISO" || { echo "*** CDLOADER is still on the disc"; exit 1; }
grep -a -q "TRAY-NOCART: NO START-UP CART CHECK" "$ISO" || { echo "*** the start-up cart check is still in the Sub-CPU program"; exit 1; }

echo "==> 4. the test tone (track 2) and the burnable image"
python3 "$BC/tools/make_test_track.py" "$T/Track02.wav"
# a CD track must be at least 4 s (300 sectors): pad the small data track with empty sectors (as build.sh)
python3 -c "import os,sys; p=sys.argv[1]; n=os.path.getsize(p)//2048; open(p,'ab').write(bytes(2048*(600-n))) if n<600 else None" "$ISO"
python3 "$T/stack/subcpu/make_mixed_cd2.py" "$ISO" "$T/Track02.wav" --out "$OUT/$NAME"
python3 "$BC/tools/check_boot.py" > /dev/null || { echo "*** the BootCamp's stack changed during the build"; exit 1; }
( cd "$OUT" && md5sum "$NAME.cue" "$NAME.bin" )
echo "==> done: $OUT/$NAME.cue (with its .bin).  Sega CD + 32X, no cartridge."
