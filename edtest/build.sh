#!/bin/bash
# edtest/build.sh - the two EverDrive PRO RAM test ROMs (run from the EverDrive menu, 32X attached, no CD)
#   bash edtest/build.sh                 -> edtest/EDTEST_X.32X (CTRL0 0xE000) + edtest/EDTEST_W.32X (0xA000)
#   ROOTDIR= / SEGA_TOOLCHAIN=           the toolchain, as for the main build.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -n "${SEGA_TOOLCHAIN:-}" ]; then ROOTDIR="$SEGA_TOOLCHAIN"; fi
ROOTDIR="${ROOTDIR:-/opt/toolchains/sega}"
[ -x "$ROOTDIR/sh-elf/bin/sh-elf-gcc" ] || { echo "*** no toolchain at $ROOTDIR (set ROOTDIR= or SEGA_TOOLCHAIN=)"; exit 1; }
for v in X:0xE000 W:0xA000; do
  make -C "$HERE" clean > /dev/null
  make -C "$HERE" ROOTDIR="$ROOTDIR" ED_CTRL0="${v#*:}"
  cp "$HERE/edtest.32x" "$HERE/EDTEST_${v%%:*}.32X"
done
make -C "$HERE" clean > /dev/null
ls -l "$HERE"/EDTEST_*.32X
echo "==> copy both to the EverDrive's SD card; run each with the 32X attached (docs/10_EVERDRIVE_TEST.md)"
