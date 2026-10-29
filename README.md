# CD32X BootCamp

Everything you need to boot **your own game on a Sega CD + 32X** ("CD32X") from one CD, with a
battery-backed RAM cart or a Mega EverDrive as a **RAM cart** in the 32X slot. This is the boot stack that OpenLara (Tomb Raider), Jazz
Jackrabbit, Hurrican and SuperTux run on. Here it comes as a clean kit, with a small example game that
builds and boots.

```
bash build.sh            # hello/ -> disc/BootCamp.cue + .bin, ready to burn
```

What the example shows on screen:

- the 32X frame buffer in 256 colours;
- the pad, which moves a box;
- a data file loaded from the CD into the RAM cart;
- CD audio (A plays, B stops);
- a save in the cart: a boot counter that survives power-off.

The example boots in emulation (see the [checks](#status) below). The stack under it is the code OpenLara
CD32X ran on the console (v1.0–v1.4).

**Something wrong?** Every number the disc can show is in [docs/09_ERROR_CODES.md](docs/09_ERROR_CODES.md).

## What you need

| | |
|---|---|
| Console | Genesis / Mega Drive + Sega CD / Mega-CD + 32X, and a **4 MB RAM cart** (battery-backed) in the 32X slot |
| Toolchain | Chilly Willy's Sega toolchain (`sh-elf` + `m68k-elf` GCC, `ldscripts/`, `bootblocks/US_BOOT.BIN`), default `/opt/toolchains/sega` (`ROOTDIR=` to change) |
| Host | Linux or WSL, `make`, `genisoimage`, Python 3 with Pillow |
| Burning | a CD-R and a burner that writes `.cue/.bin` (data + audio) |

## How to build

On Linux, or in WSL on Windows.

### 1. Install the host tools

```
sudo apt install build-essential genisoimage python3 python3-pil
```

### 2. Get Chilly Willy's Sega toolchain

You need `sh-elf/`, `m68k-elf/`, `ldscripts/` and `bootblocks/US_BOOT.BIN` (`EU_BOOT.BIN` / `JP_BOOT.BIN`
for those regions). The default place is `/opt/toolchains/sega`.

### 3. Build

```
cd /path/to/CD32X-BootCamp          # e.g. /mnt/c/... or /mnt/s/... in WSL
bash build.sh                       # hello/ -> disc/BootCamp.cue + disc/BootCamp.bin
```

**The result:** `disc/<DISC_NAME>.cue` + `.bin`: the data track, plus the CD audio tracks.
- Load the `.cue` in ares (*Mega CD 32X*, RAM cart on).
- Or burn the `.cue` (ImgBurn, or `cdrdao` on Linux) and run it on a Sega CD + 32X, with the 4 MB RAM cart
  in the 32X slot.

### 4. Every switch

All switches are environment variables set in front of `bash build.sh`, and any of them can be combined.

**The build** (`build.sh`):

| Switch | Default | What it does |
|---|---|---|
| `GAME=<folder>` | `hello` | The game folder to build, laid out like `hello/`: `Makefile` (makes `<game>.32x`), `splash.png`, `cd/` (data files), `music/TrackNN.wav` (CD audio). |
| `DISC_NAME=<name>` | `BootCamp` | The disc's file names: `disc/<name>.cue` + `.bin`. |
| `ROOTDIR=<folder>` | `/opt/toolchains/sega` | Where the toolchain is. |
| `SEGA_TOOLCHAIN=<folder>` | — | The same as `ROOTDIR`, and it also adds `<folder>/usr/bin` to `PATH` (for a `genisoimage` kept there). |
| `REGION=US` / `EU` / `JP` | `US` | The boot block for the console's region. Only US is tested. |
| `DIAG=1` | `0` | **The diagnostic disc.** It shows the 32X's white start-up lines and the Sub-CPU's green boot text from the SEGA logo on, in front of the boot picture. hello starts on that text, with a pad readout; **B+C** switches to hello's screen. The release lines are put back after the build. See [docs/09](docs/09_ERROR_CODES.md). |

**The Sub-CPU program** (`stack/subcpu/Makefile`). All are **off** by default; with all of them off, the
Sub-CPU code is the code OpenLara CD32X ran on the console. Each one is tested in emulation only.

| Switch | What it does |
|---|---|
| `STREAM=1` | CD read-ahead for file loads (CMD 46 / 47): about 2.5× faster in emulation |
| `CARTW=1` | Copies to the cart in words instead of bytes: about 3× faster |
| `FMVRING=1` | Ring mode in CMD 46, for movies that play while they stream (OpenLara) |
| `SLOTS=1` | CMD 61 bit 8 writes OpenLara's save-slot area at `0x060000`. That is inside this kit's ROM area: leave it off. |
| `CART=8` / `CART=16` | The 8 / 16 MB paged-cart test build, with CMD 68 |

**The game** (`hello/Makefile`): `build.sh` passes `DIAG` on to it as `HELLO_DIAG_TEXT`. Your own
game's Makefile may use it or ignore it.

**Set in files, not on the command line:**

| Where | What |
|---|---|
| `stack/subcpu/game_files.h` | `GAME_ROM_NAME` (8.3); `GAME_BRAM_NAME` (11 characters); `GAME_DATA_SLOT` / `GAME_RESIDENT_SLOT` (4–31: slot n = cart n × 128 KB); the data and movie file lists (at most 100 names). `build.sh` takes the ROM name and the size limits from here. |
| `stack/subcpu/main.c` | `KOBO_DIAG_TEXT`, `KOBO_BOOT_TEXT`: leave them at 0 and use `DIAG=1`. |

### 5. Examples

```
bash build.sh                                          # the release disc
DIAG=1 DISC_NAME=BootCampDiag bash build.sh            # the diagnostic disc
GAME=mygame DISC_NAME=MyGame bash build.sh             # your game
SEGA_TOOLCHAIN=/opt/sega REGION=EU bash build.sh       # another toolchain place, a PAL console
STREAM=1 CARTW=1 DISC_NAME=BootCampFast bash build.sh  # faster loads (emulation-tested only)
GAME=probe DISC_NAME=CD32X_PROBE_V1 EDCD=1 bash build.sh  # the timing probe (probe/README.md)
```

The EverDrive PRO programs build on their own (same `ROOTDIR=` / `SEGA_TOOLCHAIN=`):

```
bash edtest/build.sh      # edtest/EDTEST_W.32X + EDTEST_X.32X: the 16 MB RAM test (docs/10)
bash edw0/build.sh        # edw0/EDW0E_W.32X: window 0 as the SH2 reads it while the 68000 writes (docs/12)
bash edsave/build.sh      # edsave/EDSAVE1.32X: what the EverDrive keeps across its menu (docs/12)
bash edboot/build.sh      # edboot/EDBOOT.BIN, EDBOOTD.BIN, EDPROBE.BIN: the C-bit tests (docs/10)
EDCD=1 bash build.sh      # + edboot/EDCD.32X: the disc started from the EverDrive PRO (docs/12) - console-proven
```

### 6. What build.sh checks

It stops with a message at the first problem:

- the boot stack has changed (`tools/check_boot.py`);
- `game_files.h` breaks its rules;
- the game made no ROM, or more than one;
- the ROM is not a 32X ROM, or it runs into the first data slot;
- a data file fits neither slot;
- a release build still has a diagnostic line in it.

Each step is explained in [docs/05_BUILD_TEST_BURN.md](docs/05_BUILD_TEST_BURN.md).

## Mega EverDrive PRO as a 16 MB 32X RAM cart: tested on the console

**Result.** On a Genesis + 32X, a Mega EverDrive PRO in its "extended SSF" mode works as **16 MB of writable
RAM for a 32X program, set up in software**. The test ROM (`edtest/`, details in
[docs/10](docs/10_EVERDRIVE_TEST.md)):

1. turns on the EverDrive's mapper (`SEGA SSF` in the header, write bit in `0xA130F0`);
2. writes 27 banks of 512 KB (13.5 MB) with the 68000, with RV = 1, as the CD32X stack writes its RAM cart;
3. reads every bank back with the 68000 and with both SH2s, after all the others have been switched in and
   out.

| Test (console, Genesis + 32X + Mega EverDrive PRO) | CTRL0 | Result |
|---|---|---|
| `EDTEST_W.32X` | `0xA000` (P + W) | **2 full passes** (the second with every bit inverted), pass 3 under way at 226 s: all 27 banks OK, **0 bad words** (68000 and both SH2s) |
| `EDTEST_X.32X` | `0xE000` (P + X + W) | **1 full pass**, pass 2 under way: all 27 banks OK, **0 bad words** |

Both values work. `0xA000` is the one the Pro's documentation describes; the X bit (32X mode, from the older
EverDrive notes) changed nothing measurable. Photos: [`edtest/results/`](edtest/results/).

**How the 32X program uses it:**

| Register | Value | What it does |
|---|---|---|
| `0xA130F0` (CTRL0) | `0xA000` | protection + write enable, bank 0 in window 0 |
| `0xA130F2`–`0xA130FE` (CTRL1–7) | the bank, 1–27 | selects the bank in each 512 KB window, `0x080000`–`0x3FFFFF` |

- The 68000 writes these registers, and writes and reads the cart, with RV = 1.
- The SH2s read the cart at `0x22000000` (cache-through), as from any cart.
- Banks 28–30 hold the EverDrive's own things: never write them. Bank 31 is its battery RAM - the save
  memory the EverDrive OS keeps on the SD card (`bram.srm`) when the ROM's header declares save RAM; a game
  may use its first 256 KB (`EDCD_SAVE=1`, docs/12).
- Banks 0–29 are PSRAM and **the EverDrive wipes them every time its menu runs** (`edsave/`, 25 Oct 2026):
  nothing a game leaves there is seen again after a relaunch. Saves go to bank 31.

**The CD boot does not work with it: the EverDrive stays the cartridge.** `edboot/EDBOOT.BIN` tried to let go of
the cartridge line with CTRL0's C bit and start the Sega CD BIOS. On the console the C bit changes nothing
(`edboot/EDPROBE.BIN`, 25 Oct 2026): the EverDrive stays at `0x000000`, the Sega CD at `0x400000`.
"Run Mega-CD" is not the same either: it gives the disc the EverDrive's backup-RAM cart, not this RAM
(diagnostic disc: `SAFE CART: NOT FOUND`).

**So the disc boots from the cartridge instead: `edboot/EDCD.32X` - and it works on the console (25 Oct
2026).** `EDCD=1 bash build.sh` builds it with the disc. Started from the EverDrive menu with the disc in the
Sega CD, it starts the Sega CD itself (cartridge mode), as the BIOS and the boot block would, and hands over to
the same stack, built a second time with one switch (`MODE1=1`: the 68000 listener uses the cartridge-mode map
and treats the EverDrive the way the console demands). The same disc still boots the normal way. On the
console the BootCamp disc runs this way end to end - the game in the EverDrive, saves to it, data loaded from
the CD into it - and so does OpenLara CD32X ([docs/12](docs/12_CARTRIDGE_BOOT.md)).

**What the console demanded** (the day's findings, each measured with a probe ROM, all in docs/12):
- the EverDrive's write-enable bit (CTRL0 bit 13) must be **off** whenever the 68000 writes anything but the
  cart: with it on, some of the 68000's writes to work RAM, the VDP or the gate array land in the cart and
  change it, and the SH2's reads of the cart come back as zeros meanwhile (`edw0/EDW0E_W.32X` shows it in one
  screen). The listener turns it on only inside a copy into the cart, with RV = 1;
- **an SH2's cart access while the 68000 holds RV = 1 is not held until RV = 0 - the EverDrive answers it
  at once, with garbage** (`edsave3/EDSAVE3.32X`, 26 Oct: ~100% bad words through every such copy). So a
  game must keep both SH2s in SDRAM code with interrupts masked for every 68000 copy with RV = 1 (OpenLara
  EDSAVE-4); a loop that happens to be cached is luck, and the first cold cache in the game is a freeze;
- the cart's first 512 KB cannot be written by the 68000 directly with the 32X on: the copy goes through
  window 1 with window 0's bank mapped there for the block;
- copies between Word RAM and the cart go through a work-RAM buffer (Word RAM read with RV = 0, the cart
  written with RV = 1);
- nothing in the EverDrive's PSRAM survives its menu, so saves live in its bank 31, which the EverDrive OS
  writes to the SD card (`edsave/EDSAVE1.32X`; `EDCD_SAVE=1`).

`edw0/` is the probe that found the write-enable rule (`bash edw0/build.sh`, no disc): the SH2 reads the
EverDrive's window 0 while the 68000 does one thing at a time.

**In ares:** the EverDrive PRO 16 MB cart, with everything needed to add it to ares v148 (a patch, the hardware
model, the pitfalls, the tests): [docs/11](docs/11_ARES_16MB_RAM_CART.md) and [`ares/`](ares/).

## How it boots, in one picture

```
Sega CD BIOS ─► boot block (US_BOOT.BIN, from the toolchain)
   └─► APP.BIN on the Sub-CPU (stack/subcpu: crt0.s, main.c, scd.c ...)
         ├─► copies the listener (hw_md.s) to the Genesis 68000 - from now on it only obeys commands
         ├─► wakes the 32X and boots CDLOADER on both SH2s (stack/sh2, built into APP.BIN)
         │      CDLOADER: shows IMAGE.RAW, copies GAME.32X (your ROM, file 20) into the RAM cart,
         │      reads the ROM's 32X header and starts both SH2s at its entry points
         └─► serves the game's commands forever: files, pad, CD audio, PCM sound, saves
YOUR GAME: a normal 32X cartridge program, running from the RAM cart
```

The details are in [docs/01_HOW_IT_BOOTS.md](docs/01_HOW_IT_BOOTS.md).

## The folders

| Folder | What | Do you edit it? |
|---|---|---|
| `stack/subcpu/` | The Sega CD Sub-CPU program (APP.BIN) and the Genesis 68000 listener | Only `game_files.h`: your file names |
| `stack/sh2/` | CDLOADER, the SH2 program that loads your ROM into the cart | No (it is byte-identical to OpenLara's) |
| `hello/` | The example game: a 32X ROM using `cdapi.c` for every CD service | Copy it and make it yours |
| `probe/` | The CD32X timing probe: what each 32X access costs on the console (cart, frame buffer, FIFO, SDRAM, cache, the two SH2s on one bus, the VDP fill, a Sega CD command), measured by the SH2's own timer, for setting ares to the console's timing (`probe/README.md`) | No |
| `edtest/` | The EverDrive PRO 16 MB RAM test: two 32X cartridge ROMs (`bash edtest/build.sh`), and the console photos in `edtest/results/` | No |
| `edw0/` | The EverDrive PRO window 0 probe: the SH2 reads the cart while the 68000 writes; found the write-enable rule (`bash edw0/build.sh`) | No |
| `edsave/` | The EverDrive PRO retention probe: what survives its menu (`bash edsave/build.sh`). Found that the PSRAM is wiped at every menu run and that bank 31 goes to the SD card (`bram.srm`) | No |
| `edboot/` | `EDCD.32X`, the disc started from the EverDrive PRO (`EDCD=1 bash build.sh`); `EDBOOT.BIN` and the C-bit tests (`bash edboot/build.sh`) | No |
| `ares/` | The EverDrive PRO 16 MB cart for the ares emulator, as a patch for ares v148 | No |
| `tools/` | `prep_rom.py` (checks and marks the ROM), the boot picture, a test music track, the font | No |
| `docs/` | How it works, the commands, memory maps, rules learned on the console, how the existing games differ | Read it |
| `manuals/` | [MicroNut99/docs](https://github.com/MicroNut99/docs) as a submodule: the command ledger, the programming guide, the Kobo PDFs, the addenda, Sega's hardware manuals (`git submodule update --init`). The reading rules are in [CLAUDE.md](CLAUDE.md) |
| `build.sh` | Builds the game, the loader, the Sub-CPU program and the disc | `GAME=yourfolder` |

## Bringing your own game

1. **Build your game as a normal 32X cartridge ROM** with Chilly Willy's devkit (start from `hello/`). Its
   SH2 code may run from the cart, but its SDRAM image must be at most 192 KB.
2. **Use `cdapi.h` for everything that isn't SH2 or 32X work:**
   - data files: CMD 38 + 46, from the CD into the cart;
   - the pad: CMD 48;
   - CD music: CMD 45 / 54;
   - PCM effects: CMD 53 + 48;
   - saves: CMD 58 + 61, or the Sega CD backup RAM with 56–59.

   The Genesis 68000 belongs to the CD stack. Your cartridge's 68000 code never runs.
3. **List your files** in `stack/subcpu/game_files.h` and put them in `<game>/cd/`.
4. `GAME=<game> bash build.sh`, then burn `disc/BootCamp.cue`.

The full checklist is in [docs/02_PORTING_YOUR_GAME.md](docs/02_PORTING_YOUR_GAME.md).

## Documents

| | |
|---|---|
| [01 How it boots](docs/01_HOW_IT_BOOTS.md) | Power-on to your `main()`, step by step; who runs what |
| [02 Porting your game](docs/02_PORTING_YOUR_GAME.md) | What your ROM must look like, what to replace, a checklist |
| [03 Commands](docs/03_COMMANDS.md) | Every SH2 → Sub-CPU command of this stack, with arguments and replies |
| [04 Memory maps](docs/04_MEMORY_MAPS.md) | RAM cart, SDRAM, frame buffer, Word RAM, the save area |
| [05 Build, test, burn](docs/05_BUILD_TEST_BURN.md) | Toolchain, the build steps, emulation, the console, status codes |
| [06 Hardware rules](docs/06_HARDWARE_RULES.md) | The rules learned on real hardware: break one and the console hangs |
| [07 The methods compared](docs/07_METHODS_COMPARED.md) | How OpenLara, OpenJazz REV, Hurrican, SuperTux REV and Sonic CD 2011 use the stack |
| [08 Research index](docs/08_RESEARCH_INDEX.md) | Where to read more: MicroNut99's repositories and documents |
| [09 Error codes](docs/09_ERROR_CODES.md) | **Look up any number the disc shows**: the error box, the loader's codes, the green and white diagnostic text, the screen you are stuck on |
| [10 EverDrive PRO test](docs/10_EVERDRIVE_TEST.md) | Can a Mega EverDrive PRO be a 16 MB RAM cart? The test ROMs, the console results, and `EDBOOT.BIN` |
| [11 A 16 MB RAM cart for ares](docs/11_ARES_16MB_RAM_CART.md) | Everything needed to add the EverDrive PRO cart to ares: what is proven, the board, where it plugs in, the pitfalls, the tests |
| [12 Cartridge boot](docs/12_CARTRIDGE_BOOT.md) | The disc started from the EverDrive PRO (`EDCD.32X`): why, how, the `MODE1` switch, the console's three rules, what the screen colours mean - console-proven |

## Status

| What | Status |
|---|---|
| The stack (`stack/`) | The code OpenLara CD32X ran on the console (v1.0–v1.4). CDLOADER is byte-identical. With its default switches (`STREAM`, `CARTW`, `FMVRING`, `SLOTS` all 0), the Sub-CPU program compiles to the same code, apart from the file table (`game_files.h`) and the backup RAM file name. The four switches are tested in emulation only. |
| `hello/` + `build.sh` | Built and booted in emulation with a 4 MB RAM cart: the boot picture, the ROM loaded, `4B10`, file loads, the pad, the save surviving a second boot, CD audio, the error box, the diagnostic disc's text view. Not yet run on a console. |

## Credits

- **Joseph Fenton (Chilly Willy):** the Sega CD / 32X toolchain and the Sub-CPU / 68000 / SH2 support code
  this stack is built on (MIT, `stack/subcpu/LICENSE.md`).
- **Kobo Deluxe CD32X:** where the command set and the boot sequence come from. Its boot sequence was
  worked out from the Night Trap CD32X disc.
- **Victor Luchits and the D32XR team** (Doom 32X: Resurrection): CDLOADER was first written to boot D32XR
  from CD, and CMD 60 comes from that work.
- **XProger:** OpenLara, the first game on this stack.
- **MicroNut99:** the CD32X stack, the 4 MB RAM cart work, the ports, and this kit
  (https://github.com/MicroNut99/docs).

No game data, BIOS or disc images are included. Licences: [LICENSE.md](LICENSE.md). Sega, Mega Drive,
Genesis, Sega CD and 32X are Sega's trademarks; this project is not affiliated with Sega.
