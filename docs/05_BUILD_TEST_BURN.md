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
| 0 | `tools/check_boot.py`: the stack's files against `stack/BOOT_FILES.md5` | a stack file changed (only `game_files.h` and the two diagnostic switches may differ) |
| 1 | `make -C <game>` builds `<game>.32x`. `tools/prep_rom.py` then checks the header as CDLOADER will, writes `CDB2` at `0x3BC`, trims the padding and writes `stack/subcpu/ROMS/GAME.32X`. | not a 32X ROM; SDRAM image too big; entries not in SDRAM |
| 2 | `splash.png` → `IMAGE.RAW` (320 × 224, 256 colours) | |
| 3 | `<game>/cd/*` → `ROMS/` | |
| 4 | CDLOADER (`stack/sh2`) | |
| 5 | `APP.BIN` + `genisoimage` → `CDROMPlayer.iso` | CDLOADER or `GAME.32X` missing from the image |
| 6 | the ISO padded to at least 600 sectors, then `make_mixed_cd2.py` → `disc/<name>.cue/.bin` with `music/Track*.wav` as tracks 2… (with no music, a 4 s test tone is made) | |

**Sub-CPU build switches** (`make cd` in `stack/subcpu`, all on by default; every one is a HWFLAGS switch
in `main.c`):

| Switch | What it does |
|---|---|
| `STREAM=0` | Turns off the CD read-ahead for CMD 46 / 47. |
| `CARTW=0` | Copies to the cart in bytes instead of words. |
| `FMVRING=0` | Turns off movie ring mode. |
| `SLOTS=0` | Turns off OpenLara's slot area in CMD 61. |
| `CART=8` / `CART=16` | Builds the paged-cart test build with CMD 68. |

## 3. Test in emulation: ares CD32X

The ares CD32X fork (MicroNut99/ares_android, ares v148) adds the battery-backed RAM cart, including the
8 / 16 MB paged board. It is how every boot-stack change, and every new game, is checked before a burn.

> Note: ares_android is private at the time of writing. Ask MicroNut99 for the Windows build or the
> Android APK.

1. **Settings → Firmware → Mega CD**: your BIOS.
2. **Settings → Options → Mega Drive Settings**: *Mega CD 32X: RAM cart* is on. It defaults to the 16 MB
   cart, which a 4 MB game sees as a 4 MB cart.
3. **Load → Sega → Mega CD 32X**: `disc/BootCamp.cue`.

The cart is kept like the real one (`Systems\RAM Cart 16 MB-<n>.ram`, nine carts), so a save survives a
"power-off". Delete the file to wipe the cart. On Android, *Show fps and COMM12 STATE* shows your status
codes on the screen.

**Scripted runs.** `ares-headless` is in the same repository (`build-headless/`):

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
- **Sonic CD 2011's `tools/sh2emu`**: checks that no SH2 touches the cart while RV = 1. Run it before
  burning anything that changes the copy timing.

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
Turn the diagnostic text on:

1. In `stack/subcpu/main.c`, set `#define KOBO_DIAG_TEXT 1`. Set `KOBO_BOOT_TEXT 1` for the 32X's
   start-up messages too.
2. Rebuild. `check_boot.py` allows these two switches.

The green `STATE:` line then shows COMM12, and pad C hides or shows the text. See also
`DIAG_TEXT_HOWTO.md` in MicroNut99/docs.

| Last thing seen | Where it stopped | Look at |
|---|---|---|
| Black screen, no text, no boot picture | before CDLOADER: the boot block, `APP.BIN`, or the 32X did not wake up. (No `IMAGE.RAW` also leaves the screen black, but `STATE:` still shows.) | Is the boot stack unchanged (`check_boot.py`)? Is the 32X on? Is the disc burnt as `.cue/.bin`? |
| Black, no `STATE:` at all | the handshake (`0x0AAA` / `0x0BBB`) or CMD 52 never finished | the boot stack; the 32X |
| Boot picture, then it stops (`4D06`, `4C00`+n or `4DE2`) | the ROM copy into the cart | Is the cart seated, and is it the RAM cart? A stock emulator has no cart and stops here. |
| `4DE1` | `GAME.32X` not on the disc | `game_files.h` `GAME_ROM_NAME`; step 5 of the build |
| `4DE2` | a chunk read back wrong | the cart or its battery; the disc |
| `4DE3` | not a 32X ROM, or a bad header | `prep_rom.py` output; `crt0.s` first in the link |
| `4DE4` | the slave did not park | timing. Report it: this has not been seen with this loader. |
| `4D05`, then nothing | your `crt0.s` / `main()` | your entries at `0x3E0`; `.data` size; a wait on COMM that never comes |
| your own code, then a hang | a command that was never answered | a timeout on every wait; one writer per COMM register; music stopped before a load ([06](06_HARDWARE_RULES.md)) |
| `4B10` and a picture | it runs | |
