# sdcard/ for an AI – SD-card saves on the Mega EverDrive PRO

**Who this is for:** an AI adding saves to one of MicroNut99's CD32X games started from the EverDrive
(`EDCD_<GAME>.32X`). The human version is [README.md](README.md). Read this whole file before you change
anything; it is short on purpose.

## 0. Rules (the owner's, not suggestions)

1. **Do not touch the boot stack.** `stack/` is console-proven and guarded by `tools/check_boot.py`. The save
   commands go into a **copy** (`subcpu/merge_sdsave.py` refuses the BootCamp itself), behind `SDSAVE ?= 0`.
   With the switch off the copy must build **byte for byte** the stack's programs – prove it with md5s.
2. **OpenLara CD32X-REV EDCD is the blueprint.** Same commands, same order, same timings. Do not invent
   another route to bank 31, do not "simplify" the waits, do not drop the slave park.
3. **Bank 31 is the battery's.** Reach it only through the listener's MODE1-7 route (cart `0x800000`–
   `0x83FFFF`, `M1SAVE=1`). Banks 28–31 (page 7) are the EverDrive's: never page them into a window.
4. **The emulator follows the hardware.** A green ares run is a filter, not a pass. Never write "tested"
   about a build unless the md5s of what you ran match the owner's files ([docs/13](../docs/13_ARES_FOR_AI.md)).
5. No game data, BIOS or disc images in any repository. Every launcher gets a unique `EDCD_<NAME>` tag.

## 1. The route a byte takes

```
master SH2 ──CMD 58 per word──▶ Sub-CPU 4 KB buffer (bram_buf) ──Word RAM──▶ 68000 listener
                                                                            │ RV = 1, window 1 = bank 31,
                                                                            │ W on only inside the write
                                                                            ▼
                                       EverDrive bank 31 (cart 0x800000 + piece × 4 KB in the listener's map)
                                                                            │ the EverDrive OS, at the menu
                                                                            ▼
                                             SD card: MEGA/gamedata/<launcher>/bram.srm (256 KB)
```

The OS saves bank 31 to the card only if the launcher's header has the `RA` save-RAM entry
(`EDCD_SAVE=1` → `edboot/make_edcd.py`). The SH2s cannot address bank 31 at all.

## 2. The commands (merged by `subcpu/merge_sdsave.py`)

| CMD | Who | COMM2 in | What | COMM12 out |
|---|---|---|---|---|
| 58 | stack | word index 0–2047, COMM8 = word | SH2 → buffer | – |
| 57 | stack | word index 0–2047 | buffer → COMM8 | – |
| `SDSAVE_CMD_OUT` 74 | merged | piece 0–63 | buffer → bank `piece × 4 KB`, read back, compared | `0x201` ok, else 0 |
| `SDSAVE_CMD_IN` 75 | merged | piece 0–63; `0x8000` = only ask | bank → buffer | `0x201` ok / bank here, else 0 |

The SH2 clears COMM12 before 74/75 and writes COMM2 before COMM0 (the Sub-CPU reads COMM0+COMM2 in one
32-bit read). `0x200` = "the save bank is here". The **disc's** program must also be built with
`SDSAVE=1`: there the commands answer 0. Without them COMM0 is never cleared – the stack has no default
branch – and the SH2 times out every time it asks.

These are OpenLara's CMD 61/62 under `OL_EDSAVE` and Rayman's 72/73, with the piece numbered across the
whole bank. 74/75 are free in the stack (it uses 0–68); change with `SDSAVE_OUT=`/`SDSAVE_IN=` (make) and
`-DSDSAVE_CMD_OUT=`/`-DSDSAVE_CMD_IN=` (SH2) together if a game has its own commands there.

## 3. The SH2 side (`sh2/sdsave.c`) – what must stay as it is

Mapping to `engine/openlara-edsave.patch`: `sds_spin` = `csSpin`, `sds_cmd_rv` = `cdCmdSafe`, `sds_cmd` =
`cdCmd`, `sds_park`/`sds_unpark` = `csPark`/`csUnpark`, piece in/out = `saveBankRead`/`csFlushPiece`.

- **Why it parks:** on the EverDrive an SH2 cart access while the 68000 holds RV = 1 is answered at once
  with **garbage** (edsave3, console). ares instead holds the SH2 until RV = 0, so ares hides this bug.
- `sds_spin` lives in `.data.sdram` (mars.ld puts `.data.*` at `0x06000000`), masks interrupts
  (`SR |= 0xF0`), touches only the register and its own stack, 4,000,000 rounds with a 64-count pause. The
  assembler warns `setting incorrect section attributes for .data.sdram` – OpenLara has the same; harmless.
- The master parks the slave first (COMM4 = `0x7A60`, waits for `0x7A61`), then sends 74/75 and waits in
  `sds_spin`; afterwards COMM4 = 0. Nested (`sds_park_depth`).
- **One deliberate difference:** OpenLara's two slave waits are unbounded. Here they stop after
  `SDSAVE_WAIT` rounds and the call returns `SDSAVE_E_PARK` – the copy is **never** done unparked.
- Plain commands (57/58, the `0x8000` query) use the bounded `sds_cmd` (2,000,000 × pause), as OpenLara.
- The master's **interrupt handlers and stack must be in SDRAM** (the spin masks interrupts, but the code
  around it runs normally). A game that runs from the cart (hello does) is fine because nothing is fetched
  from the cart between COMM0 and COMM0 = 0.

### The slave

The slave's loop must call `sdsave_slave_park()` when COMM4 reads `SDSAVE_CMD_PARK`, and must **not** clear
COMM4 afterwards (the master does). Its sound DMA keeps running from SDRAM; its interrupts wait. If the
slave already dispatches commands from COMM4 (OpenLara: `case MARS_CMD_PARK: csSpin(...); continue;`), add
one case. If the slave uses another register, set `SDSAVE_SLAVE_REG`. `SDSAVE_PARK=0` only if the slave can
never touch the cart (code, data, literal pools, stack, vectors) – prove that with the RV probe (section 6).

### Buffer sharing

CMD 56/59 (Sega CD backup RAM) and 57/58 use the same 4 KB buffer. If the game uses them too, call
`sdsave_forget()` afterwards, or the next read may skip the bank copy and hand out the wrong piece.

## 4. Layout in the bank

Every launcher has its **own** `bram.srm` (the folder is the launcher's name), so the 256 KB belong to that
game. Known users in their own launchers: OpenLara `0x00000`–`0x2FFFF` (save area at 0, slots at
`0x10000`), Rayman `0x30000`–`0x3FFFF`, the demo `0x00000` (`SDV1`). A fresh `bram.srm` (or ares's fresh RAM)
is not zero – use `sdsave_save_record`/`sdsave_load_record` (magic, length, FNV-1a, ~sum, then the data, written in
one pass; a save cut off half way fails the sum) or your own check. Keep saves to few pieces: each 4 KB
piece written is 2,048 word round trips plus two 4 KB copies; a partial piece is read first (2,048 more),
unless it is the last piece read or written - `sds_buf` keeps it, since the bank changes only through
this file.

## 5. Building

```bash
bash sdcard/build_sdsave.sh <out> <NAME>      # CDROMPlayer_SDSAVE.bin, app_mode1_SDSAVE.bin, EDCD_<NAME>.32X
bash sdcard/demo/build_demo.sh <out>          # SDSAVE_DEMO.cue/.bin + EDCD_SDSAVEDEMO.32X
```

Both build in `mktemp -d` copies and run `check_boot.py` before and after. The launcher's program is
`MODE1=1 M1CTRL0=0x8000 M1SAVE=1 SDSAVE=1`; the disc's is `SDSAVE=1`. `SDSAVE` is a make `?=` variable, so
`SDSAVE=1 bash build.sh` (in a merged copy) reaches the stack's make through the environment.
`genisoimage` is needed for a disc; a container without it cannot make the demo's disc (do not fake one
for the owner).

## 6. Checking your work (in this order)

1. **Byte-identical with the switch off.** In a merged copy, `make CDROMPlayer.bin` and
   `make CDROMPlayer.bin MODE1=1 M1CTRL0=0x8000 M1SAVE=1` must give the stack's md5s. (27 Oct 2026:
   disc `c78e6f49…`, MODE1+M1SAVE `64aa0b19…` – both identical.)
2. **Compile the SH2 side** with the game's flags (`sh-elf-gcc -m2 -mb -O2 -Wall`); check `objdump -h` shows
   `.data.sdram` holding `sds_spin` with its literal pool.
3. **ares, EverDrive** (`tools/ares_test.sh` does 3 and 4 for the demo; [docs/13](../docs/13_ARES_FOR_AI.md)): `--ramcart-type everdrive --ramcart-boot
   EDCD_<NAME>.32X --ramcart ed.ram`. Then fill banks 0–30 of `ed.ram` (bank n = `n × 0x80000`) as the menu
   does, keep bank 31, run again: the save must come back. `tools/srm_dump.py --ares ed.ram` shows the records.
4. **ares, disc boot** (no launcher): `sdsave_present()` = 0, no hang.
5. **The RV probe** (`ares/rv-garbage-probe.patch`, `ARES_RV_GARBAGE=1`, `--interpreter`): no `RV-CART`
   line during a save or load. A line there means an SH2 is on the cart during a copy → console garbage.
   Caution (27 Oct 2026): on the demo the probe stayed silent even with a deliberately broken control
   (no park, plain wait), so a silent probe there proves nothing - the console is the test.
6. **The console.** Only the owner can run it. Give them the md5s; say "ares only" until they report back.

## 7. Pitfalls already paid for

- In m68k gas `|` starts a comment: write `A + B`, never `A | B` (hw_md.s `SCD_M1CTRL0_W` bug, Sonic).
- The listener runs relocated at `0xFF1000`: its stores must be PC-relative.
- CTRL0 needs bit 15 (P); W (bit 13) only inside a cart write; even-byte writes to the mapper are ignored.
- The EverDrive menu **wipes banks 0–30** (all PSRAM); only bank 31 survives. Saves in the RAM cart's own
  save area (`0x3F0000`, CMD 61) are lost on the EverDrive – that is why this kit exists.
- Do not page bank 31 into a window the game uses, and do not write it from the 68000 any other way.
