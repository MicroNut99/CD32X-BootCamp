#!/usr/bin/env python3
"""check_boot.py - is the boot stack still the one that runs on the console?

    python3 tools/check_boot.py            check (build.sh runs it first; exit 1 = changed)
    python3 tools/check_boot.py --update   write stack/BOOT_FILES.md5 again (only when you MEANT to change
                                           the stack, and you have run the new one on a console)

Every source file of stack/subcpu and stack/sh2 is compared with stack/BOOT_FILES.md5.  Two things may
differ without failing the check:
  - stack/subcpu/game_files.h  (your file names: not part of the hash list)
  - the diagnostic switches in stack/subcpu/main.c (KOBO_DIAG_TEXT, KOBO_BOOT_TEXT): their value is
    ignored, so switching the boot text on or off is allowed.
The idea is SuperTux CD32X REV's tools/check_boot.py.
"""
import hashlib, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
LIST = os.path.join(ROOT, "stack", "BOOT_FILES.md5")
DIRS = ["stack/subcpu", "stack/sh2"]
EXTS = (".c", ".h", ".s", ".ld", ".py", ".md")
NAMES = ("Makefile", "Makefile.cdloader")
SKIP = {"stack/subcpu/game_files.h"}
SWITCH = re.compile(rb"^(#define\s+(?:KOBO_DIAG_TEXT|KOBO_BOOT_TEXT)\s+)[01]\b", re.M)


def files():
    out = []
    for d in DIRS:
        for n in sorted(os.listdir(os.path.join(ROOT, d))):
            rel = d + "/" + n
            if rel in SKIP or not os.path.isfile(os.path.join(ROOT, rel)):
                continue
            if n.endswith(EXTS) or n in NAMES:
                out.append(rel)
    return out


def digest(rel):
    data = open(os.path.join(ROOT, rel), "rb").read().replace(b"\r\n", b"\n")
    if rel == "stack/subcpu/main.c":
        data = SWITCH.sub(rb"\g<1>X", data)
    return hashlib.md5(data).hexdigest()


def main():
    now = {rel: digest(rel) for rel in files()}
    if "--update" in sys.argv:
        with open(LIST, "w") as f:
            for rel in sorted(now):
                f.write("%s  %s\n" % (now[rel], rel))
        print("check_boot: wrote %d files to %s" % (len(now), os.path.relpath(LIST, ROOT)))
        return 0
    want = {}
    for line in open(LIST):
        if line.strip():
            h, rel = line.split(None, 1)
            want[rel.strip()] = h
    bad = []
    for rel in sorted(set(want) | set(now)):
        if rel not in now:
            bad.append("missing  " + rel)
        elif rel not in want:
            bad.append("new      " + rel)
        elif want[rel] != now[rel]:
            bad.append("changed  " + rel)
    if bad:
        print("check_boot: the boot stack is NOT the one this kit ships:")
        for b in bad:
            print("  " + b)
        print("Undo the change (only game_files.h and the two diagnostic switches may differ).")
        print("See docs/06_HARDWARE_RULES.md before changing the stack on purpose.")
        return 1
    print("check_boot: the boot stack is unchanged (%d files)" % len(now))
    return 0


if __name__ == "__main__":
    sys.exit(main())
