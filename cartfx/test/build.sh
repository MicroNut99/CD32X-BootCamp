#!/bin/bash
# cartfx/test/build.sh - the cartfx console test ROM, CFXTEST.32X (run from the EverDrive PRO's menu: 32X attached,
# no CD needed), with a cartfx mega-pro.rbf beside it on the SD card (cartfx/quartus: Hurrican's, Jazz's or SuperTux's)
#   bash cartfx/test/build.sh        -> cartfx/test/CFXTEST.32X (9 MB: the program, tools/cfxgen.c's data at 0x040000,
#                                       0xFF from 0x200000, the pictures again at 0x880000 for the second PSRAM chip)
#   ROOTDIR= / SEGA_TOOLCHAIN=       the toolchain, as for the main build.sh (default /opt/toolchains/sega)
#   HOSTCC=                          the C compiler for tools/cfxgen.c, which runs on this machine (default cc)
# (a plain script, no make: edw0's two Makefiles as commands)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -n "${SEGA_TOOLCHAIN:-}" ]; then ROOTDIR="$SEGA_TOOLCHAIN"; fi
R="${ROOTDIR:-/opt/toolchains/sega}"
HOSTCC="${HOSTCC:-cc}"
SH="$R/sh-elf/bin/sh-elf-"; MD="$R/m68k-elf/bin/m68k-elf-"
[ -x "${SH}gcc" ] || { echo "*** no toolchain at $R (set ROOTDIR= or SEGA_TOOLCHAIN=)"; exit 1; }
cd "$HERE"
rm -rf build && mkdir build

# 1. the data: the pictures, the script, what the model makes of it (tools/cfxgen.c, on this machine)
"$HOSTCC" -O2 -o build/cfxgen tools/cfxgen.c ../model/cartfx.c
build/cfxgen build/cfxtest_data.bin build/cfxtest_chip1.bin | tee build/cfxgen.txt

# 2. the 68000 side (edw0's: it waits for commands this test never sends)
MDV=$(${MD}gcc -dumpversion)
MDLIB="-L$R/m68k-elf/lib -L$R/m68k-elf/lib/gcc/m68k-elf/$MDV -L$R/m68k-elf/m68k-elf/lib"
for f in crt0 kos cd; do ${MD}as -m68000 --register-prefix-optional -Isrc-md src-md/$f.s -o build/md_$f.o; done
${MD}gcc -m68000 -Wall -O1 -c -fomit-frame-pointer -Isrc-md src-md/main.c -o build/md_main.o
${MD}gcc -T "$R/ldscripts/mars-md.ld" -Wl,-Map=build/md.map -nostdlib build/md_crt0.o build/md_main.o build/md_kos.o \
    build/md_cd.o $MDLIB -lc -lgcc -lnosys -o build/m68k.elf
${MD}objcopy -O binary build/m68k.elf build/m68k.bin

# 3. the SH2 side: crt0.s (the ROM header, the 32X header, the start-up, the 68000 binary) first
SHV=$(${SH}gcc -dumpversion)
SHLIB="-L$R/sh-elf/lib -L$R/sh-elf/lib/gcc/sh-elf/$SHV -L$R/sh-elf/sh-elf/lib"
${SH}as --big -I. -o build/crt0.o src/crt0.s
${SH}gcc -D__32X__ -m2 -mb -O2 -Wall -c -fomit-frame-pointer -std=gnu99 -Isrc -o build/main.o src/main.c
${SH}gcc -T "$R/ldscripts/mars.ld" -Wl,-Map=build/output.map -nostdlib build/crt0.o build/main.o $SHLIB \
    -lc -lgcc -lgcc-Os-4-200 -lnosys -o build/cfxtest.elf
${SH}objcopy -O binary build/cfxtest.elf build/temp.bin
[ "$(stat -c %s build/temp.bin)" -le 262144 ] || { echo "*** the program is over 256 KB"; exit 1; }

# 4. the ROM: the program in the first 256 KB, the data after it to 2 MB, 0xFF to 8.5 MB, the pictures' copy for chip 1
#    (the EverDrive menu loads the file whole and in order: its 0x800000 on is the second chip)
dd if=build/temp.bin of=CFXTEST.32X bs=256K conv=sync count=1 status=none
cat build/cfxtest_data.bin >> CFXTEST.32X
head -c $((0x880000 - 0x200000)) /dev/zero | tr '\000' '\377' >> CFXTEST.32X
cat build/cfxtest_chip1.bin >> CFXTEST.32X
[ "$(stat -c %s CFXTEST.32X)" -eq $((0x900000)) ] || { echo "*** CFXTEST.32X is not 9 MB"; exit 1; }
ls -l CFXTEST.32X; md5sum CFXTEST.32X
echo "==> CFXTEST.32X and a cartfx mega-pro.rbf in one folder on the SD card; start CFXTEST.32X from the menu"
