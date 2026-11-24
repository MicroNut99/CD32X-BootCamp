#!/usr/bin/env python3
"""merge_mdp.py <the build's COPY of the BootCamp> - MD+ for OutRun CD32X: one Sub-CPU command, one listener route

    python3 tools/merge_mdp.py build/edcd/bootcamp          (edcd/build_edcd.sh runs it, after merge_sdsave.py)

The BootCamp's stack is never edited: this patches the build's COPY (stack/subcpu main.c, hw_md.s, Makefile),
behind switches that are off by default, and checks that without its blocks the files are what they were.

MD+ (krikzz's mapper/lib_base/mdp.sv, in the EverDrive's core): its registers are words on the cart bus at
0x03F7FA (0xCD54 opens them) and 0x03F7FE (a command: 0x11tt play track tt once, 0x12tt repeating, 0x1300 stop);
0x03F7F6 / 0x03F7F8 then read "RA" "TE". They are in window 0, and the cartridge-boot listener sends every
window-0 write through window 1 (MODE1-3) - another bus address, which MD+ does not see.

  CMD 105 (MDP=1, with MODE1=1)   COMM2 = the MD+ command.  The Sub-CPU puts CD54, 0000, <command> in Word RAM and
                                  has the 68000 copy those 3 words to cart 0x03F7FA (routine CHILLY_RV_MEMCPY).
                                  Reply COMM12 = 0x4D50 ("MP").  Without MODE1: COMM12 = 0.
  hw_md.s (M1MDP=1, with M1SAVE=1) a copy whose cart address is 0x03F7FA goes straight to the cart with RV = 1
                                  (m1_rvcpy), not through window 1.

Number 105: the ledger (CD32X_COMMAND_LEDGER_ADDITIONS.md, section 40); 103 and 104 are TPSG and CDFADE.
Not known when written (23 Nov 2026): whether a 68000 write at a window-0 address reaches the cart with the 32X on
(the BootCamp's 25 Oct note: writes at the cart's first bytes did not land), and whether the EverDrive's OS turns
MD+ on for a .32X.
"""
import os, subprocess, sys

BEGIN, END = "MDP {", "} MDP"

MAIN_BEFORE = """        else if (cmd == 48) {
            /* (K5: normally handled by the fast path at the top of the loop)"""
MAIN = """/* MDP { (OutRun CD32X tools/merge_mdp.py: an MD+ command to the EverDrive) */
#if OL_MDP
        else if (cmd == 105) {
            int c = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF;
            volatile uint8_t *wr = (volatile uint8_t *)0x0C0000;
            wr[0] = 0xCD; wr[1] = 0x54; wr[2] = 0; wr[3] = 0; wr[4] = (uint8_t)(c >> 8); wr[5] = (uint8_t)c;
            switch_banks();
            do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x400000 + 0x03F7FA, 0x200000, 6);
            switch_banks();
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, 0x4D50);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#else
        else if (cmd == 105) {
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, 0);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif
/* } MDP */
"""
ASM_AFTER = """3:      cmpi.l  #0x080000,d1
        bhs     m1_rvcpy
"""
ASM = """        .ifdef OL_M1MDP                 /* MDP { the MD+ registers: this bus address itself, not window 1 */
        cmpi.l  #0x03F7FA,d1
        beq     m1_rvcpy
        .endif                          /* } MDP */
"""
MAKE_AFTER = "CARTW ?= 0\nHWFLAGS += -DOL_CARTW=$(CARTW)\n"
MAKE = """# MDP { MDP=1 (with MODE1=1): CMD 105, an MD+ command to the EverDrive; M1MDP=1: the listener's route for it
MDP ?= 0
ifeq ($(MODE1)$(MDP),11)
HWFLAGS += -DOL_MDP=1
else
HWFLAGS += -DOL_MDP=0
endif
# } MDP
"""

MAKE2_BEFORE = "ifeq ($(M1SAVE),1)\nASFLAGS += --defsym OL_M1SAVE=1\n"
MAKE2 = """# MDP { (here: ASFLAGS is set above this line, not before)
ifeq ($(M1MDP),1)
ASFLAGS += --defsym OL_M1MDP=1
endif
# } MDP
"""

def die(m): sys.exit("*** merge_mdp: " + m)

def strip(text):
    out, skip = [], False
    for line in text.splitlines(keepends=True):
        one = BEGIN in line and END in line
        if not skip and BEGIN in line and not one: skip = True
        if not skip and not one: out.append(line)
        if skip and END in line: skip = False
    if skip: die("a block is not closed")
    return "".join(out)

def insert(text, anchor, block, after=True):
    if text.count(anchor) != 1: die("the anchor is not in the file exactly once:\n" + anchor)
    i = text.index(anchor)
    return text[:i + len(anchor)] + block + text[i + len(anchor):] if after else text[:i] + block + text[i:]

def main():
    if len(sys.argv) != 2: sys.exit(__doc__)
    root = os.path.realpath(sys.argv[1])
    if os.path.exists(os.path.join(root, ".git")):
        die("that is the BootCamp itself - run it on a copy (the stack is never edited)")
    sub = os.path.join(root, "stack", "subcpu")
    check = os.path.join(root, "tools", "check_boot.py")
    names = ("main.c", "hw_md.s", "Makefile")
    files = {n: open(os.path.join(sub, n), newline="").read() for n in names}
    if any(BEGIN in t for t in files.values()): die("already merged")
    new = dict(files)
    new["main.c"] = insert(new["main.c"], MAIN_BEFORE, MAIN, after=False)
    new["hw_md.s"] = insert(new["hw_md.s"], ASM_AFTER, ASM)
    new["Makefile"] = insert(insert(new["Makefile"], MAKE_AFTER, MAKE), MAKE2_BEFORE, MAKE2, after=False)
    for n, t in new.items():
        if strip(t) != files[n]: die(n + ": without the MDP blocks the file is not what it was")
        open(os.path.join(sub, n), "w", newline="").write(t)
    r = subprocess.run([sys.executable, check, "--update"], capture_output=True, text=True)
    if r.returncode != 0: die("check_boot.py --update failed in the copy:\n" + r.stdout + r.stderr)
    print("    merge_mdp: CMD 105 and the listener's route in the copy (main.c, hw_md.s, Makefile)")

if __name__ == "__main__":
    main()
