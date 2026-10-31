# Evolution3 - the CD32X BootCamp, as read on 31 Oct 2026 (commit `a8c8afd`)

The third full reading. Evolution1 (`9e9ca98`) said what the kit is; Evolution2 (`cdab01b`) read the
code and listed where the documents and the code disagree. This pass read the kit again, then the
**eight repositories the kit stands between**: the ports (OpenLara REV `a9f13c4`, OpenJazz `9bd336e`
and OpenJazz REV `d486f58`, Hurrican `009912a`, SuperTux REV `83032ed`, Sonic CD 2011 `bc3c30b`, D32XR
`91a7baf`, Rayman `ab5e08b`), the `manuals/` repository as a whole (the two must-read documents, the
Kobo PDFs, the addenda, the four paging proposals, the stream-read note, the release notes), and - new
to this kit - **krikzz's public Mega EverDrive PRO sources** (krikzz/mega-ed-pub `7064f45`: the
extended-SSF specification, the FPGA mapper, the `edio` client and its samples). Everything in
Evolution1 and Evolution2 still holds. This file adds what the ports do that the ledger did not
record, what krikzz's source confirms of the console measurements, and the SD-card route that the
EverDrive has and no CD32X program uses yet.

The documents changed by this pass: `manuals/CD32X_COMMAND_LEDGER_ADDITIONS.md` (sections 2, 3, 4, 5,
7, 10 corrected; 16-19 added), `manuals/CD32X_COMMAND_PROGRAMMING_GUIDE.md` (the free lists, the RV
rule, the paging status), and a new `manuals/MegaEverDrivePro_SDCARD.md`. Nothing under `stack/` was
touched; `BOOT_FILES.md5` is unchanged.

## What the ports do that the ledger did not say (now ledger §16-19)

1. **Command 62 means two things.** SuperTux's 62 is `VRAM word` (ledger §1, boot-proven). On every
   EverDrive launcher build of OpenLara (EDSAVE-1, console 26 Oct), OpenJazz REV and Hurrican,
   **62 is "save piece in"**: a 4 KB piece of bank 31 → the Sub-CPU's buffer (`CART_EDSAVE`, `ed_page
   == 1`). On D32XR **62 is "Genesis background"** (the HUR genbg command Hurrican since replaced, with a
   listener routine 36 `set_scroll`); its game reads bank 31 with its own 68000 after CMD 60. SuperTux,
   whose 62 was taken, put its copy on **69** - which is OpenJazz REV's LZ unpack. Four meanings for 62. The
   ledger's rule is one number, one meaning; both breaches are recorded now (§4's clash table lists every
   number with two meanings across the six programs).
2. **Command 74 has two owners, one day apart.** Rayman R1 (26 Oct, `tools/stack_genvdp.py`): plane A's
   scroll written in the vertical blank, a new listener routine 38 `set_scroll_vb`. The BootCamp's
   `sdcard/` kit (27 Oct): `SDSAVE_CMD_OUT`. The kit's README reasoned from the stack ("it uses 0-68") and
   from the ledger ("69 and up free"), both stale. Ledger §18 asks the kit to move to 76/77; its numbers
   are make/`-D` variables, Rayman's is in a built launcher. **Not changed in this pass** - `sdcard/` is
   not the boot stack, but it is the owner's kit, so the renumbering is theirs to order.
3. **Every port numbers its own listener routines above 35.** SuperTux 36-37 (`MD_CMD_END` 38), D32XR 36
   `set_scroll` (`MD_CMD_END` 37), OpenJazz REV 36-45 (`genbg_*`, `rv_copy_fast`, `bmd_*`; `MD_CMD_END`
   46), Rayman 36-38 (`MD_CMD_END` 39), the BootCamp 1-35 (`MD_CMD_END` 36). MD 36 is `set_vsram` in three
   and `set_scroll` in D32XR; MD 37 is `copy_vram_col` in two and `genbg_load` in the third; MD 38 is
   `genbg_start` or `set_scroll_vb`. The paged-cart proposal's plan to move OpenJazz to 38-41 was
   never done. Ledger §2 has the table.
4. **Rayman's 72 / 73** are OpenLara's 61 / 62 at bank offset `0x30000`; **the kit's 74 / 75** are the same
   with the piece numbered across the whole 256 KB. Six programs, four numberings, one console-proven
   routine underneath (`m1_cart`'s bank-31 route). Ledger §16 is the table.
5. **Save piece 12 is Hurrican's** (its ED launcher reroutes CMD 56 / 59 - the high scores that used to go
   to the Sega CD's backup RAM - to bank-31 piece 12). The ledger said free; docs/02 and docs/07 said
   Hurrican; Evolution2 flagged it. The code wins: Hurrican. `hello/` borrows 12 for its boot counter, which
   is fine for a demo and must not be copied into a game. **No piece of the shared area is free.** On
   bank 31 the numbers are per launcher (each has its own `bram.srm`), so the shortage is the 4 MB cart's.
6. **Command 68 on the EverDrive**: the console-measured facts (docs/12, `edpage`) and the games' fix are
   now in one place (§16): `CART_ED` sends four **word** writes to CTRL4-7 through **MD 27**, pages 1-6
   only; Rayman alone keeps the one-byte request and relies on the launcher's `m1_page`. Open: the CTRL4-7
   writes the games make go through `m1_cpy_w` with `W` on; every CTRL write measured so far had `W` off.
7. **OpenJazz REV's 69 and 71 read COMM12 (and 71 COMM4) as inputs.** Against docs/06's habit of one
   writer per register; recorded in §19 so a port that copies them knows its status word is gone.
8. **Sonic's Kobo path, three more differences** now in the ledger: 45 has no "play once" bit; 56 / 59 take
   COMM2 = file (0 progress, 1 time attack, `SCD_SAVE_MIN`) and its launcher keeps both in bank 31 at
   `file × 512`; the chunk-run form of 46 answers with the Sub-CPU's `load_sum` checksum in COMM8, which is
   what the R4I warm start compares. Hurrican's and D32XR's paged builds give 38 a bit 15: the destination
   in COMM8L.
9. **Smaller corrections**: CMD 44 is always acknowledged (the ledger said "only while `SLIDESHOW_DEBUG`
   is 1" - it is the clears that depend on it); 50 / 51 write COMM12 32 bits wide (COMM14 goes with it);
   the free list is 39-43, 49, 55, 76-79, 81+; the programming guide's "free pieces 11, 12, 14" was wrong
   on all three.

## What krikzz's source says about what the console measured (`manuals/MegaEverDrivePro_SDCARD.md` §1)

- **The C bit does nothing, and the public RTL shows why**: `map_ssf.sv:68` is `assign cart =
  ssf_ctrl[0];` to a wire that goes nowhere; `mout.cart` is never driven. The console finding of 25 Oct
  (`EDPROBE.BIN`) is the design, not a fault of the probe.
- Banks 0-15 → PSRAM chip 0, 16-30 → chip 1, 31 → the battery RAM: `map_ssf.sv:58-61`, as docs/12 said
  from the older MEGA-PRO repository. The 68000's writes are gated by `W` (`wr_on = ssf_ctrl[2]`); the
  MCU's own writes come in on a separate DMA bus that `mem_ctrl` puts in front of the mapper, so `W` does
  not apply to them.
- "16-bit writes only" to the CTRL registers is in krikzz's specification (`extended-ssf.md`), which is
  the even-byte rule `edpage` measured.
- The PSRAM's top (`0xF00000` data buffer, `0xF40000` save state, `0xF80000` menu) is where the OS keeps
  its own things - banks 30-31 in SSF numbering, which matches "banks 28-30 are the EverDrive's" closely
  enough to keep page 7 off limits.
- Nothing about the 32X is in the public source beyond `MAP_32X = 2` and a "specific reset mode for 32x"
  flag. The `W` disturbance, the garbage during RV = 1 and the window-0 rule remain console-only facts.

## The SD card (`manuals/MegaEverDrivePro_SDCARD.md` §1.3, §4)

The card belongs to the EverDrive's ARM MCU. A 68000 program reaches it through a register block at
**`0xA130D0`** (FIFOs, a status word with a strobe, a 1 ms timer, a serial memory window, a mailbox) that
is decoded under every mapper, and a file API over the FIFO (`CMD_F_FOPN 0xC9`, **`CMD_F_FRD_MEM 0xCB`:
the MCU writes a file straight into cart memory by physical address**, the 68000 spinning from work RAM
while the MCU owns the PSRAM). No CD32X program uses it; the stack has no command for it. The research
in that document ends in one design - **preload the game's packs from the card in the launcher
(`edcd_md.s`), between the `M_OK`/`S_OK` wait and the mapper set-up, by physical address into the places
the game's own CMD 38 / 46 would fill, and let the ports' existing "is it in the cart?" checks hit** - and
seven console questions that come before it (does `0xA130D0` answer through the 32X adapter with RV = 1;
what an SH2 sees during MCU DMA; the bank arithmetic; throughput). It is a plan, not a result; nothing
has run.

## Where the documents and the code disagree (new this pass; Evolution2's nine still stand)

10. **The ledger's free line vs. the code**: fixed in the ledger (§3). docs/03's list needs 72-75 taken
    out ("72-79, 81 and up" → "76-79, 81 and up") and the Rayman / `sdcard/` rows added to its clash table.
    Not changed here: docs/03 is the kit's, and the kit's owner may want the `sdcard/` renumbering first.
11. **`sdcard/README_AI.md` §2 "74/75 are free in the stack (it uses 0-68)"**: true of the stack, false of
    the family (Rayman 74). Not changed: see item 2.
12. **STREAM's console status**: `CD32X_STREAM_READ.md` says ares only; the ledger §10 and the OpenLara
    release notes say OpenJazz R7 / Hurrican P2b ran it on the console; docs/07 lists OpenJazz R7-R9 as
    emulation only. Noted in the ledger §10: treat it as emulation-proven until a dated console run is
    written down. The kit ships it off by default, which is right either way.
13. **docs/11 §1 "banks 28-31: 28 save area, 30-31 the OS, 31 the PRO's save RAM (512 KB)"** is the
    pre-console guess; docs/12 and krikzz's RTL say bank 31 is the battery RAM and the OS's PSRAM use is
    the top of chip 1. docs/11 §8 already records the 25 Oct answer; §1's row is stale and should say so.
14. **docs/02 step 6 "Owners as of 26 Oct: ... 12 Hurrican's high scores (ED builds)"** was right and the
    ledger was wrong; the ledger now agrees.
15. **The OpenJazz addendum cites an OpenLara document (§2.1, §2.3, §5) that is not the OpenLara addendum
    in `manuals/`** (whose §2 is comm-register direction and §5 the handshakes); it is most likely the
    OpenLara repository's hand-off file. A reader following the citation into the PDF finds the wrong
    sections.
16. **`releases.txt`**: `ares-cd32x-v1.2-everdrive.zip` and `TombRaider_EDCD.zip` carry the same link.
17. **The programming guide's rule "while RV = 1, SH2 cart reads are simply held, so no parking code is
    needed"** was the 4 MB cart's truth and the EverDrive's trap; it now carries the `edsave3` exception.

## Process rules (unchanged, plus one)

`stack/` is off limits and md5-guarded; every addition goes behind an `HWFLAGS` switch in a copy and is
proved byte-identical with the switch off; numbers and pieces come from the ledger and are written back
into it; only MicroNut99's ares counts, and a green run there is a filter, never a pass; nothing from the
owner's machine enters a repository; when a document and the code disagree, the code and the console win.
New from this pass: **a number is taken the day a repository uses it, not the day the ledger learns of
it - so the ledger is read against every port's `main.c` before a number is picked, and the pick is
written into §3 in the same commit.** Two clashes (62, 74) came from trusting the ledger's free line.

## Read, skimmed, not read

- Read line by line: the kit's `README.md`, `CLAUDE.md`, Evolution1-2, docs/01-04, 06, 07, 10-12,
  `sdcard/` (both READMEs, `merge_sdsave.py`), `stack/subcpu/main.c` (the command loop), `hw_md.s` (the
  command table, the whole MODE1 block: `m1_xfer`, `M1_WINBLOCK`, `m1_cart`, `m1_cpy_w`, `m1_page`),
  `edboot/make_edcd.py` (the `RA` entry); in `manuals/`: the ledger and the programming guide in full,
  the two Kobo PDFs, the four addenda, the four paging proposals, the stream note, the release notes,
  `DIAG_TEXT_HOWTO.md`, `releases.txt`; in the ports: every `cd32x/subcpu/main.c` command handler above 60,
  the REV listener's table, Rayman's `EVERDRIVE_FINDINGS.md`, `EVERDRIVE_REPLY.md`, `HANDOFF.md`,
  `SAVES_SEGA_CD.md`, `stack_save.py`, `stack_genvdp.py`; in mega-ed-pub: `extended-ssf.md`,
  `edio/everdrive.{h,c}`, `edio/main.c`, `fpga/mapper/map_ssf/map_ssf.sv`, `lib_base/base_io.sv`,
  `pi_map.sv`, `var.sv` (`mem_ctrl`), `edapp/`.
- Skimmed: docs/05, 08, 09, 13; `hello/src/cdapi.c`; the ports' SH2 sides; `probe/`, `edtest/`, the
  ED probes (Evolution2 read them line by line; nothing there changed since `cdab01b`).
- Not read: `32x_hardware_manual.pdf` and `Mega-CD_Hardware_Manual.pdf` as PDFs (Evolution2's note
  stands: read them before touching what they describe); the MCU firmware and `edlink.exe` protocol
  (not public); OpenLara REV's EDCD branch (not on `main`; its EDSAVE-1 is known through
  `sdcard/README_AI.md`, Hurrican's and OpenJazz REV's copies of it, and the OpenLara release notes).
