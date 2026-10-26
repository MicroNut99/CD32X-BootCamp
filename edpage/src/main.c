/* edpage - after a page switch, what does the SH2 actually see?
 *
 * 26 Oct 2026.  Jazz, Doom and Rayman all LOAD completely on the Mega EverDrive PRO and then fail to
 * find what they loaded; OpenLara, the one game whose launcher is built WITHOUT M1PAGE, works.  The
 * loader verifies every chunk it writes (the 68000 reads it back), so the writes land - which leaves
 * the question this probe answers: does the SH2's view of the cart agree with the 68000's after the
 * windows have been switched the way hw_md.s's m1_page switches them?
 *
 * No disc, no game data.  For each of the pages the games use (1-6 - page 7 is the EverDrive's own
 * banks 28-31), it sets EverDrive windows 4-7 to banks 4p..4p+3 exactly as m1_page does, has the
 * 68000 poke a marker into cart 0x200000 (W on, inside RV = 1, as the listener's copies do), and
 * reads it back from the SH2 cache-through at 0x22200000:
 *   WROTE  the marker the 68000 put there
 *   AFTER  what the SH2 reads with that page still mapped
 *   LATER  what the SH2 reads when every page has been written and page p is mapped again
 * Two more rows: the resident window (cart 0x080000, bank 1 - where Rayman's file 200 and OpenJazz's
 * packs go) and the save bank (31) through that same window.
 *
 * AFTER wrong            -> the SH2 and the 68000 disagree at once: a window/cache problem.
 * AFTER right, LATER wrong -> the mapping drifts once other pages have been used: m1_page / m1_wb.
 * All right              -> the listener is fine and each game's own page bookkeeping is at fault.
 */

#include <stdint.h>
#include "32x.h"
#include "font8x8.h"

#define COMM8_32   (*(volatile uint32_t *)0x20004028)

/* ---- what crt0.s needs from the program ---- */
volatile uint32_t gTicks = 0;
volatile unsigned mars_pwdt_ovf_count = 0;
volatile unsigned mars_swdt_ovf_count = 0;
void pri_vbi_handler(void) { gTicks++; }
void sec_dma1_handler(void) { }

/* shared between the SH2s: always through the cache-through mirror (each SH2 has its own cache) */
volatile uint32_t hammer_on_v = 0, slave_reads_v = 0, slave_bad_v = 0, slave_alive_v = 0, slave_mode_v = 0;
#define CT(x) (*(volatile uint32_t *)((uint32_t)&(x) | 0x20000000u))
#define hammer_on   CT(hammer_on_v)
#define slave_reads CT(slave_reads_v)
#define slave_bad   CT(slave_bad_v)
#define slave_alive CT(slave_alive_v)
#define slave_mode  CT(slave_mode_v)

static uint16_t want4(uint32_t i) { return (uint16_t)((i & 0xFFFF) ^ (((i >> 16) & 3) << 12) ^ 0x4444); }
static uint16_t want2(uint32_t i) { return (uint16_t)((i & 0xFFFF) ^ (((i >> 16) & 3) << 12) ^ 0x2222); }

/* phase 9: a function the 68000 copies into window 4 (bank 4, offset 0x70000) and the slave runs
   from there - instruction fetches out of the cart while the 68000 holds RV.  It returns a constant;
   anything else = a fetch went wrong (or the slave is gone). */
#define CODE_WIN   (0x02000000u + (4u << 19) + 0x70000u)      /* cached, as code runs */
__attribute__((noinline, section(".text.probe"))) uint32_t probe_fn(uint32_t x)
{
    uint32_t a = x * 0x9E37u + 0x1234u;
    a ^= (a >> 7); a += 0x55AAu; a ^= (a << 3);
    return a;
}

void secondary(void)
{
    const volatile uint16_t *w4 = (const volatile uint16_t *)(0x22000000u + (4u << 19));
    while (1)
    {
        slave_alive = slave_alive + 1;
        if (!hammer_on) continue;
        uint32_t reads = 0, bad = 0;
        if (slave_mode == 1)
        {   /* code out of window 4 */
            uint32_t (*f)(uint32_t) = (uint32_t (*)(uint32_t))CODE_WIN;
            uint32_t want = probe_fn(7);
            for (uint32_t i = 0; i < 4096u && hammer_on; i++)
            {
                *(volatile uint8_t *)0xFFFFFE92u = 0x11;     /* CCR: purge the cache - fetch it again */
                if (f(7) != want) bad++;
                reads++;
            }
        }
        else
        {
            for (uint32_t i = 0; i < 262144u && hammer_on; i += 16)
            {
                if (i >= 0x38000u && i < 0x38080u) continue;   /* probe_fn's copy lives there */
                uint16_t v = w4[i];
                if (v != want4(i)) bad++;
                reads++;
            }
        }
        slave_reads = slave_reads + reads;
        slave_bad = slave_bad + bad;
    }
}

/* ---- the 68000 side's commands (src-md/crt0.s) ---- */
static uint32_t master_bad = 0, master_reads = 0;
static int master_hammer = 0;
static void master_chunk(void)                   /* 256 words of window 2, cache-through */
{
    static uint32_t pos = 0;
    const volatile uint16_t *w2 = (const volatile uint16_t *)(0x22000000u + (2u << 19));
    for (int n = 0; n < 256; n++)
    {
        uint32_t i = pos;
        if (w2[i] != want2(i)) master_bad++;
        master_reads++;
        pos = (pos + 16) & 0x3FFFFu;
    }
}
static int md_cmd(uint16_t cmd, uint16_t arg)
{
    uint32_t t0 = gTicks;
    while (MARS_SYS_COMM0 != 0 && gTicks - t0 < 600) { }
    MARS_SYS_COMM2 = (short)arg;
    MARS_SYS_COMM0 = (short)cmd;
    t0 = gTicks;
    while (MARS_SYS_COMM0 != 0 && gTicks - t0 < 600)
    {
        if (master_hammer) master_chunk();
    }
    return MARS_SYS_COMM0 == 0;
}

/* ---- the screen: 320 x 224, 256 colours, text 8 x 8 ---- */
#define FB          ((volatile uint16_t *)&MARS_FRAMEBUFFER)
enum { C_BLACK, C_WHITE, C_RED, C_GREEN, C_YELLOW, C_GREY, C_BLUE, C_CYAN };

static void palette(void)
{
    static const uint16_t pal[8] = { 0x0000, 0x7FFF, 0x001F, 0x03E0, 0x03FF, 0x4210, 0x7C00, 0x7FE0 };
    volatile uint16_t *cram = (volatile uint16_t *)&MARS_CRAM;
    while ((MARS_VDP_FBCTL & MARS_VDP_PEN) == 0) { }
    for (int i = 0; i < 256; i++)
        cram[i] = (i < 8) ? pal[i] : 0;
}
static void line_table(void)
{
    for (int i = 0; i < 256; i++)
        FB[i] = (uint16_t)(0x100 + (i < 224 ? i : 223) * 160);
}
static void clear(void)
{
    volatile uint16_t *p = FB + 0x100;
    for (int i = 0; i < 160 * 224; i++)
        p[i] = 0;
}
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
static char *dec(char *d, uint32_t v)
{
    char t[12]; int n = 0; char *s = d;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *d++ = t[--n];
    *d = 0;
    return s;
}


/* ---- the probe ---- */
#define W1_BASE    0x080000u        /* EverDrive window 1 = cart 0x080000: the resident slot */
#define PAGE_BASE  0x200000u        /* the games' "window 1" = EverDrive windows 4-7 */
#define SAVE_BANK  31
#define PAGE_LOW   1
#define PAGE_HIGH  6

static const char *phase = "START";

static void fail_stop(const char *why)
{
    clear(); text(8, 100, why, C_RED); flip();
    while (1) { }
}

static char *hex4(char *d, uint32_t v)
{
    static const char H[] = "0123456789ABCDEF";
    for (int i = 3; i >= 0; i--) d[3 - i] = H[(v >> (4 * i)) & 15];
    d[4] = 0;
    return d;
}

/* CTRL<win> = bank: a word write by the 68000 with RV = 1, as m1_page makes them */
static int set_win(int win, int bank) { return md_cmd((uint16_t)(0x0600 | win), (uint16_t)bank); }

/* page p of the games' window 1: EverDrive windows 4-7 = banks 4p..4p+3 (hw_md.s m1_page) */
static int set_page(int p)
{
    for (int k = 0; k < 4; k++)
        if (!set_win(4 + k, 4 * p + k)) return 0;
    return 1;
}

/* one word into the cart by the 68000 (W on inside RV = 1), the way the listener's copies write */
static int poke(uint32_t addr, uint16_t v) { COMM8_32 = addr; return md_cmd(0x1300, v); }

/* and what the SH2 reads there, cache-through, after dropping the cache */
static uint16_t sh2_read(uint32_t addr)
{
    *(volatile uint8_t *)0xFFFFFE92 = 0x11;
    return *(volatile uint16_t *)(0x22000000u + addr);
}

static uint16_t mark(int p) { return (uint16_t)(0x5000 | (p << 8) | p); }

typedef struct { const char *name; uint16_t want, after, later; int done; } Row;
static Row rows[8];
static int nrows = 0;

static void draw(void)
{
    char b[8];
    clear();
    text(8, 4, "EDPAGE  THE SH2 AFTER A PAGE SWITCH", C_YELLOW);
    text(8, 16, "PHASE", C_GREY); text(56, 16, phase, C_WHITE);
    text(8, 34, "WHAT", C_GREY);
    text(96, 34, "WROTE", C_GREY); text(152, 34, "AFTER", C_GREY); text(208, 34, "LATER", C_GREY);
    for (int i = 0; i < nrows; i++)
    {
        Row *r = &rows[i];
        int y = 46 + i * 11;
        int bad = r->done && (r->after != r->want || (r->done > 1 && r->later != r->want));
        text(8, y, r->name, bad ? C_RED : r->done ? C_GREEN : C_GREY);
        if (!r->done) continue;
        text(96, y, hex4(b, r->want), C_WHITE);
        text(152, y, hex4(b, r->after), r->after == r->want ? C_WHITE : C_RED);
        if (r->done > 1) text(208, y, hex4(b, r->later), r->later == r->want ? C_WHITE : C_RED);
    }
    text(8, 150, "AFTER WRONG = SH2 AND 68000 DISAGREE", C_GREY);
    text(8, 160, "  AT ONCE (WINDOW OR CACHE).", C_GREY);
    text(8, 172, "LATER WRONG ONLY = THE MAPPING DRIFTS", C_GREY);
    text(8, 182, "  ONCE OTHER PAGES ARE USED (M1_WB).", C_GREY);
    text(8, 194, "ALL RIGHT = THE LISTENER IS GOOD AND", C_GREY);
    text(8, 204, "  THE GAMES' OWN PAGING IS AT FAULT.", C_GREY);
    flip();
}

int main(void)
{
    MARS_VDP_DISPMODE = MARS_224_LINES | MARS_VDP_MODE_256;
    palette();
    for (int i = 0; i < 2; i++) { line_table(); clear(); flip(); }

    rows[0].name = "RESIDENT 080000 B1 "; rows[0].want = 0xA501;
    for (int p = PAGE_LOW; p <= PAGE_HIGH; p++)
    {
        rows[1 + p - PAGE_LOW].name = p == 1 ? "PAGE 1  200000     " :
                                      p == 2 ? "PAGE 2  200000     " :
                                      p == 3 ? "PAGE 3  200000     " :
                                      p == 4 ? "PAGE 4  200000     " :
                                      p == 5 ? "PAGE 5  200000     " : "PAGE 6  200000     ";
        rows[1 + p - PAGE_LOW].want = mark(p);
    }
    rows[7].name = "SAVE BANK 31 VIA W1"; rows[7].want = 0xB31B;
    nrows = 8;
    draw();

    /* CTRL0 = P only, W off - what an EDCD launcher sets (M1CTRL0=0x8000); the pokes turn W on
       inside RV = 1 themselves, as the listener does */
    phase = "CTRL0 = 8000"; draw();
    if (!md_cmd(0x0600, 0x8000)) fail_stop("68000 NOT ANSWERING");

    /* 1. the resident window: bank 1 in EverDrive window 1 (cart 0x080000) */
    phase = "RESIDENT WINDOW"; draw();
    if (!set_win(1, 1)) fail_stop("68000 NOT ANSWERING (CTRL1)");
    if (!poke(W1_BASE, rows[0].want)) fail_stop("68000 NOT ANSWERING (POKE W1)");
    rows[0].after = sh2_read(W1_BASE);
    rows[0].done = 1; draw();

    /* 2. every page: switch, poke, read back at once */
    for (int p = PAGE_LOW; p <= PAGE_HIGH; p++)
    {
        Row *r = &rows[1 + p - PAGE_LOW];
        phase = "WRITING THE PAGES"; draw();
        if (!set_page(p)) fail_stop("68000 NOT ANSWERING (PAGE)");
        if (!poke(PAGE_BASE, r->want)) fail_stop("68000 NOT ANSWERING (POKE PAGE)");
        r->after = sh2_read(PAGE_BASE);
        r->done = 1; draw();
    }

    /* 3. and again, now that every page has been used - the drift test */
    for (int p = PAGE_LOW; p <= PAGE_HIGH; p++)
    {
        Row *r = &rows[1 + p - PAGE_LOW];
        phase = "READING THEM AGAIN"; draw();
        if (!set_page(p)) fail_stop("68000 NOT ANSWERING (PAGE AGAIN)");
        r->later = sh2_read(PAGE_BASE);
        r->done = 2; draw();
    }

    /* 4. the save bank through the same window the listener uses for it */
    phase = "SAVE BANK 31"; draw();
    if (!set_win(1, SAVE_BANK)) fail_stop("68000 NOT ANSWERING (CTRL1 B31)");
    if (!poke(W1_BASE, rows[7].want)) fail_stop("68000 NOT ANSWERING (POKE B31)");
    if (!set_win(1, 1)) fail_stop("68000 NOT ANSWERING (CTRL1 BACK)");
    (void)sh2_read(W1_BASE);                       /* bank 1 again: the marker must NOT be here */
    if (!set_win(1, SAVE_BANK)) fail_stop("68000 NOT ANSWERING (CTRL1 B31 AGAIN)");
    rows[7].after = sh2_read(W1_BASE);
    rows[7].later = rows[7].after;
    rows[7].done = 2;
    if (!set_win(1, 1)) fail_stop("68000 NOT ANSWERING (CTRL1 HOME)");

    /* the resident window once more: it must still read what it did at the start */
    rows[0].later = sh2_read(W1_BASE);
    rows[0].done = 2;

    phase = "DONE"; draw();
    while (1) { }
}
