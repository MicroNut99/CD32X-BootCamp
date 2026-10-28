#!/usr/bin/env python3
"""merge_sdsave.py <a COPY of the BootCamp> <the BootCamp> - the SD-card save commands into a copy of the stack

    python3 sdcard/subcpu/merge_sdsave.py build/bootcamp .

The boot stack is never edited (stack/ is the console-proven one; tools/check_boot.py guards it).  This
puts two Sub-CPU commands into a COPY of stack/subcpu (main.c, Makefile), behind a switch that is off by
default (SDSAVE ?= 0), and then proves the merge can be taken out again: without the SDSAVE blocks the
copy's files are byte for byte what they were (the BootCamp's, on a fresh copy).  The copy's
BOOT_FILES.md5 is updated (only the copy's).

The commands are OpenLara CD32X-REV EDCD's EDSAVE-1 (its Sub-CPU CMD 61 / 62 with OL_EDSAVE; Rayman CD32X's
CMD 72 / 73 are the same), console-proven 26 Oct 2026, with the piece numbered across the whole bank:

  SDSAVE_CMD_OUT (74)  COMM2 = piece 0-63: the 4 KB buffer (filled with CMD 58) -> the EverDrive's save
                       bank at piece * 4 KB, read back and compared
  SDSAVE_CMD_IN  (75)  COMM2 = piece 0-63: the bank -> the 4 KB buffer (then read with CMD 57);
                       COMM2 = 0x8000: no copy, only the answer
  reply COMM12 (both) = 0x201 done (0x200: the save bank is here); 0 failed, or no save bank

The bank is bank 31, the EverDrive PRO's battery RAM, which its OS keeps on the SD card
(MEGA/gamedata/<launcher>/bram.srm, 256 KB) when the launcher's header declares save RAM (EDCD_SAVE=1).
The cartridge-boot listener reaches it as cart 0x800000-0x83FFFF (hw_md.s MODE1-7, M1SAVE=1, the
BootCamp's own route: window 1 = bank 31 for the copy, W on only inside the write).  Only the launcher's
program (MODE1=1 M1SAVE=1) has the bank: the disc's own program (SDSAVE=1 without M1SAVE) answers 0, so
the game knows it has no SD-card saves - and it must have the commands, or COMM0 is never cleared.
"""
import hashlib, os, subprocess, sys

BEGIN, END = "SDSAVE {", "} SDSAVE"

MAIN_BEFORE = """        else if (cmd == 48) {
            /* (K5: normally handled by the fast path at the top of the loop)"""
MAIN = """/* SDSAVE { (CD32X-BootCamp sdcard/subcpu/merge_sdsave.py: game saves in the EverDrive's save bank) */
#if SDSAVE
        /* SDSAVE_CMD_OUT / SDSAVE_CMD_IN: one 4 KB piece of the EverDrive's save bank (bank 31, the battery
           RAM its OS keeps on the SD card as bram.srm) - OpenLara CD32X-REV EDCD's EDSAVE-1 (its CMD 61 / 62).
           Piece n (COMM2, 0-63) is bank offset n * 4 KB, which the listener's MODE1-7 route (M1SAVE=1)
           reaches as cart 0x800000 + n * 4 KB.
             OUT  the 4 KB buffer (filled with CMD 58) -> the piece, read back and compared
             IN   the piece -> the 4 KB buffer (then read with CMD 57); COMM2 = 0x8000 only asks
           Reply COMM12: 0x201 done; 0 failed, or no save bank (not the launcher's program). */
        else if (cmd == SDSAVE_CMD_OUT || cmd == SDSAVE_CMD_IN) {
            int ok = 0;
#if SDSAVE_M1SAVE
            int piece = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF;
            if (cmd == SDSAVE_CMD_IN && piece == 0x8000)
                ok = 0x201;
            else if (piece < 64) {
                uint32_t bank = 0x800000UL + (uint32_t)piece * 4096;
                volatile uint8_t *rd = (volatile uint8_t *)0x0C8000;
                int k;
                if (cmd == SDSAVE_CMD_OUT) {
                    volatile uint8_t *wr = (volatile uint8_t *)0x0C0000;
                    for (k = 0; k < 4096; k++) wr[k] = bram_buf[k];
                    switch_banks();
                    do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, bank, 0x200000, 4096);
                    do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x208000, bank, 4096);
                    switch_banks();
                    for (k = 0; k < 4096; k++)
                        if (rd[k] != bram_buf[k]) break;
                    ok = k == 4096;
                } else {
                    switch_banks();
                    do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x208000, bank, 4096);
                    switch_banks();
                    for (k = 0; k < 4096; k++) bram_buf[k] = rd[k];
                    ok = 1;
                }
                if (ok) ok = 0x201;
            }
#endif
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, ok);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif
/* } SDSAVE */
"""

MAKE_AFTER = "CARTW ?= 0\nHWFLAGS += -DOL_CARTW=$(CARTW)\n"
MAKE = """# SDSAVE { SDSAVE=1: game saves on the EverDrive's SD card, CMD 74 / 75 (CD32X-BootCamp sdcard/); the save bank
# only in the launcher's program (MODE1=1 M1SAVE=1: the listener's route to the EverDrive's bank 31)
SDSAVE ?= 0
SDSAVE_OUT ?= 74
SDSAVE_IN ?= 75
HWFLAGS += -DSDSAVE=$(SDSAVE) -DSDSAVE_CMD_OUT=$(SDSAVE_OUT) -DSDSAVE_CMD_IN=$(SDSAVE_IN)
ifeq ($(MODE1)$(M1SAVE),11)
HWFLAGS += -DSDSAVE_M1SAVE=1
else
HWFLAGS += -DSDSAVE_M1SAVE=0
endif
# } SDSAVE
"""


def die(msg):
    sys.exit("*** merge_sdsave: " + msg)


def strip(text):
    out, skipping = [], False
    for line in text.splitlines(keepends=True):
        if not skipping and BEGIN in line:
            skipping = True
        if not skipping:
            out.append(line)
        if skipping and END in line:
            skipping = False
    if skipping:
        die("an SDSAVE block is not closed")
    return "".join(out)


def insert(text, anchor, block, after=True):
    if text.count(anchor) != 1:
        die("the anchor is not in the file exactly once:\n" + anchor)
    i = text.index(anchor)
    return text[:i + len(anchor)] + block + text[i + len(anchor):] if after else text[:i] + block + text[i:]


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    root, orig = (os.path.realpath(a) for a in sys.argv[1:])
    here = os.path.realpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    if root in (orig, here):
        die("that is the BootCamp itself - run it on a copy (the stack is never edited)")
    sub = os.path.join(root, "stack", "subcpu")
    check = os.path.join(root, "tools", "check_boot.py")
    for p in (os.path.join(sub, "main.c"), os.path.join(sub, "Makefile"), check):
        if not os.path.isfile(p):
            die("not a copy of the BootCamp (missing " + os.path.relpath(p, root) + ")")
    if subprocess.run([sys.executable, check], capture_output=True, text=True).returncode != 0:
        die("the copy's boot stack does not pass check_boot.py before the merge")
    names = ("main.c", "Makefile")
    files = {n: open(os.path.join(sub, n), newline="").read() for n in names}
    if any(BEGIN in t for t in files.values()):
        die("already merged (run it on a fresh copy)")
    bootcamp = {n: open(os.path.join(orig, "stack", "subcpu", n), newline="").read() for n in names}
    for n in names:
        if files[n] != bootcamp[n]:
            print("    note: %s of the copy is not the BootCamp's (another merge first?) - checked against the copy" % n)
    new = dict(files)
    new["main.c"] = insert(new["main.c"], MAIN_BEFORE, MAIN, after=False)
    new["Makefile"] = insert(new["Makefile"], MAKE_AFTER, MAKE)
    for n, t in new.items():
        if strip(t) != files[n]:
            die(n + ": the SDSAVE blocks taken out do not give the copy's file back")
    for n, t in new.items():
        open(os.path.join(sub, n), "w", newline="").write(t)
    r = subprocess.run([sys.executable, check, "--update"], capture_output=True, text=True)
    if r.returncode != 0 or subprocess.run([sys.executable, check], capture_output=True).returncode != 0:
        die("check_boot.py --update failed in the copy:\n" + r.stdout + r.stderr)
    for n in names:
        same = "the BootCamp's" if files[n] == bootcamp[n] else "the copy's as it was"
        print("    %s: + %d SDSAVE block; without it byte for byte %s (md5 %s)"
              % (n, new[n].count(BEGIN), same, hashlib.md5(files[n].encode()).hexdigest()[:8]))


if __name__ == "__main__":
    main()
