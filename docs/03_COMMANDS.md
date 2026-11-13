# 03 – Commands (SH2 → Sub-CPU)

These are the commands of **this stack** (`stack/subcpu/main.c`, the OpenLara path). Other stacks give some
of the same numbers other meanings: see the clash table at the end. The full registry, with history and
status, is the command ledger in https://github.com/MicroNut99/docs: the Kobo PDF plus
`CD32X_COMMAND_LEDGER_ADDITIONS.md`.

## How to send one

**Registers.** The master SH2 only (the slave never sends) uses these, at their cache-through addresses:

| Register | Address | Use |
|---|---|---|
| COMM0 | `0x20004020` | the command (low byte). The Sub-CPU writes 0 when it is done. |
| COMM2 | `0x20004022` | the first argument |
| COMM8 / COMM8L | `0x20004028` (16 / 32 bit) | the second argument, or the reply |
| COMM12 | `0x2000402C` (write 16 bit only: COMM14 is the Sub-CPU's heartbeat) | the reply flag of some commands; otherwise your status code |

**Steps.**

1. Write COMM2 / COMM8.
2. Clear COMM12 if the command replies there.
3. Write COMM0.
4. Wait for COMM0 == 0, with a pause between reads and a timeout (`cd_cmd` in `hello/src/cdapi.c`).
5. Read the reply.

**The other COMM registers:**

| Register | Rule |
|---|---|
| COMM4 | Free for master ↔ slave messages. |
| COMM6 | Yours. In OpenLara it is the renderer's: nothing else may write it. |
| COMM10 | Start-up handshake only, then part of CMD 48's reply. |
| COMM14 | The Sub-CPU's. Never write it. |

## The table

| CMD | Name | Arguments | Reply | Notes |
|---|---|---|---|---|
| **38** | open file | COMM2 = file number (`game_files.h`) | COMM8L = length, 0 = not on the disc | Also sets the cart slot for the next CMD 46: file n → slot n × 128 KB, or the slot `game_files.h` gives it |
| **46** | chunk → cart | COMM2 = chunk (32 KB); bit 15 = skip the read-back. COMM8L = the file's length | COMM12 = 0 ok, 1 bad | The 68000 writes the cart with RV = 1; the SH2s' cart reads wait meanwhile. Clear caches afterwards. Switches (off by default here, emulation-tested only): `FMVRING` bit 14 = ring mode (movies), reply \| 0x100; `CARTW` word copy (3× faster); `STREAM` read-ahead. |
| **47** | chunk compare | as 46 | COMM12 = 0 same, 1 different | Nothing is written; used for "is it already in the cart?" |
| **48** | pad (+ sound effect) | COMM0 high byte = PCM effect (0 = none); COMM2 = its volume | COMM8 = pad 1 (`SEGA_CTRL_*`, `0xF000` = no pad); COMM10 = the 68000's tick counter, or pad 2 when the Sub-CPU program is built `PAD2=1` (ledger 23) | Answered first in the Sub-CPU's loop. Send it once a frame. |
| **45** | play CD track | COMM2 = track; bit 15 = play once (else repeat) | — | Disc track N. Rule: no CD data reads while it plays. |
| **54** | stop CD audio | — | — | Send it before any CD data read (`cd_load` does) |
| **53** | load PCM effects | COMM2 = n: `SFXnn.BIN` | COMM12 = number of effects (0 = failed) | Packs as made by OpenLara's `make_sfx_packs.py` (8-bit samples, a table) |
| **52** | Genesis display | COMM2 = 1 on / 0 off | — | Off: only the 32X picture shows. CDLOADER switches it off. |
| **51** | file chunk → frame buffer | COMM2 = chunk | COMM12 + COMM14 overwritten (chunk, sectors) | The SH2 first gives the frame buffer to the 68000 (FM = 0). Chunk c goes to byte `0x200 + c × 32 KB`. Used for `IMAGE.RAW`. Post your status again after it. |
| **50** | file chunk → frame buffer (16 KB, off-screen) | COMM2 = chunk | — | Kobo's no-cart mode; not needed with a cart |
| **56** | backup RAM: load | — | COMM12 = 1 ok, 0 no file | Fills the Sub-CPU's 4 KB buffer from the backup-RAM file `GAME_BRAM_NAME` |
| **57** | buffer: get word | COMM2 = word 0–2047 | COMM8 = word | |
| **58** | buffer: put word | COMM2 = word 0–2047, COMM8 = word | — | Fills the buffer for 59 or 61 |
| **59** | backup RAM: save | — | COMM12 = 1 ok | Writes the 4 KB buffer as a backup-RAM file (64 blocks: half the internal backup RAM). SuperTux reports that backup-RAM saving never worked on its console; cart saves (61) did. |
| **61** | buffer → cart save piece | COMM2 = piece 0–15 | COMM12 = 1 ok | Cart `0x3F0000 + piece × 4 KB`, written by the 68000 and read back. Only the low 4 bits count. (`SLOTS=1`, off by default: bit 8 writes OpenLara's slot area at `0x060000`, which is inside this kit's ROM area.) |
| **44** | (clear Genesis planes) | — | — | Answered; does nothing in this build |
| **60** | 68000 → cartridge code | — | — | The D32XR experiment: hands the 68000 to the ROM's Genesis code and ends the CD stack. **Never** use it with this loader. |
| **68** | cart page, window 1 | COMM2 = page 1–7 | COMM12 = 1 done, 0 refused | Only in `CART=8` / `CART=16` builds, for the 8/16 MB paged cart. Clear caches after it. |
| **62** | VRAM word | COMM2 = VRAM byte address (even); COMM8 (high 16) = the word | — | **Only in `GENPLANE=1` builds** (62-67: SuperTux CD32X REV's Genesis picture commands, ledger §1, §20). MD 5 `set_vram` |
| **63** | cart → VRAM | COMM2 = VRAM byte address; COMM8L = words << 22 \| cart offset (even; 1–1023 words) | COMM12 = 1 ok, 0 refused | The 68000 reads the cart through `0x900000` (RV 0): the SH2 is never locked out. MD 7 `copy_vram` |
| **64** | cart → CRAM | COMM2 = first colour 0–63; COMM8L = count << 22 \| cart offset | COMM12 = 1 ok, 0 refused | MD 12 `set_palette` |
| **65** | VDP register | COMM2 = `0x8000 \| reg << 8 \| value` (registers 0–23) | — | MD 34 `set_vdp_reg` |
| **66** | V scroll | COMM2 = VSRAM byte address (0 plane A, 2 plane B); COMM8 (high 16) = the value | — | MD 36 `set_vsram` (GENPLANE) |
| **67** | cart → VRAM map columns | COMM2 = VRAM address of the first column's top word; COMM8L = (columns − 1) << 30 \| words a column << 22 \| cart offset | COMM12 = 1 ok, 0 refused | 1–4 columns of 1–255 words, 128 bytes apart (a 64-cell plane); all inside one 1 MB bank of the window. MD 37 `copy_vram_col` (GENPLANE) |
| **78** | V scroll line split | COMM2 = index 0–15: COMM8L = entry (line << 20 \| V scroll A << 10 \| V scroll B); COMM2 = `0x8000 \| count`: show that list, its first count entries (0 = off) | — | **Only in `VSPLIT=1` builds (needs `GENPLANE=1`).** Entry 0 = the top of the picture, the others in line order. The Genesis H interrupt (every line, level 4; `$A12006` → `hint_vs`, and in a `MODE1` (EDCD) build also the RAM jump table's H-INT entry at `$FFFD0C` → `hint_vs`: a cartridge boot takes its vectors from the cart, where `$A12006` redirects nothing; ledger §21b, 7 Nov 2026) writes VSRAM where an entry starts and puts entry 0 back after line 223. **Two lists (D5):** entries go into the list not shown; a commit while the splits run takes effect at the end of the picture, the vblank at which a 32X swap asked for after it is shown (the first commit shows at once). Level 4 is masked only while a listener routine that touches the VDP runs (not 3, 11, 19-22), so a split due then can land late on that picture. MD 38 `set_vs_line` |
| **79** | The Genesis VDP from the 32X frame buffer | 0: run the list at frame buffer offset COMM8L; 1: the commit (run what the lists kept, write the pairs); 2 / 3: the list, then the 68000 shows 32X frame buffer 0 / 1 (FBCTL FS) and runs the kept part from its vblank interrupt at the vblank that shows it | COMM12 = 1 ok, 0 a bad list | **Only in `FBVDP=1` builds (needs `GENPLANE=1 VSPLIT=1`).** The SH2 writes a list of runs into the frame buffer it draws in (`0x1nnn` control words, `0x2nnn` data words, `0x3nnn` kept for the commit, `0x4nnn` pairs written into the frame buffer at the commit, `0x0000` the end), gives the frame buffer to the 68000 (`FM` = 0) for the command and takes it back after. For what a running game makes each frame: map words, converted tiles, H scroll tables, colours (about 6.3 µs a word in ares). MD 39 `fb_list`. Ledger §24. **`VSSWAP=1`** (ledger §24b; Sapphire CD32X docs/26, 35): a command-78 list committed while the splits run switches at the vblank of this command's swap (`fb_vrun`, after the kept runs), with entry 0 taken from VSRAM; and ops 2 / 3 wait for the 32X's blank to end as well as the Genesis's before the FS write, so a swap never comes at once with its kept runs (VSRAM's top) after line 0 With `HWIN=1` (ledger §33) the list lets `VSPLIT`'s H interrupt in before each of its control runs, so a split due while a long list runs is late by a run, not by the list. |
| **99** | A tile page's cache (Metal Slug CD32X) | COMM2 = the page window 1 shows (1-6), COMM8L = where the tile part (`MSPG`) starts in every page | COMM12 = the tiles copied, `0xFFFF` refused | **Only in `TILEP=1` builds, on the EverDrive launcher's listener (MODE1 + M1PAGE).** While it runs, the SH2 hands the 68000 the tiles one by one through COMM4-COMM10 (`0x8000 \| k` in COMM12 asks for tile k, `0x4000 \| k` in COMM2 says it is there, `0x7FFF` = no more); the 68000 copies each from page q into a slot of page p and patches the page's leaf. Both SH2s must stay off the cart meanwhile. MD 40 `tile_copy`. Ledger §25 |
| **91** | Sectors of the open file → cart | COMM2 = the first sector in the file | COMM12 = 0 ok, 1 refused | **Only in `CDRING=1` builds.** COMM8L = the cart byte offset (2 KB aligned, below the save area) \| (sectors − 1) << 24, 1-16 sectors. Read and copied as CMD 46 (`STREAM`, `CARTW`). For a disc image the game reads while it runs (Dracula X CD32X's PC Engine data track). Ledger §29 |
| **100** | CMD 46 from an LZ4-packed file (Metal Slug CD32X) | as 46: the chunk (bit 15 = no read-back) | COMM8L as 46: the chunk's file length (unpacked) | COMM12 = 0 ok, 2 a bad block | **Only in `ZCHUNK=1` builds (with `STREAM=1`).** The file: sector 0 a table (`MSZ1`, then each chunk's first sector, sectors, packed length, length), each chunk an LZ4 block from a sector's start. Unpacked on the Sub-CPU (68000 code, ~100 ms a chunk in ares) into Word RAM, then 46's copy; with the cartridge-boot listener the next chunk is unpacked while the 68000 copies. The read-ahead ring 192 KB. With `ROMPAUSE=1` (no number: the drive paused instead of losing sectors when the ring is full). Ledger §26-27 |
| **101** | The 32X frame buffer → cart | the frame buffer byte offset >> 4 | COMM12 = 0 ok, 1 refused | **Only in `FBCART=1` builds.** COMM8L = the cart byte offset (even, below the save area) \| (2 KB units − 1) << 24, 2-32 KB. The SH2 gives the 68000 the frame buffer (FM = 0) first; the 68000 copies it (at 0x840000 + offset) into the cart with the listener's word copy, 4 KB at a time. For a game whose RAM lives in the cart (the SH2 cannot write the cart): Dracula X CD32X's PC Engine RAM banks. Ledger §30 |
| **103** | The PC Engine's PSG on the PCM chip: the ring on / off; its volume, the CD music's volume (Sapphire CD32X) | low byte: 1 on, 0 off, 2 the effects' volume (high byte 0-8), 3 the CD music's volume (COMM8L 0-0x400) | COMM12 = 1 | **Only in `TPSG=1` builds.** On: COMM8L = the 68000 address of the cart core's PSG write ring (a long `0x5450 << 16 \| count`, from + 0x100 1,024 words of register << 8 \| value). From then on the Sub-CPU reads the ring every pass (the 68000's `READ_32X_32` as it is) and plays the six PSG channels on PCM channels 0-5 (`tpsg/`). Not with 48's effects or 53. Ledger §36 |
| **104** | The CD music's volume set at once or faded (Dracula X CD32X) | COMM2 = the volume to go to, 0-0x400 (full); COMM8 = the slope: 0 at once, else that much a step, 75 steps a second | none | **Only in `CDFADE=1` builds.** The BIOS's FDRSET (slope 0) or FDRCHG; not queued, so the drive is not waited for. A fade from full to nothing in t seconds is a slope of 1024 / (75 t): 2.5 s is 5. Command 45 still sets the volume full at each track's start. Ares only. Ledger §38 |

**Free numbers.** Free on every stack, per the ledger's list:

- 39–43, 49, 55;
- 83, 85-89, 92-98 and 105 and up (101 is `FBCART`'s, 3 Nov, §30; 102 is `USBLOG`'s, §34; 103 is `TPSG`'s, 12 Nov, §36; 104 is `CDFADE`'s, 12 Nov, §38; 81 is `CDPAUSE`'s, §31; 82 is Jumping Flash's, §32) (72–75 went to Rayman and `sdcard/` on 26–27 Oct, ledger §17-18; 76 / 77 are
  the `sdcard/` kit's save commands as the games build them since 13 Nov (`SDSAVE_OUT=76 SDSAVE_IN=77`: Road Rash, Dracula X), §18; 78 is `VSPLIT`'s, 31 Oct; 79 is `FBVDP`'s, 3 Nov, §24; 84 and 90 are
  `traytest/`'s, 2 Nov, §22; 99 is `TILEP`'s and 100 `ZCHUNK`'s, 3 Nov, §25, §27; 91 is `CDRING`'s, 3 Nov, §29).

Do not take these: 62–67 (this stack's `GENPLANE`, SuperTux REV, Rayman), 67 and 69–71 (OpenJazz REV), 72–75
(Rayman, `sdcard/`), 76–77 (`sdcard/` as Road Rash and Dracula X build it: save piece out / in, the EverDrive's bank 31), 78 (this stack's `VSPLIT`), 79 (this stack's `FBVDP`), 80 (Sonic CD 2011), 84 and 90 (`traytest/`: CD status, the tray), 99 (this stack's `TILEP`), 100 (this stack's `ZCHUNK`).

A number must never mean two things on any stack. Your command must sit behind a build switch in
`main.c`, which is compiled with **HWFLAGS** (not CCFLAGS).

**A number this Sub-CPU does not have is never answered.** There is no catch-all in `main.c`, so COMM0
stays set and the SH2's wait times out (hello: error `4EE4`, [09](09_ERROR_CODES.md)).

## The PCM pack (`SFXnn.BIN`, CMD 53)
Loaded whole into the PCM chip's 64 KB wave RAM. No tool for it ships with this kit. OpenLara's
`tools/make_sfx_packs.py` writes it; the reader is `pcm_load()` in `stack/subcpu/main.c`.

| Offset | Size | What |
|---|---|---|
| 0 | 4 | `KSFX` |
| 4 | 2 | number of effects (big-endian, at most 31) |
| 6 | 2 | size of the wave image in bytes (0 = 65536) |
| 8 + 8 × i | 8 | effect i + 1: start page (`ST`, the high byte of the wave address), volume (`ENV`), frequency step (`FD`, 2 bytes), loop address (2 bytes), length (2 bytes) |
| 256 | image | the wave RAM image: the PCM chip's sample format (8-bit sign-magnitude; `0xFF` = the loop marker). At most 48 sectors of the file are read. |
| 256 + image | 256 | optional: a map from the game's sample number to effect number |

Play an effect with CMD 48: the high byte of COMM0 is the effect number (1 … n), or, when the pack has the
map, the game's sample number + 1 (looked up in the map). COMM2 is its volume (1–64,
64 = full). Effect 1 always gets channel 8; the others take turns on channels 1–7.

## Status codes (COMM12)
These are conventions the existing games share. Show your own as you like, but keep `4B10`.

| Code | Meaning |
|---|---|
| `4D01`–`4D05`, `4C00`+n, `4DE1`–`4DE4` | CDLOADER's steps and errors (see [01](01_HOW_IT_BOOTS.md)) |
| `4Exx` | hello's start-up steps (`4E00` started, `4E01` saved, `4EE1` save failed) |
| `4B10` | game running (OpenLara, Jazz, SuperTux, Kobo) |
| `4Fxx`, `4E8n`, `4E14` | OpenLara's level loading (level xx, chunk n, level ready) |

## The Genesis 68000 routines (MD commands, `hw_md.s`)
The Sub-CPU calls these with `do_md_cmdN`. You only need them when you add a command. The ones used at
run time:

| MD | Routine | What it does |
|---|---|---|
| 19 / 21 | `READ_32X_16` / `READ_32X_32` | Read a 32X register. This is how the Sub-CPU reads COMM. |
| 20 / 22 | `WRITE_32X_16` / `WRITE_32X_32` | Write a 32X register |
| 26 / 27 | `SAFE_CART_XFER` / `SAFE_CART_XFER_W` | Cart copy, interrupts off, RV = 1 (bytes / words) |
| 33 | `CHILLY_RV_MEMCPY` | Cart copy, RV = 1, bytes |
| 32 | `CHILLY_MEMCPY` | Plain copy; reads the cart through the `0x900000` window |
| 34 | `SET_VDP_REG` | One Genesis VDP register |
| 5 / 7 / 12 | `set_vram` / `copy_vram` / `set_palette` | One VRAM word / words from the 68000's map (the cart window) to VRAM / colours to CRAM: Chilly's, used by 62 / 63 / 64 |
| 36 / 37 | `set_vsram` / `copy_vram_col` | **`GENPLANE=1` only** (`MD_CMD_END` 38, else 36): one VSRAM word; `copy_vram` with the auto-increment at 128 (a map column). SuperTux REV2 / REV4's routines, unchanged |
| 38 | `set_vs_line` | **`VSPLIT=1` only** (`MD_CMD_END` 39): the split list of command 78 (two lists, switched by `hint_vs` at the end of a picture); with it, the H interrupt handler `hint_vs` and the dispatcher's level-4 mask during the routines that touch the VDP |
| 39 | `fb_list` | **`FBVDP=1` only** (`MD_CMD_END` 40): command 79's list from the 32X frame buffer to the VDP's ports; 256 words kept for the commit (`fb_keep`) |
| 40 | `tile_copy` | **`TILEP=1` only** (`MD_CMD_END` 41; 36-39 `cmd_nop` when their switches are off): command 99's tiles, each copied with RV = 1 from page q into page p's slot (MODE1-9's banks, W on only for the writes into the cart, MODE1-6); needs MODE1 + M1PAGE, else returns -1 |
| 18 | `INIT_32X` | The 32X power-up (boot only) |

**Rule:** the 68000 reads the Sub-CPU's command flag twice and acts only when both reads match (HW10). A
torn read once ran routine 2 with a garbage argument (`SR = A71E`) and hung the console. Routine 2 is
disabled since.

## Clashes between the stacks

| CMD | This stack (OpenLara path) | Its forks (OpenJazz REV, SuperTux REV) | Kobo path (Sonic CD 2011) | Cartridge stack (OpenTyrian) |
|---|---|---|---|---|
| 46 | chunk → cart; bit 15 = no read-back | same | 4 forms: `0x00nn` chunk, `0x01nn`–`0x03nn` runs of up to 4 chunks, `0xF8nn` cart page, `0xF0nn` slot override | — |
| 47 | chunk compare | same | cart → frame buffer | — |
| 60–63 | 60 D32XR start, 61 save piece; 62-63 Genesis picture (`GENPLANE`) | 62–66 Genesis VDP from the cart (SuperTux); 62 = save piece in on the EverDrive launcher builds of OpenJazz REV / Hurrican / OpenLara EDCD, Genesis background on D32XR (ledger §4) | — | CD find, CD read, SRAM read / write |
| 67 | map columns into VRAM (`GENPLANE`, as SuperTux) | **two meanings:** OpenJazz = Genesis plane (load, start, off, one map cell); SuperTux = map columns into VRAM | — | — |
| 68 | cart page (window 1) | same | act file → Genesis planes | — |
| 69–71 | — | OpenJazz: 69 unpack LZ, 70 disc stamp, 71 bonus floor | — | — |
| 80 | — | — | the graphics chip's stamp floor | — |

**File numbers clash too.** CMD 38 numbers 200 and up are OpenLara's resident files, but `OJnnn.PAK` in
OpenJazz, Hurrican and SuperTux. Sonic uses its own numbering. A Sub-CPU program belongs to one game.
