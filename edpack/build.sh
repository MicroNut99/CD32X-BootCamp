#!/bin/bash
# edpack/build.sh - the EverDrive PRO 14 MB file probe (run from the EverDrive menu, 32X attached, no CD)
#   bash edpack/build.sh              -> edpack/EDPACK_14MB.32X (14 MB: the probe in bank 0, a pattern in banks 1-27)
#   NONCE=0x5A5A bash edpack/build.sh    a chosen nonce (default: from the clock); it shows top right on screen
#   ROOTDIR= / SEGA_TOOLCHAIN=           the toolchain, as for the main build.sh
# What it answers: does the EverDrive OS load a .32X of 14 MB into its PSRAM whole in 32X mode?
# (MegaEverDrivePro_SDCARD.md section 4.0).  Read the screen: "LOADED 14 OF 14 MB" = yes.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -n "${SEGA_TOOLCHAIN:-}" ]; then ROOTDIR="$SEGA_TOOLCHAIN"; fi
ROOTDIR="${ROOTDIR:-/opt/toolchains/sega}"
[ -x "$ROOTDIR/sh-elf/bin/sh-elf-gcc" ] || { echo "*** no toolchain at $ROOTDIR (set ROOTDIR= or SEGA_TOOLCHAIN=)"; exit 1; }
NONCE="${NONCE:-$(printf '0x%04X' $(( $(date +%s) & 0xFFFF )))}"
[ "$NONCE" != 0x0000 ] || NONCE=0x1234
make -C "$HERE" clean > /dev/null
make -C "$HERE" ROOTDIR="$ROOTDIR" PACK_NONCE="$NONCE"
python3 "$HERE/tools/pad_markers.py" "$HERE/edpack.32x" "$NONCE" "$HERE/EDPACK_14MB.32X"
make -C "$HERE" clean > /dev/null
ls -l "$HERE/EDPACK_14MB.32X"
echo "==> copy EDPACK_14MB.32X to the EverDrive's SD card; start it from the menu with the 32X attached (no disc)."
echo "    Time the menu's load with a watch: that is the cost per launch of a packed launcher of this size."
