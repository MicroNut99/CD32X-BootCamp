/* traytest - open and close the Sega CD's tray from buttons on the screen (CD32X BootCamp)
 *
 * For testing Sega CD Model 1 units, most of which do not open from software: its tray is motorised and it
 * has no eject button.  The program runs from the 32X's SDRAM (T3, sdram.c), so the disc can come out while
 * it runs, and no cartridge is needed: Sega CD + 32X, any cart in the slot or none.
 *
 * On the screen: four buttons (LEFT / RIGHT choose, A presses)
 *   OPEN   CMD 90, COMM2 = 1: BIOS DRVOPEN 0x0A - the drive stops and the tray opens
 *   CLOSE  CMD 90, COMM2 = 0: BIOS DRVINIT 0x10 - the tray closes and the disc is read
 *   PLAY   CMD 45: CD audio track 2 (the test disc's tone, or track 2 of any audio CD put in)
 *   STOP   CMD 54
 * and what the BIOS reports (CMD 84): its status word and the drive's status word every frame, every
 * change of them in a list with the time it happened.
 *
 * T2 (the owner, 2 Nov 2026: "something displayed on the screen to indicate a button press - this is made
 * for a human"): while A is held the chosen button is drawn pushed in (white); when it acts it turns green
 * (asked) or red (refused / no answer) for half a second, and a large banner says what it is doing
 * ("OPENING THE TRAY") for three seconds.  The rest of the time the banner shows the drive's state in
 * large letters ("TRAY OPEN", "READING DISC", "NO DISC", "PLAYING").
 *
 * T3 (the owner, 2 Nov 2026: "we cannot have the test tied to a MedPro or 16MB cart"): built like CDLOADER
 * (the stack's cdl_crt0.s and cdloader.ld, with sdram.c) and put in CDLOADER's place, with the stack's
 * start-up cart check off (TRAY_NOCART=1).  No RAM cart, EverDrive or 16 MB cart.
 *
 * T4 (the owner, 2 Nov 2026: "include data in the screen shot that would be useful if it didn't work"):
 * one photo of the screen is the whole record.  It has the result of the last OPEN / CLOSE in words
 * (RESULT), the BIOS's whole 32-byte status table in hex, the BIOS ROM's own name, date, version and region
 * (its header, read by the 68000), the Mega Drive's version register, the Sega CD gate array's version and
 * where the Sub-CPU BIOS's two entry jumps go (they differ from one BIOS version to the next, so they name
 * the BIOS even if the header read shows nothing), and the last five changes with the drive's full status
 * word.  The subtitle (the owner): "MODEL 1: OPEN/CLOSE TEST".
 *
 * Commands 84 and 90 are merged into a copy of the stack by traytest/merge_tray.py (TRAYTEST=1);
 * ledger section 22.  cdapi.c, 32x.h and font8x8.h are hello's.
 */
#include <stdint.h>
#include "32x.h"
#include "cdapi.h"
#include "font8x8.h"

#define TRAYTEST_NAME     "TRAY TEST T4"
#define CD_CMD_STATUS     84          /* traytest/merge_tray.py */
#define CD_CMD_TRAY       90

#define COMM8_32   (*(volatile uint32_t *)0x20004028)
#define COMM12_16  (*(volatile uint16_t *)0x2000402C)

/* ---- what the start-up (stack/sh2/cdl_crt0.s) needs from the program; sdram.c has the rest ---- */
volatile uint32_t gTicks = 0;
volatile unsigned mars_pwdt_ovf_count = 0;
volatile unsigned mars_swdt_ovf_count = 0;
void pri_vbi_handler(void) { gTicks++; }
void sec_dma1_handler(void) { }
void secondary(void) { while (1) { } }

/* ---- the frame buffer (hello's) ---- */
#define FB          ((volatile uint16_t *)&MARS_FRAMEBUFFER)
enum { C_BLACK, C_WHITE, C_RED, C_GREEN, C_YELLOW, C_GREY, C_BLUE, C_CYAN, C_DARK };

static void palette(void)
{
    static const uint16_t pal[9] = {
        0x0000, 0x7FFF, 0x001F, 0x03E0, 0x03FF, 0x4210, 0x7C00, 0x7FE0, 0x2108   /* BGR555 */
    };
    volatile uint16_t *cram = (volatile uint16_t *)&MARS_CRAM;
    while ((MARS_VDP_FBCTL & MARS_VDP_PEN) == 0) { }
    for (int i = 0; i < 256; i++)
        cram[i] = (i < 9) ? pal[i] : 0;
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

/* T4: digits and hex letters that cannot be mixed up in a photo of a TV.  In font8x8.h 0 and 8 differ by one
   pixel, and B and D are close to both.  Here 0 is slashed, 8 has a waist, B and D have a flat left side.
   Digits always use these; the letters A-F only in numbers (text_hex). */
static const unsigned char hexfont[16][8] = {
    {0x7C, 0xC6, 0xCE, 0xD6, 0xE6, 0xC6, 0x7C, 0x00},   /* 0 */
    {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},   /* 1 */
    {0x7C, 0xC6, 0x06, 0x1C, 0x70, 0xC0, 0xFE, 0x00},   /* 2 */
    {0x7C, 0xC6, 0x06, 0x3C, 0x06, 0xC6, 0x7C, 0x00},   /* 3 */
    {0x0E, 0x1E, 0x36, 0x66, 0xFE, 0x06, 0x06, 0x00},   /* 4 */
    {0xFE, 0xC0, 0xFC, 0x06, 0x06, 0xC6, 0x7C, 0x00},   /* 5 */
    {0x3C, 0x60, 0xC0, 0xFC, 0xC6, 0xC6, 0x7C, 0x00},   /* 6 */
    {0xFE, 0xC6, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00},   /* 7 */
    {0x7C, 0xC6, 0xC6, 0x7C, 0xC6, 0xC6, 0x7C, 0x00},   /* 8 */
    {0x7C, 0xC6, 0xC6, 0x7E, 0x06, 0x0C, 0x78, 0x00},   /* 9 */
    {0x38, 0x6C, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0x00},   /* A */
    {0xFC, 0xC6, 0xC6, 0xFC, 0xC6, 0xC6, 0xFC, 0x00},   /* B */
    {0x7C, 0xC6, 0xC0, 0xC0, 0xC0, 0xC6, 0x7C, 0x00},   /* C */
    {0xF8, 0xCC, 0xC6, 0xC6, 0xC6, 0xCC, 0xF8, 0x00},   /* D */
    {0xFE, 0xC0, 0xC0, 0xFC, 0xC0, 0xC0, 0xFE, 0x00},   /* E */
    {0xFE, 0xC0, 0xC0, 0xFC, 0xC0, 0xC0, 0xC0, 0x00},   /* F */
};
static int hex_letters;                     /* text_hex: A-F from hexfont too */

static const unsigned char *glyph(int ch)
{
    if (ch >= '0' && ch <= '9') return hexfont[ch - '0'];
    if (hex_letters && ch >= 'A' && ch <= 'F') return hexfont[ch - 'A' + 10];
    if (ch < 32 || ch > 127) ch = '?';
    return font8x8[ch - 32];
}

static void text_bg(int x, int y, const char *s, uint8_t c, uint8_t bg)   /* x a multiple of 8 */
{
    for (; *s; s++, x += 8)
    {
        const unsigned char *g = glyph((uint8_t)*s);
        for (int r = 0; r < 8; r++)
            for (int b = 0; b < 8; b += 2)
                plot2(x + b, y + r, (g[r] & (0x80 >> b)) ? c : bg, (g[r] & (0x40 >> b)) ? c : bg);
    }
}
static void text(int x, int y, const char *s, uint8_t c) { text_bg(x, y, s, c, C_BLACK); }
static void text_hex(int x, int y, const char *s, uint8_t c) { hex_letters = 1; text_bg(x, y, s, c, C_BLACK); hex_letters = 0; }

static void text2x(int x, int y, const char *s, uint8_t c, uint8_t bg)   /* twice the size: 16 x 16 a letter */
{
    for (; *s; s++, x += 16)
    {
        const unsigned char *g = glyph((uint8_t)*s);
        for (int r = 0; r < 8; r++)
            for (int b = 0; b < 8; b++)
            {
                uint8_t v = (g[r] & (0x80 >> b)) ? c : bg;
                plot2(x + 2 * b, y + 2 * r, v, v);
                plot2(x + 2 * b, y + 2 * r + 1, v, v);
            }
    }
}
static int len_of(const char *s) { int n = 0; while (s[n]) n++; return n; }

static void hexn(char *d, uint32_t v, int n)
{
    for (int i = 0; i < n; i++)
        d[i] = "0123456789ABCDEF"[(v >> (4 * (n - 1 - i))) & 15];
    d[n] = 0;
}

static void dec(char *d, uint32_t v)
{
    char t[12]; int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *d++ = t[--n];
    *d = 0;
}

static char *cat(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }

static void flip(void)
{
    uint16_t cur = MARS_VDP_FBCTL & MARS_VDP_FS;
    MARS_VDP_FBCTL = (uint16_t)(cur ^ 1);
    while ((MARS_VDP_FBCTL & MARS_VDP_FS) == cur) { }
}

/* ---- what the codes mean ---- */
/* the BIOS status word's high byte (bit 7 = busy: a command is being carried out) - US BIOS 2.00W code */
static const char *bios_name(uint16_t w)
{
    switch ((w >> 8) & 0x7F)
    {
    case 0x00: return "STOPPED";
    case 0x01: return (w & 0xFF) ? "READING DATA" : "PLAYING";
    case 0x03: return "SCANNING";
    case 0x05: return (w & 0xFF) ? "DATA PAUSED" : "PAUSED";
    case 0x10: return "NO DISC";
    case 0x20: return "READING DISC";
    case 0x40: return "TRAY OPEN";
    default:   return "?";
    }
}
/* the drive's own status code (the high byte of its status word) */
static const char *drive_name(int c)
{
    static const char *const n[16] = {
        "STOPPED", "PLAYING", "SEEKING", "SCANNING", "PAUSED", "TRAY OPEN", "CHECKSUM ERROR",
        "COMMAND ERROR", "FUNCTION ERROR", "READING TOC", "TRACKING", "NO DISC", "LEAD OUT",
        "LEAD IN", "TRAY MOVING", "TEST"
    };
    return n[c & 15];
}

/* the drive's state for the banner, in words a person reads at a glance */
static const char *big_state(uint16_t bios)
{
    if ((bios & 0x8000) && (bios & 0x7F00) != 0x4000)
        return "BUSY - WAIT";           /* a command is being carried out */
    switch ((bios >> 8) & 0x7F)
    {
    case 0x40: return "TRAY OPEN";
    case 0x20: return "READING DISC";
    case 0x10: return "NO DISC";
    case 0x01: return (bios & 0xFF) ? "READING DATA" : "PLAYING";
    case 0x03: return "SCANNING";
    case 0x05: return "READY (PAUSED)";
    case 0x00: return "STOPPED";
    default:   return "WORKING";
    }
}

/* ---- the commands ---- */
static uint16_t reply12;                    /* COMM12 after the last command */
static uint32_t reply8;                     /* COMM8L after the last command */

static int cmd(uint16_t c, uint16_t arg)
{
    MARS_SYS_COMM2 = (short)arg;
    COMM12_16 = 0;
    if (!cd_cmd(c))
        return 0;
    reply8 = COMM8_32;
    reply12 = COMM12_16;
    cd_status(0x4B10);                      /* COMM12 back to "running" (the Sub-CPU's diagnostic text shows it) */
    return 1;
}

/* T4: the BIOS ROM's header (68000 0x100-0x1FF, CMD 84 COMM2 0x20 + n): n bytes from off, as text.
   Anything that is not a printable letter shows as '.', runs of spaces become one, the end is trimmed. */
static void rom_text(char *d, int off, int n)
{
    char raw[48];
    int i, k = 0;
    for (i = 0; i < n; i += 4)
    {
        uint32_t l = cmd(CD_CMD_STATUS, (uint16_t)(0x20 + ((off - 0x100 + i) >> 2))) ? reply8 : 0;
        for (int b = 0; b < 4 && i + b < n; b++)
            raw[i + b] = (char)(l >> (24 - 8 * b));
    }
    for (i = 0; i < n; i++)
    {
        char c = raw[i];
        if (c < 32 || c > 126) c = '.';
        if (c == ' ' && (k == 0 || d[k - 1] == ' ')) continue;
        d[k++] = c;
    }
    while (k && d[k - 1] == ' ') k--;
    if (k > 37) k = 37;                     /* the screen's width inside the margins */
    d[k] = 0;
}

/* the change list */
#define LOG_N 5
static struct { uint32_t t; uint16_t bios; uint16_t drive; char what[14]; } log_[LOG_N];
static int log_n;

static void log_add(uint16_t bios, uint16_t drive, const char *what)
{
    if (log_n == LOG_N)
    {
        for (int i = 1; i < LOG_N; i++) log_[i - 1] = log_[i];
        log_n--;
    }
    log_[log_n].t = gTicks;
    log_[log_n].bios = bios;
    log_[log_n].drive = drive;
    int i = 0;
    for (; what[i] && i < 13; i++) log_[log_n].what[i] = what[i];
    log_[log_n].what[i] = 0;
    log_n++;
}

static void secs(char *d, uint32_t ticks)  /* "123.4" seconds (NTSC) */
{
    uint32_t ds = ticks / 6;
    dec(d, ds / 10);
    while (*d) d++;
    *d++ = '.'; *d++ = (char)('0' + ds % 10); *d = 0;
}

/* ---- T4: the result of the last OPEN / CLOSE, in words ---- */
#define OPEN_WAIT   600                     /* 10 s */
#define CLOSE_WAIT  1200                    /* 20 s: closing reads the disc's table of contents too */
static int pend = -1;                       /* 0 OPEN / 1 CLOSE being watched, -1 none */
static uint32_t pend_t;
static uint16_t pend_bios;
static uint8_t pend_drive;
static int pend_changed, pend_moved, pend_read;
static char result[40] = "NONE YET - PRESS OPEN";
static uint8_t result_c = C_GREY;

static void say(const char *a, uint32_t ticks, const char *b, uint8_t c)
{
    char *p = cat(result, a);
    if (ticks != 0xFFFFFFFFu) { secs(p, ticks); while (*p) p++; p = cat(p, " S"); }
    if (b) cat(p, b);
    result_c = c;
}

static void watch(uint16_t bios, uint8_t drive)
{
    if (pend < 0)
        return;
    uint32_t dt = gTicks - pend_t;
    int code = (bios >> 8) & 0x7F;
    char h[4];
    if (bios != pend_bios || drive != pend_drive) pend_changed = 1;
    if (drive == 0x0E) pend_moved = 1;
    if (code == 0x20 || drive == 0x09) pend_read = 1;   /* the BIOS reading the disc, the drive its contents */
    if (pend == 0)                          /* OPEN: the drive says "tray open" (05), or the BIOS 4000 */
    {
        if (drive == 0x05 || code == 0x40) { say("OPEN OK IN ", dt, 0, C_GREEN); pend = -1; }
        else if (dt >= OPEN_WAIT)
        {
            hexn(h, drive, 2);
            if (!pend_changed)   say("OPEN FAILED: NO CHANGE, 10 S", 0xFFFFFFFFu, 0, C_RED);
            else if (pend_moved) say("OPEN FAILED: MOVED, NOT OPEN", 0xFFFFFFFFu, 0, C_RED);
            else                 say("OPEN FAILED: LAST DRIVE ", 0xFFFFFFFFu, h, C_RED);
            pend = -1;
        }
        else say(pend_moved ? "OPEN: TRAY MOVING " : "OPEN: WAITING ", dt, 0, C_YELLOW);
    }
    else                                    /* CLOSE: the disc read (2000 or drive 09 seen), then paused (0500 /
                                               04); or no disc */
    {
        if (pend_read && !(bios & 0x8000) && code == 0x05 && drive == 0x04)
            { say("CLOSE OK: DISC READ IN ", dt, 0, C_GREEN); pend = -1; }
        else if (pend_changed && !(bios & 0x8000) && (code == 0x10 || drive == 0x0B))
            { say("CLOSE OK: NO DISC, ", dt, 0, C_GREEN); pend = -1; }
        else if (dt >= CLOSE_WAIT)
        {
            hexn(h, drive, 2);
            if (!pend_changed) say("CLOSE FAILED: NO CHANGE, 20 S", 0xFFFFFFFFu, 0, C_RED);
            else               say("CLOSE FAILED: LAST DRIVE ", 0xFFFFFFFFu, h, C_RED);
            pend = -1;
        }
        else say(pend_read ? "CLOSE: READING DISC " : pend_moved ? "CLOSE: TRAY MOVING " : "CLOSE: WAITING ", dt, 0, C_YELLOW);
    }
}

int main(void)
{
    static const char *const label[4] = { "OPEN", "CLOSE", "PLAY 2", "STOP" };
    static const char *const doing[4] = { "OPENING THE TRAY", "CLOSING THE TRAY", "PLAYING TRACK 2", "STOPPING MUSIC" };
    static char rom_serial[20], rom_region[8], rom_date[20], rom_name[44];
    static uint8_t tab[32];                 /* the BIOS's status table (_CDSTAT), refreshed every 10 frames */
    char b[48], *p;
    int sel = 0;
    uint16_t prev = 0xFFFF, chord = 0;
    uint16_t bios = 0, last_bios = 0xFFFF;
    uint8_t drive = 0;
    uint16_t drivew = 0, last_drivew = 0xFFFF;
    uint32_t drv32 = 0;
    uint16_t ga = 0, mdver = 0, buram = 0;
    uint32_t cdbios = 0;
    const char *said = "-";                 /* what the last button did */
    uint32_t said_t = 0;
    uint16_t said_reply = 0;
    const char *banner = 0;                 /* the press's banner (3 s), else the drive's state */
    uint8_t banner_c = C_GREEN;
    int act_btn = -1, act_ok = 0;           /* the button that acted: green / red for half a second */

    cd_status(0x4E00);
    cd_genesis_display(0);
    MARS_VDP_DISPMODE = MARS_224_LINES | MARS_VDP_MODE_256;
    palette();
    for (int i = 0; i < 2; i++) { line_table(); clear(C_BLACK); flip(); }

    /* T4: what this console is - read once */
    rom_text(rom_serial, 0x180, 14);        /* "BR 000003-1.10" */
    rom_text(rom_region, 0x1F0, 3);         /* "U" / "J" / "E" */
    rom_text(rom_date, 0x110, 16);          /* "(C)SEGA 1992.OCT" */
    rom_text(rom_name, 0x120, 48);          /* "SEGA-CD BOOT ROM10/11-1992 18:30 1.10" */
    if (cmd(CD_CMD_STATUS, 3)) { ga = reply12; mdver = (uint16_t)reply8; }
    if (cmd(CD_CMD_STATUS, 4)) { buram = reply12; cdbios = reply8; }
    if (!cd_error.code)
        cd_status(0x4B10);

    while (1)
    {
        uint16_t pad = cd_pad(0, 0);
        /* the buttons act when released (hello's chord rule); LEFT / RIGHT on the press */
        const uint16_t keys = SEGA_CTRL_A | SEGA_CTRL_B | SEGA_CTRL_C | SEGA_CTRL_START;
        uint16_t hit = 0, press = 0;
        if (prev == 0xFFFF)
        {
            if (!(pad & (keys | SEGA_CTRL_LEFT | SEGA_CTRL_RIGHT))) prev = 0;
        }
        else
        {
            press = pad & ~prev;
            chord |= pad & keys;
            if (chord && !(pad & keys)) { hit = chord; chord = 0; }
            prev = pad;
        }
        if (press & SEGA_CTRL_LEFT)  sel = (sel + 3) & 3;
        if (press & SEGA_CTRL_RIGHT) sel = (sel + 1) & 3;

        /* the status, every frame; the whole table, every 10th */
        if (cmd(CD_CMD_STATUS, 0))
        {
            bios = reply12;
            drv32 = reply8;
            drivew = (uint16_t)(drv32 >> 16);
            drive = (uint8_t)(drv32 >> 24);
        }
        if ((gTicks % 10) == 0)
            for (int k = 0; k < 8; k++)
                if (cmd(CD_CMD_STATUS, (uint16_t)(0x10 + k)))
                    for (int i = 0; i < 4; i++) tab[4 * k + i] = (uint8_t)(reply8 >> (24 - 8 * i));
        /* a change of the BIOS word or of the drive's code (the high byte: its low byte only says which
           report the drive is sending, and cycles all the time) */
        if (bios != last_bios || (drivew >> 8) != (last_drivew >> 8))
        {
            log_add(bios, drivew, "");
            last_bios = bios; last_drivew = drivew;
        }
        watch(bios, drive);

        if (hit == SEGA_CTRL_A || hit == SEGA_CTRL_C || hit == SEGA_CTRL_START)
        {
            int busy = (bios & 0x8000) != 0, ok = 1;
            said_reply = 0;
            switch (sel)
            {
            case 0: ok = cmd(CD_CMD_TRAY, 1) && reply12 == 1; said = ok ? "OPEN ASKED" : "NO ANSWER";   said_reply = reply12; break;
            case 1: ok = cmd(CD_CMD_TRAY, 0) && reply12 == 1; said = ok ? "CLOSE ASKED" : "NO ANSWER";  said_reply = reply12; break;
            case 2:                         /* 45 and 54 wait for a BIOS that is not busy (stack, no time limit) */
                if (busy) { said = "BUSY - WAIT"; ok = 0; break; }
                cd_music_play(2, 0); said = "PLAY ASKED"; break;
            default:
                if (busy) { said = "BUSY - WAIT"; ok = 0; break; }
                cd_music_stop(); said = "STOP ASKED"; break;
            }
            said_t = gTicks;
            act_btn = sel; act_ok = ok;
            banner = ok ? doing[sel] : (busy ? "BUSY - TRY AGAIN" : "NO ANSWER");
            banner_c = ok ? C_GREEN : C_RED;
            log_add(bios, drivew, label[sel]);
            if (sel < 2)                    /* T4: watch what the drive does next */
            {
                if (ok) { pend = sel; pend_t = gTicks; pend_bios = bios; pend_drive = drive; pend_changed = pend_moved = pend_read = 0; watch(bios, drive); }
                else    { pend = -1; say(sel ? "CLOSE: NO ANSWER (SUB-CPU)" : "OPEN: NO ANSWER (SUB-CPU)", 0xFFFFFFFFu, 0, C_RED); }
            }
        }

        clear(C_BLACK);
        text(24, 4, "CD32X BOOTCAMP - " TRAYTEST_NAME, C_YELLOW);
        text(24, 13, "MODEL 1: OPEN/CLOSE TEST", C_GREY);

        const int held = (pad & keys) != 0 && prev != 0xFFFF;   /* A (or C, START) is down now */
        const int flash = act_btn >= 0 && gTicks - said_t < 30;  /* half a second after a button acted */
        for (int i = 0; i < 4; i++)         /* the four buttons */
        {
            int x = 8 + i * 78, y = 23;
            uint8_t edge = (i == sel) ? C_YELLOW : C_GREY, fill = (i == sel) ? C_BLUE : C_DARK, ink = C_WHITE;
            if (i == sel && held)           /* pushed in: white, two pixels lower */
            {
                y += 2; edge = C_WHITE; fill = C_WHITE; ink = C_BLACK;
            }
            else if (i == act_btn && flash) /* it acted: green, or red when refused */
            {
                edge = C_WHITE; fill = act_ok ? C_GREEN : C_RED; ink = C_BLACK;
            }
            box(x, y, 72, 18, edge);
            box(x + 2, y + 2, 68, 14, fill);
            int len = len_of(label[i]);
            text_bg(x + ((72 - len * 8) / 2 & ~7), y + 5, label[i], ink, fill);
        }

        /* the banner: what the button is doing (3 s after a press), else the drive's state */
        {
            const char *msg = big_state(bios);
            uint8_t c = C_CYAN, bg = C_DARK;
            if (banner && gTicks - said_t < 180) { msg = banner; c = C_BLACK; bg = banner_c; }
            box(8, 45, 304, 18, bg);
            int n = len_of(msg); if (n > 19) n = 19;
            text2x(8 + ((304 - n * 16) / 2 & ~1), 46, msg, c, bg);
        }

        text(8, 66, "BIOS", C_GREY);
        hexn(b, bios, 4); text_hex(64, 66, b, C_WHITE);
        text(112, 66, bios_name(bios), C_CYAN);
        if (bios & 0x8000) text(264, 66, "BUSY", C_RED);

        text(8, 75, "DRIVE", C_GREY);
        hexn(b, drivew, 4); text_hex(64, 75, b, C_WHITE);
        text(112, 75, drive_name(drive & 15), C_CYAN);

        text(8, 84, "TRACKS", C_GREY);
        hexn(b, (drv32 >> 8) & 0xFF, 2); p = b + 2; *p++ = '-'; hexn(p, drv32 & 0xFF, 2);
        text_hex(64, 84, b, C_WHITE);
        text(136, 84, "LIGHTS", C_GREY);
        hexn(b, (uint32_t)tab[2] << 8 | tab[3], 4); text_hex(192, 84, b, C_WHITE);

        text(8, 93, "LAST", C_GREY);
        p = cat(b, said);
        if (said_t) { p = cat(p, " AT "); secs(p, said_t); }
        text(64, 93, b, C_GREEN);
        if (said_reply) { text(272, 93, "R", C_GREY); hexn(b, said_reply, 2); text_hex(288, 93, b, C_WHITE); }

        text(8, 102, "RESULT", C_GREY);
        text(64, 102, result, result_c);

        for (int r = 0; r < 2; r++)         /* the BIOS's status table, 16 bytes a line */
        {
            p = b;
            for (int g = 0; g < 4; g++)
            {
                if (g) *p++ = ' ';
                for (int i = 0; i < 4; i++) { hexn(p, tab[16 * r + 4 * g + i], 2); p += 2; }
            }
            *p = 0;
            text_hex(8, 111 + 9 * r, b, r ? C_WHITE : C_CYAN);   /* bytes 0-15 cyan, 16-31 white */
        }

        p = cat(b, rom_serial); p = cat(p, "  "); p = cat(p, rom_region); p = cat(p, "  "); cat(p, rom_date);
        text(8, 130, b, C_YELLOW);
        text(8, 139, rom_name, C_YELLOW);
        p = cat(b, "MD "); hexn(p, mdver & 0xFF, 2); p += 2;
        p = cat(p, "  GA "); hexn(p, ga, 4); p += 4;
        p = cat(p, "  SUB "); hexn(p, cdbios, 6); p += 6;  /* where _CDBIOS and _BURAM go */
        *p++ = ' '; hexn(p, buram, 4);
        text_hex(8, 148, b, C_GREY);

        box(8, 158, 304, 1, C_GREY);
        for (int i = 0; i < log_n; i++)     /* the last five changes and buttons */
        {
            int y = 161 + i * 9;
            secs(b, log_[i].t);             text(8, y, b, C_WHITE);
            hexn(b, log_[i].bios, 4);       text_hex(64, y, b, C_WHITE);
            hexn(b, log_[i].drive, 4);      text_hex(104, y, b, C_WHITE);
            if (log_[i].what[0])            text(152, y, log_[i].what, C_YELLOW);
            else                            text(152, y, drive_name((log_[i].drive >> 8) & 15), C_CYAN);
        }
        text(8, 207, "LEFT/RIGHT CHOOSE   A PRESS", C_GREEN);
        if (cd_error.code)
        {
            hexn(b, cd_error.code, 4);
            text_hex(256, 207, b, C_RED);
        }
        flip();
    }
    return 0;
}
