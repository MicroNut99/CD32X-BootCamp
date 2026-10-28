/* probe - the CD32X timing probe (CD32X BootCamp)
 *
 * What each kind of 32X access costs on this console, measured by the master SH2's own free-running
 * timer (FRT), one kind at a time, so that an emulator (the CD32X build of ares) can be set to match.
 * The results are one screen of numbers: photograph it.  A runs the whole probe again.
 *
 * Every number is SH2 clocks (23.01 MHz) per access, with one decimal, unless its name says otherwise:
 *   - each test runs a loop of 16 identical instructions 128 times (2048 accesses) with interrupts off;
 *     the loop's own cost (DT + BF) is measured with NOPs and taken off;
 *   - each test runs three times and the middle result is shown;
 *   - the addresses are the cache-through ones (0x2xxxxxxx) unless the test is about the cache.
 * The slave SH2 is idle (a loop in its cache) except in the BUS tests, where it hammers one memory
 * while the master measures another: what the two SH2s cost each other on the shared bus.
 *
 * The probe is a normal BootCamp game: GAME=probe bash build.sh (EDCD=1 for the Mega EverDrive PRO).
 */
#include <stdint.h>
#include "32x.h"
#include "cdapi.h"
#include "font8x8.h"

/* ---- what crt0.s needs from the program ---- */
volatile uint32_t gTicks = 0;
volatile unsigned mars_pwdt_ovf_count = 0;
volatile unsigned mars_swdt_ovf_count = 0;
void pri_vbi_handler(void) { gTicks++; }
void sec_dma1_handler(void) { }

#define THRU(p)     ((volatile uint32_t *)((uint32_t)(p) | 0x20000000u))

/* ---- the FRT (SH7604): FRC counts at clock / 8 (CKS 0) or clock / 128 (CKS 2) ---- */
#define FRT_TCR     (*(volatile uint8_t *)0xFFFFFE16)
#define FRT_FRCH    (*(volatile uint8_t *)0xFFFFFE12)
#define FRT_FRCL    (*(volatile uint8_t *)0xFFFFFE13)
#define FRT_TIER    (*(volatile uint8_t *)0xFFFFFE10)
#define FRT_FTCSR   (*(volatile uint8_t *)0xFFFFFE11)
#define FRT_OCRH    (*(volatile uint8_t *)0xFFFFFE14)
#define FRT_OCRL    (*(volatile uint8_t *)0xFFFFFE15)
#define FRT_TOCR    (*(volatile uint8_t *)0xFFFFFE17)

static inline void frt_start(int cks) { FRT_TCR = (uint8_t)cks; FRT_FRCH = 0; FRT_FRCL = 0; }
/* the 32X boot ROM leaves the FRT cleared at every compare match A with OCRA = 1 (FTCSR 0x09, TOCR 0xE2):
   it never counts past 1.  The probe lets it run free while it measures (CCLRA 0, OCRA 0xFFFF, no
   interrupts) and puts the boot ROM's set-up back afterwards. */
static uint8_t frt_tocr0, frt_ftcsr0, frt_ocrah, frt_ocral;
static void frt_take(void)
{
    frt_tocr0 = FRT_TOCR; frt_ftcsr0 = FRT_FTCSR;
    FRT_TOCR = (uint8_t)(frt_tocr0 & ~0x10);                /* OCRS 0: OCRA */
    frt_ocrah = FRT_OCRH; frt_ocral = FRT_OCRL;
    FRT_TIER = 0x01;                                         /* no FRT interrupts */
    FRT_FTCSR = 0x00;                                        /* CCLRA 0: count on */
    FRT_OCRH = 0xFF; FRT_OCRL = 0xFF;
}
static void frt_give(void)
{
    FRT_TOCR = (uint8_t)(frt_tocr0 & ~0x10);
    FRT_OCRH = frt_ocrah; FRT_OCRL = frt_ocral;
    FRT_TOCR = frt_tocr0;
    FRT_FTCSR = (uint8_t)(frt_ftcsr0 & 0x01);               /* CCLRA as it was (the flags clear) */
}
static inline uint32_t frt_read(void) { uint32_t h = FRT_FRCH; uint32_t l = FRT_FRCL; return h << 8 | l; }

static inline uint32_t ints_off(void)
{
    uint32_t sr;
    __asm__ __volatile__("stc sr,%0" : "=r"(sr));
    __asm__ __volatile__("ldc %0,sr" : : "r"(sr | 0xF0) : "memory");
    return sr;
}
static inline void ints_restore(uint32_t sr) { __asm__ __volatile__("ldc %0,sr" : : "r"(sr) : "memory"); }

/* ---- the test loops: 16 identical instructions, DT, BF, run n times; FRT ticks (clock / 8) ---- */
#define R4(x)   x x x x
#define R16(x)  R4(R4(x))
#define N_LOOP  128                         /* 2048 accesses */

#define LOOP_RD(name, body)                                                         \
static uint32_t name(uint32_t addr)                                                 \
{                                                                                   \
    int i = N_LOOP; uint32_t v = 0, a = addr;                                       \
    uint32_t sr = ints_off();                                                       \
    frt_start(0);                                                                   \
    __asm__ __volatile__("1:\n\t" R16(body) "dt %0\n\tbf 1b"                        \
                         : "+r"(i), "+z"(v), "+r"(a) : : "memory", "t");            \
    uint32_t t = frt_read();                                                        \
    ints_restore(sr);                                                               \
    return t;                                                                       \
}
/* %1 = r0 (the value), %2 = the address */
LOOP_RD(t_nop16,  "nop\n\t")
LOOP_RD(t_rd_w,   "mov.w @%2,%1\n\t")
LOOP_RD(t_rd_l,   "mov.l @%2,%1\n\t")
LOOP_RD(t_rd_b,   "mov.b @%2,%1\n\t")
LOOP_RD(t_wr_w,   "mov.w %1,@%2\n\t")
LOOP_RD(t_wr_l,   "mov.l %1,@%2\n\t")
LOOP_RD(t_wr_w_sp, "mov.w %1,@%2\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t")  /* 6 NOPs between (more: BF cannot reach) */
LOOP_RD(t_fill_l, "mov.l @%2,%1\n\tadd #16,%2\n\t")                 /* a new cache line each read */
LOOP_RD(t_use,    "mov.l @%2,%1\n\tadd %1,%1\n\t")                  /* the loaded value used at once */
LOOP_RD(t_add,    "add #1,%1\n\t")
LOOP_RD(t_bra,    "bra 0f\n\tnop\n0:\n\t")

static uint32_t t_nop32(uint32_t addr)
{
    int i = N_LOOP; uint32_t v = 0, a = addr;
    uint32_t sr = ints_off();
    frt_start(0);
    __asm__ __volatile__("1:\n\t" R16("nop\n\t") R16("nop\n\t") "dt %0\n\tbf 1b"
                         : "+r"(i), "+z"(v), "+r"(a) : : "memory", "t");
    uint32_t t = frt_read();
    ints_restore(sr);
    return t;
}

static uint32_t t_mul(uint32_t addr)
{
    int i = N_LOOP; uint32_t x = 1234, y = addr | 3;
    uint32_t sr = ints_off();
    frt_start(0);
    __asm__ __volatile__("1:\n\t" R16("mul.l %1,%2\n\t") "dt %0\n\tbf 1b"
                         : "+r"(i) : "r"(x), "r"(y) : "memory", "t", "macl");
    uint32_t t = frt_read();
    ints_restore(sr);
    return t;
}

/* ---- the slave SH2: what it does to the bus while the master measures ---- */
enum { S_IDLE, S_CART, S_SDRAM, S_FB, S_SDRAM_W, S_N };
static const char *const slave_name[S_N] = { "IDLE", "CART", "SDRM", "FB W", "SD W" };
volatile uint32_t slave_mode = S_IDLE, slave_ack = 0xFF;

void secondary(void)
{
    volatile uint32_t *mode = THRU(&slave_mode), *ack = THRU(&slave_ack);
    for (;;)
    {
        uint32_t m = *mode;
        if (*ack != m)
            *ack = m;
        int i = 64;
        uint32_t v = 0x11111111, a;
        switch (m)
        {
        case S_CART:   a = 0x22100000u;
            __asm__ __volatile__("1:\n\t" R16("mov.w @%2,%1\n\t") "dt %0\n\tbf 1b" : "+r"(i), "+z"(v), "+r"(a) : : "memory", "t"); break;
        case S_SDRAM:  a = 0x26030000u;
            __asm__ __volatile__("1:\n\t" R16("mov.w @%2,%1\n\t") "dt %0\n\tbf 1b" : "+r"(i), "+z"(v), "+r"(a) : : "memory", "t"); break;
        case S_FB:     a = 0x24010000u;
            __asm__ __volatile__("1:\n\t" R16("mov.w %1,@%2\n\t") "dt %0\n\tbf 1b" : "+r"(i), "+z"(v), "+r"(a) : : "memory", "t"); break;
        case S_SDRAM_W: a = 0x26030000u;
            __asm__ __volatile__("1:\n\t" R16("mov.w %1,@%2\n\t") "dt %0\n\tbf 1b" : "+r"(i), "+z"(v), "+r"(a) : : "memory", "t"); break;
        default:       i = 2000;            /* in the cache: about 4000 clocks between two looks */
            __asm__ __volatile__("1:\n\tdt %0\n\tbf 1b" : "+r"(i) : : "t"); break;
        }
    }
}

static void slave_set(uint32_t m)
{
    *THRU(&slave_mode) = m;
    for (uint32_t n = 0; n < 2000000u && *THRU(&slave_ack) != m; n++) { }
}

/* ---- the screen: 320 x 224, one byte a pixel ---- */
#define FB          ((volatile uint16_t *)&MARS_FRAMEBUFFER)
enum { C_BLACK, C_WHITE, C_RED, C_GREEN, C_YELLOW, C_GREY, C_BLUE, C_CYAN };

static void palette(void)
{
    static const uint16_t pal[8] = { 0x0000, 0x7FFF, 0x001F, 0x03E0, 0x03FF, 0x4210, 0x7C00, 0x7FE0 };
    volatile uint16_t *cram = (volatile uint16_t *)&MARS_CRAM;
    while ((MARS_VDP_FBCTL & MARS_VDP_PEN) == 0) { }
    for (int i = 0; i < 256; i++) cram[i] = (i < 8) ? pal[i] : 0;
}
static void line_table(void) { for (int i = 0; i < 256; i++) FB[i] = (uint16_t)(0x100 + (i < 224 ? i : 223) * 160); }
static void clear(void) { volatile uint16_t *p = FB + 0x100; for (int i = 0; i < 160 * 224; i++) p[i] = 0; }
static void plot2(int x, int y, uint8_t a, uint8_t b) { FB[0x100 + y * 160 + (x >> 1)] = (uint16_t)((a << 8) | b); }
static void text(int x, int y, const char *s, uint8_t c)
{
    for (; *s; s++, x += 8)
    {
        int ch = (uint8_t)*s;
        if (ch < 32 || ch > 127) ch = '?';
        const unsigned char *g = font8x8[ch - 32];
        for (int r = 0; r < 8; r++)
            for (int b = 0; b < 8; b += 2)
                plot2(x + b, y + r, (g[r] & (0x80 >> b)) ? c : C_BLACK, (g[r] & (0x40 >> b)) ? c : C_BLACK);
    }
}
static void flip(void)
{
    uint16_t cur = MARS_VDP_FBCTL & MARS_VDP_FS;
    MARS_VDP_FBCTL = (uint16_t)(cur ^ 1);
    while ((MARS_VDP_FBCTL & MARS_VDP_FS) == cur) { }
}
static void wait_vblank_start(void)
{
    while (MARS_VDP_FBCTL & MARS_VDP_VBLK) { }
    while (!(MARS_VDP_FBCTL & MARS_VDP_VBLK)) { }
}
static void wait_display(int lines)          /* the display has started, plus some lines */
{
    while (!(MARS_VDP_FBCTL & MARS_VDP_VBLK)) { }
    while (MARS_VDP_FBCTL & MARS_VDP_VBLK) { }
    for (int n = 0; n < lines; n++)
    {
        while (!(MARS_VDP_FBCTL & MARS_VDP_HBLK)) { }
        while (MARS_VDP_FBCTL & MARS_VDP_HBLK) { }
    }
}

/* ---- the results ---- */
#define MAXR 52
static struct { const char *name; int32_t v; int whole; } res[MAXR];
static int nres;
static void put(const char *name, int32_t v10, int whole) { if (nres < MAXR) { res[nres].name = name; res[nres].v = v10; res[nres].whole = whole; nres++; } }

static uint32_t med3(uint32_t a, uint32_t b, uint32_t c)
{
    if (a > b) { uint32_t t = a; a = b; b = t; }
    if (b > c) { uint32_t t = b; b = c; c = t; }
    if (a > b) { uint32_t t = a; a = b; b = t; }
    return b;
}
typedef uint32_t (*loop_fn)(uint32_t);
static uint32_t run3(loop_fn f, uint32_t a) { uint32_t x = f(a), y = f(a), z = f(a); return med3(x, y, z); }

/* the loop's own clocks per pass (x 16), from the NOP loops: nop16 = 16 nop + loop, nop32 = 32 nop + loop */
static int32_t nop_x10, loop_x16;
static void calibrate(void)
{
    uint32_t a = run3(t_nop16, 0), b = run3(t_nop32, 0);
    /* per pass, in clocks x 16: a * 8 * 16 / N_LOOP */
    int32_t pa = (int32_t)(a * 128 / N_LOOP), pb = (int32_t)(b * 128 / N_LOOP);
    nop_x10 = (pb - pa) * 10 / (16 * 16);   /* (32 - 16) NOPs, x 16 */
    loop_x16 = pa - (pb - pa);              /* what is left of the 16-NOP pass: the loop */
}
/* clocks x 10 per instruction of a 16-instruction body, the loop taken off; extra = clocks x 10 of
   the other instructions in the body per access (an ADD: 10) */
static int32_t per_access(uint32_t ticks, int32_t extra)
{
    int32_t pass16 = (int32_t)(ticks * 128 / N_LOOP);           /* clocks x 16 per pass */
    return (pass16 - loop_x16) * 10 / (16 * 16) - extra;
}

static void measure(void)
{
    nres = 0;
    frt_take();
    slave_set(S_IDLE);
    calibrate();
    put("NOP", nop_x10, 0);
    put("ADD", per_access(run3(t_add, 0), 0), 0);
    put("MUL.L", per_access(run3(t_mul, 0), 0), 0);
    put("BRA+NOP", per_access(run3(t_bra, 0), 0), 0);
    put("LD+USE SD", per_access(run3(t_use, 0x06020000u), 10), 0);   /* cached SDRAM, hits */

    put("CART RD.W", per_access(run3(t_rd_w, 0x22100000u), 0), 0);
    put("CART RD.L", per_access(run3(t_rd_l, 0x22100000u), 0), 0);
    put("CART RD.B", per_access(run3(t_rd_b, 0x22100000u), 0), 0);
    put("CART HIT", per_access(run3(t_rd_l, 0x02100000u), 0), 0);
    put("CART LINE", per_access(run3(t_fill_l, 0x02180000u), 10), 0);

    put("SDRM RD.W", per_access(run3(t_rd_w, 0x26020000u), 0), 0);
    put("SDRM RD.L", per_access(run3(t_rd_l, 0x26020000u), 0), 0);
    put("SDRM WR.W", per_access(run3(t_wr_w, 0x26020000u), 0), 0);
    put("SDRM WR.L", per_access(run3(t_wr_l, 0x26020000u), 0), 0);
    put("SDRM HIT", per_access(run3(t_rd_l, 0x06020000u), 0), 0);
    put("SDRM LINE", per_access(run3(t_fill_l, 0x06020000u), 10), 0);

    put("FB RD.W", per_access(run3(t_rd_w, 0x24008000u), 0), 0);
    put("FB WR.W", per_access(run3(t_wr_w, 0x24008000u), 0), 0);
    put("FB WR.L", per_access(run3(t_wr_l, 0x24008000u), 0), 0);
    put("FB WR+6NOP", per_access(run3(t_wr_w_sp, 0x24008000u), 60), 0);
    put("OVR WR.W", per_access(run3(t_wr_w, 0x24028000u), 0), 0);
    put("REG RD COM", per_access(run3(t_rd_w, 0x2000402Au), 0), 0);
    put("REG WR COM", per_access(run3(t_wr_w, 0x2000402Au), 0), 0);
    put("REG RD VDP", per_access(run3(t_rd_w, 0x2000410Au), 0), 0);

    /* the frame buffer in the vertical blank and during the display: 256 writes / reads */
    {
        uint32_t r[3], w[3], rd[3], wd[3];
        for (int k = 0; k < 3; k++)
        {
            int i; uint32_t v = 0x2222, a = 0x24008000u, sr, t;
            wait_vblank_start(); sr = ints_off(); i = 16; frt_start(0);
            __asm__ __volatile__("1:\n\t" R16("mov.w %1,@%2\n\t") "dt %0\n\tbf 1b" : "+r"(i), "+z"(v), "+r"(a) : : "memory", "t");
            t = frt_read(); ints_restore(sr); w[k] = t;
            wait_vblank_start(); sr = ints_off(); i = 16; frt_start(0);
            __asm__ __volatile__("1:\n\t" R16("mov.w @%2,%1\n\t") "dt %0\n\tbf 1b" : "+r"(i), "+z"(v), "+r"(a) : : "memory", "t");
            t = frt_read(); ints_restore(sr); r[k] = t;
            wait_display(40); sr = ints_off(); i = 16; frt_start(0);
            __asm__ __volatile__("1:\n\t" R16("mov.w %1,@%2\n\t") "dt %0\n\tbf 1b" : "+r"(i), "+z"(v), "+r"(a) : : "memory", "t");
            t = frt_read(); ints_restore(sr); wd[k] = t;
            wait_display(40); sr = ints_off(); i = 16; frt_start(0);
            __asm__ __volatile__("1:\n\t" R16("mov.w @%2,%1\n\t") "dt %0\n\tbf 1b" : "+r"(i), "+z"(v), "+r"(a) : : "memory", "t");
            t = frt_read(); ints_restore(sr); rd[k] = t;
        }
        /* 16 passes: clocks x 10 per access = ticks * 8 * 10 / 256, loop taken off */
#define PA16(t) ((int32_t)((t) * 8 * 10 / 256) - loop_x16 * 10 / (16 * 16))
        put("FB WR VBL", PA16(med3(w[0], w[1], w[2])), 0);
        put("FB WR DISP", PA16(med3(wd[0], wd[1], wd[2])), 0);
        put("FB RD VBL", PA16(med3(r[0], r[1], r[2])), 0);
        put("FB RD DISP", PA16(med3(rd[0], rd[1], rd[2])), 0);
    }

    /* the VDP's fill: 256 words, from the data write until FEN clears (clocks, whole) */
    {
        uint32_t f[2][3];
        for (int k = 0; k < 3; k++)
            for (int d = 0; d < 2; d++)
            {
                while (MARS_VDP_FBCTL & MARS_VDP_FEN) { }
                if (d) wait_display(40); else wait_vblank_start();
                uint32_t sr = ints_off();
                MARS_VDP_FILLEN = 255;
                MARS_VDP_FILADR = 0xA000;
                frt_start(0);
                MARS_VDP_FILDAT = 0x3333;
                while (MARS_VDP_FBCTL & MARS_VDP_FEN) { }
                f[d][k] = frt_read() * 8;
                ints_restore(sr);
            }
        put("FILL256 VBL", (int32_t)med3(f[0][0], f[0][1], f[0][2]), 1);
        put("FILL256 DSP", (int32_t)med3(f[1][0], f[1][1], f[1][2]), 1);
    }

    /* the frame and the line, in clocks; a Sega CD command's round trip (the pad, CMD 48) */
    {
        uint32_t fr[3], ln[3], cd[3];
        for (int k = 0; k < 3; k++)
        {
            uint32_t sr;
            wait_vblank_start(); sr = ints_off(); frt_start(2);
            wait_vblank_start(); fr[k] = frt_read() * 128; ints_restore(sr);
            wait_display(4); sr = ints_off(); frt_start(0);
            for (int n = 0; n < 64; n++)
            {
                while (!(MARS_VDP_FBCTL & MARS_VDP_HBLK)) { }
                while (MARS_VDP_FBCTL & MARS_VDP_HBLK) { }
            }
            ln[k] = frt_read() * 8 / 64; ints_restore(sr);
            frt_start(2);
            for (int n = 0; n < 16; n++) cd_pad(0, 0);
            cd[k] = frt_read() * 128 / 16;
        }
        put("FRAME CLK", (int32_t)med3(fr[0], fr[1], fr[2]), 1);
        put("LINE CLK", (int32_t)med3(ln[0], ln[1], ln[2]), 1);
        put("CD CMD CLK", (int32_t)med3(cd[0], cd[1], cd[2]), 1);
    }

    /* the shared bus: the master's cart read, SDRAM read and frame buffer write while the slave
       hammers each memory in turn */
    static char names[3][S_N][12];
    static const char *const mname[3] = { "CA", "SD", "FB" };
    static const uint32_t maddr[3] = { 0x22100000u, 0x26020000u, 0x24008000u };
    for (int s = 0; s < S_N; s++)
    {
        slave_set((uint32_t)s);
        for (int m = 0; m < 3; m++)
        {
            char *n = names[m][s];
            n[0] = mname[m][0]; n[1] = mname[m][1]; n[2] = '/';
            for (int c = 0; c < 4; c++) n[3 + c] = slave_name[s][c];
            n[7] = 0;
            uint32_t t = m == 2 ? run3(t_wr_w, maddr[m]) : run3(t_rd_w, maddr[m]);
            put(n, per_access(t, 0), 0);
        }
    }
    slave_set(S_IDLE);
    frt_give();
}

static void dec(char *d, int32_t v)
{
    char t[12]; int n = 0, neg = v < 0;
    uint32_t u = neg ? (uint32_t)-v : (uint32_t)v;
    do { t[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (neg) *d++ = '-';
    while (n) *d++ = t[--n];
    *d = 0;
}

static void show(int runs)
{
    char b[16];
    clear();
    text(8, 0, "CD32X TIMING PROBE V1  CLOCKS", C_YELLOW);
    dec(b, runs); text(288, 0, b, C_GREY);
    for (int i = 0; i < nres; i++)
    {
        int col = i / 26, row = i % 26;
        int x = col * 160, y = 10 + row * 8;
        text(x, y, res[i].name, C_GREY);
        if (res[i].whole)
            dec(b, res[i].v);
        else
        {   /* one decimal */
            int32_t v = res[i].v;
            char *p = b;
            if (v < 0) { *p++ = '-'; v = -v; }
            dec(p, v / 10);
            while (*p) p++;
            *p++ = '.'; *p++ = (char)('0' + v % 10); *p = 0;
        }
        int len = 0; while (b[len]) len++;
        text(x + 152 - len * 8, y, b, C_WHITE);
    }
}

int main(void)
{
    cd_status(0x4E00);
    cd_genesis_display(0);
    MARS_VDP_DISPMODE = MARS_224_LINES | MARS_VDP_MODE_256;
    palette();
    for (int i = 0; i < 2; i++) { line_table(); clear(); text(96, 100, "MEASURING...", C_YELLOW); flip(); }
    cd_status(0x4B10);

    int runs = 0;
    uint16_t prev = 0xFFFF;
    for (;;)
    {
        measure();
        runs++;
        for (int i = 0; i < 2; i++) { line_table(); show(runs); flip(); }
        for (;;)                                /* A: measure again */
        {
            uint16_t pad = cd_pad(0, 0);
            if (prev == 0xFFFF) { if (!(pad & SEGA_CTRL_A)) prev = 0; continue; }
            if ((pad & SEGA_CTRL_A) && !(prev & SEGA_CTRL_A)) { prev = pad; break; }
            prev = pad;
        }
        for (int i = 0; i < 2; i++) { line_table(); clear(); text(96, 100, "MEASURING...", C_YELLOW); flip(); }
    }
    return 0;
}
