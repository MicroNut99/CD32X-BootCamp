# CD32X BootCamp

Everything you need to boot **your own game on a Sega CD + 32X** ("CD32X") from one CD, with a
battery-backed **4 MB RAM cart** in the 32X slot. This is the boot stack that OpenLara (Tomb Raider), Jazz
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

On Linux, or in WSL on Windows:

1. **Install the host tools:**

   ```
   sudo apt install build-essential genisoimage python3 python3-pil
   ```

2. **Get Chilly Willy's Sega toolchain.** You need `sh-elf/`, `m68k-elf/`, `ldscripts/` and
   `bootblocks/US_BOOT.BIN`. The default place is `/opt/toolchains/sega`.

3. **Build:**

   ```
   cd /path/to/CD32X-BootCamp                                      # e.g. /mnt/c/... in WSL
   bash build.sh                                                   # toolchain in /opt/toolchains/sega
   SEGA_TOOLCHAIN=/path/to/sega bash build.sh                      # toolchain somewhere else
   GAME=mygame DISC_NAME=MyGame bash build.sh                      # your own game folder
   DIAG=1 DISC_NAME=BootCampDiag bash build.sh                     # the diagnostic disc (docs/09)
   ```

4. **The result:** `disc/BootCamp.cue` + `disc/BootCamp.bin`, which hold the data track plus CD audio.
   - Load the `.cue` in ares (*Mega CD 32X*, RAM cart on).
   - Or burn the `.cue` with ImgBurn and run it on a Sega CD + 32X with the 4 MB RAM cart in the
     32X slot.

`build.sh` first checks that the boot stack is unchanged (`tools/check_boot.py`). It stops with a message
at the first problem. Each step is explained in [docs/05_BUILD_TEST_BURN.md](docs/05_BUILD_TEST_BURN.md).

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
| `tools/` | `prep_rom.py` (checks and marks the ROM), the boot picture, a test music track, the font | No |
| `docs/` | How it works, the commands, memory maps, rules learned on the console, how the existing games differ | Read it |
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
- **Victor Luchits and the D32XR team** (Doom 32X: Resurrection): CDLOADER began as D32XR's CD loader.
- **XProger:** OpenLara, the first game on this stack.
- **MicroNut99:** the CD32X stack, the 4 MB RAM cart work, the ports, and this kit
  (https://github.com/MicroNut99/docs).

No game data, BIOS or disc images are included. Licences: [LICENSE.md](LICENSE.md). Sega, Mega Drive,
Genesis, Sega CD and 32X are Sega's trademarks; this project is not affiliated with Sega.
