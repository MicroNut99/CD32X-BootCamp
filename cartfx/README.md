# cartfx: the CD32X cart compositor (Mega EverDrive PRO FPGA)

One engine on the EverDrive PRO's FPGA for three games: **Hurrican CD32X**, **Jazz Jackrabbit CD32X REV** and
**SuperTux CD32X REV**. The master SH2 describes a frame in commands (layers, sprites, the cells to redraw). The engine
composes it, 16 x 8 pixels at a time, into the cart's own SRAM. The SH2 then copies the finished cells into the 32X
frame buffer. It grew from Rayman CD32X's compositor (on the console since 8 Nov 2026) and
keeps its window, key and command encoding. The design and each game's plan are in the design document
("FPGA accelerators for Hurrican, Jazz and SuperTux CD32X") and in each game's `fpga/` folder.

**Status, 14 Nov 2026: built (v2.3), never on a console, used by no game yet.** It is checked against the reference
model in Verilator and fitted by Quartus for all three games, each with the cart's PSRAM read in 5 clocks and in 8 (the
console test says which the cart needs). A green simulation or ares run is never a console pass.

| Folder | What |
|---|---|
| [`model/`](model/) | `cartfx.h`: **the specification**, to the bit (window, key, commands, status, counters). `cartfx.c`: the reference model, plain C. The same C goes into a game's PC build and into ares |
| [`rtl/`](rtl/) | `cfx_core.sv`: the engine. `map_cfx.sv`: krikzz's `map_ssf` instantiated unchanged, its outputs taken over only once the key is read |
| [`sim/`](sim/) | `tb_cfx.cpp`: the engine against the model in Verilator, with the console's reads cutting in |
| [`quartus/`](quartus/) | the six builds: `hub_<game>.sv` (krikzz's `hub.sv`, one line changed), `setup_cfx.sh`, `build_cfx.bat`, krikzz's licence |
| [`test/`](test/) | `CFXTEST.32X`, the console test: a 32X ROM started from the EverDrive menu with a core beside it (below) |

## The builds

One source; `KEY1` and `ID` name the game (`hub_<game>.sv`), `PSRAM_T` the cart's PSRAM read: 5 clocks (Rayman's
compositor reads so on the console) or 8 (Dracula X CD32X's core: `ROM_LAT` 8). Counter 9 reads it back. The game
turns the engine on with three reads of the command port, `KEY0` (0x4346), its `KEY1`, then `KEY2` (0x0C01, the
version). The status's high byte is the ID, so a game refuses the wrong `.rbf`.

| Build | KEY1 | ID | Read | Logic elements | Worst setup slack | Worst hold slack | `mega-pro.rbf` md5 |
|---|---|---|---|---|---|---|---|
| `hur` Hurrican | 0x5848 `XH` | 0xC1 | 5 | 7,467 (48 %) | +4.244 ns | +0.114 ns | `ed22e589b33b5c9e356c81cd5f3768e1` |
| `oj` Jazz REV | 0x584A `XJ` | 0xC2 | 5 | 7,438 (48 %) | +3.015 ns | +0.105 ns | `4a1a7f3c6adc6fe3195b6fef43db3ec2` |
| `stx` SuperTux REV | 0x5853 `XS` | 0xC3 | 5 | 7,449 (48 %) | +4.230 ns | +0.138 ns | `8143053db26f06a9123fa4eaad9b60de` |
| `hur8` Hurrican | 0x5848 `XH` | 0xC1 | 8 | 7,452 (48 %) | +3.648 ns | +0.138 ns | `f2f90f31f52674d72fe91231a0780f53` |
| `oj8` Jazz REV | 0x584A `XJ` | 0xC2 | 8 | 7,423 (48 %) | +3.729 ns | +0.102 ns | `68db3ba73088650c87d57fe2de719305` |
| `stx8` SuperTux REV | 0x5853 `XS` | 0xC3 | 8 | 7,416 (48 %) | +4.526 ns | +0.107 ns | `c6bbbfd1d2d647c28e13872f802c99d2` |

v2.3, Quartus 25.1std Lite, EP4CE15F23C8, 50 MHz, 14 Nov 2026: timing met at every corner (the slacks above are the
worst of the three), 197,792 memory bits (38 %) each. krikzz's `map_ssf` alone is 1,754 LEs and 114,688 memory bits.
The SHA-256 of each (ares knows the cores by it) and those of v2.2 and v2.1 (read in 5, no counter 9):
`quartus/RBF_SHA256.txt`.

To build them again: `bash quartus/setup_cfx.sh <work>`, where `<work>/mega-ed-pub` is krikzz's tree (on the owner's
PC, `fpga_work`). It lays out `fpga_pro/map_cfx_<build>` (krikzz's `map_ssf` project with its two mapper lines
replaced by four) and `mapper/map_cfx_<build>`, and puts `build_cfx.bat` beside them. Double-click that `.bat`: it
writes each build's log, exit code, md5, fit and timing summaries, and `cfx_out\<build>\mega-pro.rbf` (about a
minute a build). **Read the timing summary before using an `.rbf`; never use one whose timing failed.**

## How a game uses it

- The `.rbf` goes beside the game's EDCD launcher (`.32X`) on the SD card. The EverDrive menu loads it in place of its
  built-in mapper. Without the file, or before the key, the cart is krikzz's SSF mapper exactly, and the game must
  run as it does today (its fallback).
- **The window** is cart 0x3E0000-0x3FFFFF (SH2: + 0x22000000), whatever page window 1 shows, so the game's packer
  keeps the **last 128 KB of every 2 MB page free** (Rayman's rule). Never bank 31 (the EverDrive's save RAM) and
  never page 7.
- The picture: 0x3E0000 + y x PW + x (PW = 16 x cols, at most 336 x 192); the cell map (which cells this frame
  composed) at 0x3EFC00. The command port: a 16-bit read of 0x3F0000 + 2w hands the engine the 15-bit word w and
  returns the status. Counters (frames, cells, sprites, the engine's time, accesses redone, words lost) are read at
  w = 1-8, for the game's STATUS page.
- A frame: `BEGIN`, `LAYER` / `SCROLL` / `SPRITE` / `CELLS` as needed, `GO`, then `SHOW` the buffer it composed.
  Commands run in the order they came. GO composes into the buffer not shown. **Read the window only once the
  status's shown bit says the buffer you asked for**, and poll with pauses (BootCamp `docs/06` rule 7).
- **Hold the engine while the 68000 writes the cart** (a level load): wait until the status is not busy, and send
  nothing until the load is done (Metal Slug CD32X's v53/v54 lesson).
- The tables the engine reads (pixels, tiles, maps in PSRAM, the 64 KB blend table, the shade table) are the game's,
  in its pack. The engine writes only its SRAM: the two pictures (words 0-0xFFFF), and the maps and broken bits the
  game puts there with `UPLOAD` and `POKE`.

## How it works

Rayman's front end with Metal Slug CD32X's console fixes. A command read's address is taken only once it has held
steady for two clocks. The console wins every memory at once, decided from the bus pins with no clock in between, and
its picture reads go straight to the SRAM's pins. Any engine access the console overlapped, or came into within two
clocks after, is done again. PSRAM reads hold address and OE for `PSRAM_T` clocks (5: Rayman's console-proven read;
8: Dracula X's), back to back. SRAM writes go from a 128-word queue in block RAM. Reads that must be exact (map entries, broken bits) are
sampled twice and kept only when both samples agree.

A frame is composed a cell at a time in block RAM: zeros; the layers under the sprites, 0-3; the sprites in the order
they came; the over layers. Then the cell's 64 words go to the SRAM. A layer is a bitmap or a tile map (tiles of any
size from 2 to 32 pixels; 4 or 8 bits a pixel; the map in PSRAM or SRAM; h-flip, broken bits, a colour offset, the
16 x 256 shade table). Sprites are keyed, or blended through a 64 KB table of their own. Four PSRAM words are held,
read up to two ahead in the direction the run goes and no further than the run needs. A pixel takes a clock while its
word is there, an empty tile span is passed in one clock, and the sprite list is scanned a sprite a clock.

Measured in the simulation (`sim/`, the cart's 50 MHz, the engine alone), on test frames deliberately heavier than the
games':

| Frame | Cells | Layers | Engine time |
|---|---|---|---|
| Hurrican: 2 bitmaps, 2 tile maps (tiles of 10), 10-24 sprites | 440 (all) | 4 | 14.9-18.3 ms |
| Jazz: a sky, 2-3 tile maps of 32 in SRAM, 7-22 sprites | 76-260 | 3-4 | 3.6-12.0 ms |
| SuperTux: a bitmap, a tile map of 16, 33-38 small sprites | 87-189 | 2 | 4.1-9.2 ms |

With the console reading the cart 30 % of the time, a full Hurrican frame took 18.7 ms. A word of PSRAM every
`PSRAM_T` + 1 clocks (one chip, OE pulsed as Rayman's) is the limit: with 5, about 3 clocks a pixel at 8 bits, 1.5 at
4 bits; with 8, 4.5 and 2.25, and frames take 1.3-1.5 times as long (the console test's frames with the console reading
60 % of the time: 2.3-47.9 ms against 1.4-37.2).

## Checked so far

- `sim/`: `make && TRAFFIC=<% of console reads> ./obj/Vcfx_core <frames>`. The SRAM after each frame is compared
  with the model's word for word, the counters and the error bit too, and 64 words are read back through the window
  as the SH2 would read them. **0 words differ**: 12 frames each at 0, 30 and 85 % console traffic and two more seeds;
  the Jazz and SuperTux builds (`-GKEY1 -GID`) 6 frames each. `make t8` builds the engine with `PSRAM_T` 8 in `obj8/`.
  The bench's PSRAM gives the right word only once the address has been on the pins `PSRAM_T=n` clocks (garbage
  before): the engine with 5 is right with `PSRAM_T=5` and wrong with 6; with 8, right with 8 and wrong with 9.
  Debug: `-DDBGPUB --public-flat-rw` with `DBGHIST=1` (clocks per state), `DBGRUN`, `DBGFROM`, `DBGW`.
- Quartus: the table above.
- The model's estimate of the engine's time (`cfx_t est`, for ares's busy bit, from the build's read `psram_t`):
  `EST=1` prints it beside the engine's. Within 0-20 % of the engine alone, on the slow side, with 5 and with 8
  (13-14 Nov 2026); the console's reads add 10-20 %.
- **Not checked: the console.** First check: each game's current build with its `.rbf` beside the launcher must run
  exactly as without it (the engine stays off). Then a game's STATUS page with the engine on.

## The console test (`test/`)

`bash test/build.sh` makes `test/CFXTEST.32X` (a plain script, no make: the owner's toolchain, `ROOTDIR=` as the
main build): 9 MB, the program and the script in the first 2 MB, the test pictures again from 0x880000 (the menu loads
the file whole and in order, so they are in the second PSRAM chip). Put it in one folder on the SD card with any of
the six `mega-pro.rbf` and start it from the menu, 32X attached; no CD is needed. It:

1. tries the three keys and names the core that answered (its ID) and its PSRAM read (counter 9: `T 5` or `T 8`; a
   v2.2 core shows `T 5 OLD`), or says that none did;
2. plays a script of 8,748 command words (`test/tools/cfxgen.c`: 8 frames - whole and partial, all four layer
   kinds, tiles of 2 to 32, maps in PSRAM and SRAM, broken bits, shade, h-flip, sprites flipped, blended, clipped,
   257 of them for the error bit - from the pictures in the first chip, then the same 8 from their copy in the
   second) over test pictures made there (no game's), checking each frame word for word against what the reference
   model made of the same script: the picture, the cell map, the error bit, counters 2 and 3; each picture is shown
   for a second; the results in two columns, CHIP 0 and CHIP 1;
3. times three ways of copying 320 x 192 of the picture into the frame buffer (word reads, long reads, cached line
   fills): the copy cost every game's frame budget rests on (design step M0);
4. plays the script again and again while the slave SH2 reads the cart without a pause (window and ROM): frames
   wrong, the engine's accesses done again (counters 5, 6), words lost (7), the slowest frame.

One screen of results to photograph (the digits are drawn by hand: 0 and 8, 5 and S stay apart on a television).
Checked: `sim/` plays the same script into the engine (`SCRIPT=build/cfxtest_data.bin SCRIPT1=build/cfxtest_chip1.bin
TRAFFIC=60`, `PSRAM_T=5 ./obj/Vcfx_core` and `PSRAM_T=8 ./obj8/Vcfx_core`): 16 checks right with each, the SRAM full
of other garbage than the model's (every word the engine reads is written by the script first); without `SCRIPT1` the
8 checks from the second chip fail. ares (`ares_android` branch `fpga-cartfx`, the owner's toolchain's build, md5
`34f65ea3...`): all six v2.3 cores found with their read, all 16 checks right, the stress loop 400-470 checks right
in 40 s; without a core, "NO CARTFX CORE ANSWERED". **Not yet run on the console**: the kit with the six cores, one
folder each, is on the owner's PC (`cartfx_work\sd2`).

## Next

1. The console: `CFXTEST.32X` with `hur` and `hur8` at least (photograph its screen, then again after a minute of
   the stress loop). It settles the PSRAM read: the games keep the faster one that is right on both chips. Then the
   transparency check above; then each game's STATUS page.
2. Jazz's side behind `OJ32X_FX=1` (EDCD build only), then SuperTux's, then Hurrican's (the design document's order).
   ares models the engine (`ares_android` branch `fpga-cartfx`, never `main` without the owner): it knows the cores
   by the `.rbf`'s SHA-256, answers once keyed, stays busy for the model's estimate after a GO (from the core's read
   time), holds the words sent meanwhile as the cart's queue does, and traps a full queue and the 68000 writing the
   cart while the engine composes.

MIT licence for `rtl/` and `quartus/`, as krikzz's mega-ed-pub that they sit beside
([`quartus/LICENSE-krikzz-mega-ed-pub`](quartus/LICENSE-krikzz-mega-ed-pub)); `map_ssf.sv` is krikzz's, copied
unchanged by `setup_cfx.sh` and not kept here.
