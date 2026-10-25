#!/bin/bash
# edboot/build.sh - EDBOOT.BIN, the launcher that makes a Mega EverDrive PRO the RAM cart of a CD32X disc
#   bash edboot/build.sh           -> edboot/EDBOOT.BIN (32 KB, header "SEGA SSF")
#   ROOTDIR= / SEGA_TOOLCHAIN=     the toolchain, as for the main build.sh (only m68k-elf is used)
# Copy EDBOOT.BIN to the EverDrive's SD card; docs/10_EVERDRIVE_TEST.md says how to use it.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -n "${SEGA_TOOLCHAIN:-}" ]; then ROOTDIR="$SEGA_TOOLCHAIN"; fi
ROOTDIR="${ROOTDIR:-/opt/toolchains/sega}"
P="$ROOTDIR/m68k-elf/bin/m68k-elf-"
[ -x "${P}as" ] || { echo "*** no m68k-elf toolchain at $ROOTDIR (set ROOTDIR= or SEGA_TOOLCHAIN=)"; exit 1; }
B="$HERE/build"; mkdir -p "$B"
"${P}as" -m68000 --register-prefix-optional -o "$B/edboot.o" "$HERE/edboot.s"
"${P}ld" -Ttext 0 -o "$B/edboot.elf" "$B/edboot.o"
"${P}objcopy" -O binary "$B/edboot.elf" "$B/edboot.bin"
python3 - "$B/edboot.bin" "$HERE/EDBOOT.BIN" <<'EOF'
import sys
d = bytearray(open(sys.argv[1], 'rb').read())
assert len(d) == 0x8000, "EDBOOT.BIN must be 32 KB, it is %d bytes" % len(d)
assert d[0x100:0x108] == b"SEGA SSF", "the header must start with SEGA SSF"
assert d[0x1F0:0x1F3] == b"JUE", "the header fields are out of place"
s = sum(int.from_bytes(d[i:i + 2], 'big') for i in range(0x200, len(d), 2)) & 0xFFFF
d[0x18E:0x190] = s.to_bytes(2, 'big')
open(sys.argv[2], 'wb').write(d)
print("EDBOOT.BIN: %d bytes, checksum %04X" % (len(d), s))
EOF
rm -rf "$B"
