#!/usr/bin/env python3
"""pad_markers.py <edpack.32x> <nonce> <out.32X> [banks] - pad the probe to 14 MB with edtest's pattern

The probe ROM (bank 0, 512 KB) is followed by banks 1..27 (default), each 512 KB of the pattern edtest's
68000 and the probe's SH2 program check:

    word i (0..262143) = (i & 0xFFFF) ^ ((i >> 16) << 12) ^ seed,   seed = (bank * 0x1F3D) ^ nonce

So word 0 of a bank is its seed, word 1 is seed ^ 1: the probe tells a bank's pattern from another's.
The result is 14 MB (banks 0-27), which stays clear of banks 28-31 (the EverDrive OS and the battery
RAM).  Nothing in the file is random; the nonce only keeps a pattern left in the PSRAM by an earlier
run of a different build from passing for this one.
"""
import struct
import sys


def bank_pattern(bank, nonce):
    seed = ((bank * 0x1F3D) ^ nonce) & 0xFFFF
    out = bytearray()
    for q in range(4):
        s = seed ^ (q << 12)
        out += struct.pack('>65536H', *[(i ^ s) & 0xFFFF for i in range(65536)])
    return bytes(out)


def main():
    if len(sys.argv) not in (4, 5):
        sys.exit(__doc__)
    rom = open(sys.argv[1], 'rb').read()
    nonce = int(sys.argv[2], 0) & 0xFFFF
    banks = int(sys.argv[4]) if len(sys.argv) == 5 else 27
    if len(rom) > 0x80000:
        sys.exit('*** the probe ROM is %d bytes: it must fit bank 0 (512 KB)' % len(rom))
    if not 1 <= banks <= 27:
        sys.exit('*** banks must be 1..27 (28-31 are the EverDrive\'s)')
    with open(sys.argv[3], 'wb') as f:
        f.write(rom + b'\xff' * (0x80000 - len(rom)))
        for bank in range(1, banks + 1):
            f.write(bank_pattern(bank, nonce))
    print('%s: %d banks, %.1f MB, nonce %04X' % (sys.argv[3], banks + 1, (banks + 1) * 0.5, nonce))


if __name__ == '__main__':
    main()
