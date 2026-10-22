#!/bin/bash
# CD32X BootCamp - build a bootable Sega CD + 32X disc for a game (the example: hello/)
#
#   bash build.sh                       hello -> disc/BootCamp.cue + .bin (data + CD audio)
#   GAME=mygame bash build.sh           your game folder, laid out like hello/ (see docs/02_PORTING_YOUR_GAME.md)
#   ROOTDIR=/opt/toolchains/sega        Chilly Willy's Sega toolchain (sh-elf, m68k-elf, ldscripts, bootblocks)
#   SEGA_TOOLCHAIN=<folder>             the same, plus <folder>/usr/bin on PATH (genisoimage)
#   DIAG=1                              the diagnostic disc: the stack's boot text on (docs/09_ERROR_CODES.md)
#   REGION=US | EU | JP                 the boot block for the console's region (default US; only US is tested)
#   STREAM= CARTW= FMVRING= SLOTS= CART= the Sub-CPU's build switches (stack/subcpu/Makefile; all off by default)
#
# A game folder holds:  Makefile (makes <game>.32x, a normal 32X cartridge ROM), splash.png (the boot
# picture), cd/ (data files for the disc, named as in stack/subcpu/game_files.h), music/TrackNN.wav
# (CD audio, NN = 02, 03, ... in order; optional).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="${GAME:-hello}"
G="$HERE/$GAME"
# SEGA_TOOLCHAIN=<folder>: a toolchain somewhere else (sh-elf, m68k-elf, ldscripts, bootblocks; genisoimage
# in <folder>/usr/bin if it is not installed), as in OpenJazz CD32X REV's build.sh
if [ -n "${SEGA_TOOLCHAIN:-}" ]; then ROOTDIR="$SEGA_TOOLCHAIN"; export PATH="$PATH:$SEGA_TOOLCHAIN/usr/bin"; fi
ROOTDIR="${ROOTDIR:-/opt/toolchains/sega}"
[ -x "$ROOTDIR/sh-elf/bin/sh-elf-gcc" ] || { echo "*** no toolchain at $ROOTDIR (set ROOTDIR= or SEGA_TOOLCHAIN=)"; exit 1; }
OUT="$HERE/disc"
ROMS="$HERE/stack/subcpu/ROMS"
NAME="${DISC_NAME:-BootCamp}"
[ -f "$G/Makefile" ] || { echo "*** $G/Makefile not found"; exit 1; }
command -v genisoimage >/dev/null || { echo "*** genisoimage missing (sudo apt install genisoimage)"; exit 1; }
python3 -c "import PIL" 2>/dev/null || { echo "*** Python Pillow missing (sudo apt install python3-pil)"; exit 1; }

echo "==> 0. the boot stack is unchanged"
python3 "$HERE/tools/check_boot.py"

echo "==> 1. the game ROM ($GAME)"
make -C "$G" clean > /dev/null 2>&1 || true     # a DIAG change must rebuild everything
make -C "$G" ROOTDIR="$ROOTDIR" DIAG="${DIAG:-0}"
ROMLIST=$(ls "$G"/*.32x 2>/dev/null || true)
[ -n "$ROMLIST" ] || { echo "*** $G made no .32x ROM"; exit 1; }
[ "$(echo "$ROMLIST" | wc -l)" = 1 ] || { echo "*** more than one .32x in $G - keep only the game's ROM there"; exit 1; }
ROM="$ROMLIST"
rm -rf "$ROMS"; mkdir -p "$ROMS"                  # stack/subcpu/ROMS is made by this script: keep nothing of yours there
python3 "$HERE/tools/prep_rom.py" "$ROM" "$ROMS/GAME.32X"
# the default cart layout (stack/subcpu/game_files.h, docs/04): the ROM ends below the resident slot
[ "$(stat -c %s "$ROMS/GAME.32X")" -le $((0x80000)) ] || { echo "*** GAME.32X is over 512 KB: it would overlap the resident slot at cart 0x080000 (change GAME_RESIDENT_SLOT)"; exit 1; }

echo "==> 2. the boot picture"
if [ -f "$G/splash.png" ]; then
  python3 "$HERE/tools/make_ol_splash.py" "$G/splash.png" "$ROMS/SPLASH.tmp" > /dev/null
  python3 "$HERE/tools/make_boot_image.py" "$ROMS/SPLASH.tmp" "$ROMS/IMAGE.RAW"
  rm -f "$ROMS/SPLASH.tmp"
fi

echo "==> 3. the game's data files"
if [ -d "$G/cd" ] && [ -n "$(ls -A "$G/cd")" ]; then
  for f in "$G"/cd/*; do
    n=$(stat -c %s "$f")
    # one array of names serves both the level slot (100 + n, cart 0x200000-0x3EFFFF) and the resident
    # slot (200 + n, cart 0x080000-0x1FFFFF): a bigger file would run into the next area
    [ "$n" -le $((0x1F0000)) ] || { echo "*** $(basename "$f") is over 1984 KB: it would overwrite the save area at cart 0x3F0000"; exit 1; }
    [ "$n" -le $((0x180000)) ] || echo "    note: $(basename "$f") is over 1.5 MB: load it as 100 + n only (as 200 + n it would overwrite the level slot)"
  done
  cp -v "$G"/cd/* "$ROMS/"
fi

echo "==> 4. the SH2 loader (stack/sh2, unchanged)"
make -C "$HERE/stack/sh2" -f Makefile.cdloader clean > /dev/null
make -C "$HERE/stack/sh2" -f Makefile.cdloader ROOTDIR="$ROOTDIR"

# DIAG=1: the diagnostic disc - the stack's own two switches (KOBO_DIAG_TEXT, KOBO_BOOT_TEXT) set to 1 for
# this build only, so the Sub-CPU's green text and the 32X start-up messages show until the boot picture
# (docs/09_ERROR_CODES.md).  main.c is put back afterwards, even when the build fails.
if [ "${DIAG:-0}" = 1 ]; then
  echo "==> DIAG=1: the boot text is on (diagnostic disc)"
  cp "$HERE/stack/subcpu/main.c" "$HERE/stack/subcpu/main.c.diagsave"
  trap 'mv -f "$HERE/stack/subcpu/main.c.diagsave" "$HERE/stack/subcpu/main.c"' EXIT
  sed -i -e 's/^#define KOBO_DIAG_TEXT        0/#define KOBO_DIAG_TEXT        1/' \
         -e 's/^#define KOBO_BOOT_TEXT        0/#define KOBO_BOOT_TEXT        1/' "$HERE/stack/subcpu/main.c"
  [ "$(grep -c '^#define KOBO_\(DIAG\|BOOT\)_TEXT        1' "$HERE/stack/subcpu/main.c")" = 2 ] || { echo "*** DIAG=1: the switches were not found"; exit 1; }
fi

echo "==> 5. the Sub-CPU program + the disc image (stack/subcpu)"
# REGION=EU / JP: the toolchain's EU_BOOT.BIN / JP_BOOT.BIN boot block for those consoles (only US is tested)
case "${REGION:-US}" in US|EU|JP) ;; *) echo "*** REGION must be US, EU or JP"; exit 1;; esac
cd "$HERE/stack/subcpu" && make clean > /dev/null && make cd ROOTDIR="$ROOTDIR" BOOTBLOCK="$ROOTDIR/bootblocks/${REGION:-US}_BOOT.BIN"
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
