/* hello - the smallest CD32X game (CD32X BootCamp)
 *
 * A normal 32X cartridge program.  On the CD32X, CDLOADER (stack/sh2) loads this ROM from the
 * disc into the RAM cart and starts both SH2s at the entries in its header, exactly as the 32X
 * boot ROM starts a cartridge.  From then on the game runs from the cart; everything it cannot
 * do itself goes to the Sega CD through cdapi.c.
 *
 * What it shows:
 *   - the frame buffer in 256-colour mode, double buffered (flip at the vertical blank)
 *   - the pad (CMD 48) - the D-pad moves the square
 *   - a data file from the disc into the cart (CMD 38 + 46): LEVEL00.BIN at start, C loads LEVEL01.BIN
 *   - CD audio (CMD 45 / 54): A plays track 2, B stops it
 *   - a save in the cart (CMD 58 + 61): the boot counter, START saves it again
 *   - status codes in COMM12 (4Exx while starting, 4B10 = running)
 *   - an error box: any failed CD service shows its code (docs/09_ERROR_CODES.md) on the screen
 */
#include <stdint.h>
#include "32x.h"
#include "cdapi.h"
#include "font8x8.h"

/* HELLO_DIAG_TEXT (build.sh DIAG=1 -> make DIAG=1): the diagnostic disc.  hello starts on the Sub-CPU's
   green text (the boot history: STATE, files opened, cart chunks and addresses) and B+C switches
   between that text and hello's own screen.  0: the Genesis display stays off (release). */
#ifndef HELLO_DIAG_TEXT
#define HELLO_DIAG_TEXT 0
#endif

#define SAVE_PIECE  12                      /* a cart save piece: check the ledger before you take one */
#define SAVE_MAGIC  0x4243                  /* "BC" */

/* ---- what crt0.s needs from the program ---- */
volatile uint32_t gTicks = 0;
volatile unsigned mars_pwdt_ovf_count = 0;
volatile unsigned mars_swdt_ovf_count = 0;
void pri_vbi_handler(void) { gTicks++; }   /* the master's vertical blank interrupt */
void sec_dma1_handler(void) { }
void secondary(void)                        /* the slave SH2: idle in this example */
{
    while (1) { }
}

/* ---- the frame buffer: 320 x 224, one byte a pixel, written 16 bits at a time ---- */
#define FB          ((volatile uint16_t *)&MARS_FRAMEBUFFER)
enum { C_BLACK, C_WHITE, C_RED, C_GREEN, C_YELLOW, C_GREY, C_BLUE, C_CYAN };

static void palette(void)
{
    static const uint16_t pal[8] = {
        0x0000, 0x7FFF, 0x001F, 0x03E0, 0x03FF, 0x4210, 0x7C00, 0x7FE0   /* BGR555 */
    };
    volatile uint16_t *cram = (volatile uint16_t *)&MARS_CRAM;
    while ((MARS_VDP_FBCTL & MARS_VDP_PEN) == 0) { }   /* the palette: only in blanking */
    for (int i = 0; i < 256; i++)
        cram[i] = (i < 8) ? pal[i] : 0;
}

static void line_table(void)                /* line n starts at word 0x100 + n x 160 */
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

static void plot2(int x, int y, uint8_t a, uint8_t b)     /* two pixels, x even */
{
    FB[0x100 + y * 160 + (x >> 1)] = (uint16_t)((a << 8) | b);
}

static void box(int x0, int y0, int w, int h, uint8_t c)
{
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0 & ~1; x < x0 + w; x += 2)
            plot2(x, y, c, c);
}

static void text(int x, int y, const char *s, uint8_t c)   /* x a multiple of 8 */
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

static void hex4(char *d, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        d[i] = "0123456789ABCDEF"[(v >> (12 - 4 * i)) & 15];
    d[4] = 0;
}

/* the pad as words, one line: "UP LEFT A START" ("-" = nothing pressed or no pad); at most max chars */
static void pad_words(char *d, uint16_t pad, int max)
{
    static const struct { uint16_t bit; const char *name; } k[] = {
        { SEGA_CTRL_UP, "UP" }, { SEGA_CTRL_DOWN, "DOWN" }, { SEGA_CTRL_LEFT, "LEFT" },
        { SEGA_CTRL_RIGHT, "RIGHT" }, { SEGA_CTRL_A, "A" }, { SEGA_CTRL_B, "B" }, { SEGA_CTRL_C, "C" },
        { SEGA_CTRL_X, "X" }, { SEGA_CTRL_Y, "Y" }, { SEGA_CTRL_Z, "Z" }, { SEGA_CTRL_START, "START" },
        { SEGA_CTRL_MODE, "MODE" },
    };
    int n = 0;
    for (unsigned i = 0; i < sizeof k / sizeof k[0]; i++)
    {
        if (!(pad & k[i].bit))
            continue;
        const char *w = k[i].name;
        int len = 0;
        while (w[len]) len++;
        if (n + (n ? 1 : 0) + len > max)
            break;
        if (n) d[n++] = ' ';
        while (*w) d[n++] = *w++;
    }
    if (!n) d[n++] = '-';
    d[n] = 0;
}

static void dec(char *d, uint32_t v)
{
    char t[12]; int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *d++ = t[--n];
    *d = 0;
}

static void flip(void)                      /* show what was drawn at the next vertical blank */
{
    uint16_t cur = MARS_VDP_FBCTL & MARS_VDP_FS;
    MARS_VDP_FBCTL = (uint16_t)(cur ^ 1);
    while ((MARS_VDP_FBCTL & MARS_VDP_FS) == cur) { }
}

static void hexn(char *d, uint32_t v, int n)    /* n hex digits */
{
    for (int i = 0; i < n; i++)
        d[i] = "0123456789ABCDEF"[(v >> (4 * (n - 1 - i))) & 15];
    d[n] = 0;
}

static const char *error_text(uint16_t code)
{
    switch (code)
    {
    case CD_ERR_SAVE:      return "CART SAVE FAILED";
    case CD_ERR_NO_FILE:   return "FILE NOT ON THE DISC";
    case CD_ERR_CHUNK:     return "FILE DID NOT REACH CART";
    case CD_ERR_NO_ANSWER: return "SEGA CD DID NOT ANSWER";
    default:               return "UNKNOWN ERROR";
    }
}

/* the error box: the code, where it was raised (the BC-Ex tag in cdapi.c), what was asked, and the
   COMM registers as they were at that moment.  Every number is in docs/09_ERROR_CODES.md. */
static void error_box(void)
{
    char b[12];
    const CdError *e = &cd_error;
    box(4, 140, 312, 78, C_RED);
    box(6, 142, 308, 74, C_BLACK);
    hex4(b, e->code);
    text(8, 145, "ERROR", C_RED);  text(56, 145, b, C_YELLOW);
    text(104, 145, error_text(e->code), C_WHITE);
    text(8, 156, e->tag ? e->tag : "?", C_CYAN);
    text(8, 167, "CMD", C_GREY);  dec(b, e->cmd);  text(40, 167, b, C_WHITE);
    switch (e->code)
    {
    case CD_ERR_SAVE:                       /* the piece and where it lives in the cart */
        text(72, 167, "PIECE", C_GREY); dec(b, e->arg); text(120, 167, b, C_WHITE);
        text(152, 167, "CART", C_GREY); hexn(b, 0x3F0000u + ((uint32_t)e->arg << 12), 6); text(192, 167, b, C_WHITE);
        break;
    case CD_ERR_CHUNK:                      /* the file, the chunk, its offset in the file */
        text(72, 167, "FILE", C_GREY);  dec(b, e->file); text(112, 167, b, C_WHITE);
        text(152, 167, "CHUNK", C_GREY); dec(b, e->arg); text(200, 167, b, C_WHITE);
        text(232, 167, "+", C_GREY);    hexn(b, (uint32_t)e->arg << 15, 6); text(240, 167, b, C_WHITE);
        break;
    case CD_ERR_NO_FILE:
        text(72, 167, "FILE", C_GREY);  dec(b, e->arg); text(112, 167, b, C_WHITE);
        break;
    default:
        text(72, 167, "COMM2", C_GREY); hex4(b, e->arg); text(120, 167, b, C_WHITE);
        break;
    }
    static const char *const name[8] = { "C0", "C2", "C4", "C6", "C8", "CA", "CC", "CE" };
    for (int i = 0; i < 8; i++)             /* COMM0 ... COMM14 (CE = the Sub-CPU's heartbeat) */
    {
        int x = 8 + (i & 3) * 64, y = 180 + (i >> 2) * 11;
        text(x, y, name[i], C_GREY);
        hex4(b, e->comm[i]); text(x + 24, y, b, C_WHITE);
    }
    text(8, 205, "LOOK UP: DOCS 09 ERROR CODES", C_GREEN);
}

/* the first line of a text file in the cart (the files are plain text, see disc/data) */
static void cart_line(char *d, uint32_t addr, int max)
{
    const volatile uint8_t *p = (const volatile uint8_t *)addr;
    int i = 0;
    while (i < max - 1 && p[i] >= 32 && p[i] < 127) { d[i] = (char)p[i]; i++; }
    d[i] = 0;
}

int main(void)
{
    char file_text[40] = "(not loaded)", buf[48];
    uint32_t boots = 0;
    int music = 0, x = 152, y = 150;
    uint16_t prev = 0xFFFF;                 /* buttons held at start don't count */

    cd_status(0x4E00);                      /* 4E00: hello started */
    int view_text = HELLO_DIAG_TEXT;        /* 1: the Sub-CPU's green text shows (diagnostic disc) */
    cd_genesis_display(view_text);          /* 0: only the 32X picture */

    MARS_VDP_DISPMODE = MARS_224_LINES | MARS_VDP_MODE_256;
    palette();
    for (int i = 0; i < 2; i++) { line_table(); clear(C_BLACK); flip(); }

    /* the save: boot counter in cart save piece 12 (battery-backed, read straight from the cart) */
    const volatile uint16_t *sv = (const volatile uint16_t *)CART_SAVE(SAVE_PIECE);
    if (sv[0] == SAVE_MAGIC)
        boots = sv[1];
    boots++;
    uint16_t words[2] = { SAVE_MAGIC, (uint16_t)boots };
    int saved = cart_save(SAVE_PIECE, words, 2);
    if (saved)
        cd_status(0x4E01);                  /* 4E01 saved (a failure posts 4EE1 itself) */

    /* a data file from the disc into the cart's level slot (file 100 = LEVEL00.BIN, slot 16) */
    if (cd_load(100))
        cart_line(file_text, CART_SLOT(16), 39);
    uint16_t shown = cd_error.code;         /* an error code stays in COMM12 while its box shows */
    if (!shown)
        cd_status(0x4B10);                  /* 4B10: running */

    while (1)
    {
        uint16_t pad = cd_pad(0, 0);
        uint16_t hit = pad & ~prev;
        prev = pad;

        if (pad & SEGA_CTRL_LEFT)  x -= 2;
        if (pad & SEGA_CTRL_RIGHT) x += 2;
        if (pad & SEGA_CTRL_UP)    y -= 2;
        if (pad & SEGA_CTRL_DOWN)  y += 2;
        if (x < 0) x = 0;
        if (x > 304) x = 304;
        if (y < 140) y = 140;
        if (y > 208) y = 208;

        int bc = SEGA_CTRL_B | SEGA_CTRL_C;
        if (HELLO_DIAG_TEXT && (pad & bc) == bc && (hit & bc))
        {
            view_text ^= 1;                 /* diagnostic disc: B+C = green text / hello's screen */
            cd_genesis_display(view_text);
            hit = 0;
        }
        if (hit & SEGA_CTRL_A) { cd_music_play(2, 0); music = 1; }
        if (hit & SEGA_CTRL_B) { cd_music_stop(); music = 0; }
        if (hit & SEGA_CTRL_C)
        {
            music = 0;                      /* cd_load stops the music first */
            if (cd_load(101))
                cart_line(file_text, CART_SLOT(16), 39);
            else
                file_text[0] = '-', file_text[1] = 0;
        }
        if (hit & SEGA_CTRL_START)
        {
            words[1] = (uint16_t)boots;
            saved = cart_save(SAVE_PIECE, words, 2);
        }

        if (!cd_error.code && shown)
            cd_status(0x4B10);              /* the error went away (the action worked again) */
        shown = cd_error.code;

        clear(C_BLACK);                     /* colour 0x0000: see-through, the Genesis text shows there */
        if (view_text)
        {
            /* the Genesis text fills all 28 rows, so these go where its rows are short: row 5
               ("CD DRIVE READY", up to x 128) and row 8 ("68K LISTENER ACTIVE", up to x 168) */
            pad_words(buf, pad, 18);        /* the pad still works here: one line, in words */
            text(136, 40, "PAD", C_GREY);   text(168, 40, buf, C_WHITE);
            text(232, 64, "B+C:HELLO", C_YELLOW);
            if (cd_error.code)
                error_box();
            flip();
            continue;
        }
        text(64, 16, "CD32X BOOTCAMP - HELLO", C_YELLOW);
        box(16, 30, 288, 2, C_BLUE);
        dec(buf, boots);
        text(16, 40, "BOOTS:", C_GREY);  text(80, 40, buf, C_WHITE);
        text(160, 40, saved ? "SAVED" : "SAVE FAILED", saved ? C_GREEN : C_RED);
        pad_words(buf, pad, 30);            /* what is held, in words */
        text(16, 56, "PAD:", C_GREY);    text(64, 56, buf, C_WHITE);
        text(16, 72, "FILE:", C_GREY);   text(64, 72, file_text, C_CYAN);
        text(16, 88, "MUSIC:", C_GREY);  text(72, 88, music ? "TRACK 2" : "OFF", C_WHITE);
        dec(buf, gTicks / 60);
        text(160, 88, "SECONDS:", C_GREY); text(232, 88, buf, C_WHITE);
        text(16, 108, "A MUSIC  B STOP  C FILE  START SAVE", C_GREEN);
        text(16, 120, "D-PAD MOVES THE BOX", C_GREEN);
        if (HELLO_DIAG_TEXT)
            text(200, 120, "B+C:TEXT", C_YELLOW);
        box(0, 136, 320, 1, C_GREY);
        box(x, y, 16, 16, C_RED);
        if (cd_error.code)
            error_box();
        flip();
    }
    return 0;
}
