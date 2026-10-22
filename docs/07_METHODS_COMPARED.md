# 07 – The methods compared

How the existing CD32X games use the same base, what they all do the same way, and where they went
their own way. This chapter covers:

- the documents in **MicroNut99/docs** (Kobo, the command ledger, the addenda);
- **OpenLara CD32X REV**, which this kit's stack is taken from;
- **OpenJazz CD32X REV**;
- **Hurrican CD32X**;
- **SuperTux CD32X REV**;
- **Sonic CD 2011 CD32X**.

The repository states compared are listed in [08](08_RESEARCH_INDEX.md).

## 1. The family tree

```
Night Trap CD32X (Sega, 1994) ── its boot sequence, worked out by hand
   └─ Kobo Deluxe CD32X ─ the Sub-CPU coordinator, the 68000 listener, the command set (docs: KOBO-*.pdf)
        ├─ OpenLara CD32X ─ + CDLOADER (from D32XR): the game is a 32X ROM copied into the RAM cart
        │    ├─ OpenLara CD32X REV ─ + streaming, word copies, slots, movies   ◄── this kit's stack
        │    └─ OpenJazz CD32X (J19b) ─ the same loader, the quick check switched on
        │         ├─ OpenJazz CD32X REV ─ + Genesis plane, bonus floor (graphics chip), LZ packs
        │         ├─ Hurrican CD32X ─ OpenJazz's port layer, the slave as a worker
        │         │    └─ SuperTux CD32X REV ─ Hurrican's boot path + md5 check, Genesis VDP commands
        └─ Sonic CD 2011 CD32X ─ Kobo's own path: its own SH2 loader stays resident, engine in the cart
OpenTyrian 32X + Sega CD ─ a different architecture: a 32X cartridge with the CD attached (docs: addendum)
```

## 2. What every disc game does the same way

1. **The same start.** `genisoimage -generic-boot US_BOOT.BIN` → `APP.BIN` on the Sub-CPU → the listener
   (`hw_md.s`) copied to the Genesis 68000 at `0xFF1000` → `INIT_32X` → `"_CD_"` → the SH2 program in
   SDRAM → the COMM10 handshake `0x0AAA` / `0x0BBB`.
2. **The Sub-CPU is the coordinator; the 68000 is a listener.** Nobody hands the 68000 to the game. CMD 60
   (D32XR) was tried and dropped.
3. **The game runs from the RAM cart.** The 68000 writes it with RV = 1 and the SH2s read it like ROM.
   Every game keeps its hot code in SDRAM.
4. **The same core commands, numbered the same:**

   | CMD | Use |
   |---|---|
   | 38 | open |
   | 45 / 54 | CD audio |
   | 46 | chunk → cart |
   | 48 | pad + effect |
   | 51 / 52 | boot picture, Genesis display |
   | 53 | PCM pack |
   | 56–59 | backup RAM |

5. **CD audio for music.** Every game renders its music to CD tracks. None of them plays YM2612 music.
6. **Status codes in COMM12**, shown by the Sub-CPU's diagnostic text, with `4B10` meaning "running".
7. **The boot path is frozen.** Each team learned it separately and wrote it down.
8. **Emulation first, then the console.** ares with the RAM cart is the common test bed, and Kega Fusion
   stops at the boot picture.
9. **The same hardware rules** ([06](06_HARDWARE_RULES.md)).

## 3. Where they differ

### 3.1 Boot and loader

| | OpenLara REV (this kit) | OpenJazz REV | Hurrican | SuperTux REV | Sonic CD 2011 |
|---|---|---|---|---|---|
| SH2 boot program | CDLOADER | CDLOADER, 4 edits | CDLOADER, OpenJazz's edits | CDLOADER, OpenJazz's edits | its own loader, built into `APP.BIN` |
| What it loads | file 20 `OPENLARA.32X`, a 32X ROM with a header | file 20 `OPENLARA.32X` (module "OpenJazz") | file 20 `OPENLARA.32X` (module "OpenJazz 32X") | file 20 `OPENLARA.32X` (module "OpenJazz") | file 21 `SONICCD.PAK`, a headerless engine. It also loads the data pack and the first scene. |
| Hand-over | copies the ROM's SDRAM image over itself; both SH2s jump to the ROM's entries | same | same | same | the loader **stays resident** at `0x06000000`–`0x06002000` and calls `0x02000000`. It leaves a boot record at `0x06001F00` (file loader, cart checksum, the slave's mailbox) for the engine. |
| Quick check (skip the copy when the cart already holds it) | off: always copies | **on**: first / middle / last chunk; A+B+C forces the copy | on | on | warm start: files whose cart checksum matches are skipped |
| Boot picture | `IMAGE.RAW`, no bar | `IMAGE.RAW` + a progress bar | + bar | + bar | `IMAGE.RAW` + bar |
| ROM made by | engine Makefile + `prep_openlara_cd.py` | `make_oj_rom.py` (`mars_boot.bin` + program at `0x8000`) + prep | same as OpenJazz | same + `check_sdram_code.py` | engine linked to run at `0x02000000`, no 32X header |
| Boot path check | the build checks the loader is in OpenLara mode, the loader marker and the files on the disc, the module name, and the ROM / Caves sizes | build checks the module name | build refuses a wrong module name | `check_boot.py`: md5 of 70 files | sh2emu run before every burn |

**The BootCamp choice.** OpenLara's loader, with the quick check off, and its Sub-CPU program with the
v1.5–v1.7 switches off (`STREAM`, `CARTW`, `FMVRING`, `SLOTS`). That is the code proven on the console
(OpenLara v1.0–v1.4); the switches are emulation-tested only. A game that wants fast restarts can copy OpenJazz's quick check, but
that is a CDLOADER change: see rule 2 in [06](06_HARDWARE_RULES.md).

### 3.2 The Sub-CPU program (`main.c`)

| | OpenLara REV | OpenJazz REV | Hurrican | SuperTux REV | Sonic CD 2011 |
|---|---|---|---|---|---|
| Base | OpenLara's, with switches `STREAM`, `CARTW`, `FMVRING`, `SLOTS`, `CART`, `UB` | OpenJazz fork, 1639 lines | older OpenLara (no `STREAM` / `CARTW` / `SLOTS`) | Hurrican's, 1086 lines | Kobo's |
| Files: CMD 38 numbers | 20 ROM; 100+ levels → `0x200000`; 200+ Caves → `0x080000`; 300+ movies | 200–1199 `OJnnn.PAK` → `0x280000`; 1200–2199 `GBnnn.PAK` → `0x080000`; 2200+ / 3200+ compressed; 8 `IMAGE8.RAW` → `0x100000` | 200–1199 `OJnnn.PAK` → `0x280000` | same as Hurrican | 21 engine; 22 data; `0x1nn`–`0x4nn` scene images; `0x11nn`–`0x14nn` act files |
| Directory | read on every open | **cached root directory** (no disc read per open) | on every open | on every open | **cached root directory** (R4K) |
| CD reads | read-ahead streaming (`STREAM`) | streaming always on (R7) | a seek per chunk: the repository's `main.c` has no read-ahead | a seek per chunk | runs of up to 4 chunks per command (CMD 46 forms) |
| Cart copy (68000) | words (`CARTW`, MD 27) | bytes (MD 33) | bytes | bytes | MD 33 with `move.l` when aligned |
| Added commands | 61 bit 8 (slots), 68 (paging) | 67 Genesis plane, 69 LZ, 70 disc stamp, 71 bonus floor, 68 | 68 (paged build) | 62–67 Genesis VDP, 68 | 46 forms, 47 cart → frame buffer, 68 act → planes, 80 stamp floor |
| Listener (`hw_md.s`) additions | none (routines 1–35) | 36 → 46 routines (`genbg_md.s`, `bonusmd_md.s`) | none | 36 `set_vsram`, 37 `copy_vram_col` | picture record polling, vblank copy, an H-interrupt colour swap at the water line |

**Similar.** Every new command sits behind a build switch, gets a number from the ledger, and is
registered in it.

**Different.** The command numbers are not portable:

- 46, 47 and 68 mean different things on the Kobo path (Sonic) and on the OpenLara path.
- CMD 38 file numbers 200+ mean Caves in OpenLara but `OJnnn.PAK` in the OpenJazz family.

A Sub-CPU program belongs to one game ([03](03_COMMANDS.md)).

### 3.3 Cart use (details in [04](04_MEMORY_MAPS.md))

| | Method |
|---|---|
| OpenLara REV | **one resident + one level slot.** Caves (the title) stays at `0x080000`, with a record (length + checksum) in save piece 14. The level slot at `0x200000` is reloaded only when the level changes. Movies stream through a 60 × 32 KB ring in the level slot while they play. |
| OpenJazz REV | **packs by kind.** The engine, a Genesis-plane pack (`0x080000`), a common pack (`0x100000`, quick-checked) and a world pack (`0x280000`, loaded without read-back on a new planet). 8 / 16 MB: a world cache in the extra pages. |
| Hurrican | **resident pack + stage pack.** `IMAGE8.RAW` at `0x100000` (quick-checked). Stages at `0x280000` are skipped when the stage is already in the cart. |
| SuperTux REV | **two areas.** Area A at `0x120000` is kept. Area B (`0x280000`–`0x3F0000`) is a ring of pack homes, loaded without read-back. 16 MB: all 70 packs preloaded at the first start and checked against `SUMS.BIN` at each power-on. |
| Sonic CD 2011 | **scene streaming.** Up to a 2.5 MB scene image at `0x080000`. Only the 32 KB chunks whose CRC changed are loaded. The data pack is at `0x300000`, the act file at `0x380000`. |

**Similar.** Everyone keeps the engine at 0 and data on 128 KB slots, and avoids reloading what the
battery-backed cart already holds. The checks differ: compare chunks with CMD 47, or keep a record of
length + checksum.

### 3.4 The two SH2s

| | Master | Slave |
|---|---|---|
| OpenLara REV | game logic, half the drawing | the other drawing rows (`OVERLAP`). It also mixes the sound effects and the movie sound into the PWM, in its DMA interrupt. |
| OpenJazz REV | drawing | **game logic** (`OJ32X_SLAVE_LOGIC`, on by default; off while the Genesis plane is on) |
| Hurrican | draws the tiles | runs the game steps: `SlaveJob {fn, arg, go, done}` in SDRAM, cache-through, a purge per job, its own 8 KB stack. While a job runs, the master queues music requests. |
| SuperTux REV | game | `plat_parallel` jobs: cell drawing, line moves |
| Sonic CD 2011 | engine | the loader's mailbox job loop at `0x26001F40`; used for the 32X picture mode ("2ND SH2", off at power-on) |

**Similar.** Only the master talks to the Sub-CPU, and the slave gets work through SDRAM, cache-through.
**Different:** what work the slave gets (drawing, logic, sound, or jobs).

### 3.5 Pictures: the 32X only, or the Genesis VDP too

| | Method |
|---|---|
| OpenLara REV, Hurrican | the 32X frame buffer only; the Genesis display is off (CMD 52) |
| OpenJazz REV | **Genesis plane** behind the 32X picture: tiles packed at build time (≤ 1600 patterns, 4 palettes). The 68000 loads them from the cart through `0x900000`; scrolling comes from COMM4 / COMM6 every vblank. **Bonus floor:** the Sega CD graphics chip draws into Word RAM (2M mode), and the 68000 copies it to VRAM. |
| SuperTux REV | the level on Genesis plane A (64-column ring), the sky on plane B, the 32X sprites in front (PRI = 1). The SH2 sends VRAM / CRAM / VSRAM work as commands 62–67, and the 68000 copies straight from the cart. |
| Sonic CD 2011 | the whole Genesis picture: plane A level, plane B parallax, hardware sprites, the window plane for the floor. The SH2 writes a record into the frame buffer; the 68000 picks it up and writes it in vblank. Stamp floor through the graphics chip (CMD 80). |

**Similar.** The SH2 decides what changes, and the 68000 only copies, in vblank, never reading the cart
inside vblank. **Different:** how the work gets to the 68000:

| Game | Path |
|---|---|
| OpenJazz | listener routines started by commands |
| SuperTux | one command per VDP job |
| Sonic | records polled from the frame buffer |

### 3.6 Sound, saves, pad

| | Music | Effects | Saves | Pad |
|---|---|---|---|---|
| OpenLara REV | CD audio, tracks up to 84 | 32X PWM, mixed on the slave (bank in the level file); Sega CD PCM packs as the fallback | cart piece 0 (settings, game) + 14 (records) + its 16 slots at `0x060000` | CMD 48 every frame |
| OpenJazz REV | CD tracks 2–41 | PCM `SFX00` / `SFX01` (CMD 53), no PWM | cart pieces 0–10 | CMD 48 sent at the frame's end, collected later; 6-button |
| Hurrican | CD tracks 2–24 (from IT modules) | PCM `SFX00.BIN`, no PWM | backup RAM high scores (`OPENJAZZ_JJ`) + cart piece 10 | CMD 48, polled at most every 4 ms |
| SuperTux REV | CD tracks 2–13 | PCM, 20 effects | cart piece 13. Backup RAM "never worked on the console". | CMD 48 |
| Sonic CD 2011 | CD audio | PCM banks (CMD 53) | **backup RAM only** (`SONICCD32X_`, 512 bytes) | CMD 48 |

### 3.7 Build and test

| | Build | Tests before a burn | On the console |
|---|---|---|---|
| OpenLara REV | `build.sh`: packer → engine → ROM prep → loader → Sub-CPU → disc | ares runs (level loads, triggers), PC build | yes, up to v1.6 (v1.7: emulation only) |
| OpenJazz REV | `make_disc.sh` → `build.sh`; the packs are made by running the PC build | frame md5 regression (`regress.sh`), Genesis test ROM pixel check | R1 (20–22 fps); R7–R9 emulation only |
| Hurrican | `build.sh` / `make_disc.sh`, music rendered with libxmp-lite | PC host tests: frames, ASan over 20000 frames, reachability, demo recorder | P1–P1q |
| SuperTux REV | `check_boot` → convert → … → `make_mixed_cd2.py`; stops if a paged build lacks its switch | PC model, ares headless runner | REV5, REV10b; REV11 emulation only |
| Sonic CD 2011 | `build.sh` is PC-side checks only; the disc is a separate set of steps | **sh2emu** must report "no problems found" (it checks SH2 cart access while RV = 1); ares for the picture | up to R4B (R5G per the ledger); R5E untested |

**Similar.** A PC build of the same engine catches game bugs. Emulation catches stack and timing bugs,
but not all of them. The console is the only final word. **Different:** how much is automated before the
burn (SuperTux and Sonic the most).

### 3.8 The other architecture: a cartridge with the CD attached (OpenTyrian)
OpenTyrian 32X boots from a **32X cartridge** with the Sega CD attached (Mode 1).

- The Genesis 68000 is the coordinator (`cart_md.s`).
- The Sub-CPU runs D32XR's `cd.bin` and only serves CD requests.
- No RAM cart is needed. The game is in the cartridge, and the CD holds music and data.

Its commands 60–63 (CD find, CD read, SRAM) clash with this stack's numbers. Use it when you ship a
cartridge. Use this kit when the whole game must come from one CD. See `CD32X_ADDENDUM_OPENTYRIAN.pdf`.

## 4. What BootCamp takes from each

| From | What |
|---|---|
| Kobo (docs) | the command set, the boot sequence, the listener |
| OpenLara REV | the whole stack, unchanged; the file table idea, generalised as `game_files.h` |
| SuperTux REV | the md5 boot check (`tools/check_boot.py`) |
| Hurrican, OpenJazz | the slave-worker pattern and the quick check, as documented patterns ([02](02_PORTING_YOUR_GAME.md)), not code |
| Sonic CD 2011 | the warning that the Kobo-path command numbers differ; the 2M Word RAM rule |
| All of them | the hardware rules ([06](06_HARDWARE_RULES.md)) and the save-piece ownership table |
