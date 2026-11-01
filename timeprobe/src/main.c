/* timeprobe - does /TIME (cart edge B31) reach the cart through the 32X?  (CD32X BootCamp)
 *
 * The 16 MB paged cart's GreenPAK takes a page only when the cart edge's /TIME pin (B31) is low while
 * /UWR (B29) falls, on a write to 0xA130F0 / 0xA130F2.  Nothing on the 4 MB RAM cart reacts to /TIME,
 * and the Mega EverDrive PRO decodes 0xA130xx from the address lines (krikzz mega-ed-pub,
 * fpga/mapper/lib_base/everdrive.sv:84; its FPGA has no /TIME pin), so no console run so far shows
 * whether the 32X passes /TIME to its top slot.  Software cannot see the pin; this disc makes it move,
 * and a logic probe, scope or logic analyzer on the cart edge shows whether it arrives.
 *
 * What it does: sends command 68 ("cart page", COMM2 = 1) as fast as the Sub-CPU answers.  Each one
 * is the exact write the 16 MB board's page switch makes: the 68000 listener's routine 26
 * (safe_cart_xfer: interrupts off, RV = 1) writes ONE even byte, 0x01, to 0xA130F2 (/TIME + /UWR,
 * D8 high, D9-D15 low).  Page 1 is what window 1 holds after reset, so on any cart nothing changes:
 * the 4 MB cart ignores the write (ledger: run on the console, 19 Oct 2026), and a 16 MB board would
 * re-take page 1.
 *
 * The writes come in bursts: WRITING for one second, then QUIET for one second, so a pulse-stretching
 * logic probe blinks in step with the screen.  START: continuous writes (for a scope / analyzer).
 * The screen shows the writes per second and the total.
 *
 * Build it with command 68 in the Sub-CPU:   GAME=timeprobe CART=16 DISC_NAME=CD32X_TIMEPROBE bash build.sh
 * Without CART=16 (or 8), command 68 is not in the Sub-CPU and the screen says so after 10 s.
 * Never with EDCD=1 / MODE1: the EverDrive builds turn the page write into EverDrive register writes.
 * What to measure, and how to read it: timeprobe/README.md.
 */
#include <stdint.h>
#include "32x.h"
#include "cdapi.h"
#include "font8x8.h"

#define CD_CMD_CART_PAGE  68                /* ledger section 7: COMM2 = page 1-7 of window 1 -> COMM12 = 1 done */
#define COMM12_16  (*(volatile uint16_t *)0x2000402C)

/* ---- what crt0.s needs from the program ---- */
volatile uint32_t gTicks = 0;
volatile unsigned mars_pwdt_ovf_count = 0;
volatile unsigned mars_swdt_ovf_count = 0;
void pri_vbi_handler(void) { gTicks++; }   /* the master's vertical blank interrupt */
void sec_dma1_handler(void) { }
void secondary(void)                        /* the slave SH2: idle, and off the cart (RV = 1 holds it anyway) */
{
    while (1) { }
}

/* ---- the frame buffer: 320 x 224, one byte a pixel (as hello) ---- */
#define FB          ((volatile uint16_t *)&MARS_FRAMEBUFFER)
enum { C_BLACK, C_WHITE, C_RED, C_GREEN, C_YELLOW, C_GREY, C_BLUE, C_CYAN };

static void palette(void)
{
    static const uint16_t pal[8] = {
        0x0000, 0x7FFF, 0x001F, 0x03E0, 0x03FF, 0x4210, 0x7C00, 0x7FE0   /* BGR555 */
    };
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

static void clear(uint8_t c)
{
    uint16_t w = (uint16_t)((c << 8) | c);
    volatile uint16_t *p = FB + 0x100;
    for (int i = 0; i < 160 * 224; i++)
        p[i] = w;
}

static void plot2(int x, int y, uint8_t a, uint8_t b)
{
    FB[0x100 + y * 160 + (x >> 1)] = (uint16_t)((a << 8) | b);
}

static void box(int x0, int y0, int w, int h, uint8_t c)
{
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0 & ~1; x < x0 + w; x += 2)
            plot2(x, y, c, c);
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
                plot2(x + b, y + r, (g[r] & (0x80 >> b)) ? c : C_BLACK,
                                     (g[r] & (0x40 >> b)) ? c : C_BLACK);
    }
}

static void dec(char *d, uint32_t v)
{
    char t[12]; int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *d++ = t[--n];
    *d = 0;
}

static void hex4(char *d, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        d[i] = "0123456789ABCDEF"[(v >> (12 - 4 * i)) & 15];
    d[4] = 0;
}

static void flip(void)
{
    uint16_t cur = MARS_VDP_FBCTL & MARS_VDP_FS;
    MARS_VDP_FBCTL = (uint16_t)(cur ^ 1);
    while ((MARS_VDP_FBCTL & MARS_VDP_FS) == cur) { }
}

/* one page write: 0x01 -> 0xA130F2 by the 68000 with RV = 1.  1 = done, 0 = refused, -1 = no answer */
static int page_write(void)
{
    MARS_SYS_COMM2 = 1;
    COMM12_16 = 0;                          /* the reply comes back in COMM12 */
    if (!cd_cmd(CD_CMD_CART_PAGE))
        return -1;
    return COMM12_16 == 1 ? 1 : 0;
}

int main(void)
{
    char b[16];
    uint32_t total = 0, this_sec = 0, last_rate = 0, refused = 0;
    uint32_t sec_start;
    (void)sec_start;
    int continuous = 0, broken = 0;
    uint16_t prev = 0xFFFF;

    cd_status(0x4E00);
    cd_genesis_display(0);
    MARS_VDP_DISPMODE = MARS_224_LINES | MARS_VDP_MODE_256;
    palette();
    for (int i = 0; i < 2; i++) { line_table(); clear(C_BLACK); flip(); }

    /* the first write tells whether this Sub-CPU has command 68 (a disc built without CART=16 does not
       answer it: cd_cmd gives up after 10 s, BC-E4) */
    clear(C_BLACK);
    text(16, 16, "CD32X /TIME PROBE", C_YELLOW);
    text(16, 40, "FIRST WRITE TO A130F2...", C_WHITE);
    flip();
    int r = page_write();
    if (r < 0) broken = 1;
    else if (r == 0) refused++;
    else total++;
    cd_status(broken ? 0x4EE4 : 0x4B10);

    sec_start = gTicks;
    int was_writing = 0;
    while (1)
    {
        uint32_t now = gTicks;
        int second = (int)((now / 60) & 1);         /* 0: WRITING, 1: QUIET (one second each) */
        int writing = !broken && (continuous || second == 0);

        /* writes per second: counted over each WRITING second (bursts), or each second (continuous) */
        if (writing && !was_writing) { this_sec = 0; sec_start = now; }
        if (!writing && was_writing) last_rate = this_sec;
        if (writing && continuous && now - sec_start >= 60) { last_rate = this_sec; this_sec = 0; sec_start = now; }
        was_writing = writing;

        if (writing)
        {
            /* as many writes as fit in about one frame, then redraw */
            while (gTicks == now)
            {
                r = page_write();
                if (r < 0) { broken = 1; break; }
                if (r == 0) refused++; else { total++; this_sec++; }
            }
        }
        else
            while (gTicks == now) { }               /* QUIET: no command at all, so no /TIME from us */

        uint16_t pad = cd_pad(0, 0);                /* pad reads go through the Sub-CPU too, never A130xx */
        if (prev == 0xFFFF) { if (!(pad & SEGA_CTRL_START)) prev = 0; }
        else
        {
            if ((pad & SEGA_CTRL_START) && !(prev & SEGA_CTRL_START)) continuous ^= 1;
            prev = pad;
        }

        clear(C_BLACK);
        text(16, 16, "CD32X /TIME PROBE", C_YELLOW);
        box(16, 28, 288, 2, C_BLUE);
        text(16, 40, "EACH WRITE: 01 -> A130F2 (EVEN BYTE)", C_GREY);
        text(16, 52, "68000, RV=1, INTS OFF (CMD 68)", C_GREY);
        if (broken)
        {
            text(16, 80, "CMD 68 NOT ANSWERED", C_RED);
            text(16, 96, "REBUILD WITH CART=16:", C_WHITE);
            text(16, 108, "GAME=TIMEPROBE CART=16", C_CYAN);
            text(16, 120, "BASH BUILD.SH", C_CYAN);
        }
        else
        {
            text(16, 80, continuous ? "WRITING (CONTINUOUS)" : (writing ? "WRITING" : "QUIET"),
                 writing ? C_GREEN : C_GREY);
            if (writing) box(240, 76, 48, 16, C_GREEN);
            text(16, 100, "WRITES/SECOND:", C_GREY); dec(b, last_rate); text(136, 100, b, C_WHITE);
            text(16, 112, "TOTAL:", C_GREY);          dec(b, total);     text(136, 112, b, C_WHITE);
            if (refused) { text(16, 124, "REFUSED:", C_RED); dec(b, refused); text(136, 124, b, C_RED); }
        }
        text(16, 150, "PROBE B31 /TIME AND B29 /UWR", C_YELLOW);
        text(16, 162, "THEY SHOULD PULSE ONLY IN WRITING", C_YELLOW);
        text(16, 174, "B28 /LWR SHOULD STAY QUIET", C_YELLOW);
        text(16, 196, continuous ? "START: BURSTS" : "START: CONTINUOUS", C_GREEN);
        hex4(b, (uint32_t)(gTicks / 60)); text(256, 196, b, C_GREY);
        flip();
    }
}
