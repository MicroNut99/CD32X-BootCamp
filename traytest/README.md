# traytest: open and close the Sega CD's tray from the screen

The Sega CD **Model 1** has a motorised tray and **no eject button**: only software opens it (the Sega CD's own
screen, or a game). This disc is the test for a game doing it.

**T3 (2 Nov 2026): no cartridge needed.** The disc boots the system and runs the test through the whole 32X boot
stack (the Sub-CPU program, the 68000 listener, `INIT_32X`, the 32X boot ROM, the 0x0AAA / 0x0BBB handshake),
like every BootCamp disc. The only difference is that the test program is not copied into a cart: it runs from the
32X's own memory (SDRAM), in CDLOADER's place, as Kobo Deluxe (the root of this stack) did. Any cart in the slot
is left alone: the 4 MB RAM cart, the 16 MB cart, the Mega EverDrive PRO, or none.

- T2 had a game in the RAM cart, so it needed one (with no cart it stopped at `4DE2` with a black screen).
- T2 added the visible button presses (below); T1 had no sign of a press.

**Status: untested on a console (2 Nov 2026).** Run in ares only (below). The two commands it uses are the
ledger's section 22 proposals 84 and 90, built here for the first time.

## Build

From the BootCamp folder (WSL):

```
cd /mnt/s/KoboPort/sega-toolchain-12.1/sega/CD32X-BootCamp
bash /mnt/s/KoboPort/sega-toolchain-12.1/sega/CD32X-BootCamp/traytest/build_traytest.sh /mnt/s/KoboPort/sega-toolchain-12.1/sega/CD32X-BootCamp/disc CD32X_TRAYTEST_T3 2>&1 | tee /mnt/s/KoboPort/sega-toolchain-12.1/sega/CD32X-BootCamp/build_traytest_log.txt
```

It makes `disc/CD32X_TRAYTEST_T3.cue` + `.bin`. The second argument is the disc's name: give every disc you burn
its own. `ROOTDIR=` / `SEGA_TOOLCHAIN=` / `REGION=` work as for `build.sh`.

The build works in a temporary **copy** of the BootCamp's `stack/` and `tools/`:

1. `merge_tray.py` puts commands 84 and 90 into the copy's `stack/subcpu/main.c` behind `TRAYTEST ?= 0`, and a
   start-up switch behind `TRAY_NOCART ?= 0`: the stack's own no-cart mode (`KOBO_NOCART 1`), so the start-up
   cart check is not made.
2. The test program (`main.c`, `sdram.c`, hello's `cdapi.c`) is built like CDLOADER, with the stack's
   `stack/sh2/cdl_crt0.s` (the CD boot table the 32X boot ROM reads) and `cdloader.ld`, both unchanged. It then
   replaces CDLOADER in the copy. `sdram.c`'s start is CDLOADER's own start-up, line for line.
3. The copy's Sub-CPU program and disc image are made with `TRAYTEST=1 TRAY_NOCART=1` (as `build.sh` step 5).
4. The test tone goes on as track 2 (as `build.sh` step 6).

The BootCamp's own `stack/` is never touched (`check_boot.py` before and after). With both switches off, the merged
copy builds the same Sub-CPU program as the stack, byte for byte (`CDROMPlayer.bin` md5 checked, 2 Nov 2026).

## Run it

Sega CD + 32X. No cartridge needed; whatever cart is in the slot is not read or written. The test screen comes up
after the Sega CD logo.

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

What the screen shows (command 84, BIOS `CDBSTAT` 0x81, every frame):

| Line | What |
|---|---|
| The banner (large letters) | After a press: what the button is doing (3 s). Otherwise the drive's state in words: `TRAY OPEN`, `READING DISC`, `NO DISC`, `READY (PAUSED)`, `PLAYING`, `STOPPED`, `BUSY - WAIT` (the BIOS is carrying out a command) |
| BIOS | The BIOS status word and its meaning: 0000 stopped, 0100 playing, 0500 paused, 1000 no disc, 2000 reading the disc, 4000 **tray open**; BUSY = bit 15 (a command is being carried out) |
| DRIVE | The drive's own status word; its high byte is the drive's code: 00 stopped, 01 playing, 02 seeking, 04 paused, 05 **tray open**, 07 command error, 09 reading the table of contents, 0B no disc, 0E **tray moving** |
| TRACKS | First and last track of the disc in the drive (FF = not known) |
| LIGHTS | The drive lights word |
| TRACK / DISC | The current track and its time; the time on the disc |
| LAST | The last button, when (seconds since start), and the command's reply (R 01 = asked) |
| The list | Every change of the BIOS or drive code, and every button press, with the time in seconds (the last six) |

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

A photo of the screen after each step is the record to keep (the list keeps the last six changes).

## In ares (2 Nov 2026)

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
| `main.c` | The test screen (the SH2 side) |
| `sdram.c` | The start of the test program in SDRAM: CDLOADER's start-up handshake, then `main.c` (compiled as `tray_main`) |
| `build_traytest.sh` | The copy, the merge, the test program, the disc, the md5s |
