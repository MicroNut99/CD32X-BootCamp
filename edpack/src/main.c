/* edpack - does the EverDrive PRO menu load a 14 MB .32X into its PSRAM whole?  (CD32X BootCamp)
 *
 * The question behind MegaEverDrivePro_SDCARD.md section 4.0: if a launcher file (EDCD_<GAME>.32X)
 * carries the game's data behind its first 512 KB, the EverDrive OS has already "preloaded" the PSRAM
 * when it starts the launcher - IF it loads a multi-megabyte .32X in full in its 32X mode.  Nobody has
 * tried a .32X bigger than a few hundred KB on it.
 *
 * This ROM is edtest's 68000 side with a read-only SH2 program.  tools/pad_markers.py pads the ROM to
 * 14 MB (banks 0-27) and fills banks 1-27 with edtest's pattern, one seed per bank.  At run time nothing
 * is written: CTRL0 = P only (W off, as a launcher runs), each bank is mapped into window 1 and read back
 * by the 68000 (RV = 1) and by both SH2s (cache-through, as a game reads its cart).
 *
 * Per bank:  OK     = the file's pattern is there, read the same by the 68000 and both SH2s
 *            MISS   = something else is there (the menu did not load this far); the first words tell
 *                     what: "BANK n" = another bank's pattern (an alias or a 4 MB mirror), else the word
 *            SH2    = the 68000 reads the pattern, an SH2 does not
 * VERDICT: LOADED n MB = banks 0..k all OK (k+1 banks x 512 KB); the file is 14 MB.
 */
#include <stdint.h>
#include "32x.h"
#include "font8x8.h"

#ifndef PACK_NONCE
#error "PACK_NONCE must be the number tools/pad_markers.py was given (edpack/build.sh does both)"
#endif
#define ED_CTRL0    0x8000                  /* P only: W off, bank 0 in window 0 - as a launcher runs */
#define FIRST_BANK  1
#define LAST_BANK   27                      /* the file ends at 14 MB; 28-31 are the EverDrive's */
#define WORDS       262144u

#define COMM8_32   (*(volatile uint32_t *)0x20004028)
#define COMM12_32  (*(volatile uint32_t *)0x2000402C)

volatile uint32_t gTicks = 0;
volatile unsigned mars_pwdt_ovf_count = 0;
volatile unsigned mars_swdt_ovf_count = 0;
void pri_vbi_handler(void) { gTicks++; }

typedef struct { uint32_t go, seq, window, seed, bad, first, got, done_seq; } Job;
static Job job_mem;
#define JOB ((volatile Job *)((uint32_t)&job_mem | 0x20000000u))

static uint16_t pat(uint32_t i, uint16_t seed)          /* = edtest's, = the 68000's, = pad_markers.py's */
{
    return (uint16_t)((i & 0xFFFF) ^ (((i >> 16) & 3) << 12) ^ seed);
}
static uint16_t seed_of(int bank) { return (uint16_t)((bank * 0x1F3D) ^ PACK_NONCE); }

static void verify_range(int window, uint16_t seed, uint32_t from, uint32_t to,
                         uint32_t *bad, uint32_t *first, uint32_t *got)
{
    const volatile uint16_t *w = (const volatile uint16_t *)(0x22000000u + ((uint32_t)window << 19));
    uint32_t n = 0, f = 0xFFFFFFFFu, g = 0;
    for (uint32_t i = from; i < to; i++)
    {
        uint16_t v = w[i];
        if (v != pat(i, seed)) { if (!n) { f = i * 2; g = v; } n++; }
    }
    *bad = n; *first = f; *got = g;
}

void sec_dma1_handler(void) { }
void secondary(void)
{
    while (1)
    {
        while (JOB->go != 1) { }
        uint32_t bad, first, got;
        verify_range((int)JOB->window, (uint16_t)JOB->seed, WORDS / 2, WORDS, &bad, &first, &got);
        JOB->bad = bad; JOB->first = first; JOB->got = got;
        JOB->done_seq = JOB->seq;
        JOB->go = 2;
    }
}

static uint16_t last_cmd, last_arg;
static int md_cmd(uint16_t cmd, uint16_t arg)
{
    last_cmd = cmd; last_arg = arg;
    uint32_t t0 = gTicks;
    while (MARS_SYS_COMM0 != 0 && gTicks - t0 < 600) { }
    MARS_SYS_COMM2 = (short)arg;
    MARS_SYS_COMM0 = (short)cmd;
    t0 = gTicks;
    while (MARS_SYS_COMM0 != 0 && gTicks - t0 < 600) { }
    return MARS_SYS_COMM0 == 0;
}

#define FB          ((volatile uint16_t *)&MARS_FRAMEBUFFER)
enum { C_BLACK, C_WHITE, C_RED, C_GREEN, C_YELLOW, C_GREY, C_BLUE, C_CYAN };

static void palette(void)
{
    static const uint16_t pal[8] = { 0x0000, 0x7FFF, 0x001F, 0x03E0, 0x03FF, 0x4210, 0x7C00, 0x7FE0 };
    volatile uint16_t *cram = (volatile uint16_t *)&MARS_CRAM;
    while ((MARS_VDP_FBCTL & MARS_VDP_PEN) == 0) { }
    for (int i = 0; i < 256; i++) cram[i] = (i < 8) ? pal[i] : 0;
}
static void line_table(void)
{
    for (int i = 0; i < 256; i++) FB[i] = (uint16_t)(0x100 + (i < 224 ? i : 223) * 160);
}
static void clear(void)
{
    volatile uint16_t *p = FB + 0x100;
    for (int i = 0; i < 160 * 224; i++) p[i] = 0;
}
static void text(int x, int y, const char *s, uint8_t c)
{
    for (; *s; s++, x += 8)
    {
        int ch = (uint8_t)*s;
        if (ch < 32 || ch > 127) ch = '?';
        const unsigned char *g = font8x8[ch - 32];
        for (int r = 0; r < 8; r++)
            for (int b = 0; b < 8; b += 2)
                FB[0x100 + (y + r) * 160 + ((x + b) >> 1)] =
                    (uint16_t)((((g[r] & (0x80 >> b)) ? c : 0) << 8) | ((g[r] & (0x40 >> b)) ? c : 0));
    }
}
static void flip(void)
{
    uint16_t cur = MARS_VDP_FBCTL & MARS_VDP_FS;
    MARS_VDP_FBCTL = (uint16_t)(cur ^ 1);
    while ((MARS_VDP_FBCTL & MARS_VDP_FS) == cur) { }
}
static char *hexn(char *d, uint32_t v, int n)
{
    for (int i = 0; i < n; i++) d[i] = "0123456789ABCDEF"[(v >> (4 * (n - 1 - i))) & 15];
    d[n] = 0;
    return d;
}
static char *dec(char *d, uint32_t v)
{
    char t[12]; int n = 0; char *s = d;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *d++ = t[--n];
    *d = 0;
    return s;
}

enum { ST_UNTESTED, ST_OK, ST_MISS, ST_SH2 };
static uint8_t  status[32];
static int16_t  found[32];                  /* MISS: which bank's pattern was there (-1 = none known) */
static uint16_t word0[32];                  /* MISS: the first word read */
static uint32_t bad68_total, badsh2_total;
static int      have_first;
static uint32_t first_bank, first_off, first_want, first_got;
static const char *first_who = "";
static const char *phase = "START";
static int      cur_bank, slave_dead, stuck, done_all;
static uint16_t stuck_comm[8];
static const char *mapper_text = "NOT CHECKED";
static int      mapper_ok;

static void note_first(int bank, uint32_t off, uint16_t want, uint16_t got, const char *who)
{
    if (have_first) return;
    have_first = 1;
    first_bank = (uint32_t)bank; first_off = off; first_want = want; first_got = got; first_who = who;
}

static void draw(void)
{
    char b[16];
    clear();
    text(8, 4, "EVERDRIVE PRO 14 MB FILE PROBE", C_YELLOW);
    text(264, 4, hexn(b, PACK_NONCE, 4), C_GREY);
    text(8, 16, phase, C_CYAN);
    if (cur_bank && !stuck) { text(200, 16, "BANK", C_GREY); text(240, 16, dec(b, (uint32_t)cur_bank), C_WHITE); }
    text(280, 16, dec(b, gTicks / 60), C_GREY);

    for (int bank = FIRST_BANK; bank <= LAST_BANK; bank++)
    {
        int k = bank - FIRST_BANK, x = 8 + (k / 9) * 104, y = 32 + (k % 9) * 11;
        b[0] = 'B'; b[1] = (char)('0' + bank / 10); b[2] = (char)('0' + bank % 10); b[3] = 0;
        text(x, y, b, C_GREY);
        if (status[bank] == ST_OK)        text(x + 32, y, "OK", C_GREEN);
        else if (status[bank] == ST_SH2)  text(x + 32, y, "SH2", C_RED);
        else if (status[bank] == ST_MISS)
        {
            if (found[bank] >= 0) { text(x + 32, y, "=B", C_RED); text(x + 48, y, dec(b, (uint32_t)found[bank]), C_RED); }
            else                  { text(x + 32, y, hexn(b, word0[bank], 4), C_RED); }
        }
        else text(x + 32, y, "-", C_GREY);
    }

    text(8, 134, "BAD WORDS 68K", C_GREY);  text(120, 134, dec(b, bad68_total), C_WHITE);
    text(208, 134, "SH2", C_GREY);           text(240, 134, dec(b, badsh2_total), C_WHITE);
    if (have_first)
    {
        text(8, 148, "FIRST DIFF: BANK", C_RED); text(144, 148, dec(b, first_bank), C_WHITE);
        text(176, 148, first_who, C_WHITE);
        text(8, 159, "OFFSET +", C_GREY); text(72, 159, hexn(b, first_off, 5), C_WHITE);
        text(128, 159, "WANT", C_GREY);   text(168, 159, hexn(b, first_want, 4), C_WHITE);
        text(208, 159, "GOT", C_GREY);    text(240, 159, hexn(b, first_got, 4), C_WHITE);
    }
    text(8, 167, "MAPPER:", C_GREY);
    text(72, 167, mapper_text, mapper_ok ? C_GREEN : C_RED);
    if (slave_dead) text(208, 148, "SLAVE SH2 NO ANSWER", C_RED);

    /* the verdict: how far the file arrived, counted from bank 0 (this program, which is running) */
    int k = 0;
    while (k < LAST_BANK && status[k + 1] == ST_OK) k++;
    int miss = 0, sh2 = 0;
    for (int bank = FIRST_BANK; bank <= LAST_BANK; bank++) { miss += status[bank] == ST_MISS; sh2 += status[bank] == ST_SH2; }
    text(8, 176, "VERDICT:", C_GREY);
    if (!done_all) text(80, 176, "READING...", C_YELLOW);
    else
    {
        char v[24], d[8];
        uint32_t half = (uint32_t)(k + 1);              /* 512 KB units, bank 0 included */
        int n = 0;
        for (const char *s = "LOADED "; *s; s++) v[n++] = *s;
        for (const char *s = dec(d, half / 2); *s; s++) v[n++] = *s;
        if (half & 1) { v[n++] = '.'; v[n++] = '5'; }
        for (const char *s = " OF 14 MB"; *s; s++) v[n++] = *s;
        v[n] = 0;
        text(80, 176, v, k == LAST_BANK ? C_GREEN : C_YELLOW);
        if (sh2) text(8, 186, "68000 OK BUT AN SH2 READ DIFFERS", C_RED);
        else if (k == LAST_BANK) text(8, 186, "THE WHOLE FILE IS IN THE PSRAM", C_GREEN);
        else if (miss && found[k + 1] >= 0) text(8, 186, "PAST THAT: ANOTHER BANK'S DATA (MIRROR)", C_YELLOW);
        else if (miss) text(8, 186, "PAST THAT: NOT THE FILE (NOT LOADED)", C_YELLOW);
    }
    if (stuck)
    {
        text(8, 196, "CMD", C_RED); text(40, 196, hexn(b, last_cmd, 4), C_WHITE);
        text(88, 196, "ARG", C_RED); text(120, 196, hexn(b, last_arg, 4), C_WHITE);
        for (int i = 0; i < 8; i++)
            text(8 + (i & 3) * 80, 206 + (i >> 2) * 9, hexn(b, stuck_comm[i], 4), C_WHITE);
        flip();
        return;
    }
    text(8, 206, "NOTHING IS WRITTEN. W OFF. BANKS 28-31 NOT READ", C_GREY);
    flip();
}

static void fail_stop(const char *why)
{
    phase = why;
    stuck = 1;
    for (int i = 0; i < 8; i++) stuck_comm[i] = ((volatile uint16_t *)0x20004020)[i];
    while (1) draw();
}

/* MISS: whose pattern is this?  word 0 of a bank is its seed; word 1 is seed ^ 1. */
static int16_t whose(uint16_t w0, uint16_t w1)
{
    if ((uint16_t)(w0 ^ w1) != 1) return -1;
    for (int j = 0; j < 32; j++) if (seed_of(j) == w0) return (int16_t)j;
    return -1;
}

int main(void)
{
    MARS_VDP_DISPMODE = MARS_224_LINES | MARS_VDP_MODE_256;
    palette();
    for (int i = 0; i < 2; i++) { line_table(); clear(); flip(); }
    for (int i = 0; i < 32; i++) found[i] = -1;
    draw();

    if (!md_cmd(0x0600, ED_CTRL0)) fail_stop("68000 NOT ANSWERING");

    {   /* the mapper: bank 0 in window 1 reads as window 0; bank 1 does not */
        const volatile uint16_t *w0 = (const volatile uint16_t *)0x22000000u;
        const volatile uint16_t *w1 = (const volatile uint16_t *)0x22080000u;
        int same0 = 1, same1 = 1;
        if (!md_cmd(0x0601, 0)) fail_stop("68000 NOT ANSWERING");
        for (int i = 0; i < 8192; i++) if (w0[i] != w1[i]) { same0 = 0; break; }
        if (!md_cmd(0x0601, 1)) fail_stop("68000 NOT ANSWERING");
        for (int i = 0; i < 8192; i++) if (w0[i] != w1[i]) { same1 = 0; break; }
        mapper_ok = same0 && !same1;
        mapper_text = mapper_ok ? "SWITCHES BANKS" : same0 ? "NO CHANGE AFTER CTRL1 WRITE" : "DOES NOT RESPOND";
    }

    phase = "READING";
    for (int bank = FIRST_BANK; bank <= LAST_BANK; bank++)
    {
        uint16_t seed = seed_of(bank);
        cur_bank = bank;
        draw();
        if (!md_cmd(0x0601, (uint16_t)bank)) fail_stop("68000 NOT ANSWERING");   /* window 1 = the bank */
        {
            const volatile uint16_t *w1 = (const volatile uint16_t *)0x22080000u;
            word0[bank] = w1[0];
            found[bank] = whose(w1[0], w1[1]);
        }
        if (!md_cmd(0x0801, seed)) fail_stop("68000 NOT ANSWERING");             /* the 68000 reads it, RV = 1 */
        uint32_t bad68 = COMM8_32, off68 = COMM12_32;
        uint16_t got68 = (uint16_t)MARS_SYS_COMM2;

        static uint32_t seq;
        JOB->window = 1; JOB->seed = seed; JOB->seq = ++seq; JOB->go = 1;
        uint32_t badm, firstm, gotm;
        verify_range(1, seed, 0, WORDS / 2, &badm, &firstm, &gotm);
        uint32_t t0 = gTicks;
        while (JOB->go != 2 && gTicks - t0 < 600) { }
        int slave_ok = JOB->go == 2 && JOB->done_seq == seq;
        uint32_t bads = slave_ok ? JOB->bad : 0, firsts = JOB->first, gots = JOB->got;
        if (!slave_ok) slave_dead = 1;
        JOB->go = 0;

        if (bad68)
        {
            status[bank] = ST_MISS;
            note_first(bank, off68, pat(off68 / 2, seed), got68, "68K");
        }
        else if (badm || bads)
        {
            status[bank] = ST_SH2;
            if (badm) note_first(bank, firstm, pat(firstm / 2, seed), (uint16_t)gotm, "MASTER SH2");
            else      note_first(bank, firsts, pat(firsts / 2, seed), (uint16_t)gots, "SLAVE SH2");
        }
        else status[bank] = ST_OK;
        bad68_total += bad68;
        badsh2_total += badm + bads;
    }
    if (!md_cmd(0x0601, 1)) fail_stop("68000 NOT ANSWERING");                     /* window 1 = bank 1 again */
    cur_bank = 0;
    done_all = 1;
    phase = "DONE";
    while (1) draw();
    return 0;
}
