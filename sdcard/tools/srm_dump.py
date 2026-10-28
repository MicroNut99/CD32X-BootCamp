#!/usr/bin/env python3
"""srm_dump.py - what is in an EverDrive save bank (bram.srm from the SD card, or bank 31 of an ares RAM cart file)

    python3 sdcard/tools/srm_dump.py MEGA/gamedata/EDCD_MYGAME/bram.srm
    python3 sdcard/tools/srm_dump.py --ares ed.ram          (ares-headless --ramcart-type everdrive file)

Prints the 4 KB pieces that hold anything other than one repeated byte, and every checked record that
sdsave_save_record() wrote (sdcard/sh2/sdsave.c: magic, length, FNV-1a sum, ~sum, then the data), with its
offset, magic and the first bytes of its data.  Reads only; never writes the file.
"""
import struct, sys

BANK = 0x40000          # what the listener shows of bank 31 (cart 0x800000-0x83FFFF); bram.srm is this size


def fnv(b):
    h = 0x811C9DC5
    for x in b:
        h = ((h ^ x) * 0x01000193) & 0xFFFFFFFF
    return h


def main():
    a = sys.argv[1:]
    if not a or a[0] in ("-h", "--help"):
        sys.exit(__doc__)
    if a[0] == "--ares":
        d = open(a[1], "rb").read()[31 * 0x80000:31 * 0x80000 + BANK]
    else:
        d = open(a[0], "rb").read()[:BANK]
    print("%d bytes" % len(d))
    used = [p for p in range(len(d) // 4096) if len(set(d[p * 4096:(p + 1) * 4096])) > 1]
    print("pieces with data: " + (", ".join("%d (0x%05X)" % (p, p * 4096) for p in used) or "none"))
    n = 0
    for o in range(0, len(d) - 16, 4):
        magic, ln, s, ns = struct.unpack(">4I", d[o:o + 16])
        if ns != (~s & 0xFFFFFFFF) or ln > len(d) - o - 16 or magic in (0, 0xFFFFFFFF):
            continue
        body = d[o + 16:o + 16 + ln]
        if fnv(body) != s:
            continue
        name = magic.to_bytes(4, "big").decode("latin-1")
        print("record at 0x%05X: magic %08X '%s', %d bytes: %s%s"
              % (o, magic, name if name.isprintable() else "?", ln, body[:32].hex(), "..." if ln > 32 else ""))
        n += 1
    print("%d checked record(s)" % n)


if __name__ == "__main__":
    main()
