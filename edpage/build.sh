#!/bin/bash
# edsave/build.sh - what does the EverDrive PRO keep? (run from the EverDrive menu, 32X attached, no CD)
#   bash edpage/build.sh   -> edpage/EDPAGE.32X  (no disc, no save RAM: run it from the EverDrive menu)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -n "${SEGA_TOOLCHAIN:-}" ]; then ROOTDIR="$SEGA_TOOLCHAIN"; fi
ROOTDIR="${ROOTDIR:-/opt/toolchains/sega}"
make -C "$HERE" clean > /dev/null
make -C "$HERE" ROOTDIR="$ROOTDIR" > /dev/null
cp "$HERE/edpage.32x" "$HERE/EDPAGE.32X"
make -C "$HERE" clean > /dev/null
ls -l "$HERE"/EDPAGE.32X
