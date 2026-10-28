/* sdsave.h - game saves on the Mega EverDrive PRO's SD card, for a CD32X game (the master SH2's side)
 *
 * The method is OpenLara CD32X-REV EDCD's (EDSAVE-1 .. EDSAVE-4, console-proven 26 Oct 2026: save,
 * load, relaunch, load).  The EverDrive's bank 31 is battery RAM that its OS keeps on the SD card as
 * MEGA/gamedata/<launcher>/bram.srm (256 KB) when the launcher's header declares save RAM
 * (EDCD_SAVE=1).  The SH2s cannot see that bank: every byte goes through the Sub-CPU (the commands
 * merged by sdcard/subcpu/merge_sdsave.py) and the 68000's cartridge-boot listener (hw_md.s MODE1-7,
 * M1SAVE=1), which copies with RV = 1.  While it does, both SH2s keep off the cart (EDSAVE-4).
 *
 * Only the launcher's program (MODE1=1 M1SAVE=1) has the bank.  Booted from the disc, sdsave_present()
 * is 0 and every call returns SDSAVE_E_NOBANK: the game then simply has no SD-card saves.
 *
 * Offsets are 0 .. SDSAVE_BYTES - 1 in the bank (the launcher's own bram.srm).  Any length, any
 * alignment: the library reads, changes and writes back whole 4 KB pieces.
 */
#ifndef SDSAVE_H
#define SDSAVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- settings (define before including, or with -D) ------------------------------------------- */
#ifndef SDSAVE_CMD_PUT
#define SDSAVE_CMD_PUT      58      /* the stack's own: COMM2 = word index, COMM8 = word -> its 4 KB buffer */
#endif
#ifndef SDSAVE_CMD_GET
#define SDSAVE_CMD_GET      57      /* the stack's own: COMM2 = word index -> COMM8                         */
#endif
#ifndef SDSAVE_CMD_OUT
#define SDSAVE_CMD_OUT      74      /* merge_sdsave.py: the buffer -> bank piece COMM2, read back, compared  */
#endif
#ifndef SDSAVE_CMD_IN
#define SDSAVE_CMD_IN       75      /* merge_sdsave.py: bank piece COMM2 -> the buffer (0x8000 = only ask)   */
#endif
#ifndef SDSAVE_PARK
#define SDSAVE_PARK         1       /* 1: park the slave SH2 in SDRAM for each copy (EDSAVE-4).  0 only if
                                       the slave can never touch the cart (code, data, stack, vectors) */
#endif
#ifndef SDSAVE_SLAVE_REG
#define SDSAVE_SLAVE_REG    (*(volatile uint16_t *)0x20004024)   /* COMM4: the slave's command register */
#endif
#ifndef SDSAVE_CMD_PARK
#define SDSAVE_CMD_PARK     0x7A60  /* master -> slave: go into the SDRAM loop        (OpenLara's value) */
#endif
#ifndef SDSAVE_CMD_PARK_ACK
#define SDSAVE_CMD_PARK_ACK 0x7A61  /* slave -> master: I am there; stay while it is this (OpenLara's)   */
#endif
#ifndef SDSAVE_RAMFUNC
/* where the spin loop lives: SDRAM.  mars.ld puts .data.* at 0x06000000 (SDRAM); OpenLara's own choice */
#define SDSAVE_RAMFUNC      __attribute__((section(".data.sdram"), noinline))
#endif

#define SDSAVE_BYTES        0x40000u    /* the bank as the listener shows it: cart 0x800000-0x83FFFF   */
#define SDSAVE_PIECE        4096u       /* one Sub-CPU copy                                            */

/* ---- results ---------------------------------------------------------------------------------- */
#define SDSAVE_OK            0
#define SDSAVE_E_NOBANK     -1      /* not the launcher's program (disc boot), or EDCD_SAVE was not 1  */
#define SDSAVE_E_CMD        -2      /* the Sub-CPU did not answer (command missing from its program?) */
#define SDSAVE_E_PARK       -3      /* the slave SH2 did not park                                     */
#define SDSAVE_E_VERIFY     -4      /* written, but the read-back differed: the save is not good      */
#define SDSAVE_E_RANGE      -5      /* ofs + len outside 0 .. SDSAVE_BYTES                             */
#define SDSAVE_E_EMPTY      -6      /* sdsave_load_record: no record with that magic there            */
#define SDSAVE_E_BAD        -7      /* sdsave_load_record: a record, but its length or checksum is off */

int  sdsave_present(void);                                       /* 1 yes, 0 no (asked once, kept)  */
int  sdsave_read(uint32_t ofs, void *dst, uint32_t len);
int  sdsave_write(uint32_t ofs, const void *src, uint32_t len);  /* read back and compared          */
void sdsave_forget(void);    /* call after the game itself used CMD 56/57/58/59: the Sub-CPU's buffer
                                is no longer the piece this library left there */

/* a checked record: 16-byte header (magic, length, FNV-1a sum, ~sum) then the data, at ofs */
int  sdsave_save_record(uint32_t ofs, uint32_t magic, const void *data, uint32_t len);
int  sdsave_load_record(uint32_t ofs, uint32_t magic, void *data, uint32_t len);

/* the SLAVE SH2 calls this when it reads SDSAVE_CMD_PARK in SDSAVE_SLAVE_REG, and does NOT clear the
   register afterwards (the master does).  See README_AI.md "The slave". */
void sdsave_slave_park(void);

#ifdef __cplusplus
}
#endif
#endif
