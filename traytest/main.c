/* traytest - open and close the Sega CD's tray from buttons on the screen (CD32X BootCamp)
 *
 * For the Sega CD Model 1 above all: its tray is motorised and it has no eject button, so only
 * software opens it.  The game runs from the RAM cart, so the disc can come out while it runs.
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
 * Commands 84 and 90 are merged into a copy of the stack by traytest/merge_tray.py (TRAYTEST=1);
 * ledger section 22.  Everything else is hello's (cdapi.c, crt0.s, the 68000 side).
 */
#include <stdint.h>
#include "32x.h"
#include "cdapi.h"
#include "font8x8.h"

#define TRAYTEST_NAME     "TRAY TEST T1"
#define CD_CMD_STATUS     84          /* traytest/merge_tray.py */
#define CD_CMD_TRAY       90

#define COMM8_32   (*(volatile uint32_t *)0x20004028)
#define COMM12_16  (*(volatile uint16_t *)0x2000402C)

/* ---- what crt0.s needs from the program ---- */
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
#define LOG_N 7
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
            int busy = (bios & 0x8000) != 0;
            switch (sel)
            {
            case 0: said = cmd(CD_CMD_TRAY, 1) ? "OPEN ASKED" : "NO ANSWER";   said_reply = reply12; break;
            case 1: said = cmd(CD_CMD_TRAY, 0) ? "CLOSE ASKED" : "NO ANSWER";  said_reply = reply12; break;
            case 2:                         /* 45 and 54 wait for a BIOS that is not busy (stack, no time limit) */
                if (busy) { said = "BUSY - WAIT"; break; }
                cd_music_play(2, 0); said = "PLAY ASKED"; said_reply = 0; break;
            default:
                if (busy) { said = "BUSY - WAIT"; break; }
                cd_music_stop(); said = "STOP ASKED"; said_reply = 0; break;
            }
            said_t = gTicks;
            log_add(bios, drive, label[sel]);
        }

        clear(C_BLACK);
        text(24, 6, "CD32X BOOTCAMP - " TRAYTEST_NAME, C_YELLOW);
        text(24, 18, "MODEL 1: NO EJECT BUTTON", C_GREY);
        box(8, 29, 304, 1, C_BLUE);

        for (int i = 0; i < 4; i++)         /* the four buttons */
        {
            int x = 8 + i * 78, y = 36;
            uint8_t edge = (i == sel) ? C_YELLOW : C_GREY, fill = (i == sel) ? C_BLUE : C_DARK;
            box(x, y, 72, 22, edge);
            box(x + 2, y + 2, 68, 18, fill);
            int len = 0; while (label[i][len]) len++;
            text_bg(x + ((72 - len * 8) / 2 & ~7), y + 7, label[i], C_WHITE, fill);
        }

        text(8, 66, "BIOS", C_GREY);
        hexn(b, bios, 4); text(56, 66, b, C_WHITE);
        text(104, 66, bios_name(bios), C_CYAN);
        if (bios & 0x8000) text(240, 66, "BUSY", C_RED);

        text(8, 78, "DRIVE", C_GREY);
        hexn(b, drv32 >> 16, 4); text(56, 78, b, C_WHITE);
        text(104, 78, drive_name(drive >> 0 & 15), C_CYAN);

        text(8, 90, "TRACKS", C_GREY);
        hexn(b, (drv32 >> 8) & 0xFF, 2); p = b + 2; *p++ = '-'; hexn(p, drv32 & 0xFF, 2);
        text(64, 90, b, C_WHITE);
        text(128, 90, "LIGHTS", C_GREY);
        hexn(b, led, 4); text(184, 90, b, C_WHITE);

        text(8, 102, "TRACK", C_GREY);
        hexn(b, trackw & 0xFF, 2); text(56, 102, b, C_WHITE);
        bcd_time(b, pos);    text(80, 102, b, C_WHITE);
        text(160, 102, "DISC", C_GREY);
        bcd_time(b, disc_t); text(200, 102, b, C_WHITE);

        text(8, 116, "LAST", C_GREY);
        p = cat(b, said);
        if (said_t) { p = cat(p, "  AT "); secs(p, said_t); }
        text(48, 116, b, C_GREEN);
        if (said_reply) { text(256, 116, "R", C_GREY); hexn(b, said_reply, 2); text(272, 116, b, C_WHITE); }

        box(8, 127, 304, 1, C_GREY);
        text(8, 131, "SECONDS  BIOS  DRIVE", C_GREY);
        for (int i = 0; i < log_n; i++)     /* every change of the two codes, and every button */
        {
            int y = 141 + i * 9;
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
