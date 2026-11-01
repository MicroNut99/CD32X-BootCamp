# Evolution4 - the CD32X BootCamp, as read on 1 Nov 2026 (commit `2846135`, manuals `d5d850a`)

The fourth full reading, and the first made by following [CLAUDE.md](CLAUDE.md)'s reading order from
step 0 to step 7. Evolution1-3 still hold. This pass adds the reference material none of them read in
full: the rest of `manuals/sega-vr-disasm/`, Hitachi's SH2 manuals, Sega's Genesis software manuals and
bulletins, and the three outside repositories of rule 3 (SGDK `2.11`, megadev `1.2.0`, kiloblaster-32x).
Nothing under `stack/` was touched, and no document in `manuals/` or `docs/` was changed. This file
only records what was found.

## What was read, and by whom

- **Read directly, every line:** CLAUDE.md, Evolution1-3, README, docs/01-13. The two must-read
  documents (ledger additions, programming guide). Both Kobo PDFs, `32x_hardware_manual.pdf` (102
  pages), `Mega-CD_Hardware_Manual.pdf` (97 pages). The OpenLara, OpenJazz 2 and OpenTyrian addenda,
  the RAM-cart overview PDF, `JazzBuildNotes`. Every other `.md` in `manuals/`. In `sega-vr-disasm/`:
  `32x-technical-info.md` + attachment 1, supplement 2, the SRAM cartridge manual, `32x-hardware-manual.md`,
  `32x-hardware-information.md`, `genesis-32x-overview.md`, `32x-introduction-and-system-features.md`.
- **Read in full by a separate reader, reported back:** `sh7604-hardware-manual.md`,
  `sh1-sh2-cpu-core-architecture.md`; the 68000 reference, `sega-genesis-software-manual.md`,
  `genesis-technical-overview.md`, `sega-genesis-reference-sheets.md`, `sound-driver-v3.md`;
  `genesis-technical-bulletins.md`, the Accolade and SNASM notes, the three IC BD manuals,
  `32x-eprom-cartridge-manual.md`, the three datasheets, `ROM_SIZE_CLARIFICATION.md`,
  `DOCUMENTATION_INDEX.md`, `genvdp.txt`; SGDK, megadev and kiloblaster-32x (READMEs, docs, and the
  boot, VDP, DMA, SRAM, pad, BIOS, Word RAM and CD-read sources). A reader's report is not the reading;
  the line numbers below are theirs, check them before relying on them.
- **Testing (step 7):** ares_android's `ares-headless` builds cleanly in a container (78/78, exit 0).
  `hello/` was not run: no US BIOS, no disc and no Sega toolchain in the container (docs/13 says both
  come from the owner's computer).

## What the reference material adds for this stack

1. **A real 32X holds an SH2 cart read while RV = 1; it does not return garbage.** The July 1994
   manual (`32x-hardware-manual.md` §3.1 "Cartridge ROM Access", §3.2.1 DREQ control): "When the RV
   bit is 1 and SH2 accesses the cartridge ROM, a wait occurs until 68000 replaces the RV bit with 0."
   This is the programming guide's §4 split, from Sega's side: on a real cart no parking code is
   needed; the EverDrive's garbage (`edsave3`) is the EverDrive's.
2. **Reset with RV = 1 needs Sega's corrective action.** Technical Info 6 / attachment 1: with RV = 1 a
   reset can leave the Mega Drive misbehaving, so from board Ver. 2.1 the master SH2's VRES handler
   checks RV and, if 1, resets the 32X through the FRT (`TOCR |= 1`, then loop); the slave loops.
   Applications that never set RV don't need it. Every CD32X program sets RV for cart writes, so
   pressing reset during a 46 / 61 / 68 is the case this covers.
3. **The 68000's own DMA into the SH2 FIFO works only from Sega CD Word RAM.** The April 1994 spec
   (`genesis-32x-overview.md`, DMA, and DREQ control `DMA` + `68S` = "DMA Write ... valid only when CD is
   connected"): "the MD Source Address works properly only by the CD word RAM. FIFO will not run
   properly even if you set the MD work RAM." No command here uses this path yet; it is the fast route
   from a CD read to SDRAM, and it is CD32X-only.
4. **The paged cart's even-byte page register sits on an address Sega's own board uses.** Sega's 32M SRAM
   development board (837-11068, `ic-bd-32m-sram-manual.md` 109-125) has a write-only "ROM area write
   enable" bit at `0xA130F0` **D15**, 0 at reset. The 8 / 16 MB cart's page register (`0xA130F0` /
   `0xA130F2`, page on D8-D10, one even-byte /UWR write, ledger §6 and §7) does not use D15, and the
   listener's write is already a byte (`move.b`), so the only risk is code written for Sega's board.
   Sega's bank registers are the odd bytes `0xA130F1-FF` and Sega's address checker allows bytes only
   in `0xA130F0-FF` (bulletins 1362-1363): a **word** write to `0xA130F0` would also write `0xA130F1`
   (Sega's REG0: ROM/RAM above `0x200000`, backup-RAM write protect). That holds for a real paged cart.
   The EverDrive is a different mapper: its word writes go to CTRL4-7 at `0xA130F8-FE` (ledger §16).
5. **The Main CPU's top of work RAM belongs to the CD BIOS.** megadev: BIOS RAM `0xFFF700-0xFFFC00`,
   the jump table `0xFFFD00-0xFFFDB4`, BIOS variables to about `0xFFFE58`. Sega of America's 1993
   Sega CD sample code (quoted in megadev `docs/main_bios.md` ~98) says anything after `0xFFFDDD` locks
   the machine. Sega's sound driver V3 fixes its work area at `0xFFF000` (about `0xB00` + a `0x200` YM
   buffer, to about `0xFFFD00`), so it cannot be used unchanged in a CD-booted 68000. The listener at
   `0xFF1000` is clear of all of this.
6. **SGDK cannot be dropped into this stack as it is.** Its start-up (`sys.c` `_start_entry`) clears
   `0xFF0000-0xFFFFFF`, which erases the listener and the BIOS jump table; its boot assumes cart
   vectors at 0 and writes TMSS; its SRAM is fixed at `0x200001`, which is Word RAM on a Mega CD; its
   VINT never raises IFL2, so the Sub CPU would get no INT2. Its VDP and DMA code is the useful part
   (register cache, 128 KB source split, the triggering command word as a 16-bit write from memory,
   Z80 bus held round DMA, the 6-button TH protocol).
7. **The end of a Word-RAM-to-VDP DMA is described three ways.** Sega's Genesis manual: for a source
   in `0x000000-0x3FFFFF` the final command write must be a word write taken from work RAM (SW
   1269-1274, 3052-3073). megadev's header says to write the final word by hand; the version usually
   quoted from Sega says source + 2 and write the first word by hand. Settle it against
   `Mega-CD_Hardware_Manual.pdf` before a new command DMAs from Word RAM.
8. **The one-byte cache purge.** Hitachi (SH7604 §8, 3701, 3768-3777, 3828) clears CE, then purges,
   then sets CE again, and changes CCR only with the cache off. The stack writes `0x11` in one byte
   (Evolution2's list, console-proven; kiloblaster-32x does the same). Not new, now confirmed against
   the manual. Hitachi also does not forbid TAS (it recommends it for semaphores, 3804) or SLEEP: both
   bans in `32x-hardware-manual.md` §5.3 are Sega's, for the 32X.
9. **kiloblaster-32x's boot uses COMM8.** Its `sega_startup.inc` clears COMM8 / COMM10, then, if the
   header checksum at `0x18E` is non-zero, waits for COMM8 to equal it. Fine for a cartridge; worth
   knowing before borrowing that start-up into anything that uses COMM8 for commands, as this stack does.

## What not to trust in the reference copies

- **`32x-introduction-and-system-features.md` and `genesis-32x-overview.md`** are the April 1994
  MARS-10 spec. Where they differ from `32x-hardware-manual.md` (July 1994) the later one wins: PEN's
  polarity, FM shown in the frame-buffer control register, the PWM L/R table, the interrupt-mask bit
  positions, A15200 / A15400 for VDP REG / palette, two-word frame-buffer FIFO, the access-time tables.
- **`sh7604-hardware-manual.md` Appendix B:** wrong DIVU (`FFFFFE00` for `FFFFFF00`), BSC and DRCR
  addresses; OCRA's initial value contradicts §11 (`FFFF`). `sh1-sh2-cpu-core-architecture.md` is
  missing its sections 6-7 (instructions, pipeline) and appendix B.
- **The 68000 reference** is an excerpt: no exception chapter, no bus / address error frame.
- **`genesis-technical-overview.md`** draws the VSRAM command with the wrong bit and puts the status
  register's DMA bit at 3; the software manual and reference sheets (DMA = bit 1, VB = bit 3) agree.
  The "DMA can't cross 64 KB" note in the reference sheets is unverified; SGDK splits at 128 KB.
- **`32x-eprom-cartridge-manual.md`** carries ic-bd-16m's document number but differs from it (parts,
  sizes, DIP tables) and has non-Sega text at 361-489. **`ROM_SIZE_CLARIFICATION.md`** is a project
  note, and its SH2 map contradicts itself.
- **megadev:** the DMA read path resets its frame counter at `0x76` against `0x75` on the CPU path;
  `new_project` clears 4x its BSS; `EXVEC_ADDRERR` = `EXVEC_ILLEGAL`; COMSTAT is labelled Main→Sub.

## For the next reading

- Run `hello/` in `ares-headless` with the owner's BIOS and disc (docs/13), and record the result here.
- Decide item 7 from the Mega-CD manual, and whether item 2's VRES fix belongs in the stack (rule 4:
  only if the owner asks).
- Note items 3 and 5 in docs/04 if the owner wants them there.
