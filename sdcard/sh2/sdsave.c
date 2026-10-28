/* sdsave.c - game saves on the Mega EverDrive PRO's SD card (the master SH2's side).  See sdsave.h.
 *
 * Everything here is OpenLara CD32X-REV EDCD's engine/openlara-edsave.patch, with the same timings:
 *   sds_spin      = csSpin      (SDRAM, SR |= 0xF0, 4,000,000 rounds, a 64-count pause per round)
 *   sds_cmd_rv    = cdCmdSafe   (a command whose 68000 side copies with RV = 1)
 *   sds_cmd       = cdCmd       (2,000,000 rounds of the 64-count pause)
 *   sds_park/unpark = csPark/csUnpark (COMM4 = PARK, wait for PARK_ACK; nested)
 *   piece in / out = saveBankRead / csFlushPiece (CMD 58 per word, then the copy; the copy, then CMD 57)
 * One difference: OpenLara's two waits for the slave are unbounded; here they give up after
 * SDSAVE_WAIT rounds and the save or load is REFUSED (SDSAVE_E_PARK) - never done unparked.
 */
#include "sdsave.h"

#define SDS_COMM0   (*(volatile uint16_t *)0x20004020)
#define SDS_COMM2   (*(volatile uint16_t *)0x20004022)
#define SDS_COMM8   (*(volatile uint16_t *)0x20004028)
#define SDS_COMM12  (*(volatile uint16_t *)0x2000402C)

#ifndef SDSAVE_WAIT
#define SDSAVE_WAIT 2000000u        /* rounds of sds_pause for the slave (as cdCmd's bound)            */
#endif
#define SDS_DONE    0x201           /* the Sub-CPU's reply: 0x200 = the save bank, 1 = done            */

static int     sds_bank = -1;       /* -1 not asked yet, 0 no bank, 1 the bank                          */
static int     sds_rd_piece = -1;   /* the piece in the Sub-CPU's 4 KB buffer (-1: unknown)            */
static int     sds_buf_piece = -1;  /* the piece sds_buf holds as the bank has it (-1: none)           */
static int     sds_park_depth = 0;
static uint8_t sds_buf[SDSAVE_PIECE];

static void sds_pause(void)         /* hardware rule: never hammer a COMM register */
{
    for (volatile int k = 0; k < 64; k++) {}
}

static int sds_cmd(uint16_t cmd)
{
    uint32_t n;
    SDS_COMM0 = cmd;
    for (n = 0; n < 2000000u && SDS_COMM0 != 0; n++)
        sds_pause();
    if (SDS_COMM0 == 0) return 0;
    SDS_COMM0 = 0;                  /* nobody answered: give the register back */
    return -1;
}

/* runs from SDRAM: interrupts masked, spin while (*reg == value) == whileEqual.  No calls, no cart
   data: this function and its stack are the only memory touched.  ack != 0: write it to *reg first
   (the slave: "I am in SDRAM now"). */
SDSAVE_RAMFUNC
static void sds_spin(volatile uint16_t *reg, uint16_t value, int32_t whileEqual, uint16_t ack)
{
    uint32_t sr, masked;
    __asm__ volatile ("stc sr,%0" : "=r"(sr));
    masked = sr | 0xF0u;
    __asm__ volatile ("ldc %0,sr" : : "r"(masked));
    if (ack) *reg = ack;
    for (int32_t n = 0; n < 4000000; n++)
    {
        if (((*reg == value) ? 1 : 0) != whileEqual) break;
        for (volatile int32_t k = 0; k < 64; k++) {}
    }
    __asm__ volatile ("ldc %0,sr" : : "r"(sr));
}

static int sds_cmd_rv(uint16_t cmd)  /* the 68000 copies with RV = 1: the master waits in SDRAM */
{
    SDS_COMM0 = cmd;
    sds_spin(&SDS_COMM0, 0, 0, 0);   /* from SDRAM until COMM0 == 0 */
    return SDS_COMM0 == 0 ? 0 : -1;
}

static int sds_park(void)
{
#if SDSAVE_PARK
    if (sds_park_depth++ == 0)
    {
        uint32_t n;
        for (n = 0; n < SDSAVE_WAIT && SDSAVE_SLAVE_REG != 0; n++) sds_pause();   /* between commands */
        if (SDSAVE_SLAVE_REG != 0) { sds_park_depth--; return -1; }
        SDSAVE_SLAVE_REG = SDSAVE_CMD_PARK;
        for (n = 0; n < SDSAVE_WAIT && SDSAVE_SLAVE_REG != SDSAVE_CMD_PARK_ACK; n++) sds_pause();
        if (SDSAVE_SLAVE_REG != SDSAVE_CMD_PARK_ACK) { SDSAVE_SLAVE_REG = 0; sds_park_depth--; return -1; }
    }
#endif
    return 0;
}

static void sds_unpark(void)
{
#if SDSAVE_PARK
    if (--sds_park_depth == 0) SDSAVE_SLAVE_REG = 0;
#endif
}

void sdsave_slave_park(void)        /* the slave: into SDRAM until the master lets go */
{
    sds_spin(&SDSAVE_SLAVE_REG, SDSAVE_CMD_PARK_ACK, 1, SDSAVE_CMD_PARK_ACK);
}

int sdsave_present(void)
{
    if (sds_bank < 0)
    {
        SDS_COMM2 = 0x8000;          /* only ask: no copy, so no park */
        SDS_COMM12 = 0;
        if (sds_cmd(SDSAVE_CMD_IN) != 0) { sds_bank = 0; return 0; }
        sds_bank = (SDS_COMM12 & 0x200) ? 1 : 0;
    }
    return sds_bank;
}

void sdsave_forget(void)
{
    sds_rd_piece = -1;
    sds_buf_piece = -1;
}

static int sds_copy(uint16_t cmd, int piece)    /* one 4 KB copy between the buffer and the bank */
{
    int r;
    SDS_COMM2 = (uint16_t)piece;
    SDS_COMM12 = 0;
    if (sds_park() != 0) return SDSAVE_E_PARK;
    r = sds_cmd_rv(cmd);
    sds_unpark();
    if (r != 0) return SDSAVE_E_CMD;
    if (SDS_COMM12 != SDS_DONE) return cmd == SDSAVE_CMD_OUT ? SDSAVE_E_VERIFY : SDSAVE_E_CMD;
    return SDSAVE_OK;
}

static int sds_piece_in(int piece)  /* the piece -> sds_buf */
{
    int r, w;
    if (piece == sds_buf_piece)
        return SDSAVE_OK;           /* already here: the bank changes only through this file */
    sds_buf_piece = -1;
    if (piece != sds_rd_piece)
    {
        sds_rd_piece = -1;
        r = sds_copy(SDSAVE_CMD_IN, piece);
        if (r != SDSAVE_OK) return r;
        sds_rd_piece = piece;
    }
    for (w = 0; w < (int)SDSAVE_PIECE / 2; w++)
    {
        uint16_t v;
        SDS_COMM2 = (uint16_t)w;
        if (sds_cmd(SDSAVE_CMD_GET) != 0) return SDSAVE_E_CMD;
        v = SDS_COMM8;
        sds_buf[w * 2] = (uint8_t)(v >> 8);
        sds_buf[w * 2 + 1] = (uint8_t)v;
    }
    sds_buf_piece = piece;
    return SDSAVE_OK;
}

static int sds_piece_out(int piece) /* sds_buf -> the piece, read back and compared by the Sub-CPU */
{
    int w, r;
    sds_rd_piece = -1;              /* the Sub-CPU's buffer is ours now (as OpenLara: forget it) */
    sds_buf_piece = -1;
    for (w = 0; w < (int)SDSAVE_PIECE / 2; w++)
    {
        SDS_COMM8 = (uint16_t)((sds_buf[w * 2] << 8) | sds_buf[w * 2 + 1]);
        SDS_COMM2 = (uint16_t)w;
        if (sds_cmd(SDSAVE_CMD_PUT) != 0) return SDSAVE_E_CMD;
    }
    r = sds_copy(SDSAVE_CMD_OUT, piece);
    if (r == SDSAVE_OK) sds_buf_piece = piece;  /* read back and equal: the bank holds sds_buf */
    return r;
}

static int sds_check(uint32_t ofs, uint32_t len)
{
    if (!sdsave_present()) return SDSAVE_E_NOBANK;
    if (ofs > SDSAVE_BYTES || len > SDSAVE_BYTES - ofs) return SDSAVE_E_RANGE;
    return SDSAVE_OK;
}

int sdsave_read(uint32_t ofs, void *dst, uint32_t len)
{
    uint8_t *d = (uint8_t *)dst;
    int r = sds_check(ofs, len);
    if (r != SDSAVE_OK) return r;
    while (len > 0)
    {
        int piece = (int)(ofs / SDSAVE_PIECE);
        uint32_t o = ofs % SDSAVE_PIECE, n = SDSAVE_PIECE - o, i;
        if (n > len) n = len;
        r = sds_piece_in(piece);
        if (r != SDSAVE_OK) return r;
        for (i = 0; i < n; i++) d[i] = sds_buf[o + i];
        ofs += n; d += n; len -= n;
    }
    return SDSAVE_OK;
}

/* two byte runs, one after the other, from ofs on: each piece they touch is written once */
static int sds_write2(uint32_t ofs, const uint8_t *a, uint32_t alen, const uint8_t *b, uint32_t blen)
{
    uint32_t len = alen + blen;
    int r = (len < alen) ? SDSAVE_E_RANGE : sds_check(ofs, len);
    if (r != SDSAVE_OK) return r;
    while (len > 0)
    {
        int piece = (int)(ofs / SDSAVE_PIECE);
        uint32_t o = ofs % SDSAVE_PIECE, n = SDSAVE_PIECE - o, i;
        if (n > len) n = len;
        if (n != SDSAVE_PIECE)      /* part of a piece: keep the rest of it */
        {
            r = sds_piece_in(piece);
            if (r != SDSAVE_OK) return r;
        }
        for (i = 0; i < n; i++)
        {
            if (alen) { sds_buf[o + i] = *a++; alen--; }
            else      { sds_buf[o + i] = *b++; blen--; }
        }
        r = sds_piece_out(piece);
        if (r != SDSAVE_OK) return r;
        ofs += n; len -= n;
    }
    return SDSAVE_OK;
}

int sdsave_write(uint32_t ofs, const void *src, uint32_t len)
{
    return sds_write2(ofs, (const uint8_t *)src, len, (const uint8_t *)0, 0);
}

/* ---- checked records ---------------------------------------------------------------------------- */
static uint32_t sds_fnv(const uint8_t *p, uint32_t len)
{
    uint32_t h = 0x811C9DC5u;
    while (len--) { h ^= *p++; h *= 0x01000193u; }
    return h;
}

static void sds_put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

static uint32_t sds_get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

int sdsave_save_record(uint32_t ofs, uint32_t magic, const void *data, uint32_t len)
{
    uint8_t h[16];
    uint32_t sum = sds_fnv((const uint8_t *)data, len);
    sds_put32(h, magic); sds_put32(h + 4, len); sds_put32(h + 8, sum); sds_put32(h + 12, ~sum);
    /* header and data in one pass (a small record is one piece, written once).  A save cut off half
       way does not pass sdsave_load_record: the sum covers the data. */
    return sds_write2(ofs, h, 16, (const uint8_t *)data, len);
}

int sdsave_load_record(uint32_t ofs, uint32_t magic, void *data, uint32_t len)
{
    uint8_t h[16];
    uint32_t sum;
    int r = sdsave_read(ofs, h, 16);
    if (r != SDSAVE_OK) return r;
    if (sds_get32(h) != magic) return SDSAVE_E_EMPTY;
    sum = sds_get32(h + 8);
    if (sds_get32(h + 4) != len || sds_get32(h + 12) != ~sum) return SDSAVE_E_BAD;
    r = sdsave_read(ofs + 16, data, len);
    if (r != SDSAVE_OK) return r;
    return sds_fnv((const uint8_t *)data, len) == sum ? SDSAVE_OK : SDSAVE_E_BAD;
}
