/* traytest - open and close the Sega CD's tray from buttons on the screen (CD32X BootCamp)
 *
 * For the Sega CD Model 1 above all: its tray is motorised and it has no eject button, so only
 * software opens it.  The program runs from the 32X's SDRAM (T3, sdram.c), so the disc can come out while
 * it runs, and no cartridge is needed: Sega CD + 32X only.
 *
 * On the screen: four buttons (LEFT / RIGHT choose, A presses)
 *   OPEN   CMD 90, COMM2 = 1: BIOS DRVOPEN 0x0A - the drive stops and the tray opens
 *   CLOSE  CMD 90, COMM2 = 0: BIOS DRVINIT 0x10 - the tray closes and the disc is read
 *   PLAY   CMD 45: CD audio track 2 (the test disc's tone, or track 2 of any audio CD put in)
 *   STOP   CMD 54
 * and what the BIOS reports, every frame (CMD 84): its status word, the drive's own status code,
 * the first and last track, the time in the track, the drive lights; every change of the two status
 * codes goes into a list with the time it happened.
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
 * Commands 84 and 90 are merged into a copy of the stack by traytest/merge_tray.py (TRAYTEST=1);
 * ledger section 22.  cdapi.c, 32x.h and font8x8.h are hello's.
 */
#include <stdint.h>
#include "32x.h"
#include "cdapi.h"
#include "font8x8.h"

#define TRAYTEST_NAME     "TRAY TEST T3"
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

static void text_bg(int x, int y, const char *s, uint8_t c, uint8_t bg)   /* x a multiple of 8 */
{
    for (; *s; s++, x += 8)
    {
        int ch = (uint8_t)*s;
        if (ch < 32 || ch > 127) ch = '?';
        const unsigned char *g = font8x8[ch - 32];
        for (int r = 0; r < 8; r++)
            for (int b = 0; b < 8; b += 2)
                plot2(x + b, y + r, (g[r] & (0x80 >> b)) ? c : bg, (g[r] & (0x40 >> b)) ? c : bg);
    }
}
static void text(int x, int y, const char *s, uint8_t c) { text_bg(x, y, s, c, C_BLACK); }

static void text2x(int x, int y, const char *s, uint8_t c, uint8_t bg)   /* twice the size: 16 x 16 a letter */
{
    for (; *s; s++, x += 16)
    {
        int ch = (uint8_t)*s;
        if (ch < 32 || ch > 127) ch = '?';
        const unsigned char *g = font8x8[ch - 32];
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

/* the change list */
#define LOG_N 6
static struct { uint32_t t; uint16_t bios; uint8_t drive; char what[12]; } log_[LOG_N];
static int log_n;

static void log_add(uint16_t bios, uint8_t drive, const char *what)
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
    for (; what[i] && i < 11; i++) log_[log_n].what[i] = what[i];
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

static void bcd_time(char *d, uint32_t l)   /* BCD mm ss ff 00 -> "mm:ss:ff" */
{
    char b[4];
    hexn(b, l >> 24, 2); d = cat(d, b); *d++ = ':';
    hexn(b, l >> 16, 2); d = cat(d, b); *d++ = ':';
    hexn(b, l >> 8, 2);  cat(d, b);
}

int main(void)
{
    static const char *const label[4] = { "OPEN", "CLOSE", "PLAY 2", "STOP" };
    char b[48], *p;
    int sel = 0;
    uint16_t prev = 0xFFFF, chord = 0;
    uint16_t bios = 0, last_bios = 0xFFFF;
    uint8_t drive = 0, last_drive = 0xFF;
    uint32_t drv32 = 0, pos = 0, disc_t = 0;
    uint16_t trackw = 0, led = 0;
    const char *said = "-";                 /* what the last button did */
    uint32_t said_t = 0;
    uint16_t said_reply = 0;
    static const char *const doing[4] = { "OPENING THE TRAY", "CLOSING THE TRAY", "PLAYING TRACK 2", "STOPPING MUSIC" };
    const char *banner = 0;                 /* the press's banner (3 s), else the drive's state */
    uint8_t banner_c = C_GREEN;
    int act_btn = -1, act_ok = 0;           /* the button that acted: green / red for half a second */

    cd_status(0x4E00);
    cd_genesis_display(0);
    MARS_VDP_DISPMODE = MARS_224_LINES | MARS_VDP_MODE_256;
    palette();
    for (int i = 0; i < 2; i++) { line_table(); clear(C_BLACK); flip(); }
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

        /* the status, every frame; the time and the lights, every 10th */
        if (cmd(CD_CMD_STATUS, 0))
        {
            bios = reply12;
            drv32 = reply8;
            drive = (uint8_t)(drv32 >> 24);
        }
        if ((gTicks % 10) == 0)
        {
            if (cmd(CD_CMD_STATUS, 1)) { pos = reply8; trackw = reply12; }
            if (cmd(CD_CMD_STATUS, 2)) { disc_t = reply8; led = reply12; }
        }
        if (bios != last_bios || drive != last_drive)
        {
            log_add(bios, drive, "");
            last_bios = bios; last_drive = drive;
        }

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
            log_add(bios, drive, label[sel]);
        }

        clear(C_BLACK);
        text(24, 6, "CD32X BOOTCAMP - " TRAYTEST_NAME, C_YELLOW);
        text(24, 18, "CART NOT USED - MODEL 1 HAS NO EJECT", C_GREY);   /* T3: runs with any cart in, or none */
        box(8, 29, 304, 1, C_BLUE);

        const int held = (pad & keys) != 0 && prev != 0xFFFF;   /* A (or C, START) is down now */
        const int flash = act_btn >= 0 && gTicks - said_t < 30;  /* half a second after a button acted */
        for (int i = 0; i < 4; i++)         /* the four buttons */
        {
            int x = 8 + i * 78, y = 36;
            uint8_t edge = (i == sel) ? C_YELLOW : C_GREY, fill = (i == sel) ? C_BLUE : C_DARK, ink = C_WHITE;
            if (i == sel && held)           /* pushed in: white, two pixels lower */
            {
                y += 2; edge = C_WHITE; fill = C_WHITE; ink = C_BLACK;
            }
            else if (i == act_btn && flash) /* it acted: green, or red when refused */
            {
                edge = C_WHITE; fill = act_ok ? C_GREEN : C_RED; ink = C_BLACK;
            }
            box(x, y, 72, 22, edge);
            box(x + 2, y + 2, 68, 18, fill);
            int len = len_of(label[i]);
            text_bg(x + ((72 - len * 8) / 2 & ~7), y + 7, label[i], ink, fill);
        }

        /* the banner: what the button is doing (3 s after a press), else the drive's state */
        {
            const char *msg = big_state(bios);
            uint8_t c = C_CYAN, bg = C_DARK;
            if (banner && gTicks - said_t < 180) { msg = banner; c = C_BLACK; bg = banner_c; }
            box(8, 62, 304, 20, bg);
            int n = len_of(msg); if (n > 19) n = 19;
            text2x(8 + ((304 - n * 16) / 2 & ~1), 64, msg, c, bg);
        }

        text(8, 86, "BIOS", C_GREY);
        hexn(b, bios, 4); text(56, 86, b, C_WHITE);
        text(104, 86, bios_name(bios), C_CYAN);
        if (bios & 0x8000) text(240, 86, "BUSY", C_RED);

        text(8, 96, "DRIVE", C_GREY);
        hexn(b, drv32 >> 16, 4); text(56, 96, b, C_WHITE);
        text(104, 96, drive_name(drive >> 0 & 15), C_CYAN);

        text(8, 106, "TRACKS", C_GREY);
        hexn(b, (drv32 >> 8) & 0xFF, 2); p = b + 2; *p++ = '-'; hexn(p, drv32 & 0xFF, 2);
        text(64, 106, b, C_WHITE);
        text(128, 106, "LIGHTS", C_GREY);
        hexn(b, led, 4); text(184, 106, b, C_WHITE);

        text(8, 116, "TRACK", C_GREY);
        hexn(b, trackw & 0xFF, 2); text(56, 116, b, C_WHITE);
        bcd_time(b, pos);    text(80, 116, b, C_WHITE);
        text(160, 116, "DISC", C_GREY);
        bcd_time(b, disc_t); text(200, 116, b, C_WHITE);

        text(8, 127, "LAST", C_GREY);
        p = cat(b, said);
        if (said_t) { p = cat(p, "  AT "); secs(p, said_t); }
        text(48, 127, b, C_GREEN);
        if (said_reply) { text(256, 127, "R", C_GREY); hexn(b, said_reply, 2); text(272, 127, b, C_WHITE); }

        box(8, 138, 304, 1, C_GREY);
        text(8, 142, "SECONDS  BIOS  DRIVE", C_GREY);
        for (int i = 0; i < log_n; i++)     /* every change of the two codes, and every button */
        {
            int y = 152 + i * 9;
            secs(b, log_[i].t);             text(8, y, b, C_WHITE);
            hexn(b, log_[i].bios, 4);       text(80, y, b, C_WHITE);
            hexn(b, log_[i].drive, 2);      text(128, y, b, C_WHITE);
            if (log_[i].what[0])            text(160, y, log_[i].what, C_YELLOW);
            else                            text(160, y, drive_name(log_[i].drive & 15), C_CYAN);
        }
        text(8, 206, "LEFT/RIGHT CHOOSE   A PRESS", C_GREEN);
        if (cd_error.code)
        {
            hexn(b, cd_error.code, 4);
            text(248, 206, b, C_RED);
        }
        flip();
    }
    return 0;
}
