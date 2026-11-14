/* cfxgen.c - cfxtest's data: the test pictures the engine reads, the script of command words the master SH2
 * sends, and what the engine must give for each check, made by the reference model (../../model/cartfx.c)
 * running the same script over the same data.
 *
 *   cc -O2 -o cfxgen cfxgen.c ../../model/cartfx.c && ./cfxgen cfxtest_data.bin
 *
 * The file is the ROM from 0x040000 to 0x1FFFFF (the program's 256 KB come before it):
 *   0x040000  the script: 16-bit words. 0x0000-0x7FFF a word for the engine (a read of cart 0x3F0000 + 2w);
 *             0x8001 wait until not busy (the GO's time measured); 0x81bb wait until buffer bb is shown;
 *             0x82kk check kk; 0x8300 a new pass starts here (the stress loop goes back to it); 0xFFFF the end
 *   0x080000  the pictures (none of any game's: patterns made here): an 8-bit bitmap 512 x 256, a 4-bit one,
 *             8-bit and 4-bit tiles, a map in PSRAM, a map to UPLOAD, sprites, broken bits to UPLOAD, the shade
 *             table, the blend table. Every map entry names a tile inside these 64 KB, and every SRAM word the
 *             engine reads is written by the script first: the engine reads nothing the model does not know
 *   0x100000  check kk at 0x100000 + kk x 0x10000: a 32-byte head (below), the picture (PW x PH bytes), then the
 *             cell map's 48 words
 * The head (big-endian, as the SH2 reads it): u16 'CK', cols, rows, the buffer shown, the status's error bit,
 * counter 2 (cells), counter 3 (sprites), the frame's number, u32 the model's estimate (clocks), u32 the
 * picture's sum (16-bit words added), 8 bytes 0.
 *
 * No game data: every byte here is a pattern. CD32X BootCamp cartfx/test, 2026 (the BootCamp's licence).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../model/cartfx.h"

#define ROM_SIZE   0x200000u
#define SCRIPT     0x040000u
#define BMP8       0x080000u        /* 512 x 256, 8 bits */
#define BMP4       0x0A0000u        /* 512 x 256, 4 bits */
#define TILES8     0x0B0000u        /* 64 KB of 8-bit tiles (t x t bytes each) */
#define TILES4     0x0C0000u        /* 64 KB of 4-bit tiles */
#define MAPP       0x0D0000u        /* a map in PSRAM: 128 x 64 entries */
#define MAPU       0x0D8000u        /* a map to UPLOAD: 256 x 64 entries */
#define SPRITES    0x0E0000u        /* sprite pictures */
#define BRKSRC     0x0EE000u        /* broken bits to UPLOAD: 2,048 words, few bits set */
#define SHADE      0x0EF000u        /* 16 x 256 */
#define BLEND      0x0F0000u        /* 256 x 256 */
#define EXPECT     0x100000u

static uint8_t rom[ROM_SIZE];
static uint16_t sram[0x40000];
static uint32_t rs = 12345;
static uint32_t rnd(void) { rs = rs * 1103515245u + 12345u; return rs >> 8; }

static uint8_t ps(void* ctx, uint32_t a) { (void)ctx; return a < ROM_SIZE ? rom[a] : 0xFF; }
static void put16(uint32_t a, uint16_t v) { rom[a] = (uint8_t)(v >> 8); rom[a + 1] = (uint8_t)v; }
static void put32(uint32_t a, uint32_t v) { put16(a, (uint16_t)(v >> 16)); put16(a + 2, (uint16_t)v); }

/* ---- the script, run through the model as it is written */
static cfx_t fx;
static uint32_t sp = SCRIPT;
static int nchecks, frame;
static void word(uint16_t w) {
    if (sp >= BMP8 - 2) { fprintf(stderr, "the script is too long\n"); exit(1); }
    put16(sp, w); sp += 2;
    if (w < 0x8000) { uint16_t d; cfx_read(&fx, CFX_CMD_BASE + 2u * w, &d); }
}
static void arg(uint32_t v) { word(CFX_ARG(v)); }
static void a24(uint32_t a) { word(CFX_HI(a)); word(CFX_LO(a)); }
static void layer(int l, unsigned fl, uint32_t pix, int w, int h, int off, int brk, int tile, uint32_t map, int x, int y) {
    word(CFX_WORD(CFX_OP_LAYER, l | fl)); a24(pix); arg(w); arg(h); arg((off & 255) | (brk ? 0x100 : 0)); arg(tile);
    a24(map); arg(x & 0x1FFF); arg(y & 0x1FFF);
}
static void layer_off(int l) { layer(l, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); }
static void sprite(int fl, int x, int y, int w, int h, int stride, uint32_t src, int off) {
    word(CFX_WORD(CFX_OP_SPRITE, fl)); arg(x & 0x1FFF); arg(y & 0x1FFF); arg(w); arg(h); arg(stride); a24(src); arg(off & 255);
}
static void setup(int cols, int rows, uint32_t brk) {
    word(CFX_WORD(CFX_OP_SETUP, 0)); arg(cols); arg(rows); a24(BLEND); word(CFX_HI(brk)); word(CFX_LO(brk));
}
static void poke(uint32_t a, uint16_t v) { word(CFX_WORD(CFX_OP_POKE, 0)); word(CFX_HI(a)); word(CFX_LO(a)); arg(v >> 8); arg(v & 255); }
static void upload(uint32_t src, uint32_t dst, int n) { word(CFX_WORD(CFX_OP_UPLOAD, 0)); a24(src); word(CFX_HI(dst)); word(CFX_LO(dst)); arg(n); }
static void cells(int row, uint32_t m) { word(CFX_WORD(CFX_OP_CELLS, row)); arg(m & 0x1FFF); arg((m >> 13) & 0xFF); }

/* GO, wait, SHOW the buffer composed, wait for it, check it */
static void go_check(void) {
    int b = !fx.shown;
    word(CFX_WORD(CFX_OP_GO, 0));
    word(0x8001);
    word(CFX_WORD(CFX_OP_SHOW, b));
    word((uint16_t)(0x8100 | b));
    int k = nchecks++;
    if (k >= 16) { fprintf(stderr, "more than 16 checks\n"); exit(1); }
    word((uint16_t)(0x8200 | k));
    uint32_t e = EXPECT + 0x10000u * (uint32_t)k, pw = 16u * (uint32_t)fx.cols, ph = 8u * (uint32_t)fx.rows, sum = 0;
    put16(e, 0x434B); put16(e + 2, (uint16_t)fx.cols); put16(e + 4, (uint16_t)fx.rows); put16(e + 6, (uint16_t)fx.shown);
    put16(e + 8, (uint16_t)fx.err); put16(e + 10, fx.cnt[2]); put16(e + 12, fx.cnt[3]); put16(e + 14, (uint16_t)frame++);
    put32(e + 16, fx.est);
    for (uint32_t i = 0; i < pw * ph / 2; i++) {
        uint16_t v = sram[(uint32_t)fx.shown * 0x8000u + i];
        put16(e + 32 + 2 * i, v); sum += v;
    }
    put32(e + 20, sum);
    for (int w = 0; w < 48; w++) put16(e + 32 + pw * ph + 2 * (uint32_t)w, sram[CFX_MAPWORD(fx.shown, w)]);
    printf("check %2d: %2d x %2d, buffer %d, cells %3u, sprites %3u, error %d, estimate %.2f ms\n", k, fx.cols, fx.rows,
           fx.shown, fx.cnt[2], fx.cnt[3], fx.err, fx.est * 20e-6);
}

/* ---- the pictures */
static void pictures(void) {
    for (int y = 0; y < 256; y++) for (int x = 0; x < 512; x++) {          /* bands, a grid, holes (0s) */
        uint8_t v = (uint8_t)(((x >> 3) + (y >> 2) * 3) & 0xFF);
        if (((x ^ y) & 31) == 0) v = 0;
        if ((x & 63) == 0 || (y & 31) == 0) v = 250;
        rom[BMP8 + (uint32_t)y * 512 + (uint32_t)x] = v;
    }
    for (uint32_t i = 0; i < 512 * 256 / 2; i++) {                         /* 4 bits: diagonal ramps */
        uint32_t x = (i * 2) % 512, y = (i * 2) / 512;
        uint8_t a = (uint8_t)((x + y) / 8 & 15), b = (uint8_t)((x + 1 + y) / 8 & 15);
        if (((x + y) & 15) == 7) a = 0;
        rom[BMP4 + i] = (uint8_t)(a << 4 | b);
    }
    for (uint32_t i = 0; i < 0x10000; i++) {                                /* tiles: any size reads these bytes */
        uint8_t v = (uint8_t)(16 + (i * 7 + (i >> 8) * 13) % 224);
        if ((i % 37) == 0) v = 0;
        rom[TILES8 + i] = v;
        rom[TILES4 + i] = (uint8_t)((i * 3 + (i >> 7)) & 0xFF);
    }
    for (uint32_t c = 0; c < 128 * 64; c++) {                               /* entries: tile, h-flip, light */
        uint16_t e = (uint16_t)(rnd() % 150);
        if (rnd() % 7 == 0) e = 0;
        e |= (uint16_t)((rnd() & 15) << 12);
        put16(MAPP + 2 * c, e);
    }
    for (uint32_t c = 0; c < 256 * 64; c++) {
        uint16_t e = (uint16_t)(rnd() % 60);
        if (rnd() % 5 == 0) e = 0;
        if (rnd() & 1) e |= 0x1000;
        put16(MAPU + 2 * c, e);
    }
    for (uint32_t i = 0; i < 2048; i++) put16(BRKSRC + 2 * i, (uint16_t)(rnd() % 3 == 0 ? 1u << (rnd() % 16) : 0));
    for (uint32_t i = 0; i < 0xE000; i++) {                                 /* sprites: rings and blocks */
        uint32_t x = i % 64, y = (i / 64) % 64;
        int dx = (int)x - 32, dy = (int)y - 32, r = dx * dx + dy * dy;
        uint8_t v = (uint8_t)(r < 900 && r > 300 ? 32 + (x + y) % 200 : (x % 16 < 2 ? 100 : 0));
        rom[SPRITES + i] = v;
    }
    for (int l = 0; l < 16; l++) for (int c = 0; c < 256; c++)
        rom[SHADE + (uint32_t)l * 256 + (uint32_t)c] = (uint8_t)(c * (16 - l) / 16);
    for (int v = 0; v < 256; v++) for (int d = 0; d < 256; d++)
        rom[BLEND + (uint32_t)v * 256 + (uint32_t)d] = (uint8_t)((v + d) / 2 ^ (v & 0x80));
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: cfxgen <out.bin>\n"); return 1; }
    memset(rom, 0xFF, sizeof rom);
    for (uint32_t i = 0; i < 0x40000; i++) sram[i] = (uint16_t)(i * 0x9E37u + 0x5A5Au);   /* (never compared before it is written) */
    pictures();
    fx.psram = ps; fx.sram = sram; fx.ctx = 0;
    cfx_reset(&fx, CFX_KEY1_HUR, CFX_ID_HUR);
    fx.on = 1;                                           /* (the key is the program's; the words start here) */

    word(0x8300);                                        /* a pass */
    word(CFX_WORD(CFX_OP_SHOW, 0));
    word(0x8100);
    word(CFX_WORD(CFX_OP_LUT, 0)); a24(SHADE);
    const uint32_t BRK = 0x30000, MS1 = 0x10000, MS2 = 0x14000;

    /* 0, 1: whole frames into both buffers, 21 x 24 (after these every byte of both pictures is defined) */
    setup(21, 24, BRK);
    for (int l = 0; l < 4; l++) layer_off(l);
    layer(0, CFX_L_ON | CFX_L_OPAQUE, BMP8, 512, 256, 0, 0, 0, 0, 5, 3);
    word(CFX_WORD(CFX_OP_BEGIN, 1));
    sprite(0, 40, 30, 64, 64, 64, SPRITES, 0); sprite(1, 300, 150, 64, 64, 64, SPRITES, 9); sprite(2, -20, -10, 64, 64, 64, SPRITES, 77);
    go_check();
    word(CFX_WORD(CFX_OP_SCROLL, 0)); arg(37); arg(100);
    word(CFX_WORD(CFX_OP_BEGIN, 1));
    sprite(3, 100, 100, 64, 64, 64, SPRITES, 3); sprite(4, 200, 20, 64, 64, 64, SPRITES, 0);
    go_check();

    /* 2: Hurrican's way: 20 x 22, a 4-bit bitmap, an 8-bit one, a lit tile map of 20 with broken bits and h-flips
          (map in PSRAM), an over tile map of 16 at 4 bits; sprites, some blended */
    setup(20, 22, BRK);
    upload(BRKSRC, BRK, 2048);
    for (int k = 0; k < 64; k++) poke(BRK + (uint32_t)k, (uint16_t)(rnd() & rnd() & 0xFFFF));
    layer(0, CFX_L_ON | CFX_L_OPAQUE | CFX_L_4BPP, BMP4, 512, 256, 16, 0, 0, 0, 1000, 77);
    layer(1, CFX_L_ON, BMP8, 512, 256, 0, 0, 0, 0, 3, 250);
    layer(2, CFX_L_ON | CFX_L_TILES | CFX_L_SHADE | CFX_L_HFLIP, TILES8, 128, 64, 0, 1, 20, MAPP, 333, 51);
    layer(3, CFX_L_ON | CFX_L_TILES | CFX_L_OVER | CFX_L_OPAQUE | CFX_L_4BPP, TILES4, 128, 64, 32, 0, 16, MAPP, 7, 900);
    word(CFX_WORD(CFX_OP_BEGIN, 1));
    for (int k = 0; k < 24; k++)
        sprite((int)(rnd() % 4) | (k % 3 == 0 ? 4 : 0), (int)(rnd() % 360) - 20, (int)(rnd() % 210) - 20, 8 + (int)(rnd() % 57),
               8 + (int)(rnd() % 57), 64, SPRITES + (rnd() % 8) * 64, k % 5 ? 0 : (int)(rnd() & 255));
    go_check();

    /* 3: Jazz's way: 21 x 24, maps UPLOADed to the SRAM and POKEd, a sky (a bitmap 4 wide), tiles of 32 under and
          over, part of the frame (CELLS), sprites */
    setup(21, 24, BRK);
    for (uint32_t q = 0; q < 4; q++) {                  /* (UPLOAD takes at most 8,191 words) */
        upload(MAPU + q * 8192, MS1 + q * 4096, 4096);
        upload(MAPU + ((q + 2) & 3) * 8192, MS2 + q * 4096, 4096);
    }
    for (int k = 0; k < 12; k++) poke(MS1 + rnd() % 4096, (uint16_t)(rnd() % 3 ? 0 : 1 + rnd() % 60));
    layer(0, CFX_L_ON | CFX_L_OPAQUE, BMP8 + 40 * 512, 4, 192, 0, 0, 0, 0, 0, 0);
    layer(1, CFX_L_ON | CFX_L_TILES | CFX_L_MAPSRAM | CFX_L_HFLIP, TILES8, 256, 64, 0, 0, 32, MS1, 4000, 1000);
    layer(2, CFX_L_ON | CFX_L_TILES | CFX_L_MAPSRAM | CFX_L_OVER, TILES8, 256, 64, 7, 0, 32, MS2, 4100, 900);
    layer_off(3);
    word(CFX_WORD(CFX_OP_BEGIN, 0));
    for (int r = 0; r < 24; r++) cells(r, rnd() & rnd() & 0x1FFFFF);
    for (int k = 0; k < 10; k++)
        sprite((int)(rnd() % 4), (int)(rnd() % 360) - 20, (int)(rnd() % 210) - 20, 16 + (int)(rnd() % 48), 16 + (int)(rnd() % 48), 64,
               SPRITES + (rnd() % 8) * 64, 0);
    go_check();

    /* 4: SuperTux's way: 21 x 23, a picture that wraps, tiles of 16 over everything (map in SRAM), a snow of small
          sprites, part of the frame */
    setup(21, 23, BRK);
    layer_off(0); layer_off(2);
    layer(1, CFX_L_ON | CFX_L_OPAQUE, BMP8, 320, 240, 192, 0, 0, 0, 7000, 7000);
    layer(3, CFX_L_ON | CFX_L_TILES | CFX_L_MAPSRAM | CFX_L_OVER, TILES8, 256, 64, 0, 0, 16, MS1, 2000, 50);
    word(CFX_WORD(CFX_OP_BEGIN, 0));
    for (int r = 0; r < 23; r += 2) cells(r, 0x1FFFFF);
    for (int k = 0; k < 80; k++) {
        int small = k % 4 != 0;
        sprite((int)(rnd() % 4), (int)(rnd() % 340) - 10, (int)(rnd() % 190) - 6, small ? 1 + (int)(rnd() % 4) : 10 + (int)(rnd() % 40),
               small ? 1 + (int)(rnd() % 4) : 10 + (int)(rnd() % 40), small ? 4 : 64, small ? SPRITES + 64 * 33 + 30 : SPRITES, 0);
    }
    go_check();

    /* 5: the edges: a sprite wider than the picture, flipped both ways, off every side; 257 sprites (the 257th is
          dropped and sets the error bit); a sprite 0 wide (nothing) */
    setup(21, 24, BRK);
    layer(0, CFX_L_ON | CFX_L_OPAQUE | CFX_L_4BPP, BMP4, 512, 256, 100, 0, 0, 0, 4095, 4095);
    layer_off(1); layer_off(3);
    word(CFX_WORD(CFX_OP_BEGIN, 1));
    sprite(3, -100, 50, 511, 40, 512, BMP8, 0);
    sprite(0, 330, 185, 64, 64, 64, SPRITES, 0); sprite(1, -60, -60, 64, 64, 64, SPRITES, 0);
    sprite(0, 10, 10, 0, 20, 64, SPRITES, 0);
    for (int k = 0; k < 253; k++) sprite(k & 3, (k * 13) % 330, (k * 7) % 190, 3, 3, 64, SPRITES + 64 * 33 + 30, k);
    sprite(0, 160, 96, 30, 30, 64, SPRITES, 0);         /* the 257th */
    go_check();

    /* 6: after the error: tiles of 13 at 4 bits, h-flipped, opaque; tiles of 2; a bitmap 6 x 5 wrapping */
    setup(17, 19, BRK);
    word(CFX_WORD(CFX_OP_BEGIN, 1));
    layer(0, CFX_L_ON | CFX_L_OPAQUE | CFX_L_TILES | CFX_L_4BPP | CFX_L_HFLIP, TILES4, 128, 64, 0, 0, 13, MAPP, 9, 4);
    layer(1, CFX_L_ON | CFX_L_TILES, TILES8, 128, 64, 50, 1, 2, MAPP, 1, 1);
    layer(2, CFX_L_ON, BMP8 + 1000, 6, 5, 0, 0, 0, 0, 2, 3);
    sprite(4, 50, 50, 64, 64, 64, SPRITES, 0); sprite(5, 120, 70, 64, 64, 64, SPRITES, 0);
    go_check();

    /* 7: part of a frame again, both buffers having been whole: only the marked cells change */
    setup(21, 24, BRK);
    layer(0, CFX_L_ON | CFX_L_OPAQUE, BMP8, 512, 256, 0, 0, 0, 0, 77, 77);
    layer_off(1); layer_off(2); layer_off(3);
    word(CFX_WORD(CFX_OP_BEGIN, 0));
    cells(0, 0x1FFFFF); cells(23, 0x1FFFFF); cells(12, 0x00F0F0);
    sprite(0, 150, 90, 64, 64, 64, SPRITES, 0);
    go_check();

    word(0xFFFF);
    FILE* f = fopen(argv[1], "wb");
    if (!f || fwrite(rom + SCRIPT, 1, ROM_SIZE - SCRIPT, f) != ROM_SIZE - SCRIPT) { perror(argv[1]); return 1; }
    fclose(f);
    printf("%d checks, the script %u words; %s: ROM 0x%06X-0x%06X\n", nchecks, (sp - SCRIPT) / 2, argv[1], SCRIPT, ROM_SIZE - 1);
    return 0;
}
