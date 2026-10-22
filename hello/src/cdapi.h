/* cdapi.h - the CD32X services for a game's master SH2 (CD32X BootCamp)
 *
 * Everything the 32X cannot do itself goes to the Sega CD's Sub-CPU (stack/subcpu/main.c) as a
 * command: the master SH2 writes the arguments to COMM2 / COMM8, the command number to COMM0,
 * and waits until the Sub-CPU sets COMM0 back to 0.  Only the MASTER SH2 sends commands.
 * Full table: docs/03_COMMANDS.md.  Status codes for debugging go to COMM12 (cd_status).
 */
#ifndef CDAPI_H
#define CDAPI_H
#include <stdint.h>

/* the command numbers of this stack (OpenLara CD32X path) */
#define CD_CMD_OPEN_FILE    38      /* COMM2 = file number            -> COMM8L = length (0 = not found) */
#define CD_CMD_PLAY_TRACK   45      /* COMM2 = track (| 0x8000: once) -> CD audio */
#define CD_CMD_CHUNK_CART   46      /* COMM2 = chunk (| 0x8000: no read-back), COMM8L = file length -> COMM12 = 0 ok */
#define CD_CMD_PAD          48      /* COMM0 high byte = PCM effect (0 none), COMM2 = its volume -> COMM8 = pad bits */
#define CD_CMD_GENESIS      52      /* COMM2 = 1 / 0: the Genesis display on / off */
#define CD_CMD_LOAD_SFX     53      /* COMM2 = n: SFXnn.BIN -> PCM chip      -> COMM12 = number of effects */
#define CD_CMD_STOP_MUSIC   54
#define CD_CMD_BRAM_LOAD    56      /* Sega CD backup RAM file -> the 4 KB buffer -> COMM12 = 1 ok */
#define CD_CMD_BUF_GET      57      /* COMM2 = word index            -> COMM8 = word of the 4 KB buffer */
#define CD_CMD_BUF_PUT      58      /* COMM2 = word index, COMM8 = word -> into the 4 KB buffer */
#define CD_CMD_BRAM_SAVE    59      /* the 4 KB buffer -> Sega CD backup RAM file -> COMM12 = 1 ok */
#define CD_CMD_SAVE_PIECE   61      /* COMM2 = piece 0-15: the 4 KB buffer -> cart 0x3F0000 + piece x 4 KB -> COMM12 = 1 ok */

#define CART_BASE           0x22000000u   /* the RAM cart as the SH2 reads it, cache-through */
#define CART_SLOT(n)        (CART_BASE + ((uint32_t)(n) << 17))   /* slot n x 128 KB */
#define CART_SAVE(piece)    (CART_BASE + 0x3F0000u + ((uint32_t)(piece) << 12))

int      cd_cmd(uint16_t cmd);                          /* 1 = answered, 0 = no answer (timeout) */
void     cd_status(uint16_t code);                      /* COMM12: shown as STATE: in the diagnostics */
uint32_t cd_open(int file);                             /* length, 0 = not on the disc */
int      cd_load(int file);                             /* open + every chunk into its cart slot; 1 = ok */
uint16_t cd_pad(int sfx, int volume);                   /* pad 1 (SEGA_CTRL_*), and play PCM effect sfx (0 = none) */
void     cd_music_play(int track, int once);
void     cd_music_stop(void);
void     cd_genesis_display(int on);
int      cart_save(int piece, const uint16_t *words, int count);   /* words -> buffer -> cart save piece; 1 = ok */

#endif
