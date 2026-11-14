// tb_cfx.cpp - cfx_core (cartfx/rtl) against the reference model (cartfx/model/cartfx.c), in Verilator
//
// A cart (two PSRAMs of 8 MB), an SRAM, and a console that reads the cart as the SH2s do: the key, then frames of
// commands through the command port in each game's way (Hurrican: full frames of parallax bitmaps, lit tile maps with
// broken walls, an over layer, blended sprites; Jazz: a sky, tile maps in SRAM with POKEd blocks, marked cells;
// SuperTux: a wrapping picture, an over tile map, a snow of small sprites), mixed with reads of the PSRAM and of the
// shown picture the engine must give way to. After each frame: the SRAM compared word for word with the model's,
// the counters, and part of the picture read back through the window. Exit 0 when nothing differs.
//
//   cd cartfx/sim && make && ./obj/Vcfx_core 12
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "Vcfx_core.h"
#include "verilated.h"
#ifdef DBGPUB
#include "Vcfx_core___024root.h"
#endif
#include "cartfx.h"   // (cartfx.c is built as C++ here)

static std::vector<uint16_t> psr[2], sram, msram;
static Vcfx_core* top;
static uint64_t cycles;
static uint32_t rs = 1;
static uint32_t rnd() { rs = rs * 1103515245u + 12345u; return rs >> 8; }
static int traffic = 30;
#ifdef DBGPUB
static uint64_t hist[128];
#endif                               // % of console reads between commands

static uint8_t ps_byte(void*, uint32_t a) {            // the PSRAM as the engine sees it: 24-bit byte address
    a &= 0xFFFFFF;
    uint16_t w = psr[a >> 23][(a & 0x7FFFFF) >> 1];
    return (a & 1) ? (uint8_t)w : (uint8_t)(w >> 8);
}
static void ps_put(uint32_t a, uint8_t v) {
    a &= 0xFFFFFF;
    uint16_t& w = psr[a >> 23][(a & 0x7FFFFF) >> 1];
    w = (a & 1) ? (uint16_t)((w & 0xFF00) | v) : (uint16_t)((w & 0x00FF) | v << 8);
}

// the memories, as the pins see them: the console has them while it uses them (map_cfx.sv's multiplexers)
static uint32_t con_a;
static void eval_mem() {
    top->eval();
    if (top->con_psram) {                              // the console reads the PSRAM (its own address): garbage for the engine
        uint32_t off = (con_a & 0x3FFFFF) >> 1;
        top->rom0_do = psr[0][off]; top->rom1_do = psr[1][off ^ 0x1234];
    } else if (top->e_rom_oe) {
        uint32_t off = (top->e_rom_addr >> 1) & 0x3FFFFF;
        uint16_t v = psr[top->e_rom1][off];
        top->rom0_do = top->e_rom1 ? 0xDEAD : v; top->rom1_do = top->e_rom1 ? v : 0xBEEF;
    } else { top->rom0_do = rnd(); top->rom1_do = rnd(); }
    if (top->con_sram) top->sram_do = sram[(top->shown ? 0x8000 : 0) + (((con_a - 0x3E0000) >> 1) & 0x7FFF)];
    else if (top->e_sram_oe) top->sram_do = sram[(top->e_sram_addr >> 1) & 0x3FFFF];
    else top->sram_do = rnd();
    if (!top->con_sram && (top->e_sram_we_hi || top->e_sram_we_lo)) {
        uint32_t w = (top->e_sram_addr >> 1) & 0x3FFFF;
        uint16_t v = sram[w];
        if (top->e_sram_we_hi) v = (v & 0x00FF) | (top->e_sram_di & 0xFF00);
        if (top->e_sram_we_lo) v = (v & 0xFF00) | (top->e_sram_di & 0x00FF);
        sram[w] = v;
    }
    top->eval();
}
static void tick() {
    top->clk = 0; eval_mem(); top->clk = 1; eval_mem(); cycles++;
#ifdef DBGPUB
    static int nr; static int last = -1;
    if (getenv("DBGRUN") && top->rootp->cfx_core__DOT__st == 2 && last != 2 && nr++ < atoi(getenv("DBGRUN")))
        printf("  run op %d arg %03X at %llu (q_wr %d q_rd %d)\n", top->rootp->cfx_core__DOT__c_op, top->rootp->cfx_core__DOT__c_arg, (unsigned long long)cycles, top->rootp->cfx_core__DOT__q_wr, top->rootp->cfx_core__DOT__q_rd);
    if (getenv("DBGFROM") && cycles >= (uint64_t)atoll(getenv("DBGFROM")) && cycles < (uint64_t)atoll(getenv("DBGFROM")) + 400 && top->rootp->cfx_core__DOT__st != last)
        printf("    %llu st %d exec %d busy-q %d mq s_empty %d s_busy %d p_busy %d\n", (unsigned long long)cycles, top->rootp->cfx_core__DOT__st, top->rootp->cfx_core__DOT__exec, (top->rootp->cfx_core__DOT__q_wr - top->rootp->cfx_core__DOT__q_rd) & 2047, top->rootp->cfx_core__DOT__sq_wr == top->rootp->cfx_core__DOT__sq_rd, top->rootp->cfx_core__DOT__s_busy, top->rootp->cfx_core__DOT__p_busy);
    last = top->rootp->cfx_core__DOT__st;
    if (getenv("DBGHIST")) hist[top->rootp->cfx_core__DOT__st & 127]++;
#endif
}
static uint16_t bus_read(uint32_t a) {                 // one 16-bit cart read: ~300-400 ns active, then a gap
    con_a = a;
    top->a = (a >> 1) & 0x1FFFFF; top->ce_lo_n = 0; top->oe_n = 0;
    int n = 14 + rnd() % 6;
    uint16_t v = 0;
    for (int i = 0; i < n; i++) tick();
    if (top->area) v = (a >> 16 == 0x3F) ? top->status : sram[(top->shown ? 0x8000 : 0) + (((a - 0x3E0000) >> 1) & 0x7FFF)];
    else v = psr[0][(a & 0x3FFFFF) >> 1];
    top->ce_lo_n = 1; top->oe_n = 1;
    int g = 2 + rnd() % 10;
    for (int i = 0; i < g; i++) tick();
    return v;
}
static void noise() {                                  // the SH2's own reads: the cart, now and then the picture
    while ((int)(rnd() % 100) < traffic) {
        if (rnd() % 4 == 0) bus_read(0x3E0000 + (rnd() % 0x10000 & ~1u));
        else bus_read(rnd() % 0x3E0000 & ~1u);
    }
}

static cfx_t model;
static long bad;
static void send(uint16_t w) {                         // a command word: to the engine and the model
    uint16_t d; cfx_read(&model, CFX_CMD_BASE + 2u * w, &d);
    uint16_t st = bus_read(CFX_CMD_BASE + 2u * w);
#ifdef DBGPUB
    static int nn; if (getenv("DBGW") && nn++ < atoi(getenv("DBGW"))) printf("sent %04X st %04X q_wr %d q_rd %d state %d cmd_w %04X taken %d\n", w, st, top->rootp->cfx_core__DOT__q_wr, top->rootp->cfx_core__DOT__q_rd, top->rootp->cfx_core__DOT__st, top->rootp->cfx_core__DOT__cmd_w, top->rootp->cfx_core__DOT__taken);
#endif
    while ((st & CFX_ST_FULL) && top->on) st = bus_read(CFX_CMD_BASE);   // (OFF: off within its own read)
    noise();
}
static uint16_t wait_idle() {
    uint16_t st;
    long n = 0;
    while ((st = bus_read(CFX_CMD_BASE)) & CFX_ST_BUSY) { noise(); if (++n > 200000) { printf("never idle (status %04X)\n", st); exit(2); } }
    return st;
}
static void a24(uint32_t a) { send(CFX_HI(a)); send(CFX_LO(a)); }

static void compare(const char* what, int fr) {
    long d = 0;
    for (uint32_t w = 0; w < 0x40000; w++) if (sram[w] != msram[w]) {
        if (d < 6) printf("%s frame %d: SRAM word %05X engine %04X model %04X\n", what, fr, w, sram[w], msram[w]);
        d++;
    }
    bad += d;
}

// ---- the scenes
static void layer(int l, unsigned fl, uint32_t pix, int w, int h, int off, int brk, int tile, uint32_t map, int x, int y) {
    send(CFX_WORD(CFX_OP_LAYER, l | fl)); a24(pix); send(CFX_ARG(w)); send(CFX_ARG(h));
    send(CFX_ARG(off | brk << 8)); send(CFX_ARG(tile)); a24(map); send(CFX_ARG(x)); send(CFX_ARG(y));
}
static void sprite(int fl, int x, int y, int w, int h, int stride, uint32_t src, int off) {
    send(CFX_WORD(CFX_OP_SPRITE, fl)); send(CFX_ARG(x & 0x1FFF)); send(CFX_ARG(y & 0x1FFF)); send(CFX_ARG(w));
    send(CFX_ARG(h)); send(CFX_ARG(stride)); a24(src); send(CFX_ARG(off));
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    setvbuf(stdout, nullptr, _IONBF, 0);
    int frames = argc > 1 ? atoi(argv[1]) : 6;
    if (getenv("TRAFFIC")) traffic = atoi(getenv("TRAFFIC"));
    if (getenv("SEED")) rs = (uint32_t)atoi(getenv("SEED"));
    psr[0].assign(1 << 22, 0); psr[1].assign(1 << 22, 0); sram.assign(1 << 18, 0); msram.assign(1 << 18, 0);
    for (auto& v : psr[0]) v = rnd();
    for (auto& v : psr[1]) v = rnd();
    for (uint32_t i = 0; i < sram.size(); i++) sram[i] = msram[i] = rnd();

    // graphics in the second chip (banks 16-27): pixels with plenty of 0s (nothing there) and every colour
    const uint32_t G = 0x800000;
    for (uint32_t a = G; a < G + 0x300000; a++) { int k = rnd() % 8; ps_put(a, k < 2 ? 0 : rnd() & 255); }
    const uint32_t BLEND = 0x900000;                   // 64 KB of any bytes
    const uint32_t SHADE = 0x910000;                   // 4 KB
    const uint32_t MAPS = 0x920000;                    // tile maps in PSRAM: entries with tile 0 often, flips, lights
    for (uint32_t i = 0; i < 0x20000; i += 2) {
        uint16_t e = rnd() % 5 == 0 ? 0 : (uint16_t)(rnd() % 300) | (uint16_t)(rnd() & 0xF000);
        ps_put(MAPS + i, e >> 8); ps_put(MAPS + i + 1, e & 255);
    }

    top = new Vcfx_core;
    top->ce_lo_n = 1; top->oe_n = 1; top->we_lo_n = 1; top->we_hi_n = 1; top->a = 0;
    top->rst = 1; for (int i = 0; i < 5; i++) tick(); top->rst = 0;
    model.psram = ps_byte; model.sram = msram.data(); model.ctx = nullptr;
    const uint16_t KEY1 = (uint16_t)(getenv("KEY1") ? strtol(getenv("KEY1"), 0, 16) : CFX_KEY1_HUR);
    const uint8_t ID = (uint8_t)(getenv("ID") ? strtol(getenv("ID"), 0, 16) : CFX_ID_HUR);
    cfx_reset(&model, KEY1, ID);

    // a wrong key first (nothing must turn on), then the right one
    bus_read(CFX_CMD_BASE + 2 * CFX_KEY0); bus_read(CFX_CMD_BASE + 2 * 0x5241); bus_read(CFX_CMD_BASE + 2 * CFX_KEY2);
    if (top->on) { printf("a wrong key turned it on\n"); return 1; }
    uint16_t d;
    cfx_read(&model, CFX_CMD_BASE + 2 * CFX_KEY0, &d); cfx_read(&model, CFX_CMD_BASE + 2 * KEY1, &d);
    cfx_read(&model, CFX_CMD_BASE + 2 * CFX_KEY2, &d);
    bus_read(CFX_CMD_BASE + 2 * CFX_KEY0); bus_read(CFX_CMD_BASE + 2 * KEY1);
    uint16_t st = bus_read(CFX_CMD_BASE + 2 * CFX_KEY2);
    if ((st >> 8) != ID || !top->on) { printf("the key did not turn it on (status %04X)\n", st); return 1; }
    if (bus_read(CFX_CMD_BASE + 2 * 8) != CFX_KEY2) { printf("counter 8 is not the version\n"); return 1; }

    printf("the key: on\n");
    // the shade table
    send(CFX_WORD(CFX_OP_LUT, 0)); a24(SHADE);
    long cells = 0;
    for (int fr = 0; fr < frames; fr++) {
        int game = fr % 3;                             // 0 Hurrican, 1 Jazz, 2 SuperTux
        int cols = game == 0 ? 20 : 21, rows = game == 0 ? 22 : 23 + rnd() % 2;
        uint32_t brkw = 0x30000 + rnd() % 0x1000;
        send(CFX_WORD(CFX_OP_SETUP, 0)); send(CFX_ARG(cols)); send(CFX_ARG(rows)); a24(BLEND);
        send(CFX_HI(brkw)); send(CFX_LO(brkw));
        for (int l = 0; l < 4; l++) send(CFX_WORD(CFX_OP_LAYER, l));       // all off
        int full = 0;
        if (game == 0) {                               // Hurrican: parallax, lit tiles with broken walls, over walls
            full = rnd() % 4 != 0;
            layer(0, CFX_L_ON | CFX_L_OPAQUE | (rnd() & 1 ? CFX_L_4BPP : 0), G + rnd() % 0x10000, 320, 240, rnd() & 255, 0, 0, 0, rnd() % 8000, rnd() % 8000);
            layer(1, CFX_L_ON | (rnd() & 1 ? CFX_L_4BPP : 0), G + 0x40000 + rnd() % 0x10000, 2 * (1 + rnd() % 300), 1 + rnd() % 400, rnd() & 255, 0, 0, 0, rnd() % 8000, rnd() % 8000);
            layer(2, CFX_L_ON | CFX_L_TILES | CFX_L_SHADE | (rnd() & 1 ? CFX_L_HFLIP : 0) | (rnd() & 1 ? CFX_L_4BPP : 0), G + 0x80000, 1 + rnd() % 700, 1 + rnd() % 400, rnd() & 255, 1, 10, MAPS + 2 * (rnd() % 0x8000), rnd() % 8000, rnd() % 8000);
            layer(3, CFX_L_ON | CFX_L_TILES | CFX_L_OVER | CFX_L_OPAQUE | (rnd() & 1 ? CFX_L_4BPP : 0) | (rnd() & 1 ? CFX_L_HFLIP : 0), G + 0xC0000, 1 + rnd() % 700, 1 + rnd() % 400, rnd() & 255, rnd() & 1, 10, MAPS + 2 * (rnd() % 0x8000), rnd() % 8000, rnd() % 8000);
            for (int k = rnd() % 8; k > 0; k--) {      // broken walls
                uint32_t a = brkw + rnd() % 512;
                send(CFX_WORD(CFX_OP_POKE, 0)); send(CFX_HI(a)); send(CFX_LO(a)); send(CFX_ARG(rnd() & 255)); send(CFX_ARG(rnd() & 255));
            }
        } else if (game == 1) {                        // Jazz: a sky, tiles of 32 with the map in SRAM, over tiles
            uint32_t m1 = 0x20000 + rnd() % 0x4000, m2 = 0x28000 + rnd() % 0x4000;
            send(CFX_WORD(CFX_OP_UPLOAD, 0)); a24(MAPS + 2 * (rnd() % 0x4000)); send(CFX_HI(m1)); send(CFX_LO(m1)); send(CFX_ARG(256 * 64 / 8));
            send(CFX_WORD(CFX_OP_UPLOAD, 0)); a24(MAPS + 2 * (rnd() % 0x4000)); send(CFX_HI(m2)); send(CFX_LO(m2)); send(CFX_ARG(256 * 64 / 8));
            for (int k = rnd() % 6; k > 0; k--) {      // destroyed blocks
                uint32_t a = m1 + rnd() % 2048;
                send(CFX_WORD(CFX_OP_POKE, 0)); send(CFX_HI(a)); send(CFX_LO(a)); send(CFX_ARG(rnd() & 1)); send(CFX_ARG(rnd() & 255));
            }
            layer(0, CFX_L_ON | CFX_L_OPAQUE, G + 0x100000 + rnd() % 0x1000, 4, 200, 0, 0, 0, 0, rnd() % 8000, rnd() % 8000);
            layer(1, CFX_L_ON | CFX_L_TILES | CFX_L_MAPSRAM | CFX_L_HFLIP, G + 0x110000, 256, 64, 0, 0, 32, m1, rnd() % 8192, rnd() % 2048);
            layer(2, CFX_L_ON | CFX_L_TILES | CFX_L_MAPSRAM | CFX_L_OVER, G + 0x110000, 256, 64, rnd() & 255, 0, 32, m2, rnd() % 8192, rnd() % 2048);
            if (rnd() & 1) layer(3, CFX_L_ON | CFX_L_TILES | CFX_L_MAPSRAM | CFX_L_4BPP, G + 0x180000, 64, 16, 0, 0, 16 + rnd() % 17, m1, rnd() % 8192, rnd() % 8192);
        } else {                                       // SuperTux: a wrapping picture, tiles of 16 over Tux
            uint32_t m = 0x24000 + rnd() % 0x4000;
            send(CFX_WORD(CFX_OP_UPLOAD, 0)); a24(MAPS + 2 * (rnd() % 0x4000)); send(CFX_HI(m)); send(CFX_LO(m)); send(CFX_ARG(1000 + rnd() % 4000));
            layer(1, CFX_L_ON | CFX_L_OPAQUE, G + 0x200000 + rnd() % 0x8000, 320, 240, 192, 0, 0, 0, rnd() % 8000, rnd() % 8000);
            layer(3, CFX_L_ON | CFX_L_TILES | CFX_L_MAPSRAM | CFX_L_OVER, G + 0x240000, 300, 15, 0, 0, 16, m, rnd() % 8192, rnd() % 240);
        }
        for (int k = rnd() % 3; k > 0; k--) {          // a scroll or two
            int l = rnd() % 4;
            send(CFX_WORD(CFX_OP_SCROLL, l)); send(CFX_ARG(rnd() % 8192)); send(CFX_ARG(rnd() % 8192));
        }
        send(CFX_WORD(CFX_OP_BEGIN, full));
        if (!full) for (int k = rnd() % 10; k > 0; k--) {
            send(CFX_WORD(CFX_OP_CELLS, rnd() % 26)); send(CFX_ARG(rnd() & 0x1FFF)); send(CFX_ARG(rnd() & 0xFF));
        }
        int ns = game == 2 ? 20 + rnd() % 60 : 3 + rnd() % 25;
        for (int k = 0; k < ns; k++) {
            int small = game == 2 && rnd() % 3 != 0;
            int w = small ? 1 + rnd() % 4 : 1 + rnd() % 80, h = small ? 1 + rnd() % 4 : 1 + rnd() % 70;
            int x = (int)(rnd() % 400) - 40, y = (int)(rnd() % 260) - 40;
            int fl = rnd() % 4 | (game == 0 && rnd() % 3 == 0 ? 4 : 0);
            sprite(fl, x, y, w, h, w + rnd() % 9, G + 0x280000 + rnd() % 0x10000, rnd() % 3 ? 0 : rnd() & 255);
        }
        if (fr == 1) {                                 // an argument with no command, an unknown operation: the error bit
            send(CFX_ARG(5)); send(CFX_WORD(13, 0));
        }
        send(CFX_WORD(CFX_OP_GO, 0));
        uint64_t t0 = cycles;
#ifdef DBGPUB
        memset(hist, 0, sizeof hist);
#endif
        st = wait_idle();
        uint64_t t1 = cycles;
#ifdef DBGPUB
        if (getenv("DBGHIST")) {
            printf("frame %d states (clocks):", fr);
            for (int n = 0; n < 14; n++) {
                int bi = 0; for (int q = 1; q < 128; q++) if (hist[q] > hist[bi]) bi = q;
                if (!hist[bi]) break;
                printf(" %d:%llu", bi, (unsigned long long)hist[bi]); hist[bi] = 0;
            }
            printf("\n");
        }
#endif
        int b = !(st & CFX_ST_SHOWN);                  // the buffer composed into
        if (getenv("DBG")) { printf("frame %d status %04X counters:", fr, st); for (int c = 1; c <= 7; c++) printf(" %u", bus_read(CFX_CMD_BASE + 2 * c)); printf(" | model nspr %d full %d\n", model.nspr, model.full); }
        if (((st & CFX_ST_ERR) != 0) != (model.err != 0)) { printf("frame %d: the error bit %d, the model's %d\n", fr, !!(st & CFX_ST_ERR), model.err); bad++; }
        compare("compose", fr);
        cells += model.cnt[2];
        for (int c = 1; c <= 3; c++) {
            uint16_t hv = bus_read(CFX_CMD_BASE + 2 * c);
            if (hv != model.cnt[c]) { printf("frame %d: counter %d engine %u model %u\n", fr, c, hv, model.cnt[c]); bad++; }
        }
        send(CFX_WORD(CFX_OP_SHOW, b));
        while (((st = bus_read(CFX_CMD_BASE)) & CFX_ST_SHOWN) != (b ? CFX_ST_SHOWN : 0)) noise();
        // part of the shown picture and its cell map through the window, as the SH2 copies them
        for (int n = 0; n < 64; n++) {
            uint32_t off = (rnd() % 0xFC60) & ~1u;
            uint16_t hv = bus_read(0x3E0000 + off), mv;
            cfx_read(&model, 0x3E0000 + off, &mv);
            if (hv != mv) { if (bad < 20) printf("frame %d: window %04X engine %04X model %04X\n", fr, off, hv, mv); bad++; }
        }
        printf("frame %d (%s): %d cells, %d sprites, the engine done %.2f ms after GO; PSRAM redone %u, SRAM %u\n", fr,
               game == 0 ? "Hurrican" : game == 1 ? "Jazz" : "SuperTux", model.cnt[2], model.cnt[3], (t1 - t0) * 20e-6,
               bus_read(CFX_CMD_BASE + 10), bus_read(CFX_CMD_BASE + 12));
    }
    send(CFX_WORD(CFX_OP_OFF, 0));
    for (int i = 0; i < 20; i++) tick();
    if (top->on) { printf("OFF did not turn it off\n"); bad++; }
    printf("cfx_core against cartfx.c: %d frames, %ld cells, %ld words differ\n", frames, cells, bad);
    delete top;
    return bad != 0;
}
