/* cdapi.c - the CD32X services (see cdapi.h).  CD32X BootCamp. */
#include "32x.h"
#include "cdapi.h"

#define COMM8_32   (*(volatile uint32_t *)0x20004028)   /* COMM8 + COMM10 as one long */
#define COMM12_16  (*(volatile uint16_t *)0x2000402C)   /* 16 bit: COMM14 is the Sub-CPU's heartbeat */

static void cd_pause(void)                  /* rule: never hammer a COMM register */
{
    for (volatile int k = 0; k < 64; k++) { }
}

int cd_cmd(uint16_t cmd)
{
    MARS_SYS_COMM0 = cmd;
    for (int n = 0; n < 2000000 && MARS_SYS_COMM0 != 0; n++)
        cd_pause();
    return MARS_SYS_COMM0 == 0;
}

void cd_status(uint16_t code)
{
    COMM12_16 = code;
}

uint32_t cd_open(int file)
{
    MARS_SYS_COMM2 = (uint16_t)file;
    if (!cd_cmd(CD_CMD_OPEN_FILE))
        return 0;
    int32_t len = (int32_t)COMM8_32;
    return len > 0 ? (uint32_t)len : 0;
}

int cd_load(int file)
{
    cd_music_stop();                        /* rule: no CD data reads while CD audio plays */
    uint32_t len = cd_open(file);
    if (len == 0)
        return 0;
    uint32_t chunks = (len + 32767) >> 15;
    for (uint32_t c = 0; c < chunks; c++)
    {
        COMM8_32 = len;                     /* CMD 46 reads the file length from COMM8 */
        MARS_SYS_COMM2 = (uint16_t)c;       /* bit 15 clear: the Sub-CPU reads it back and compares */
        COMM12_16 = 0xFFFF;
        if (!cd_cmd(CD_CMD_CHUNK_CART) || COMM12_16 != 0)
            return 0;
    }
    return 1;                               /* read it at CART_SLOT(slot) cache-through, or clear the cache */
}

uint16_t cd_pad(int sfx, int volume)
{
    MARS_SYS_COMM2 = (uint16_t)volume;
    if (!cd_cmd((uint16_t)(CD_CMD_PAD | (sfx << 8))))
        return 0;
    uint16_t pad = MARS_SYS_COMM8;
    if ((pad & SEGA_CTRL_TYPE) == SEGA_CTRL_NONE)
        return 0;                           /* no pad plugged in */
    return pad;
}

void cd_music_play(int track, int once)
{
    MARS_SYS_COMM2 = (uint16_t)(track | (once ? 0x8000 : 0));
    cd_cmd(CD_CMD_PLAY_TRACK);
}

void cd_music_stop(void)
{
    MARS_SYS_COMM2 = 0;
    cd_cmd(CD_CMD_STOP_MUSIC);
}

void cd_genesis_display(int on)
{
    MARS_SYS_COMM2 = (uint16_t)(on ? 1 : 0);
    cd_cmd(CD_CMD_GENESIS);
}

int cart_save(int piece, const uint16_t *words, int count)
{
    for (int i = 0; i < count; i++)         /* the Sub-CPU's 4 KB buffer, one word at a time */
    {
        MARS_SYS_COMM2 = (uint16_t)i;
        MARS_SYS_COMM8 = words[i];
        if (!cd_cmd(CD_CMD_BUF_PUT))
            return 0;
    }
    MARS_SYS_COMM2 = (uint16_t)piece;
    COMM12_16 = 0;
    if (!cd_cmd(CD_CMD_SAVE_PIECE))
        return 0;
    return COMM12_16 == 1;                  /* the 68000 wrote it and read it back */
}
