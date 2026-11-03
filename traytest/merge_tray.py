#!/usr/bin/env python3
"""merge_tray.py <a COPY of the BootCamp> <the BootCamp> - the tray and CD status commands into a copy of the stack

    python3 traytest/merge_tray.py /tmp/bootcamp-copy .

The boot stack is never edited (stack/ is the console-proven one; tools/check_boot.py guards it).  This
puts two Sub-CPU commands and a start-up switch into a COPY of stack/subcpu (main.c, Makefile), behind
switches that are off by default (TRAYTEST ?= 0, TRAY_NOCART ?= 0), and proves the merge can be taken out again: without the TRAYTEST blocks the copy's
files are byte for byte what they were.  The copy's BOOT_FILES.md5 is updated (only the copy's).
Same pattern as sdcard/subcpu/merge_sdsave.py.

The commands (ledger CD32X_COMMAND_LEDGER_ADDITIONS.md section 22; untested on a console):

  90  the tray        COMM2 = 1: open it  (BIOS DRVOPEN 0x0A)
                      COMM2 = 0: close it and read the disc (BIOS DRVINIT 0x10, parameters 01 FF: the BIOS
                                 closes an open tray, reads the table of contents, holds at track 1)
                      reply COMM12 = 1 asked, 0 refused (any other COMM2)
                      Both BIOS calls are queued: they return at once, the drive does the work after.
  84  CD status       BIOS CDBSTAT 0x81, its 32-byte status table (_CDSTAT, 0x5E80 in the US BIOS 1.10 and 2.00W)
                      COMM2 = 0: COMM12 = the BIOS status word (table +0)
                                 COMM8L = the drive's status word (table +4; its high byte is the drive's own
                                          status code) << 16 | first track << 8 | last track (+16 / +17)
                      COMM2 = 1: COMM12 = the table's word +6 (current track, low byte)
                                 COMM8L = the time in the track (+12, BCD mm ss ff 00)
                      COMM2 = 2: COMM12 = the drive lights (+2)
                                 COMM8L = the time on the disc (+8, BCD mm ss ff 00)

and one switch for the start-up (T3, no cart):

  TRAY_NOCART=1       the stack's own no-cart mode (KOBO_NOCART 1, main.c "KOBO STORAGE MODE"): the start-up
                      cart check is not made, so the cart is never read or written.  For a disc whose SH2
                      program runs from SDRAM (traytest/sdram.c) and needs no cart.  Only with TRAYTEST=1.
"""
import hashlib, os, subprocess, sys

BEGIN, END = "TRAYTEST {", "} TRAYTEST"

MAIN_BEFORE = """        else if (cmd == 48) {
            /* (K5: normally handled by the fast path at the top of the loop)"""
MAIN = """/* TRAYTEST { (CD32X-BootCamp traytest/merge_tray.py: the drive's tray and the CD status, ledger section 22) */
#if TRAYTEST
        /* TRAY_CMD_TRAY (90): COMM2 = 1 open the tray (BIOS DRVOPEN 0x0A: the drive stops, the tray opens, the
           BIOS status word reads 0x4000 until it is closed); COMM2 = 0 close it and read the disc (BIOS DRVINIT
           0x10 with 01 FF: an open tray is closed, the table of contents read, the drive held at track 1).
           Queued BIOS calls: they return at once and the drive does the rest; TRAY_CMD_STATUS shows it.
           Reply COMM12: 1 asked, 0 refused. */
        else if (cmd == TRAY_CMD_TRAY) {
            static uint8_t tray_init[4] = { 0x01, 0xFF, 0x00, 0x00 };
            int op = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF, ok = 0;
            if (op == 1) { scd_bios_call(0x000A, 0, 0); ok = 1; }
            else if (op == 0) { scd_bios_call(0x0010, 0, tray_init); ok = 1; }
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, ok);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        /* TRAY_CMD_STATUS (84): BIOS CDBSTAT 0x81 fills its 32-byte status table at 0x5E80 (_CDSTAT).
           COMM2 = 0: COMM12 = the BIOS status word (+0); COMM8L = the drive's status word (+4) << 16 |
                      first track << 8 | last track (+16 / +17)
           COMM2 = 1: COMM12 = the word at +6 (current track, low byte); COMM8L = the time in the track (+12)
           COMM2 = 2: COMM12 = the drive lights (+2); COMM8L = the time on the disc (+8) */
        else if (cmd == TRAY_CMD_STATUS) {
            const volatile uint8_t *t = (const volatile uint8_t *)0x5E80;
            int op = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF;
            uint32_t l;
            int w;
            scd_bios_call(0x0081, 0, 0);
            if (op == 1) {
                w = t[6] << 8 | t[7];
                l = (uint32_t)t[12] << 24 | (uint32_t)t[13] << 16 | (uint32_t)t[14] << 8 | t[15];
            } else if (op == 2) {
                w = t[2] << 8 | t[3];
                l = (uint32_t)t[8] << 24 | (uint32_t)t[9] << 16 | (uint32_t)t[10] << 8 | t[11];
            } else {
                w = t[0] << 8 | t[1];
                l = (uint32_t)t[4] << 24 | (uint32_t)t[5] << 16 | (uint32_t)t[16] << 8 | t[17];
            }
            do_md_cmd2(MD_CMD_WRITE_32X_32, 0xA15128, (int)l);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, w);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif
/* } TRAYTEST */
"""

MAIN_NOCART_AFTER = "#define KOBO_NOCART     0          /* HW1: CART MODE - real hardware WITH the cart plugged in */\n"
MAIN_NOCART = """/* TRAYTEST { (CD32X-BootCamp traytest/merge_tray.py) TRAY_NOCART=1: the stack's no-cart mode for the tray
   test (T3): its SH2 program runs from SDRAM, so the start-up cart check is not made and no cart is needed */
#if TRAYTEST && TRAY_NOCART
#undef KOBO_NOCART
#define KOBO_NOCART     1
__attribute__((used)) static const char tray_nocart_marker[] = "TRAY-NOCART: NO START-UP CART CHECK";   /* build_traytest.sh looks for it */
#endif
/* } TRAYTEST */
"""

MAKE_AFTER = "CARTW ?= 0\nHWFLAGS += -DOL_CARTW=$(CARTW)\n"
MAKE = """# TRAYTEST { TRAYTEST=1: the drive's tray (CMD 90) and the CD status (CMD 84) - CD32X-BootCamp traytest/,
# ledger section 22.  Untested on a console.  TRAY_NOCART=1 (with TRAYTEST=1): no start-up cart check.
TRAYTEST ?= 0
TRAY_TRAY ?= 90
TRAY_STATUS ?= 84
TRAY_NOCART ?= 0
HWFLAGS += -DTRAYTEST=$(TRAYTEST) -DTRAY_CMD_TRAY=$(TRAY_TRAY) -DTRAY_CMD_STATUS=$(TRAY_STATUS) -DTRAY_NOCART=$(TRAY_NOCART)
# } TRAYTEST
"""


def die(msg):
    sys.exit("*** merge_tray: " + msg)


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
        die("a TRAYTEST block is not closed")
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
    here = os.path.realpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
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
    new["main.c"] = insert(new["main.c"], MAIN_NOCART_AFTER, MAIN_NOCART)
    new["main.c"] = insert(new["main.c"], MAIN_BEFORE, MAIN, after=False)
    new["Makefile"] = insert(new["Makefile"], MAKE_AFTER, MAKE)
    for n, t in new.items():
        if strip(t) != files[n]:
            die(n + ": the TRAYTEST blocks taken out do not give the copy's file back")
    for n, t in new.items():
        open(os.path.join(sub, n), "w", newline="").write(t)
    r = subprocess.run([sys.executable, check, "--update"], capture_output=True, text=True)
    if r.returncode != 0 or subprocess.run([sys.executable, check], capture_output=True).returncode != 0:
        die("check_boot.py --update failed in the copy:\n" + r.stdout + r.stderr)
    for n in names:
        same = "the BootCamp's" if files[n] == bootcamp[n] else "the copy's as it was"
        print("    %s: + %d TRAYTEST block(s); without them byte for byte %s (md5 %s)"
              % (n, new[n].count(BEGIN), same, hashlib.md5(files[n].encode()).hexdigest()[:8]))


if __name__ == "__main__":
    main()
