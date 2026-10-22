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
| **46** | chunk → cart | COMM2 = chunk (32 KB); bit 15 = skip the read-back. COMM8L = the file's length | COMM12 = 0 ok, 1 bad | The 68000 writes the cart with RV = 1; the SH2s' cart reads wait meanwhile. Clear caches afterwards. `FMVRING`: bit 14 = ring mode (movies), reply \| 0x100. `CARTW`: word copy (3× faster). |
| **47** | chunk compare | as 46 | COMM12 = 0 same, 1 different | Nothing is written; used for "is it already in the cart?" |
| **48** | pad (+ sound effect) | COMM0 high byte = PCM effect (0 = none); COMM2 = its volume | COMM8 = pad 1 (`SEGA_CTRL_*`, `0xF000` = no pad); COMM10 = the 68000's tick counter | Answered first in the Sub-CPU's loop. Send it once a frame. |
| **45** | play CD track | COMM2 = track; bit 15 = play once (else repeat) | — | Disc track N. Rule: no CD data reads while it plays. |
| **54** | stop CD audio | — | — | Send it before any CD data read (`cd_load` does) |
| **53** | load PCM effects | COMM2 = n: `SFXnn.BIN` | COMM12 = number of effects (0 = failed) | Packs as made by OpenLara's `make_sfx_packs.py` (8-bit samples, a table) |
| **52** | Genesis display | COMM2 = 1 on / 0 off | — | Off: only the 32X picture shows. CDLOADER switches it off. |
| **51** | file chunk → frame buffer | COMM2 = chunk | — | The SH2 first gives the frame buffer to the 68000 (FM = 0). Chunk c goes to byte `0x200 + c × 32 KB`. Used for `IMAGE.RAW`. |
| **50** | file chunk → frame buffer (16 KB, off-screen) | COMM2 = chunk | — | Kobo's no-cart mode; not needed with a cart |
| **56** | backup RAM: load | — | COMM12 = 1 ok, 0 no file | Fills the Sub-CPU's 4 KB buffer from the backup-RAM file `GAME_BRAM_NAME` |
| **57** | buffer: get word | COMM2 = word 0–2047 | COMM8 = word | |
| **58** | buffer: put word | COMM2 = word 0–2047, COMM8 = word | — | Fills the buffer for 59 or 61 |
| **59** | backup RAM: save | — | COMM12 = 1 ok | Writes the 4 KB buffer as a backup-RAM file (64 blocks: half the internal backup RAM). SuperTux reports that backup-RAM saving never worked on its console; cart saves (61) did. |
| **61** | buffer → cart save piece | COMM2 = piece 0–15 | COMM12 = 1 ok | Cart `0x3F0000 + piece × 4 KB`, written by the 68000 and read back. `SLOTS`: bit 8 = OpenLara's slot area at `0x060000` (reply `0x101`); leave it alone unless your layout is OpenLara's. |
| **44** | (clear Genesis planes) | — | — | Answered; does nothing in this build |
| **60** | 68000 → cartridge code | — | — | The D32XR experiment: hands the 68000 to the ROM's Genesis code and ends the CD stack. **Never** use it with this loader. |
| **68** | cart page, window 1 | COMM2 = page 1–7 | COMM12 = 1 done, 0 refused | Only in `CART=8` / `CART=16` builds, for the 8/16 MB paged cart. Clear caches after it. |

**Free numbers.** On this stack: 39–43, 49, 55, 62–67 (SuperTux uses them), 69–79, 81 and up. Before you
take one, check the ledger's list. A number must never mean two things on any stack, and the command
must sit behind a build switch in `main.c`, which is compiled with **HWFLAGS** (not CCFLAGS).

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
| 18 | `INIT_32X` | The 32X power-up (boot only) |

**Rule:** the 68000 reads the Sub-CPU's command flag twice and acts only when both reads match (HW10). A
torn read once ran routine 2 with a garbage argument (`SR = A71E`) and hung the console. Routine 2 is
disabled since.

## Clashes between the stacks

| CMD | This stack (OpenLara path) | Kobo path (Sonic CD 2011) | Cartridge stack (OpenTyrian) |
|---|---|---|---|
| 46 | chunk → cart; bit 15 = no read-back | 4 forms: `0x00nn` chunk, `0x01nn`–`0x03nn` runs of up to 4 chunks, `0xF8nn` cart page, `0xF0nn` slot override | — |
| 47 | chunk compare | cart → frame buffer | — |
| 60–63 | 60 D32XR start, 61 save piece, 62–63 Genesis picture (SuperTux) | — | CD find, CD read, SRAM read / write |
| 68 | cart page (window 1) | act file → Genesis planes | — |
| 80 | — | the graphics chip's stamp floor | — |

**File numbers clash too.** CMD 38 numbers 200 and up are OpenLara's resident files, but `OJnnn.PAK` in
OpenJazz, Hurrican and SuperTux. Sonic uses its own numbering. A Sub-CPU program belongs to one game.
