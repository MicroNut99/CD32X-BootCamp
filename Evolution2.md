# Evolution2 - the CD32X BootCamp, as read on 29 Oct 2026 (commit `cdab01b`)

The second full reading. Evolution1 (commit `9e9ca98`) recorded what the kit is; this pass read the
code the first one left out - every source of `stack/`, CDLOADER, `cdapi.c`, `edboot/`, the six ED
probes, the timing probe, `sdcard/`, the tools, the two ares patches - and, in `manuals/`, the two
must-read documents in full, the Kobo PDFs, the OpenLara / OpenJazz / OpenTyrian / OutRun addenda,
the OpenLara release notes and the paging proposals. Everything in Evolution1 still holds. This file
adds what the code says that the documents only summarise, and the places where the documents and the
code have drifted apart.

## How the stack actually does it (from the source)

- **The Sub-CPU loop** (`main.c`): one 32-bit read of COMM0+COMM2 per pass. `cmd == 0` costs a
  150-count idle loop (with `STREAM=1` the CD read-ahead is pumped there). CMD 48 is answered before
  anything else and replies pad + 68000 tick count in one 32-bit write to COMM8/10. There is **no
  catch-all**: an unknown number leaves COMM0 set forever (hello's `4EE4`).
- **CMD 38** maps the number to a name and a slot in `cur_img`: 16 -> `KOBOGFX.BIN`, 20 -> the ROM
  (slot 0), 100+n / 200+n / 300+n from `game_files.h`, anything else `IMAGEn.RAW` at slot n.
  `saved_lba` and `saved_len` are kept for 46/47/50/51; the reply is the length in COMM8L (0 = not
  found, and the root directory is listed on rows 19-25 as a diagnostic).
- **CMD 46 / 47**: 16 sectors to Word RAM `0x0C0000`, bank swap, then the 68000 copies with
  `CHILLY_RV_MEMCPY` (MD 33, bytes, 16 KB pieces, interrupts on) - or with `CARTW=1`
  `SAFE_CART_XFER_W` (MD 27, words, 4 KB pieces, interrupts off). The read-back goes through the 32X
  window `0x900000` with bank `0xA15104` (MD 32, RV = 0) into Word RAM `+0x8000`, then a byte
  compare; a 4-word spot check of the window follows. Reply COMM12 = 0/1 (ring mode `| 0x100`).
- **CMD 61** writes `0x3F0000 + (piece & 15) x 4 KB` with MD 33 and reads it back the same way; the
  4 KB buffer is `bram_buf`, shared with CMD 56-59 (BIOS `_BURAM` at `0x5F16`, file name
  `GAME_BRAM_NAME`, 64 blocks).
- **CMD 50 / 51** write COMM12 **32 bits wide** (chunk | sectors | a data word) - the one place the
  stack itself overwrites COMM14, which docs/03 notes ("post your status again after it").
- **CMD 53** loads at most 48 sectors of `SFXnn.BIN` and writes the PCM chip's 16 wave banks byte by
  byte; effect 1 always takes channel 7, the rest round-robin on 0-6; volume = `env * vol >> 6`.
- **The listener** (`hw_md.s`): the Sub-CPU's flag at `0xFF800F`, echoed in `0xFF800E`, result in
  `0xFF8010`, four long arguments at `0xFF8020`. HW10 reads the flag twice in both `cmd_done` and
  `md_check`; routine 2 is a NOP. `init_32x` is Chilly's power-up with "_CD_" written to COMM0 and
  the two `M_OK`/`S_OK` waits; in `MODE1` it skips the reset, the clears of COMM0/4 and the CD boot
  table (the cartridge's 32X header did that). The vblank handler reads both pads into the gate
  array, counts ticks at `0xFF801C`, and raises the Sub-CPU's level 2; the COMM6 heartbeat is a NOP
  since OpenLara (COMM6 is the renderer's). `install_exc` (MD 31) patches `jmp` stubs into the BIOS
  RAM jump table for vectors 2-11 and prints `68K EXC Vnn` in red, display forced on.
- **MODE1 in the listener**: `m1_arg` moves every argument of commands other than 19-22 (Word RAM
  `0x2xxxxx -> 0x6xxxxx`, cart `0x4xxxxx-0x7xxxxx -> 0x0xxxxx-0x3xxxxx`); `m1_xfer` splits every copy
  into 1 KB blocks through `m1_buf` in work RAM (Sega CD side with RV = 0, cart side with RV = 1 and
  SR = 0x2700); `m1_cart` routes window 0 (and, with `M1SAVE`, `0x800000-0x83FFFF` = bank 31) through
  window 1 with `M1_WINBLOCK`; `m1_cpy_w` turns W on only around the copy; `m1_page` (with `M1PAGE`)
  catches the one-byte write to `0xA130F0/F2` and sets CTRL0+1..3 or CTRL4..7, keeping `m1_wb`.
  `bram_put` / `bram_get` return -1 under MODE1. Without `OL_MODE1` the file assembles byte for byte
  as before (md5-guarded).
- **CDLOADER** (`cdloader_sh2.c`): handshake `0x0AAA`/`0x0BBB` on COMM10, CMD 52 off, the splash
  (CMD 38 file 0 + CMD 51 per chunk into both frame buffers, palette from the file's tail), CMD 38
  file 20, the three-chunk CMD 47 probe (result ignored: `if (same && 0)`), CMD 46 for every chunk,
  header check (`SEGA`, size <= 0x30000, dst + size <= 0x38000, entries in `0x06`), the position-
  independent trampoline copied to `0x2603C000` with its `M_OK`/`S_OK` words zeroed, slave parked via
  `INFO[5]/[6]` at `0x2603DF00`, then the copy of the SDRAM image over itself and the jump.
- **The game's `crt0.s`** (Chilly's, with one BootCamp addition): the master's exception vectors
  report instead of returning - COMM12 = `EE0n`, then PC and SP words 20 ms apart, COMM6/COMM10 = the
  PC. The slave's stack is 2 KB at `0x06040000`. The 68000 jump table at `0x200` points at
  `0x880800/840/880/8C0` (never run on a CD boot; `prep_rom.py --mode1` repoints it at `0xFFFD00`).
- **`cdapi.c`**: every wait is bounded by `gTicks` (600 = 10 s), with 2,000,000 pauses as the
  backstop; `BC-E4` is final (later waits 6 ticks); `cd_load` stops the music, writes the length into
  COMM8 **before every chunk** (rule 17), and clears COMM12 to `0xFFFF` first; the error keeps a copy
  of all eight COMM registers. `keep_status` puts the last status back into COMM12 after a reply.

## EDCD, as the launcher runs it (`edboot/edcd_md.s`)

RV off; wait up to ~6 s for `M_OK` + `S_OK` (CYAN); the mapper from work RAM `0xFF8000` with RV = 1
(CTRL1-7 = 1-7, CTRL0 = `0x8000`, W off), RV off; the BIOS RAM jump table (29 x `jmp 0xFFFDAE`, an
`rte`); `ED01` "SEGA" at `0x400100` (BLUE); the packed Sub-CPU BIOS found at `0x415800 / 0x416000 /
0x41AD00` or by scan (MAGENTA), `ED02`; Sub-CPU reset, bus request, PRG-RAM cleared, Kosinski
decompress, `edcd_sp.bin` at `0x6000`, the MODE1 `app_mode1.bin` at `0x8000`, write-protect `0x2A`,
run; `ED03`, wait for 'I' (RED, 10 s of `frame`, each raising the Sub-CPU's IFL2); `ED04`, Word RAM to
the Sub-CPU, 'B', wait for 'M' (YELLOW, 30 s); Word RAM back; `jmp 0x600000` (or `w0_move` when
`ED_W0` != 0). `make_edcd.py` builds the ROM: standard vectors, `SEGA SSF`, the jump table with
reset -> `0x880800` and the rest -> the BIOS RAM table, the 32X header pointing at CDLOADER's code
(its 0x38-byte CD boot table stripped), Chilly's `0x3F0` block lifted from `edtest/src/crt0.s`, the
`RA` save-RAM entry only with `EDCD_SAVE=1`.

## The probes: what each one is, in one line

| Probe | 68000 side (src-md/crt0.s additions) | What it measured |
|---|---|---|
| `edtest` | `0x06nn` CTRL write (RV = 1), `0x07nn` fill a window, `0x08nn` verify; interrupts off throughout | 27 banks RAM for the 68000 and both SH2s (console, 24 Oct) |
| `edw0` | + hammer commands (`0x0B00`, `0x0F00`, `0x1100`, `0x1200`) | W on + any other 68000 write = window 0 reads 0 / stored words change (25 Oct) |
| `edsave` | + `0x1300` poke one word | 12 marker sites: banks 0/7 wiped by the menu, bank 31 kept, `bram.srm` written (25 Oct) |
| `edsave2` | + `0x14xx` 4 KB block ops, slave hammering window 4 | reads/writes of bank 31 through window 1 with the slave busy (fill was made with W off: inconclusive) |
| `edsave3` | same, fills with W on, slave also runs code from window 4 | SH2 reads during RV = 1 return garbage (965,207 / 978 K bad), not a stall (26 Oct) |
| `edpage` | + `0x1500` even-byte register write, `0x1600` the real `m1_xfer` lifted as `m1_real.s` | word writes to CTRL4-7 right, an even byte to `0xA130F2` ignored, `m1_page` correct (26 Oct) |
| `probe` (a BootCamp game) | none: the stack's | SH2 clocks per access kind by the FRT (CKS 0/2), bus contention, FILL256, CD CMD round trip |

## The `sdcard/` kit

CMD 74 (OUT) / 75 (IN, `0x8000` = only ask), merged into a **copy** of `main.c` / `Makefile` behind
`SDSAVE ?= 0` (`merge_sdsave.py` refuses the BootCamp itself and proves the blocks strip back byte for
byte). The copy goes bank `0x800000 + piece x 4 KB` <-> Word RAM through MD 33, which under
`MODE1 M1SAVE` is `m1_xfer`'s bank-31 route. On the SH2: `sds_spin` in `.data.sdram` with SR |= 0xF0
(= OpenLara's `csSpin`), slave parked with COMM4 `0x7A60`/`0x7A61`, bounded waits that **refuse** the
copy rather than run it unparked; records = magic, length, FNV-1a, ~sum. Tested in ares only (27 Oct);
the demo's console run is still open.

## Where the documents and the code disagree (found this pass)

1. **`edcd_md.s`: `ED05` is never posted.** `move.w #0xED05,0xA1512C` sits after `bra fail` and before
   label `16:`, so it is unreachable; the launcher's last code is `ED04`. docs/12 lists ED05.
2. **README "Status" is stale**: it says hello/`build.sh` is "Not yet run on a console", but docs/12
   records the BootCamp disc running end to end on the console from `EDCD.32X` (25 Oct, release and
   diagnostic discs). The folder table also lacks `edsave2/`, `edsave3/`, `edpage/`, `sdcard/`, and the
   document table lacks docs/13.
3. **Save piece 12 has two claims.** docs/04 and the ledger §5 say free (hello borrows it); docs/02
   and docs/07 say Hurrican's ED-SAVE builds route CMD 56/59 to piece 12. One of them must give way.
4. **The ledger's free lists are behind the code**: §3 says "69 and up" free, but 69-71 are OpenJazz
   REV's, 80 is Sonic's (§6), and `sdcard/` takes 74/75 - none has a row in §3. docs/03's list
   (39-43, 49, 55, 72-79, 81+) is the current one and should be the ledger's too.
5. **`scd.c` prints without a switch**: `scd_diag_print("CKPT1 ...")` runs on every file open, release
   disc included (a Word RAM swap and a 68000 round trip each time; invisible with the display off).
   The OpenJazz addendum notes the same (`SCD_DIAG_TEXT`). Part of the frozen stack: recorded, not fixed.
6. **`make_mixed_cd2.py`**: `--pregap` defaults to 150 while its docstring says the default is 0 (the
   Doom layout). The code is right for a Red Book burn; the text is stale.
7. **`.gitignore` vs the tree**: `edboot/EDBOOTD.BIN`, `edsave3/EDSAVE3.32X` and `edpage/EDPAGE.32X`
   are committed build outputs, and the ignore list has no `edsave3/` or `edpage/` entries.
8. **The repo's `ares/everdrive-pro-ares-v148.patch` is the first board** (C bit releases CART_IN, no
   W-disturbance, no PSRAM wipe, SH2s held during RV). The console-corrected board lives only in
   `ares_android`; docs/11 says so, `ares/README.md` does not.
9. `hw_md.s` says `MD_CMD_END, 36` while the ledger says 38 (SuperTux's 36/37 are not in this
   listener): consistent with "routines 1-35 here", just a different constant than the ledger's.

## What Sega's and Hitachi's manuals add (`manuals/sega-vr-disasm/`, read this pass)

The folder is Sega's 32X and Genesis documentation and Hitachi's SH7604 datasheet, converted to
Markdown (plus a Virtua Racing disassembly's notes). Read against docs/06, docs/11 and the ledger:

1. **RV, as Sega defines it** (32X HW manual 3.1, 3.4; DREQ control register): with RV = 1 an SH2
   cart access *waits* until the 68000 clears RV, and the 68000 sees the cart at `0x100-0x3FFFFF`
   only while RV = 1. docs/06 rule 11 is Sega's rule word for word; the EverDrive breaks it
   (`edsave3`). The RV bit is bit 0 of the word at `0xA15106`; the `bset #0,0xA15107` byte
   access the listener uses is the one Sega's own BIOS listing uses (OpenLara addendum §3).
2. **All 68000 interrupts must be prohibited while RV = 1** (Technical Information item 9).
   `chilly_rv_memcpy` (MD 33: CMD 46's default copy, CMD 61, the `sdcard/` copies) runs with
   interrupts **on**; `safe_cart_xfer` (26/27, `CARTW=1`) and every MODE1 copy mask them. The
   interrupts-on copy is console-proven since OpenLara v1.0 because the listener's vblank touches
   only pads and the gate array - but it is against Sega's rule, and a game that adds a vblank
   routine must know it (the paging proposals say the same).
3. **A reset with RV = 1 may not restart the console** (Technical Information item 6, attachment 1).
   Every copy in the stack clears RV before returning; `exc_common` (the red `68K EXC` trap) does
   not - a 68000 fault inside an RV copy leaves RV = 1, the SH2s on the cart and the reset button
   unreliable. Diagnostic only; recorded.
4. **The 68000 vector area under RV = 1**: the HW manual gives the cart `0x100` and up; the 32X
   Overview says the vector area `0x000-0x0FF` is the cart "only when ROM to VRAM DMA". Two Sega
   documents disagree. The console (MODE1-3) found window 0 unwritable through either; ares routes
   `0x000-0x0FF` to the 32X with RV = 1 (docs/12). Still open; `prep_rom.py --mode1` sidesteps it.
5. **Twelve bytes are not readable at RV = 1**: `$001070-73`, `$002070-73`, `$003070-73`
   (Technical Information item 12). No stack path reads them that way: the CD boot's read-back goes
   through `0x900000` with RV = 0, and MODE1 reads window 0 through window 1 at `0x080000+`.
6. **Sega's bank registers live on the odd byte** (`A130F1-A130FF`, write-only, bank on D0-D5,
   register 0 bit 1 = write-enable; SRAM and EPROM cartridge manuals). That is the lane the EverDrive
   kept: it explains `edpage`'s "an even-byte write to `0xA130F2` is ignored", and shows the 16MB-PCB
   cart's choice (even byte, /UWR, D8-D10) is the one that differs - hence MODE1-9 `m1_page`.
7. **Frame buffer byte writes**: the manual says only a byte write of **0** is ignored (3.6, VDP
   DRAM); OpenJazz saw byte writes fail in general. docs/06 rule 19 ("16-bit writes") covers both.
8. **COMM registers**: a write from one side while the other reads (not only two writes) leaves the
   register undefined (3.3). docs/06 rules 5-7 (one writer, pause between polls, read twice) are the
   mitigation; the ledger's `HW10` torn read is this clause observed.
9. **The FRT is reserved** on the 32X (5.3 restriction 2: TIER 01, OCRA 0002, FTCSR 01, TOCR E2;
   Supplement 2: every external interrupt handler must toggle TOCR bit 1 - Chilly's `crt0.s` does).
   `probe/` takes the FRT over for its measurements and puts the set-up back (`frt_take/give`);
   `probe/README.md`'s "OCRA = 1, FTCSR 0x09" is what the boot ROM leaves, not Sega's listed values.
10. **CCR purges** (SH7604 §8.3, 8.6): Hitachi says to clear CE before changing CCR or purging; the
    whole stack writes `0x11` (CP + CE) in one byte, Chilly's idiom, console-proven. Also §8.7: no
    snoop - the cache-through mirror or a purge is the only coherency, which is docs/06 rule 10.
11. **Do not access the bus state controller** (`FFFFFFE0-FF`) in an application (5.3): hello's
    diagnostic-only `STEP` trace reads BCR1/BCR2/WCR/MCR there (`HELLO_DIAG_TEXT=1`). Reads, and only
    on the diagnostic disc; recorded.
12. **The 'RA' header** (Genesis Software Development Manual §S3): `0xF8` = non-volatile, odd bytes;
    `0x20` = SRAM - `make_edcd.py`'s `EDCD_SAVE=1` entry is to the letter.
13. **Boot ROM flow** (5.1): master checks `comm0 == "CD_*"` before the initial data load, slave waits
    for `"CD_*"` or `"M_OK"`; `M_OK`/`S_OK` must be cleared by the MD side and the SH2 waits until
    they are - CDLOADER's trampoline zeroes both, which is why a game's `crt0.s` does not stall.
14. **Access times** (HW manual 4.1, Overview): cart 6-15 SH2 waits, frame buffer write FIFO 3/5
    clocks, SDRAM 12 clocks per 8-word burst (a cache-through single read still bursts) - the
    figures `probe/` measures against.
15. **CD Word RAM -> VRAM capture DMA is undefined with the 32X** (Technical Information item 13):
    the "wrong first word" Sonic R5G works around (ledger §6) is Sega's own warning.
16. **Backup RAM** (Technical Bulletin addendum 4): unformatted at shipment, occasional garbling, keep
    nothing in the first and last word - consistent with docs/04's "test both on yours" and the
    record-with-checksum pattern of `sdcard/`.

## Process rules (unchanged, plus one)

`stack/` is off limits and md5-guarded; every addition goes behind an `HWFLAGS` switch in a copy and
is proved byte-identical with the switch off; numbers and pieces come from the ledger and are written
back into it; only MicroNut99's ares counts, and a green run there is a filter, never a pass; nothing
from the owner's machine (BIOS, discs, data) enters a repository. New from this pass: **when a document
and the code disagree, the code and the console measurement win; fix the document, say so here.**

## Read, skimmed, not read

- Read line by line: all of the above.
- Skimmed (Chilly's base, unchanged, md5-guarded): `crt.s` (a complete Sub-CPU program with its own
  command loop, embedded by `cd.s`, unused but for `init_cd`), `cdfs.s` (ISO-9660 + CD BIOS routines),
  `kos.s`, `font.s`, the `src-md/` 68000 side of the games (never runs on a CD boot).
- `manuals/sega-vr-disasm/`, read: the 32X Hardware Manual, Supplement 2, Technical Information and
  both attachments, the 32X Overview, the H/W Information sheets, the Introduction, the SRAM and EPROM
  cartridge manuals, the Genesis Software Development Manual, the Technical Bulletins, the index and
  the VR notes. Consulted for the sections the stack rests on: the SH7604 manual (§3 bus modes, §8
  cache), the Genesis Software Manual (header, SRAM). Not read line by line: the rest of the SH7604
  manual (9,300 lines), the 68000 Programmer's Reference, the Genesis Software Manual's VDP/FM
  chapters, the reference sheets, the sound driver, the memory datasheets.
- Not read: `32x_hardware_manual.pdf` (its Markdown form above was read; the PDF has the figures and
  the PWM chapter the conversion marks as truncated), `Mega-CD_Hardware_Manual.pdf` (no Markdown form
  exists; the Word RAM 2M-mode rule and the BIOS calls the stack uses come from it). Read both before
  touching what they describe (CLAUDE.md rule 2).
