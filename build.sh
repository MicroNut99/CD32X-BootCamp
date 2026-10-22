#!/bin/bash
# CD32X BootCamp - build a bootable Sega CD + 32X disc for a game (the example: hello/)
#
#   bash build.sh                       hello -> disc/BootCamp.cue + .bin (data + CD audio)
#   GAME=mygame bash build.sh           your game folder, laid out like hello/ (see docs/02_PORTING_YOUR_GAME.md)
#   ROOTDIR=/opt/toolchains/sega        Chilly Willy's Sega toolchain (sh-elf, m68k-elf, ldscripts, bootblocks)
#
# A game folder holds:  Makefile (makes <game>.32x, a normal 32X cartridge ROM), splash.png (the boot
# picture), cd/ (data files for the disc, named as in stack/subcpu/game_files.h), music/TrackNN.wav
# (CD audio, NN = 02, 03, ... in order; optional).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="${GAME:-hello}"
G="$HERE/$GAME"
ROOTDIR="${ROOTDIR:-/opt/toolchains/sega}"
OUT="$HERE/disc"
ROMS="$HERE/stack/subcpu/ROMS"
NAME="${DISC_NAME:-BootCamp}"
[ -f "$G/Makefile" ] || { echo "*** $G/Makefile not found"; exit 1; }
command -v genisoimage >/dev/null || { echo "*** genisoimage missing (sudo apt install genisoimage)"; exit 1; }
python3 -c "import PIL" 2>/dev/null || { echo "*** Python Pillow missing (sudo apt install python3-pil)"; exit 1; }

echo "==> 0. the boot stack is unchanged"
python3 "$HERE/tools/check_boot.py"

echo "==> 1. the game ROM ($GAME)"
make -C "$G" ROOTDIR="$ROOTDIR"
ROM=$(ls "$G"/*.32x | head -1)
rm -rf "$ROMS"; mkdir -p "$ROMS"
python3 "$HERE/tools/prep_rom.py" "$ROM" "$ROMS/GAME.32X"

echo "==> 2. the boot picture"
if [ -f "$G/splash.png" ]; then
  python3 "$HERE/tools/make_ol_splash.py" "$G/splash.png" "$ROMS/SPLASH.tmp" > /dev/null
  python3 "$HERE/tools/make_boot_image.py" "$ROMS/SPLASH.tmp" "$ROMS/IMAGE.RAW"
  rm -f "$ROMS/SPLASH.tmp"
fi

echo "==> 3. the game's data files"
if [ -d "$G/cd" ]; then cp -v "$G"/cd/* "$ROMS/"; fi

echo "==> 4. the SH2 loader (stack/sh2, unchanged)"
make -C "$HERE/stack/sh2" -f Makefile.cdloader clean > /dev/null
make -C "$HERE/stack/sh2" -f Makefile.cdloader ROOTDIR="$ROOTDIR"

echo "==> 5. the Sub-CPU program + the disc image (stack/subcpu)"
cd "$HERE/stack/subcpu" && make clean > /dev/null && make cd ROOTDIR="$ROOTDIR"
ISO="$HERE/stack/subcpu/CDROMPlayer.iso"
grep -a -q "SH2-OPENLARA-CD-OL1-LOADER" "$ISO" || { echo "*** the disc has no CDLOADER - stopping"; exit 1; }
[ -f "$HERE/stack/subcpu/cd/GAME.32X" ] || { echo "*** GAME.32X is not on the disc - stopping"; exit 1; }

echo "==> 6. CD audio + the burnable image"
mkdir -p "$OUT"
TRACKS=$(ls "$G"/music/Track*.wav 2>/dev/null | sort || true)
if [ -z "$TRACKS" ]; then
  mkdir -p "$G/music"; python3 "$HERE/tools/make_test_track.py" "$G/music/Track02.wav"
  TRACKS="$G/music/Track02.wav"
fi
# a CD track must be at least 4 s (300 sectors): pad a small data track with empty sectors
python3 -c "import os,sys; p=sys.argv[1]; n=os.path.getsize(p)//2048; open(p,'ab').write(bytes(2048*(600-n))) if n<600 else None" "$ISO"
python3 "$HERE/stack/subcpu/make_mixed_cd2.py" "$ISO" $TRACKS --out "$OUT/$NAME"
echo "==> done: burn $OUT/$NAME.cue (with its .bin).  Needs a Sega CD + 32X + a 4 MB RAM cart."
