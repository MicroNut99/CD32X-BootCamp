#!/usr/bin/env python3
"""diag_switch.py - the diagnostic disc's switches (build.sh DIAG=1 runs "on" before the build, "off" after).

    python3 tools/diag_switch.py on      the diagnostic lines in, the files saved as <file>.diagsave
    python3 tools/diag_switch.py off     the saved files back (the release lines)
    python3 tools/diag_switch.py check   which state each switch is in

Each switch is ONE existing line of the boot stack, swapped for its diagnostic twin, the way SuperTux CD32X
REV keeps its diagnostics (docs/BOOT_PATH_NOTES.md there).  The release disc never has them; the source in
git always holds the release lines.  tools/check_boot.py accepts either line.

  1 main.c        KOBO_DIAG_TEXT 1      the Sub-CPU's green text (boot steps, STATE, files, cart chunks)
  2 main.c        KOBO_BOOT_TEXT 1      main() does not switch the Genesis display off
  3 hw_md.s       0x8174 (OL-13)        the 68000 leaves the Genesis display ON: the white 32X start-up
                                        lines and the green text show from the start
  4 cdloader      CMD 52 COMM2 = 1      CDLOADER keeps the Genesis text on at 4D01 (OL-3 sends 0)
  5 cdloader      DISPMODE 0x0081       the boot picture with the 32X priority bit: the text is in front of it
                                        (OpenJazz REV OJ-J4B, Kobo K18)
"""
import os, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (file, release line, diagnostic line) - each release line must occur exactly once
SWITCHES = [
    ("stack/subcpu/main.c",
     b"#define KOBO_DIAG_TEXT        0",
     b"#define KOBO_DIAG_TEXT        1"),
    ("stack/subcpu/main.c",
     b"#define KOBO_BOOT_TEXT        0",
     b"#define KOBO_BOOT_TEXT        1"),
    ("stack/subcpu/hw_md.s",
     b"        move.w  #0x8134,(a4)            | OL-13: display OFF (V-int on) - no white boot text",
     b"        move.w  #0x8174,(a4)            | OL-13: display OFF (V-int on) - no white boot text"),
    ("stack/sh2/cdloader_sh2.c",
     b"    SC_COMM2 = 0;                        /* OL-3: Genesis text stays off */",
     b"    SC_COMM2 = 1;                        /* OL-3: Genesis text stays off */"),
]


def to_release(rel, data):
    """data with every diagnostic line of this file put back to its release line (for check_boot.py)"""
    for f, release, diag in SWITCHES:
        if f == rel:
            data = data.replace(diag, release)
    return data


def files():
    return sorted(set(f for f, _, _ in SWITCHES))


def on():
    for rel in files():
        p = os.path.join(ROOT, rel)
        data = open(p, "rb").read()
        if os.path.exists(p + ".diagsave"):
            sys.exit("diag_switch: %s.diagsave exists - run 'off' first" % rel)
        shutil.copyfile(p, p + ".diagsave")
        for f, release, diag in SWITCHES:
            if f != rel:
                continue
            if data.count(release) != 1:
                off()
                sys.exit("diag_switch: the release line is not in %s exactly once:\n  %s" % (rel, release.decode()))
            data = data.replace(release, diag)
        open(p, "wb").write(data)
    print("diag_switch: %d diagnostic switches on" % len(SWITCHES))


def off():
    n = 0
    for rel in files():
        p = os.path.join(ROOT, rel)
        if os.path.exists(p + ".diagsave"):
            shutil.move(p + ".diagsave", p)
            n += 1
    print("diag_switch: release lines back in %d files" % n)


def check():
    for f, release, diag in SWITCHES:
        data = open(os.path.join(ROOT, f), "rb").read()
        state = "release" if data.count(release) == 1 else "DIAGNOSTIC" if data.count(diag) == 1 else "MISSING"
        print("%-10s %s: %s" % (state, f, release.decode().strip()[:60]))


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    {"on": on, "off": off, "check": check}.get(cmd, lambda: sys.exit(__doc__))()
