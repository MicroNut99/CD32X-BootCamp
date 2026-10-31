#!/usr/bin/env python3
"""pack_edcd.py - a launcher file that carries the game's data: the EverDrive menu preloads the PSRAM

    pack_edcd.py <EDCD_GAME.32X> -o <EDCD_GAME_PACK.32X> --at <offset> <file> [--at <offset> <file> ...]
    pack_edcd.py <EDCD_OPENLARA.32X> -o <EDCD_OPENLARA.32X> --openlara <disc files folder> [LEVELn.PKD]
    pack_edcd.py <EDCD_DOOM.32X> -o <EDCD_DOOM_PACK.32X> --d32xr <preload.json> <D32XR.32X> <disc files folder>

The EverDrive PRO menu copies a .32X from the card into its PSRAM, linearly from the start of the 16 MB,
before it starts it (bank n = file offset n x 0x80000).  A launcher padded with the game's data at the cart
offsets the game's own CMD 38 / 46 would fill therefore arrives in the cart "already loaded", and the
game's "is it in the cart?" checks hit - IF the OS loads a file this big in 32X mode.  edpack/ is the probe
for that; run it first (MegaEverDrivePro_SDCARD.md section 4.0).  Nothing in the boot stack changes.

The file is limited to 14 MB (banks 0-27): banks 28-30 hold the EverDrive OS's own buffers and bank 31 is
the battery RAM.  Bank 0 is the launcher itself (it must fit 512 KB), so a game's ROM cannot be packed at
offset 0; the loader copies that part from the CD as before.

--openlara <disc files folder> [<level for the level slot>] lays out OpenLara CD32X-REV's EverDrive build:
    0x080000   LEVEL1.PKD   Caves, the title menu and level 1 (CMD 38 file 202 -> slot 4; CAVES-1)
    0x200000   LEVEL2.PKD   the level slot (file 100 + id; RESUME-1), another level if named
The whole pack ends under 4 MB, so it is within reach even if the OS loads only 4 MB of a .32X.  OpenLara's
"already in the cart" checks compare the record in save piece 14 (length + checksum; in the EverDrive
build that piece lives in bank 31, kept in bram.srm) with what is in the cart: the first launch loads from
the CD and writes the record, every launch after that finds the packed Caves (and the level, when the
record names the same level) and loads nothing.  The engine (OPENLARA.32X, <= 384 KB) still comes from the
CD: bank 0 is the launcher.

--d32xr lays out DOOM CD32X Fusion's EverDrive build (CART=ED: page 1 = banks 4-7 is the ROM's second
2 MB, pages 2-6 = banks 8-27 the preload) from the plan tools/make_preload.py wrote:
    0x080000-0x3FFFFF   D32XR.32X bytes 0x080000-0x3FFFFF  (bank 0 stays the launcher: the first 512 KB of
                        the ROM is what the loader still copies from the CD - or all of it, see below)
    0x400000 + ...      each preload file at page 2 + L / 0x1F8000, offset L % 0x1F8000 (L = its linear
                        start), i.e. file offset 0x400000 + (L // 0x1F8000) * 0x200000 + L % 0x1F8000
With D32XR's loader as it is (cdloader_sh2.c), the ROM's whole-ROM checksum fails (bank 0 is not the ROM)
and the loader copies the whole 4 MB ROM; the WADs' sums match and are skipped: a 14 MB first boot becomes a
4 MB one.  A loader change (rom_fix instead of the full copy when LOADER_CHECK is on) would cut that to the
first 512 KB; it is D32XR's loader, not the stack.
"""
import json
import os
import sys

BANK = 0x80000
LIMIT = 28 * BANK            # 14 MB: banks 0-27
PAGE = 4 * BANK
D32XR_PAGE_USE = 0x1F8000
D32XR_FIRST_PAGE = 2


def place(items, off, data, what):
    end = off + len(data)
    if end > LIMIT:
        sys.exit('*** %s at 0x%06X ends at 0x%06X: past 14 MB (banks 28-31 are the EverDrive\'s)' % (what, off, end))
    for (o, d, w) in items:
        if off < o + len(d) and o < end:
            sys.exit('*** %s at 0x%06X overlaps %s at 0x%06X' % (what, off, w, o))
    items.append((off, data, what))


def main():
    args = sys.argv[1:]
    if len(args) < 3:
        sys.exit(__doc__)
    launcher = open(args[0], 'rb').read()
    if len(launcher) > BANK:
        sys.exit('*** the launcher is %d bytes: it must fit bank 0 (512 KB)' % len(launcher))
    if launcher[0x100:0x108] != b'SEGA SSF':
        sys.exit('*** the launcher\'s header is not "SEGA SSF": the EverDrive would not turn its mapper on')
    out = None
    items = []
    i = 1
    while i < len(args):
        if args[i] == '-o':
            out = args[i + 1]; i += 2
        elif args[i] == '--at':
            off = int(args[i + 1], 0)
            data = open(args[i + 2], 'rb').read()
            if off & 3:
                sys.exit('*** --at offsets must be multiples of 4')
            place(items, off, data, os.path.basename(args[i + 2]))
            i += 3
        elif args[i] == '--openlara':
            folder = args[i + 1]
            level = 'LEVEL2.PKD'
            n = 2
            if i + 2 < len(args) and not args[i + 2].startswith('-'):
                level = args[i + 2]; n = 3
            caves = open(os.path.join(folder, 'LEVEL1.PKD'), 'rb').read()
            if len(caves) > 0x180000:
                sys.exit('*** LEVEL1.PKD is %d bytes: Caves must fit 0x080000-0x1FFFFF' % len(caves))
            place(items, 0x080000, caves, 'LEVEL1.PKD (Caves, slot 4)')
            lv = open(os.path.join(folder, level), 'rb').read()
            if len(lv) > 0x1F0000:
                sys.exit('*** %s is %d bytes: a level must fit 0x200000-0x3EFFFF' % (level, len(lv)))
            place(items, 0x200000, lv, '%s (level slot)' % level)
            i += n
        elif args[i] == '--d32xr':
            plan = json.load(open(args[i + 1]))['files']
            rom = open(args[i + 2], 'rb').read()
            folder = args[i + 3]
            if len(rom) != 0x400000:
                print('    note: D32XR.32X is %d bytes, not 4 MB' % len(rom))
            place(items, BANK, rom[BANK:], 'D32XR.32X (from 0x80000)')
            for e in plan:
                data = open(os.path.join(folder, e['name']), 'rb').read()
                if len(data) != e['length']:
                    sys.exit('*** %s is %d bytes, the plan says %d: not the files the disc was built from'
                             % (e['name'], len(data), e['length']))
                lin = e['linear']
                off = D32XR_FIRST_PAGE * PAGE + (lin // D32XR_PAGE_USE) * PAGE + lin % D32XR_PAGE_USE
                # a file that runs past its page's 0x1F8000 continues at the next page's offset 0
                while data:
                    room = D32XR_PAGE_USE - (off % PAGE)
                    place(items, off, data[:room], e['name'])
                    data = data[room:]
                    off = (off // PAGE + 1) * PAGE
            i += 4
        else:
            sys.exit('*** unknown argument %s\n%s' % (args[i], __doc__))
    if not out:
        sys.exit('*** -o <output> is needed')
    size = max([len(launcher)] + [o + len(d) for (o, d, w) in items])
    size = (size + BANK - 1) & ~(BANK - 1)
    buf = bytearray(b'\xff' * size)
    buf[:len(launcher)] = launcher
    for (o, d, w) in sorted(items):
        buf[o:o + len(d)] = d
    open(out, 'wb').write(buf)
    print('%s: %.1f MB (%d banks)' % (out, size / 1048576.0, size // BANK))
    print('    0x000000  %-28s %8d bytes  bank 0' % (os.path.basename(args[0]), len(launcher)))
    for (o, d, w) in sorted(items):
        print('    0x%06X  %-28s %8d bytes  bank %d%s' % (o, w, len(d), o // BANK,
              '' if (o + len(d) - 1) // BANK == o // BANK else '-%d' % ((o + len(d) - 1) // BANK)))
    print('    banks 28-31 untouched.  Same name rule as every launcher: the file\'s name is the save\'s.')


if __name__ == '__main__':
    main()
