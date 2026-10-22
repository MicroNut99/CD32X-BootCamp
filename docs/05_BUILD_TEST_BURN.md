# 05 – Build, test, burn

## 1. Tools

| What | Where |
|---|---|
| Chilly Willy's Sega toolchain: `sh-elf-gcc`, `m68k-elf-gcc`, `ldscripts/`, `bootblocks/US_BOOT.BIN` | `/opt/toolchains/sega` by default. The existing games use the GCC 12.1 build (`sega-toolchain-12.1`). Point at another path with `ROOTDIR=`. |
| `make`, `genisoimage`, Python 3 + Pillow | `sudo apt install build-essential genisoimage python3-pil` (Linux or WSL) |
| An emulator with the RAM cart | ares CD32X (below). A stock emulator has no RAM cart: the disc stops at the boot picture. |
| A Sega CD BIOS | your own dump (US / JP / EU). It is not included. |

## 2. Build

```
bash build.sh                       # hello/  -> disc/BootCamp.cue + BootCamp.bin
GAME=mygame bash build.sh           # your folder, laid out like hello/
GAME=mygame DISC_NAME=MyGame bash build.sh
```

| Step | What happens | Stops when |
|---|---|---|
| 0 | `tools/check_boot.py`: the stack's files against `stack/BOOT_FILES.md5` | a stack file changed (`game_files.h` may differ; so may the diagnostic lines of `tools/diag_switch.py`, but a release build refuses them, see 3b). `game_files.h` holds anything but #defines and 8.3 names; the ROM name is not 8.3; a slot is outside 4–31; the backup-RAM name is not 11 characters; more than 100 names. Your game's `crt0.s` and `src-md/` are not checked: keep their start-up code as in `hello/`. |
| 1 | `make -C <game>` builds `<game>.32x`. `tools/prep_rom.py` then checks the header as CDLOADER will, writes `CDB2` at `0x3BC`, trims the padding and writes `stack/subcpu/ROMS/<GAME_ROM_NAME>`. | no or several `.32x`; not a 32X ROM; SDRAM image too big; entries not in SDRAM; the ROM runs into the first slot |
| 2 | `splash.png` → `IMAGE.RAW` (320 × 224, 256 colours) | |
| 3 | `<game>/cd/*` → `ROMS/` | a file too big for both slots |
| 3b | release disc: `diag_switch.py release`. `DIAG=1`: `diag_switch.py on` (put back on exit) | a diagnostic line or a `.diagsave` is left in the stack (run `python3 tools/diag_switch.py off`, or `git checkout` the file) |
| 4 | CDLOADER (`stack/sh2`) | |
| 5 | `APP.BIN` + `genisoimage` → `CDROMPlayer.iso` | CDLOADER or the ROM missing from the image |
| 6 | the ISO padded to at least 600 sectors, then `make_mixed_cd2.py` → `disc/<name>.cue/.bin` with `music/Track*.wav` as tracks 2… (with no music, a 4 s test tone is made) | |

**Sub-CPU build switches.** These are set on `build.sh` or on `make cd` in `stack/subcpu`. Each one is a
HWFLAGS switch in `main.c`. **All are off by default**: with all of them off, the Sub-CPU code is the code
OpenLara ran on the console. Each one has been tested in emulation only.

| Switch | What it does |
|---|---|
| `STREAM=1` | CD read-ahead for CMD 46 / 47: loads about 2.5× faster in emulation |
| `CARTW=1` | Copies to the cart in words instead of bytes (about 3× faster) |
| `FMVRING=1` | Movie ring mode in CMD 46 (OpenLara's streaming movies) |
| `SLOTS=1` | CMD 61 bit 8 writes OpenLara's slot area at `0x060000`. That is inside this kit's ROM area: leave it off. |
| `CART=8` / `CART=16` | The paged-cart test build, with CMD 68 |

**Other build options:**

| Option | What it does |
|---|---|
| `REGION=EU` / `JP` | The toolchain's EU / JP boot block. The default is US, and only US is tested. |
| `DIAG=1` | The diagnostic disc ([09](09_ERROR_CODES.md)) |

**`stack/BOOT_FILES.md5`** holds md5s of the files with CRLF turned into LF and any diagnostic line of
`tools/diag_switch.py` put back to its release form. Check it with `tools/check_boot.py`, not `md5sum -c`.

## 3. Test in emulation: ares CD32X

The ares CD32X build (ares v148 with a RAM cart) adds the battery-backed RAM cart, including the
8 / 16 MB paged board. It is how every boot-stack change, and every new game, is checked before a burn.


1. **Settings → Firmware → Mega CD**: your BIOS.
2. **Settings → Options → Mega Drive Settings**: *Mega CD 32X: RAM cart* is on. It defaults to the 16 MB
   cart, which a 4 MB game sees as a 4 MB cart.
3. **Load → Sega → Mega CD 32X**: `disc/BootCamp.cue`.

The cart is kept like the real one (`Systems\RAM Cart 16 MB-<n>.ram`, nine carts), so a save survives a
"power-off". Delete the file to wipe the cart. On Android, *Show fps and COMM12 STATE* shows your status
codes on the screen.

**Scripted runs.** The ares CD32X build comes with a headless runner, `ares-headless`:

```
ares-headless --game disc/BootCamp.cue --machine cd32x --bios-us us.bin \
              --ramcart cart.bin --ramcart-size 4 --frames 3600 \
              --press 2000:A --screenshot 2500:a.ppm --screenshot 3500:b.ppm --wav out.wav
```

It prints the COMM registers once a second, so you can watch COMM12 go `4D01` … `4D05` → your codes →
`4B10`. Keep `cart.bin` between runs to test what happens on a second boot.

**What a pass means.** The code is right. It does not prove the console will run it:

- The SH2s run faster in emulation.
- The RV timing and the paging hardware are not settled.
- A hang that needs a torn read (HW10) cannot happen in emulation.

The other tools:

- **Kega Fusion**: boots only to the boot picture (no RAM cart).
- **An SH2-side checker** (Sonic CD 2011 uses one): checks that no SH2 touches the cart while RV = 1. Worth
  having before burning anything that changes the copy timing.

**What hello does in ares** (4 MB cart):

| Step | Result |
|---|---|
| Boot | boot picture → `4D01` … `4D05` → `4E00`, `4E01` → `4B10` |
| First boot | text and box, `LEVEL00.BIN` read from the cart, `BOOTS 1` |
| C | loads `LEVEL01.BIN` |
| A | plays track 2 |
| B | stops it |
| Pad | moves the box |
| Second run with the same cart file | `BOOTS 2` |

## 4. Burn and run on the console

1. **Burn** `disc/<name>.cue` (with its `.bin`): ImgBurn on Windows, or `cdrdao write --driver generic-mmc-raw
   disc/<name>.cue` on Linux. Use a good CD-R at a low speed.
2. **Set up the console**: Genesis + Sega CD + 32X, with the **4 MB RAM cart in the 32X's cartridge slot**.
   With a normal cartridge in the slot, the console boots the cartridge.
3. **Power on**, then start the CD from the BIOS menu.
4. **Every boot** copies the ROM into the cart while the boot picture shows (a few seconds; this loader's quick check is off).

## 5. When it does not boot

1. **Look up what the screen shows** in [09](09_ERROR_CODES.md). It covers:
   - the screen you are stuck on;
   - the loader's codes;
   - hello's red error box: the code, the `BC-Ex` tag, the COMM registers.
2. **Build the diagnostic disc** and burn it:

   ```
   DIAG=1 DISC_NAME=BootCampDiag bash build.sh
   ```

   `tools/diag_switch.py` puts four diagnostic lines into the stack for that build only, and puts the
   release lines back afterwards. With them, from the SEGA logo on, you see:
   - the 32X's white start-up lines;
   - the Sub-CPU's green text, in front of the boot picture: every boot step, `STATE:`, every file opened,
     every cart chunk with its address, the cart probe, any 68000 crash.

   A boot that stops leaves its last lines on screen. Once hello runs, it stays on that text, shows a
   one-line pad readout in words, and switches to its own screen with **B+C**. Each row is explained in
   [09](09_ERROR_CODES.md) §5.
3. **In ares**, the same disc shows COMM12 at any moment, with no burn needed.

See also `DIAG_TEXT_HOWTO.md` in MicroNut99/docs.
