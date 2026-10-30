# 13 – Testing in ares from your own container (for an AI)

**Who this is for:** an AI working in a cloud container on one of MicroNut99's CD32X games. It says exactly
how to build the CD32X ares build's headless runner there, how to get the owner's discs and launchers into
the container, how to run them (RAM cart and EverDrive PRO), how to read what comes out, and what a green run
does **not** prove. Written 27 Oct 2026 from the runs that found the Hurrican and Sonic CD 2011 EverDrive faults.

1. Use the owner's ares, built from their repo, and their launcher and disc. Nothing else counts.
2. Send the owner the contact sheet from every run.
3. Never say "tested" about the owner's build until their folder's md5s match what was tested.

Read [docs/11](11_ARES_16MB_RAM_CART.md)'s "law" first: **the emulator follows the hardware, and a green run
in ares is never a pass for the console.** ares is a cheap filter for bugs, nothing more.

## 1. What you need

| What | Where it comes from |
|---|---|
| The CD32X ares build's source (ares v148 + the RAM carts + the EverDrive PRO board + `ares-headless`) | The owner's own ares: [MicroNut99/ares_android](https://github.com/MicroNut99/ares_android) (`git clone` it into your container). Made for CD32X, 32XCD and EDCD boot games; runs regular 32X games too. Its desktop/headless CMake project is in `android/native/`. |
| A US Sega CD BIOS (`us.bin`) | The owner's file. Stage it from their computer; never commit it anywhere. |
| The game's disc (`.cue` + `.bin`) and, for the EverDrive, its launcher (`EDCD_<GAME>.32X`) | The owner's build folder (`S:\KoboPort\sega-toolchain-12.1\sega\<game>`), through the device tools (section 3). Never commit discs or game data. |
| `cmake`, `ninja`, a C++ compiler, `python3` with Pillow | Usually in the container already (`which cmake ninja g++`; `python3 -c "import PIL"`). |

## 2. Build `ares-headless` (once per container, 10–20 minutes on 2 cores)

```bash
cd <ares source>
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DARES_ANDROID_HARNESS=ON -S android/native -B /home/claude/ares_build
ninja -C /home/claude/ares_build ares-headless
```

- **`-DARES_ANDROID_HARNESS=ON` is required.** Without it the target does not exist (`ninja: error: unknown
  target 'ares-headless'`).
- Run the build in the background (it outlives a 10-minute tool timeout) and wait on it; do not start two.
- `ares-headless` has **no `--help`** (it answers `unknown argument --help`). The switches are at the top of
  `android/native/src/harness.cpp` and in the table below.

| Switch | What it does |
|---|---|
| `--game file` | The disc (`.cue`) or a cartridge ROM |
| `--machine cd32x\|cd\|32x\|md` | Sega CD + 32X + RAM cart / Sega CD / 32X / Genesis. A `.cue` defaults to `cd32x` |
| `--bios-us file` (`--bios-jp`, `--bios-eu`) | The Sega CD BIOS |
| `--ramcart file` | The RAM cart's contents: created blank if missing, **written back at the end**. Reuse it for a "second boot" |
| `--ramcart-size 4\|8\|16` | The 4 MB cart or the paged 8 / 16 MB PCB (`pcb` type only) |
| `--ramcart-type pcb\|everdrive` | MicroNut99's carts (default) or the Mega EverDrive PRO (16 MB, 32 banks of 512 KB) |
| `--ramcart-boot EDCD_<GAME>.32X` | EverDrive only: boot this launcher from the cart first, as the console does (docs/12) |
| `--frames n` | How many frames to run (60 = 1 s) |
| `--press frame:buttons` | From that frame on, **hold** `U D L R A B C X Y Z M S` (M = Mode, S = Start) until the next `--press`; `--press 720:` releases all. Repeatable |
| `--screenshot frame:file.ppm` | The picture at that frame. Repeatable |
| `--save-state frame:file`, `--load-state file` | Save a state at a frame / start from one |
| `--home dir` | Where the Sega CD's backup RAM is kept. Use a fresh folder per run so runs repeat |
| `--interpreter` | SH2 interpreter instead of the recompiler: 10–20× slower, but every instruction fetch goes through the SH2 cache model (needed for section 6) |
| `--wav file` | Record the sound |

## 3. Getting the owner's files into the container

The device tools (`device_stage_files`) copy files from the owner's folder to `/mnt/user-data/uploads/...`:
**at most 400 MB per file, 500 MB per call.** Stage the launcher, the cue and the bin; check the md5 against the
owner's copy (`md5sum` in the device shell) before you trust a run.

**A disc with music is too big** (Sonic CD: 825 MB, Hurrican: 527 MB). Use its `_NOMUSIC` disc if there is one,
or cut one on the owner's machine (device shell, into the game's `_to_delete/` folder - the device cannot delete
files, so everything temporary goes there and the owner deletes the folder): the data track whole, each audio
track's pregap and 4 seconds, a new cue to match. The CD commands still find every track.

```bash
# device shell, in the game's folder; prints the new length in sectors
python3 - <<'EOF'
import os
src, cue = 'disc/GAME.bin', 'disc/GAME.cue'          # the owner's disc
def msf(s): m, s_, f = map(int, s.split(':')); return (m * 60 + s_) * 75 + f
tr, cur = [], None
for line in open(cue):
    t = line.split()
    if t and t[0] == 'TRACK': cur = {'n': int(t[1]), 'type': t[2], 'idx': {}}; tr.append(cur)
    elif t and t[0] == 'INDEX': cur['idx'][int(t[1])] = msf(t[2])
total = os.path.getsize(src) // 2352; KEEP = 300; pos = 0
fmt = lambda x: '%02d:%02d:%02d' % (x // 75 // 60, (x // 75) % 60, x % 75)
out, f = open('_to_delete/short.bin', 'wb'), open(src, 'rb'); lines = ['FILE "short.bin" BINARY']
for i, t in enumerate(tr):
    start = t['idx'].get(0, t['idx'][1]); i1 = t['idx'][1]
    end = tr[i + 1]['idx'].get(0, tr[i + 1]['idx'][1]) if i + 1 < len(tr) else total
    lines.append('  TRACK %02d %s' % (t['n'], t['type']))
    if t['type'] != 'AUDIO':
        f.seek(start * 2352); out.write(f.read((end - start) * 2352)); lines.append('    INDEX 01 ' + fmt(pos)); pos += end - start
    else:
        n = min(KEEP, end - i1); f.seek(start * 2352); out.write(f.read((i1 - start + n) * 2352))
        lines.append('    INDEX 00 ' + fmt(pos)); lines.append('    INDEX 01 ' + fmt(pos + i1 - start)); pos += i1 - start + n
open('_to_delete/short.cue', 'w').write('\n'.join(lines) + '\n'); print(pos)
EOF
```

**Check the owner's folder is the code you think it is.** The owner's folders can be plain copies that mix
versions. Sonic CD 2011 (27 Oct 2026) "would not boot on the EverDrive": its folder held 17 files that were
older than GitHub's (R5C-era game code, the first EverDrive patch, no 68000 jump table in the engine), so the
disc the owner burned was not the code that had been tested. Before you test the owner's build, md5 every
tracked file in their folder against the repository (`git ls-files` → md5 list → `md5sum -c` in the device
shell); a file that matches no commit is work only they have - import it, never overwrite it.

## 4. Running

```bash
B=/home/claude/ares_build/ares-headless; BIOS=us.bin

# a disc on the 4 MB RAM cart (16 MB PCB: --ramcart-size 16)
$B --game disc.cue --machine cd32x --bios-us $BIOS --ramcart r/cart.ram --home r/home --frames 3600 \
   --press 2000:S --press 2020: --screenshot 1800:r/a.ppm --screenshot 3500:r/b.ppm > r/out.txt 2>&1

# the EverDrive PRO, started as on the console: the launcher first, then the disc (docs/12)
$B --game disc.cue --machine cd32x --bios-us $BIOS --ramcart-type everdrive --ramcart-boot EDCD_GAME.32X \
   --ramcart r/ed.ram --home r/home --frames 4800 $(for f in $(seq 300 300 4800); do echo --screenshot $f:r/s$f.ppm; done) \
   > r/out.txt 2>&1
```

Speed (2-core container, recompiler): Hurrican's EverDrive boot to its title, 4,800 frames, about 70 s. With
`--interpreter`, 7,600 frames take about 20–25 minutes: run those in the background.

**Pressing through a game.** Run once with screenshots every 120–300 frames and no presses, find the frame
where each screen appears, then add the presses (each `--press N:S` followed by `--press N+20:`). The timeline
is repeatable for the same files, BIOS and empty `--home`.

## 5. Reading the results

**The once-a-second line:**

```
t= 18s video=1415x243 comm: 002E 0300 1234 0381 0006 39F0 [12]=FFFF [14]=4001 audio rms=0.0166
```

`comm:` is COMM0, COMM2, COMM4, COMM6, COMM8, COMM10, then COMM12 and COMM14. COMM0 is the command the SH2 has
sent and the Sub-CPU has not finished (0 = idle); COMM12 is the stack's status (`4B10` = game running;
docs/09 for every code).

**The pictures** are PPM at 1415×243 (the frame's own aspect is 4:3). Make one contact sheet and look at it:

```bash
python3 - <<'EOF'
from PIL import Image
import glob
fs = sorted(glob.glob('r/s*.ppm'), key=lambda x: int(x.split('/s')[1][:-4]))
ims = [Image.open(f).resize((300, 225)) for f in fs]
c = 5; sheet = Image.new('RGB', (300 * c, 225 * ((len(ims) + c - 1) // c)))
for i, im in enumerate(ims): sheet.paste(im, ((i % c) * 300, (i // c) * 225))
sheet.save('r/sheet.png')
EOF
```

**A run that stops making `t=` lines while the process still takes 100% CPU is a hang, not a slow run.** ares
holds an SH2 that reads the cart while the 68000 has RV = 1 (`md/m32x/bus-internal.cpp`, `while(dreq.vram)`);
if the 68000 dies in the middle of an RV copy, RV never clears and ares spins there forever. Sonic CD's stale
EverDrive build did exactly that at t = 18 s, the loading bar empty: the engine copy had overwritten the
68000's jump table at cart `0x200`, and the first interrupt killed the 68000. Watch the output file's time;
kill the process by its PID (`pgrep -f '[a]res-headless'` - a bare `pkill -f ares` also matches your own shell).

**SH2 addresses:** a PC from a log or a crash screen → `sh-elf-addr2line -f -e <the game's .elf> 0x...`.

## 6. What ares does not show, and the RV probe

| The console does | ares does | So |
|---|---|---|
| Answers an SH2 cart read with **garbage** while the 68000 holds RV = 1 (docs/12, `edsave3`) | Since 28 Oct 2026: the same on the EverDrive PRO cart (a pseudo-random word); before, it held the SH2 until RV = 0 | Run with `--interpreter` and `ARES_RV_LOG=1` (below) |
| **Wipes** the EverDrive's PSRAM (banks 0–29) every time its menu runs | Since 28 Oct 2026: a launcher boot (`--ramcart-boot`) fills banks 0–29 with a pattern first, at power-on and at a reset; bank 31 (the saves) is kept | Every start loads everything again, as on the console |
| Stops on an SH2 **word access at an odd address** (address error, the red screen) | Since 28 Oct 2026: the same in the interpreter (`--interpreter`; `ARES_SH2_ADDRESS_ERRORS=0` off); the recompiler lets it through | OpenJazz's sun drawing (27 Oct 2026) ran in ares and stopped the console |
| The real timing of the CD drive, the 68000 and the SH2s | Faster SH2s, its own CD timing | A race can hide in ares |

**The RV rule** (built into the CD32X ares build since 28 Oct 2026; [`ares/rv-garbage-probe.patch`](../ares/rv-garbage-probe.patch)
was the local probe before it). With the EverDrive PRO as the cart, an SH2 cart read while RV = 1 returns a
pseudo-random word instead of waiting. `ARES_RV_LOG=1` logs it once per PC (and every 128th read);
`ARES_RV_GARBAGE=1` gives every cart the rule (the 16MB-PCB too), `ARES_RV_GARBAGE=0` holds the SH2 instead:

```
RV-CART M pc=0204c248 pr=060003d6 addr=0204c280 (n=1)
```

M / S = master / slave, `pc` the instruction, `pr` its caller, `addr` what it read. Run with `--interpreter`
(the recompiler fetches a block once, so code fetches during RV would not be seen). **A correct game logs nothing** from power-on to play: every command whose 68000
side copies with RV = 1 must keep both SH2s off the cart - the slave parked in SDRAM, the master waiting in
SDRAM with its interrupts masked, and the interrupt handlers in SDRAM too (docs/12; OpenLara's `csSpin`, the
Jazz-shaped games' `send_cmd_rv`).

Found with it (27 Oct 2026, Hurrican's EverDrive build): the master's vblank handler ran from the cart during the
high scores' copy (CMD 56, sent with a plain wait and interrupts on). After the fix: no line from boot to stage 1.
A run that logs nothing can still fail on the console for other reasons; a run that logs something has an SH2
on the cart during a copy, which the console answers with garbage.

## 7. Testing a fix without rebuilding the whole disc

The SH2 engine is one file on the data track (`OPENLARA.32X`, `SONICCD.PAK`, ...). If your rebuilt file has
the **same length** (the ROM images are padded to 32 KB chunks, so it usually does), write it over the old one
in a copy of the owner's disc and run that:

```python
# isopatch.py disc.bin LBA file - the file's bytes over the data track's user data from LBA on
import sys
b, lba, src = sys.argv[1], int(sys.argv[2]), sys.argv[3]
d = open(src, 'rb').read()
with open(b, 'r+b') as f:
    for i in range(0, len(d), 2048):
        f.seek((lba + i // 2048) * 2352 + 16); f.write(d[i:i + 2048])
```

The file's LBA comes from the ISO's root directory (primary volume descriptor at sector 16, root record at
byte 156; each record: LBA at +2, size at +10, name length at +32, name at +33). The EDC/ECC bytes are left
stale; ares does not check them (a burned disc would need the real build). Build the engine with the game's own
Makefile and the same toolchain flags: Hurrican's rebuilt engine matched the owner's byte for byte except its
build date and time. If `/opt/toolchains/sega/sh-elf` has no `libstdc++.a`, look for another sh-elf in the
container (`find / -name libstdc++.a | grep sh-elf`) and pass it as the Makefile's toolchain prefix.

## 8. Before you report

- Say what ares showed **and** what it cannot show (section 6). Never write "works" for something only ares ran.
- Check the owner's folder against what you tested (section 3), and give the build commands with the
  `cd /mnt/s/KoboPort/sega-toolchain-12.1/sega/<game>` line first.
- Nothing from the owner's machine goes into a repository: no BIOS, no discs, no game data.
- Do not name or link the owner's private repositories in any document (ares_android is public; the others are not unless the owner says so).
- Tell the owner which `_to_delete/` folders hold your temporary files.
