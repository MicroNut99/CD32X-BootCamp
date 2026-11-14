/* cfxtest - the cartfx engine on the console (CD32X BootCamp cartfx/test, 2026)
 *
 * A 32X ROM started from the Mega EverDrive PRO's menu (32X attached, no CD needed), with one of the cartfx cores
 * beside it on the SD card as mega-pro.rbf (Hurrican's, Jazz's or SuperTux's: the test tries all three keys). It:
 *  1. turns the engine on and shows which core answered (its ID), or that none did (no .rbf, or not a cartfx one);
 *  2. plays the script tools/cfxgen.c made: 8 frames composed by the engine, each checked word for word against
 *     what the reference model (cartfx/model/cartfx.c) composed from the same script and the same pictures - the
 *     picture, the cell map, the error bit, the cell and sprite counts - each shown for a second as it is checked;
 *  3. M0 of the design: copies the shown picture (320 x 192 of it) from the window into the frame buffer three ways,
 *     timed by the master's FRT: the number every game's frame budget rests on;
 *  4. then plays the script again and again with the slave SH2 reading the cart without a pause (the window and the
 *     ROM), as a game's copy and code would: frames wrong, accesses the engine had to do again.
 * One screen of results: photograph it (and once more after a minute of the stress loop).
 */
#include <stdint.h>
#include "32x.h"
#include "font8x8.h"

/* ---- what crt0.s needs from the program ---- */
volatile uint32_t gTicks = 0;
volatile unsigned mars_pwdt_ovf_count = 0;
volatile unsigned mars_swdt_ovf_count = 0;
void pri_vbi_handler(void) { gTicks++; }
void sec_dma1_handler(void) { }

#define THRU(p)     ((volatile uint32_t *)((uint32_t)(p) | 0x20000000u))
#define CART        0x22000000u                                  /* the cart, cache-through */
#define CMD(w)      (*(volatile uint16_t *)(CART + 0x3F0000u + 2u * (uint32_t)(w)))
#define PIC         ((volatile uint16_t *)(CART + 0x3E0000u))
#define SCRIPT      ((const volatile uint16_t *)(CART + 0x040000u))
#define EXP(k)      ((const volatile uint16_t *)(CART + 0x100000u + 0x10000u * (uint32_t)(k)))
enum { ST_BUSY = 1, ST_FULL = 2, ST_SHOWN = 4, ST_ERR = 8 };

/* ---- the FRT (the probe's: the boot ROM's set-up taken over while measuring) ---- */
#define FRT_TCR     (*(volatile uint8_t *)0xFFFFFE16)
#define FRT_FRCH    (*(volatile uint8_t *)0xFFFFFE12)
#define FRT_FRCL    (*(volatile uint8_t *)0xFFFFFE13)
#define FRT_TIER    (*(volatile uint8_t *)0xFFFFFE10)
#define FRT_FTCSR   (*(volatile uint8_t *)0xFFFFFE11)
#define FRT_OCRH    (*(volatile uint8_t *)0xFFFFFE14)
#define FRT_OCRL    (*(volatile uint8_t *)0xFFFFFE15)
#define FRT_TOCR    (*(volatile uint8_t *)0xFFFFFE17)
/* the 32X boot ROM leaves the FRT cleared at every compare match A with OCRA = 1 (it never counts past 1): taken for
   each measurement (free running, no interrupts) and given back after it, as the probe does */
static uint8_t frt_tocr0, frt_ftcsr0, frt_ocrah, frt_ocral;
static void frt_take(void)
{
    frt_tocr0 = FRT_TOCR; frt_ftcsr0 = FRT_FTCSR;
    FRT_TOCR = (uint8_t)(frt_tocr0 & ~0x10);
    frt_ocrah = FRT_OCRH; frt_ocral = FRT_OCRL;
    FRT_TIER = 0x01;
    FRT_FTCSR = 0x00;
    FRT_OCRH = 0xFF; FRT_OCRL = 0xFF;
}
static void frt_give(void)
{
    FRT_TOCR = (uint8_t)(frt_tocr0 & ~0x10);
    FRT_OCRH = frt_ocrah; FRT_OCRL = frt_ocral;
    FRT_TOCR = frt_tocr0;
    FRT_FTCSR = (uint8_t)(frt_ftcsr0 & 0x01);
}
static inline void frt_start(int cks) { FRT_TCR = (uint8_t)cks; FRT_FRCH = 0; FRT_FRCL = 0; }
static inline uint32_t frt_read(void)                          /* (the high byte again: a carry between the two
                                                                    reads is not a jump - ares does not latch FRCL) */
{
    uint32_t h = FRT_FRCH, l = FRT_FRCL, h2 = FRT_FRCH;
    if (h2 != h) { l = FRT_FRCL; h = h2; }
    return h << 8 | l;
}
/* a longer time at clock / 128 (5.563 us a tick): read often enough (under 364 ms apart) and it never wraps */
static uint32_t long_acc, long_last;
static void long_start(void) { frt_start(2); long_acc = 0; long_last = 0; }
static uint32_t long_now(void)
{
    uint32_t t = frt_read(), d = (t - long_last) & 0xFFFF;
    if (d < 0x8000) { long_acc += d; long_last = t; }            /* (a step back is a misread, never 180 ms) */
    return long_acc;
}
static uint32_t ticks128_us(uint32_t t) { return (uint32_t)(((uint64_t)t * 128u * 1000u) / 23011u); }   /* 23.011 MHz */

static void pause(void) { for (volatile int i = 0; i < 60; i++) { } }   /* between status reads (docs/06 rule 7) */

/* ---- the screen: 320 x 224, 256 colours, text 8 x 8 (edw0's) ---- */
#define FB          ((volatile uint16_t *)&MARS_FRAMEBUFFER)
enum { C_BLACK, C_WHITE, C_RED, C_GREEN, C_YELLOW, C_GREY, C_BLUE, C_CYAN };
static void palette(void)
{
    static const uint16_t pal[8] = { 0x0000, 0x7FFF, 0x001F, 0x03E0, 0x03FF, 0x4210, 0x7C00, 0x7FE0 };
    volatile uint16_t *cram = (volatile uint16_t *)&MARS_CRAM;
    while ((MARS_VDP_FBCTL & MARS_VDP_PEN) == 0) { }
    for (int i = 0; i < 256; i++)                                /* the pictures' colours: a wheel, 8 shades */
    {
        int h = i >> 3, v = 8 + (i & 7) * 3;
        int r = h < 11 ? v * (11 - h) / 11 : h < 22 ? 0 : v * (h - 21) / 10;
        int g = h < 11 ? v * h / 11 : h < 22 ? v * (21 - h) / 11 : 0;
        int b = h < 11 ? 0 : h < 22 ? v * (h - 10) / 11 : v * (31 - h) / 10;
        cram[i] = (uint16_t)(i < 8 ? pal[i] : (b << 10 | g << 5 | r));
    }
}
static void line_table(void) { for (int i = 0; i < 256; i++) FB[i] = (uint16_t)(0x100 + (i < 224 ? i : 223) * 160); }
static void clear(void) { volatile uint16_t *p = FB + 0x100; for (int i = 0; i < 160 * 224; i++) p[i] = 0; }
static void text(int x, int y, const char *s, uint8_t c)       /* x a multiple of 8 */
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
static char *ms1(char *d, uint32_t us)                           /* microseconds as "12.3" */
{
    char *e = dec(d, us / 1000);
    while (*e) e++;
    *e++ = '.'; *e++ = (char)('0' + (us / 100) % 10); *e = 0;
    return d;
}
static char *cat(char *d, const char *s) { char *p = d; while (*p) p++; while ((*p++ = *s++)) { } return d; }

/* ---- the slave: reads the cart without a pause while slave_mode is 1 (the window, then the ROM's pictures) ---- */
volatile uint32_t slave_mode = 0, slave_reads = 0;
void secondary(void)
{
    volatile uint32_t *mode = THRU(&slave_mode), *n = THRU(&slave_reads);
    uint32_t i = 0, v = 0;
    while (1)
    {
        if (*mode != 1) continue;
        for (int k = 0; k < 64; k++, i++)
        {
            v += *(volatile uint16_t *)(CART + 0x3E0000u + ((i * 2u) & 0xFFFEu));
            v += *(volatile uint16_t *)(CART + 0x080000u + ((i * 34u) & 0x7FFFEu));
        }
        *n += 128 + (v & 0);
    }
}

/* ---- the results ---- */
typedef struct { uint16_t cols, rows; uint32_t diff, first; uint8_t err_ok, cnt_ok, done; uint32_t eng_us, sh2_us; } Check;
static Check ck[16];
static int nck;
static int core = -1;                                            /* 0 Hurrican, 1 Jazz, 2 SuperTux */
static uint16_t core_status, version;
static uint32_t copy_us[3], copy_bytes;
static uint32_t passes, stress_checks, stress_wrong, stress_reads, worst_us[16];
static uint16_t predo, sredo, lost;
static const char *stage = "START";
static const char *const core_name[3] = { "HURRICAN", "JAZZ", "SUPERTUX" };
static const uint16_t key1[3] = { 0x5848, 0x584A, 0x5853 };
static const uint8_t ids[3] = { 0xC1, 0xC2, 0xC3 };

static void results(void)
{
    char b[48], t[16];
    clear();
    text(8, 0, "CARTFX TEST 1  (CD32X BOOTCAMP)", C_YELLOW);
    if (core < 0)
    {
        b[0] = 0; cat(b, "NO CARTFX CORE ANSWERED (STATUS "); cat(b, hexn(t, core_status, 4)); cat(b, ")");
        text(8, 12, b, C_RED);
        text(8, 24, "PUT A CARTFX MEGA-PRO.RBF BESIDE IT", C_GREY);
        flip(); return;
    }
    b[0] = 0; cat(b, "CORE "); cat(b, core_name[core]); cat(b, "  ID "); cat(b, hexn(t, ids[core], 2));
    cat(b, "  VERSION "); cat(b, hexn(t, version, 4));
    text(8, 12, b, C_WHITE);
    text(8, 24, "CHK SIZE  RESULT       ENGINE  SH2 MS", C_GREY);
    for (int k = 0; k < nck; k++)
    {
        Check *c = &ck[k];
        int y = 34 + k * 10, ok = c->done && !c->diff && c->err_ok && c->cnt_ok;
        b[0] = 0; cat(b, dec(t, k)); cat(b, "   "); cat(b, dec(t, c->cols)); cat(b, "X"); cat(b, dec(t, c->rows));
        text(8, y, b, C_WHITE);
        if (!c->done) { text(80, y, "-", C_GREY); continue; }
        if (ok) text(80, y, "RIGHT", C_GREEN);
        else
        {
            b[0] = 0;
            if (c->diff) { cat(b, dec(t, c->diff)); cat(b, " @"); cat(b, hexn(t, c->first, 4)); }
            if (!c->err_ok) cat(b, " E");
            if (!c->cnt_ok) cat(b, " N");
            text(80, y, b, C_RED);
        }
        text(192, y, ms1(b, c->eng_us), C_CYAN);
        text(256, y, ms1(b, c->sh2_us), C_CYAN);
    }
    int y = 34 + nck * 10 + 6;
    text(8, y, "COPY 320 LINES X 192 FROM THE WINDOW:", C_GREY);
    static const char *const how[3] = { "WORD READS", "LONG READS", "CACHED    " };
    for (int m = 0; m < 3; m++)
    {
        if (!copy_us[m]) continue;
        uint32_t cpb10 = (uint32_t)(((uint64_t)copy_us[m] * 23011u) / 100u / copy_bytes);   /* clocks a byte x 10 */
        b[0] = 0; cat(b, how[m]); cat(b, " "); cat(b, ms1(t, copy_us[m])); cat(b, " MS  ");
        cat(b, dec(t, cpb10 / 10)); cat(b, "."); cat(b, dec(t, cpb10 % 10)); cat(b, " CLOCKS A BYTE");
        text(16, y + 10 + m * 10, b, C_WHITE);
    }
    y += 44;
    b[0] = 0; cat(b, "STRESS "); cat(b, stage); cat(b, "  PASSES "); cat(b, dec(t, passes));
    text(8, y, b, C_YELLOW);
    b[0] = 0; cat(b, "CHECKS "); cat(b, dec(t, stress_checks)); cat(b, " WRONG "); cat(b, dec(t, stress_wrong));
    cat(b, " SLAVE READ "); cat(b, dec(t, stress_reads >> 20)); cat(b, "M");
    text(8, y + 10, b, stress_wrong ? C_RED : C_GREEN);
    b[0] = 0; cat(b, "REDONE PSRAM "); cat(b, dec(t, predo)); cat(b, " SRAM "); cat(b, dec(t, sredo));
    cat(b, " LOST "); cat(b, dec(t, lost));
    text(8, y + 20, b, lost ? C_RED : C_WHITE);
    b[0] = 0; cat(b, "SLOWEST ENGINE, BUSY: ");
    uint32_t w = 0; for (int k = 0; k < nck; k++) if (worst_us[k] > w) w = worst_us[k];
    cat(b, ms1(t, w)); cat(b, " MS");
    text(8, y + 30, b, C_WHITE);
    flip();
}

/* the shown picture into the back frame buffer, 320 x up to 192 from line 16 (the copy measurements' copy) */
static uint32_t copy_pic(int how, int pw, int ph)
{
    if (ph > 192) ph = 192;
    volatile uint16_t *fb = FB + 0x100 + 16 * 160;
    frt_take();
    frt_start(1);                                                /* clock / 32: 1.39 us a tick */
    if (how == 0)
        for (int y = 0; y < ph; y++)
        {
            const volatile uint16_t *s = PIC + y * (pw >> 1);
            volatile uint16_t *d = fb + y * 160;
            for (int x = 0; x < 160; x++) d[x] = s[x];
        }
    else if (how == 1)
        for (int y = 0; y < ph; y++)
        {
            const volatile uint32_t *s = (const volatile uint32_t *)(PIC + y * (pw >> 1));
            volatile uint16_t *d = fb + y * 160;
            for (int x = 0; x < 80; x++) { uint32_t v = s[x]; d[2 * x] = (uint16_t)(v >> 16); d[2 * x + 1] = (uint16_t)v; }
        }
    else
    {
        *(volatile uint8_t *)0xFFFFFE92 = 0x11;                  /* purge the cache, keep it on (edw0's) */
        for (int y = 0; y < ph; y++)
        {
            const uint32_t *s = (const uint32_t *)(0x023E0000u + (uint32_t)y * (uint32_t)pw);
            volatile uint16_t *d = fb + y * 160;
            for (int x = 0; x < 80; x++) { uint32_t v = s[x]; d[2 * x] = (uint16_t)(v >> 16); d[2 * x + 1] = (uint16_t)v; }
        }
        *(volatile uint8_t *)0xFFFFFE92 = 0x11;
    }
    uint32_t t = frt_read();
    frt_give();
    copy_bytes = 320u * (uint32_t)ph;
    return (uint32_t)(((uint64_t)t * 32u * 1000u) / 23011u);
}

static int check(int k, int show)
{
    const volatile uint16_t *e = EXP(k);
    Check *c = &ck[k];
    c->cols = e[1]; c->rows = e[2];
    uint32_t n = 64u * c->cols * c->rows, diff = 0, first = 0;  /* words: 16 x 8 pixels a cell / 2 */
    for (uint32_t i = 0; i < n; i++) if (PIC[i] != e[16 + i]) { if (!diff) first = i; diff++; }
    for (uint32_t i = 0; i < 48; i++) if (PIC[0x7E00 + i] != e[16 + n + i]) { if (!diff) first = 0x7E00 + i; diff++; }
    uint16_t st = CMD(0);
    c->err_ok = ((st & ST_ERR) != 0) == (e[4] != 0);
    c->cnt_ok = CMD(2) == e[5] && CMD(3) == e[6];
    c->eng_us = (uint32_t)CMD(4) * 1280u / 1000u;
    c->diff = diff; c->first = first; c->done = 1;
    if (k >= nck) nck = k + 1;
    if (show)                                                    /* the picture for a second, its line under it */
    {
        char b[48], t[16];
        clear();
        copy_pic(0, 16 * c->cols, 8 * c->rows);
        b[0] = 0; cat(b, "CHECK "); cat(b, dec(t, k)); cat(b, diff || !c->err_ok || !c->cnt_ok ? "  WRONG" : "  RIGHT");
        text(8, 0, b, diff ? C_RED : C_GREEN);
        flip();
        uint32_t t0 = gTicks; while (gTicks - t0 < 60) { }
    }
    return !diff && c->err_ok && c->cnt_ok;
}

/* the script: returns 0 when it stopped (a wait that never ended) */
static int play(int show)
{
    int nextk = 0;
    uint32_t go_us = 0;
    for (uint32_t a = 0; ; a++)
    {
        uint16_t w = SCRIPT[a];
        if (w == 0xFFFF) return 1;
        if (w < 0x8000)
        {
            uint16_t st = CMD(w);
            uint32_t t0 = gTicks;
            while (st & ST_FULL) { pause(); st = CMD(0); if (gTicks - t0 > 300) { stage = "QUEUE FULL"; return 0; } }
        }
        else if (w == 0x8001)                                    /* the GO just sent: until not busy */
        {
            frt_take();
            long_start();
            uint32_t t0 = gTicks;
            while (CMD(0) & ST_BUSY) { pause(); long_now(); if (gTicks - t0 > 300) { frt_give(); stage = "NEVER IDLE"; return 0; } }
            go_us = ticks128_us(long_now());
            frt_give();
        }
        else if ((w & 0xFF00) == 0x8100)
        {
            uint32_t t0 = gTicks;
            while (((CMD(0) & ST_SHOWN) != 0) != (w & 1)) { pause(); if (gTicks - t0 > 300) { stage = "NEVER SHOWN"; return 0; } }
        }
        else if ((w & 0xFF00) == 0x8200)
        {
            int k = w & 0xFF;
            ck[k].sh2_us = go_us;
            if (go_us > worst_us[k]) worst_us[k] = go_us;
            int ok = check(k, show);
            if (!show) { stress_checks++; if (!ok) stress_wrong++; }
            nextk = k + 1;
        }
    }
    (void)nextk;
}

int main(void)
{
    MARS_VDP_DISPMODE = MARS_224_LINES | MARS_VDP_MODE_256;
    palette();
    for (int i = 0; i < 2; i++) { line_table(); clear(); flip(); }

    /* 1. the key: Hurrican's, Jazz's, SuperTux's in turn (with the engine off these are reads of the cart) */
    for (int k = 0; k < 3 && core < 0; k++)
    {
        (void)CMD(0x4346); (void)CMD(key1[k]);
        uint16_t st = CMD(0x0C01);
        core_status = st;
        if ((st >> 8) == ids[k] && CMD(8) == 0x0C01) { core = k; version = CMD(8); }
    }
    if (core < 0) { results(); while (1) { } }

    /* 2. the script once, each check shown */
    stage = "-";
    if (!play(1)) { results(); while (1) { } }

    /* 3. the copy, three ways, from the picture shown last */
    {
        int k = nck - 1;
        for (int m = 0; m < 3; m++) { clear(); copy_us[m] = copy_pic(m, 16 * ck[k].cols, 8 * ck[k].rows); flip(); }
    }
    results();

    /* 4. the stress loop: the slave on the cart all the time */
    *THRU(&slave_mode) = 1;
    stage = "RUNNING";
    while (1)
    {
        if (!play(0)) { *THRU(&slave_mode) = 0; results(); while (1) { } }
        passes++;
        stress_reads = *THRU(&slave_reads);
        predo = CMD(5); sredo = CMD(6); lost = CMD(7);
        results();
    }
}
