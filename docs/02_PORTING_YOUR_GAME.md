# 02 – Porting your game

## The idea
Your game is a **normal 32X cartridge program**. On the CD32X it is not burnt into a ROM chip: CDLOADER
copies it from the disc into the 4 MB RAM cart and starts it exactly as the 32X boot ROM starts a
cartridge. So:

- **Build it with Chilly Willy's 32X devkit,** as you would for a cartridge. Use `crt0.s` (ROM header + 32X
  header + SH2 start-up), `mars.ld`, and `src-md/` (the 68000 side, built into the ROM but never run here).
- **The SH2s read the cart like ROM** at `0x02000000`. Code and constant data can stay there. Hot loops run
  faster from SDRAM: put them in `.data`, or in a section your linker script copies there.
- **Everything else is a command to the Sega CD** (`hello/src/cdapi.c`): files, pad, music, sound effects,
  saves. The Genesis 68000 is busy being the CD stack's listener; your cartridge's 68000 code never runs.

## What your ROM must look like
`tools/prep_rom.py` checks the same things CDLOADER checks on the console, so a bad ROM fails on the PC
instead of after a burn.

| Where | What | Why |
|---|---|---|
| `0x100` | `SEGA` | CDLOADER error `4DE3` otherwise |
| `0x3C0` | the module name (16 characters) | yours: it identifies the ROM in your build checks |
| `0x3D4` | source in ROM, destination in SDRAM, size | the SDRAM image. Size 1 … 192 KB, and destination + size ≤ 224 KB: CDLOADER's routine lives above that |
| `0x3E0` | master entry, slave entry, master VBR, slave VBR | the entries in SDRAM (`0x06xxxxxx`), as Chilly's `crt0.s` makes them |
| `0x3BC` | `CDB2`, written by `prep_rom.py` | the CD-boot marker. This spot is all zeros in Chilly's `crt0.s` |
| file size | up to 512 KB with the default cart layout | the "resident" slot starts at `0x080000` (change `game_files.h` if you need more) |

`prep_rom.py` also trims the zero padding (fewer 32 KB chunks to load) and writes `GAME.32X`.

## What to change in a cartridge game

| A cartridge 32X game does | On the CD32X do | API (`cdapi.h`) |
|---|---|---|
| reads the pads that the 68000 puts in COMM8 / COMM10 | **CMD 48**: the Sub-CPU answers with pad 1 from the 68000's vblank copy. The SH2 must never poll a register the 68000 writes. | `cd_pad(sfx, volume)` |
| keeps all data in ROM | **ship data files on the CD** and load them into a cart slot (CMD 38 + 46). Read them at `0x22000000 + offset` (cache-through), or clear the cache after the load. | `cd_load(file)`, `CART_SLOT(n)` |
| plays YM2612 / PSG music from the 68000 | **CD audio** (CMD 45 track, bit 15 = play once; CMD 54 stop) | `cd_music_play`, `cd_music_stop` |
| plays sound effects through PWM | **PCM chip** (CMD 53 loads `SFXnn.BIN`; the effect number rides on CMD 48's high byte, its volume in COMM2). PWM still works for the SH2s: OpenLara mixes its effects and movie sound on the slave SH2. | `cd_pad(sfx, vol)` |
| saves to cartridge SRAM | **the cart's save area** (CMD 58 per word into the Sub-CPU's 4 KB buffer, then CMD 61 = piece) or the **Sega CD backup RAM** (56 load, 57/58 words, 59 save) | `cart_save(piece, words, n)` |
| has its 68000 talk to the Genesis VDP | needs listener commands. SuperTux (62–67) and OpenJazz (67) added them: see [07](07_METHODS_COMPARED.md) | — |

## The checklist

1. **Start from `hello/`.** Copy it to `mygame/` and keep `crt0.s` first in the link: it holds the headers.
2. **Name your ROM** in `crt0.s` (`0x100` name strings, `0x3C0` module name).
3. **Stay inside the limits.** SDRAM image ≤ 192 KB. Big arrays go in `.bss` (SDRAM) or the cart (read-only),
   not in `.data` (that is copied).
4. **List your data files** in `stack/subcpu/game_files.h`: ISO 8.3 names, capitals. Put the files in
   `mygame/cd/`.
   - File `100 + n` loads into the level slot (cart `0x200000`, up to 1984 KB).
   - File `200 + n` loads into the resident slot (cart `0x080000`).
   - File `300 + n` is a movie (level slot).
5. **Pick a backup-RAM name** in `game_files.h` (11 characters), if you use CMD 56–59.
6. **Pick your save pieces.** The cart's save area is shared by every CD32X game on that cart (16 × 4 KB at
   `0x3F0000`). Check the ownership table in the command ledger (MicroNut99/docs,
   `CD32X_COMMAND_LEDGER_ADDITIONS.md` §5). Free as of Oct 2026: 11, 12. Piece 15 is test scratch.
7. **Music:** `mygame/music/Track02.wav`, `Track03.wav`, … (44.1 kHz 16-bit stereo). Disc track N = CMD 45 N.
8. **Post status codes** in COMM12 (`cd_status`) at each start-up step, and `0x4B10` once running.
   - Give your game its own range, and list it as hello's are listed in [09](09_ERROR_CODES.md).
   - Keep hello's error box, or show `cd_error` your own way. On a console, the code, the `BC-Ex` tag and
     the COMM registers are your debugger.
   - Keep the diagnostic view too: `DIAG=1` shows the Sub-CPU's boot history on screen.
9. **Put a timeout on every command wait** (`cd_cmd` does: 10 s by the vertical blank counter `gTicks`,
   which your game must count). An unanswered command looks like a hang.
10. **Stop the music before any CD data read.** `cd_load` does it. The drive cannot read data and play
    audio at once.
11. **Clear the caches after cart writes.** The 68000 writes the cart behind the SH2 caches: read
    cache-through (`0x22…`) or purge, and tell the slave to purge too.
12. `GAME=mygame bash build.sh`, then test in emulation, then burn (see [05](05_BUILD_TEST_BURN.md)).

## Patterns worth copying from the existing games

| Pattern | From |
|---|---|
| **Resident data + one level slot.** Load a pack once into the resident slot and keep it across power-off. Check its length and checksum at start and reload only when it is missing; A+B+C held at start forces a reload. | OpenLara (Caves), Jazz, Hurrican |
| **Quick check of the ROM.** CDLOADER compares the first, middle and last chunks with CMD 47 and skips the copy when they match. OpenLara's loader has the check switched off (`if (same && 0)`); OpenJazz, Hurrican and SuperTux switch it on (A+B+C still forces the copy). | OpenJazz / Hurrican / SuperTux CDLOADER |
| **Read-ahead streaming** of CD data. CMD 46 no longer seeks for every chunk: loads about 2.5× faster, measured in emulation (OpenLara, OpenJazz R7). | `stack/subcpu` (`STREAM=1`; off by default here until it has run on a console) |
| **The slave SH2 as a worker.** A job struct in SDRAM, read cache-through, with a cache purge per job and the slave on its own stack. Only the master talks to the Sub-CPU. | Hurrican, OpenJazz |
| **The Genesis VDP as a second picture.** The SH2 decides what changes; the 68000 copies it in vblank, reading the cart through the `0x900000` window, never the SH2's way. | OpenJazz REV, SuperTux REV, Sonic CD 2011 |
| **A frozen, checked boot path.** `check_boot.py` compares md5s of every boot file before each build. | SuperTux REV |

## Things that will not work
- **Writing the cart from the SH2.** Only the 68000 can, with RV = 1 (CMD 46, 61).
- **Your cartridge 68000 code** (Z80 / YM2612 drivers, Genesis VDP code): it never runs. Port it to listener
  commands, or drop it.
- **Byte writes to the frame buffer:** they may not land. Use 16-bit writes.
- **Colour `0x0000`:** it is see-through on the 32X (the Genesis layer shows). Use `0x0400` for black, or
  switch the Genesis display off with CMD 52.
