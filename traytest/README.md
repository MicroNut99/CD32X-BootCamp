# traytest: open and close the Sega CD's tray from the screen

The Sega CD **Model 1** has a motorised tray and **no eject button**: only software opens it (the Sega CD's own
screen, or a game). Most Model 1 units are said not to open from a game. This disc tests one unit: it asks the
BIOS to open and close the tray. Whether that works or not, **a photo of the screen is the record**: the screen
carries everything needed to tell why.

**T5 (3 Nov 2026): also as a 32X cartridge ROM, `CD32X_TRAYTEST_T5.32X` - no disc needed.**
- It is the BootCamp's EDCD launcher (`edboot/`, [docs/12](../docs/12_CARTRIDGE_BOOT.md)), unchanged.
- Inside it: a cartridge-boot build of the same Sub-CPU program (`MODE1=1`), and this test as its SH2 program.
- Start it from the **Mega EverDrive PRO** with a CD in the tray or none: it never reads the disc.
- Its Sub-CPU program also leaves out the start-up disc read (`TRAY_NODISC=1`, below). The tray may hold a
  music CD or nothing, and on a music CD that read never ends (ares, 3 Nov 2026).
- The subtitle says `(CART)` or `(DISC)`, so a photo shows how the test was started.
- In a cartridge boot the BIOS ROM is at 0x400000, so the header is read at 0x400100.
- The disc version is built as well (`CD32X_TRAYTEST_T5.cue` / `.bin`) and needs no cart.
- RESULT counts `CLOSE OK: READ, PLAYING` too: after CLOSE with a music CD, the BIOS may start playing it.

**T4 (2 Nov 2026): the screen is the record.**
- A RESULT line says in words what OPEN or CLOSE did (`OPEN OK IN 2.3 S`, `OPEN FAILED: NO CHANGE, 10 S`, ...).
- The screen also shows the BIOS's whole status table in hex and the BIOS ROM's own name, version and region.
- It shows the console's version registers, a fingerprint of the Sub-CPU BIOS, and the last five changes with
  their times.
- Digits and hex letters have their own clear shapes: in the old font 0, 8 and B were hard to tell apart.
- The subtitle reads `MODEL 1: OPEN/CLOSE TEST`.

**T3 (2 Nov 2026): no cartridge needed.** The disc boots the system and runs the test through the whole 32X boot
stack (the Sub-CPU program, the 68000 listener, `INIT_32X`, the 32X boot ROM, the 0x0AAA / 0x0BBB handshake),
like every BootCamp disc. The only difference is that the test program is not copied into a cart: it runs from the
32X's own memory (SDRAM), in CDLOADER's place, as Kobo Deluxe (the root of this stack) did. Any cart in the slot
is left alone: the 4 MB RAM cart, the 16 MB cart, the Mega EverDrive PRO, or none.

- T3 (no cart needed): T2 had a game in the RAM cart, so it needed one (with no cart it stopped at `4DE2` with a
  black screen).
- T2 added the visible button presses (below); T1 had no sign of a press.

**Status: untested on a console (3 Nov 2026).** Run in ares only (below). The two commands it uses are the
ledger's section 22 proposals 84 and 90, built here for the first time. T4 adds three reads to command 84.
None of them changes anything:
- the BIOS ROM's header, read by the 68000 with listener routine 21 at 0x100-0x1FF (not read by the stack before);
- the gate array's $FF8000 and the Mega Drive's $A10000;
- the Sub-CPU BIOS's jump table at 0x5F16 and 0x5F22.

## Build

From the BootCamp folder (WSL):

```
cd /mnt/s/KoboPort/sega-toolchain-12.1/sega/CD32X-BootCamp
bash /mnt/s/KoboPort/sega-toolchain-12.1/sega/CD32X-BootCamp/traytest/build_traytest.sh /mnt/s/KoboPort/sega-toolchain-12.1/sega/CD32X-BootCamp/disc CD32X_TRAYTEST_T5 2>&1 | tee /mnt/s/KoboPort/sega-toolchain-12.1/sega/CD32X-BootCamp/build_traytest_log.txt
```

It makes `disc/CD32X_TRAYTEST_T5.32X` (the cartridge ROM) and `disc/CD32X_TRAYTEST_T5.cue` + `.bin` (the disc).
The second argument is the name: give every disc and ROM its own. `ROOTDIR=` / `SEGA_TOOLCHAIN=` / `REGION=` work as for `build.sh`.

The build works in a temporary **copy** of the BootCamp's `stack/`, `tools/`, `edboot/` and `edtest/src/`:

1. `merge_tray.py` puts commands 84 and 90 into the copy's `stack/subcpu/main.c` behind `TRAYTEST ?= 0`, and a
   start-up switch behind `TRAY_NOCART ?= 0`: the stack's own no-cart mode (`KOBO_NOCART 1`), so the start-up
   cart check is not made.
2. The test program (`main.c`, `sdram.c`, hello's `cdapi.c`) is built like CDLOADER, with the stack's
   `stack/sh2/cdl_crt0.s` (the CD boot table the 32X boot ROM reads) and `cdloader.ld`, both unchanged. It then
   replaces CDLOADER in the copy. `sdram.c`'s start is CDLOADER's own start-up, line for line.
3. The copy's Sub-CPU program and disc image are made with `TRAYTEST=1 TRAY_NOCART=1` (as `build.sh` step 5).
4. The test tone goes on as track 2 (as `build.sh` step 6).
5. The cartridge ROM (T5):
   - the test program again, built with `TRAY_FROM_CART=1` (the screen says CART);
   - the Sub-CPU program again, with `MODE1=1` (as `build.sh EDCD=1`) and `TRAY_NODISC=1`. That switch is in the
     merge too: the start-up's disc read (`InitCD`: DRVINIT, the wait for the track list, the search for the
     volume descriptor) is not made. The test reads no files; its CLOSE button does DRVINIT itself;
   - the copy's `edboot/build_edcd.sh` around both.

The BootCamp's own `stack/` is never touched (`check_boot.py` before and after). With the switches off, the merged
copy builds the same Sub-CPU program as the stack, byte for byte, both the disc's and the `MODE1=1` one
(`CDROMPlayer.bin` md5s checked, 3 Nov 2026).

## Run it

- **The cartridge ROM** (`CD32X_TRAYTEST_T5.32X`): Sega CD + 32X + Mega EverDrive PRO. Copy it to the SD card,
  start it from the EverDrive's menu. No disc needed; any CD in the tray is fine. The screen says `(CART)`.
  Until the first CLOSE, the BIOS may show `READING DISC` (2000): this start-up leaves the drive alone.
- **The disc** (`CD32X_TRAYTEST_T5.cue`): Sega CD + 32X, no cartridge needed; whatever cart is in the slot is not
  read or written. The test screen comes up after the Sega CD logo. The screen says `(DISC)`.

- **LEFT / RIGHT** choose a button (yellow edge), **A** (or C, or START) presses it.
- **You see every press:**
  - While A is held, the button is pushed in (white, two pixels lower).
  - When it acts (on release), it turns **green** for half a second (asked), or **red** (refused: the BIOS is
    busy, or no answer).
  - For three seconds the large banner under the buttons says what it is doing: `OPENING THE TRAY`,
    `CLOSING THE TRAY`, `PLAYING TRACK 2`, `STOPPING MUSIC`, or in red `BUSY - TRY AGAIN` / `NO ANSWER`.
- **OPEN** - command 90, COMM2 = 1: BIOS `DRVOPEN` (0x0A). The drive stops and the tray opens.
- **CLOSE** - command 90, COMM2 = 0: BIOS `DRVINIT` (0x10, parameters `01 FF`). The BIOS closes an open tray,
  reads the disc's table of contents, and holds the drive paused at track 1.
- **PLAY 2** - command 45: CD audio track 2 (the test disc's tone; or track 2 of any music CD you put in).
- **STOP** - command 54.

PLAY and STOP are refused while the BIOS says it is busy ("BUSY - WAIT"): the stack waits for a free BIOS
before those two, with no time limit.

What the screen shows, top to bottom (command 84; the BIOS's status table is `CDBSTAT` 0x81's, at 0x5E80):

| Line | What |
|---|---|
| The banner (large letters) | After a press: what the button is doing (3 s). Otherwise the drive's state in words: `TRAY OPEN`, `READING DISC`, `NO DISC`, `READY (PAUSED)`, `PLAYING`, `STOPPED`, `BUSY - WAIT` (the BIOS is carrying out a command) |
| BIOS | The BIOS status word and its meaning: 0000 stopped, 0100 playing, 0500 paused, 1000 no disc, 2000 reading the disc, 4000 **tray open**; BUSY = bit 15 (a command is being carried out) |
| DRIVE | The drive's status word. Its high byte is the drive's code: 00 stopped, 01 playing, 02 seeking, 04 paused, 05 **tray open**, 07 command error, 09 reading the table of contents, 0B no disc, 0E **tray moving**. The low byte only says which report the drive is sending and changes all the time |
| TRACKS, LIGHTS | First and last track of the disc in the drive (FF = not known); the drive lights word |
| LAST | The last button, when (seconds since start), and the command's reply (R 01 = the BIOS was asked) |
| RESULT | What the last OPEN or CLOSE did, in words (below) |
| Two lines of hex (cyan, white) | The BIOS's whole 32-byte status table, bytes 0-15 and 16-31, as it is now. Refreshed six times a second |
| `BR ...` (yellow) | The BIOS ROM's header, read by the 68000: its serial and version (`BR 000003-1.10`), region (`U`, `J`, `E`), date (`(C)SEGA 1992.OCT`) |
| `SEGA-CD BOOT ROM...` (yellow) | The header's name line: build date, time and version (`10/11-1992 18:30 1.10`) |
| `MD .. GA .... SUB ...... ....` | The Mega Drive's version register ($A10001), the Sega CD gate array's register $FF8000 (LEDs, chip version), and where the Sub-CPU BIOS's `_CDBIOS` and `_BURAM` jumps go. Those two addresses differ from one BIOS version to the next, so they name the BIOS even if the header line shows dots |
| The list | The last five changes of the BIOS word or the drive's code, and every button press: seconds, BIOS word, drive word, what |

**RESULT**, after OPEN (watched for 10 s) or CLOSE (20 s):

| Shown | Meaning |
|---|---|
| `OPEN OK IN x S` (green) | The drive said "tray open" (05), or the BIOS 4000 |
| `OPEN: WAITING x S` / `OPEN: TRAY MOVING x S` (yellow) | Still watching; the drive has said "tray moving" (0E) |
| `OPEN FAILED: NO CHANGE, 10 S` (red) | Neither the BIOS word nor the drive's code changed at all |
| `OPEN FAILED: MOVED, NOT OPEN` (red) | The drive said "tray moving" but never "open" |
| `OPEN FAILED: LAST DRIVE xx` (red) | Things changed, but not to "open"; xx = the drive's last code (07 = command error) |
| `CLOSE OK: DISC READ IN x S` (green) | The disc was read (BIOS 2000 or drive 09 seen), then paused (0500, drive 04) |
| `CLOSE OK: READ, PLAYING, x S` (green) | The disc was read, then the BIOS started playing it (0100): a music CD can do that |
| `CLOSE OK: NO DISC, x S` (green) | The tray closed with no disc in it (BIOS 1000 or drive 0B) |
| `CLOSE: READING DISC x S` (yellow) | Still watching |
| `CLOSE FAILED: NO CHANGE, 20 S` / `... LAST DRIVE xx` (red) | As for OPEN |
| `...: NO ANSWER (SUB-CPU)` (red) | Command 90 got no reply |

## What to try, in this order

1. **OPEN** with the test disc in. Expected on a Model 1: the drive stops (DRIVE 00), the tray moves (0E), the
   tray is open (BIOS 4000, DRIVE 05).
2. **CLOSE** with nothing changed. Expected: tray moving (0E), reading the table of contents (09; BIOS 2000), then
   paused (BIOS 0500, DRIVE 04, TRACKS 01-02). **PLAY 2**: the test tone.
3. **OPEN**, take the disc out, **CLOSE** with an empty tray. Expected: no disc (BIOS 1000, DRIVE 0B).
4. **OPEN**, put a music CD in, **CLOSE**, **PLAY 2**: that CD's track 2 plays. The test keeps running from the
   32X's memory through all of this.
5. Optional: the same on a Model 2. Its lid has its own button; whether `DRVOPEN` does anything there is not
   known.

**Photos.** Take one at the end of every step, when RESULT has its final colour (green or red), whether it worked or
not. A unit that opens is as useful a record as one that does not: the BIOS lines say which BIOS versions open.
For a unit that does not open, these show why:
- the list (did the drive do anything, and when);
- the RESULT line;
- the status table (the drive's own answer as the BIOS keeps it);
- the BIOS lines.

The list keeps the last five changes.

## In ares (3 Nov 2026)

T5's cartridge ROM, started the way the console starts it (ares's EverDrive PRO with the ROM as its boot program):
- With the test disc in the tray: runs. OPEN gives `OPEN FAILED: LAST DRIVE 07` (no tray in ares), CLOSE gives
  `CLOSE OK: DISC READ`. The BIOS header is read at 0x400100: `BR 000003-1.10  U`.
- With a music-only CD in the tray: runs, CLOSE reads it (tracks 01-01).
- Before `TRAY_NODISC`, with the music CD it never got past the start-up (the volume-descriptor read).
- ares cannot run with no disc at all.

T5's disc with no cart: unchanged, `(DISC)`.

### 2 Nov 2026

T4 in ares with no cart: OPEN gives `OPEN FAILED: LAST DRIVE 07` (red), as expected with no tray (below). CLOSE
then gives `CLOSE OK: DISC READ IN 0.4 S`. The BIOS lines read `BR 000003-1.10  U  (C)SEGA 1992.OCT` and
`SEGA-CD BOOT ROM10/11-1992 18:30 1.10`; `MD 94  GA 0201  SUB 002E1C 481E` (ares's values). PLAY 2 and STOP work.

ares (MicroNut99/ares_android, US BIOS 1.10) runs T3 the same way with **no cart**, the **4 MB** RAM cart, the
**16 MB** cart and the **Mega EverDrive PRO** in the slot (the disc booting): the screen, the buttons, the
pushed-in and green states, every command answered (R 01).

**ares has no tray.** Its drive answers the open and close commands with "command error" (07), so after OPEN the
BIOS stays busy (8500) instead of reporting 4000. Without an OPEN first, CLOSE (`DRVINIT`) reads the disc again
(2000 → seeking → paused 0500, tracks 01-02), and PLAY 2 plays track 2 (0100, DRIVE 01, the tone audible).
So ares checks the commands and the screen, not the tray: only the console can.

## Files

| File | What |
|---|---|
| `merge_tray.py` | Commands 84 and 90 (`TRAYTEST ?= 0`) and the no-cart start-up (`TRAY_NOCART ?= 0`) into a copy of the stack; proves the blocks strip back byte for byte |
| `main.c` | The test screen (the SH2 side), with its own clear digits for hex |
| `sdram.c` | The start of the test program in SDRAM: CDLOADER's start-up handshake, then `main.c` (compiled as `tray_main`) |
| `build_traytest.sh` | The copy, the merge, the test program, the disc, the md5s |
