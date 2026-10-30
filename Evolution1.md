# Evolution1 - the CD32X BootCamp, as read on 29 Oct 2026

What an AI found after reading this repository in full (the README, `CLAUDE.md`, docs/01-13, the
licence, `build.sh`, the boot stack's core, the example game, the tools, the EverDrive launcher,
`sdcard/`, `probe/`, `ares/`). Written as a record of the state of the kit at commit `9e9ca98`, so the
next reader starts from what is known rather than from the folder listing.

## What it is

A frozen, console-proven boot stack for running a normal 32X cartridge ROM from a Sega CD + 32X
disc, using a battery-backed 4 MB RAM cart (or a Mega EverDrive PRO) in the 32X slot. It is OpenLara
CD32X's stack (the v1.0-v1.4 code) packaged with a tiny example game. Lineage: Night Trap -> Kobo
Deluxe CD32X -> OpenLara (+ CDLOADER, written for D32XR) -> this.

## How it boots

BIOS -> boot block -> `APP.BIN` on the Sub-CPU -> the 68000 listener (`hw_md.s`) copied to
`0xFF1000`, which from then on only obeys numbered routines -> `INIT_32X` writes `"_CD_"` -> CDLOADER
runs on the SH2s, handshakes via COMM10 (`0AAA`/`0BBB`), copies `GAME.32X` (file 20) chunk by chunk
into the cart with CMD 46 (the 68000 writes with RV = 1, read-back verified), checks the 32X header,
parks the slave at `0x2603C000`, copies the SDRAM image over itself and jumps to the ROM's entries. The
68000 is never handed to the game (CMD 60 is the dead D32XR experiment).

## The contract for a game

Only the master SH2 sends commands, via COMM0/2/8/12: CMD 38/46 files -> cart slots, 48 pad (+ PCM
effect), 45/54 CD audio, 53 PCM packs, 56-59 backup RAM, 58 + 61 cart save pieces. Cart layout: the
ROM at 0 (<= 512 KB), the resident slot at `0x080000`, the level slot at `0x200000`, sixteen shared
4 KB save pieces at `0x3F0000` with a cross-game ownership ledger. SDRAM image <= 192 KB,
`.data` + `.bss` below `0x0603C000`. The hardware rules in docs/06 (one writer per COMM register, no
SH2 polling of 68000-written registers, no CD reads during CDDA, 16-bit frame buffer writes, purge
the caches after cart writes, ...) each cost someone a dead disc.

## The EverDrive PRO work (24-28 Oct 2026)

The discoveries, all probe-measured on the console:

- The SSF mapper makes it 16 MB of writable RAM (`edtest`), but the C bit does **not** release the
  cartridge, so the CD boot cannot use it. Instead `EDCD.32X` starts the Sega CD from cartridge mode
  ("mode 1") and runs the same stack built with `MODE1=1`, which remaps addresses and routes copies
  through a work-RAM buffer.
- Write-enable (W) must be off except inside a cart write, or the 68000's writes elsewhere corrupt
  the cart and zero the SH2's reads (`edw0`).
- Window 0 cannot be written directly with the 32X on -> route through window 1.
- PSRAM banks 0-29 are wiped every menu run; only bank 31 (battery RAM -> `bram.srm`) persists,
  hence `EDCD_SAVE=1` and `sdcard/`.
- An SH2 touching the cart during RV = 1 gets garbage, not a stall (`edsave3`) -> both SH2s must park
  in SDRAM with interrupts masked during copies (EDSAVE-4).
- Even-byte writes to CTRL1-7 are ignored (`edpage`), so the paged games need `M1PAGE=1`.

## Process rules

`stack/` is off limits and md5-guarded; new commands go behind HWFLAGS switches in a *copy*; the only
emulator that counts is MicroNut99's ares build, and a green run in ares is never a pass for the
console: the emulator follows the hardware, never the reverse. `CLAUDE.md` says the two manuals in
the submodule must be read in full before any stack work.

## Not read line by line in this pass

Chilly Willy's base files (`crt0.s`, `crt.s`, `cdfs.s`, `kos.s`, `font.s`), the individual ed-probe
sources, `probe/src/main.c`, `sdsave.c`, the ares patches, `ed_apply_jazzlike.py`, and the `manuals/`
submodule. Read them before touching what they describe.
