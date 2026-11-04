#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hw_md.h"
#include "files.h"

uint16_t gen_lvl2 = 0;

void write_byte(unsigned int dst, unsigned char val) { *(volatile unsigned char *)dst = val; }
void write_word(unsigned int dst, unsigned short val) { *(volatile unsigned short *)dst = val; }
void write_long(unsigned int dst, unsigned int val) { *(volatile unsigned int *)dst = val; }
unsigned char read_byte(unsigned int src) { gen_lvl2++; return *(volatile unsigned char *)src; }
unsigned short read_word(unsigned int src) { return *(volatile unsigned short *)src; }
unsigned int read_long(unsigned int src) { return *(volatile unsigned int *)src; }
void scd_init_pcm(void) {}
void bump_fm(void) { gen_lvl2++; }

int mystrlen(const char* string) {
    int rc = 0;
    while (*(string++)) rc++;
    return rc;
}

extern int sh2_app_length;
extern char sh2_app_start[];
extern char Sub_Start[];
extern char Sub_End[];
extern uint16_t InitCD(void);
extern volatile int scd_last_timeout_code;

extern int64_t scd_open_file(const char *name);
extern void scd_read_sectors(void *ptr, int lba, int len, void (*wait)(void));
extern void scd_stream_cmd(char cmd); 
extern void scd_sub_read(void *ptr, int lba, int len);
extern int  scd_cd_status(void);                           
extern int  scd_track_info(int track);                     
extern void scd_cdda_play(int track);                       
extern int  scd_bios_call(int fn, int d1, void *a0);       

/* STREAM-1 (OL_STREAM, Makefile STREAM=1, the default; STREAM=0 builds this file as before).
   From OpenJazz R7 / Hurrican P2b.  The chunks of a file (CMD 46 / 47) come from a drive that
   keeps reading: cd_stream_read starts it once (ROMREADN to the end of the file) and takes the
   sectors as they come, checking each one's header; a missed or wrong sector starts it again at
   the sector wanted.  Before, every 32 KB chunk was a CDCSTOP + ROMREADN (a seek each time).
   Anything else that uses the drive (other reads, opening a file, the CD audio, any BIOS call)
   stops the stream first (the macros below), so it starts afresh. */
#ifndef OL_STREAM
#define OL_STREAM 0
#endif
#ifndef OL_FMVRING
#define OL_FMVRING 0            /* FMV-2: CMD 46 ring mode for streamed movies (Makefile FMVRING) */
#endif
#ifndef OL_CDVOL /* DX-CDVOL begin */
#define OL_CDVOL 0              /* CDVOL-1: 1-0x3FF = the CD audio's volume (Makefile CDVOL); 0 = full, as before */
#endif /* DX-CDVOL end */
#ifndef OL_ZCHUNK /* DX-ZCHUNK begin */
#define OL_ZCHUNK 0             /* ZCHUNK-1: command 100, CMD 46 from an LZ4-packed file (Makefile ZCHUNK) */
#endif /* DX-ZCHUNK end */
#ifndef OL_CDPAUSE /* DX-CDPAUSE begin */
#define OL_CDPAUSE 0            /* CDPAUSE-1: command 81, the CD music paused / resumed in place (Makefile CDPAUSE) */
#endif /* DX-CDPAUSE end */
#ifndef OL_CDRING /* DX-CDRING begin */
#define OL_CDRING 0             /* CDRING-1: command 91, sectors of the open file into the cart (Makefile CDRING) */
#endif /* DX-CDRING end */
#ifndef OL_FBCART /* DX-FBCART begin */
#define OL_FBCART 0             /* FBCART-1: command 101, the 32X frame buffer into the cart (Makefile FBCART) */
#endif /* DX-FBCART end */
#ifndef OL_ROMPAUSE /* DX-ROMPAUSE begin */
#define OL_ROMPAUSE 0           /* ROMPAUSE-1: the read-ahead pauses the drive instead of losing sectors (Makefile ROMPAUSE) */
#endif /* DX-ROMPAUSE end */
#ifndef OL_CARTW
#define OL_CARTW 0              /* CARTW-1: CMD 46 copies to the cart in words (Makefile CARTW) */
#endif
#ifndef OL_UB
#define OL_UB 0                 /* UB-1: the Unfinished Business level files (Makefile UB) */
#endif
#ifndef OL_SLOTS
#define OL_SLOTS 0              /* SLOTS-1: CMD 61 also writes the save slot area (Makefile SLOTS) */
#endif
#if OL_STREAM
extern void scd_stream_start(int lba, int count);
extern int  scd_stream_sector(void *buf, void *hdr, int spins);
/*   scd_stream_start(lba, count)        CDCSTOP, then ROMREADN for count sectors from lba
     scd_stream_sector(buf, hdr, spins)  the next sector into buf (2048 bytes), its header (minute,
                                         second, frame in BCD, mode) into hdr; 0 done, -1 the
                                         transfer failed, -2 no sector after spins looks */
__asm__(
    "       .pushsection .text\n"
    "       .align  2\n"
    "       .globl  scd_stream_start\n"
    "scd_stream_start:\n"
    "       movem.l %d2-%d7/%a2-%a6,-(%sp)\n"
    "       move.w  #0x0089,%d0\n"            /* CDCSTOP */
    "       jsr     0x5F22.w\n"
    "       lea     48(%sp),%a0\n"            /* -> lba, count (the arguments) */
    "       move.w  #0x0020,%d0\n"            /* ROMREADN */
    "       jsr     0x5F22.w\n"
    "       movem.l (%sp)+,%d2-%d7/%a2-%a6\n"
    "       rts\n"
    "       .globl  scd_stream_sector\n"
    "scd_stream_sector:\n"
    "       movem.l %d2-%d7/%a2-%a6,-(%sp)\n"
    "       move.l  56(%sp),%d7\n"            /* spins */
    "1:     move.w  #0x008A,%d0\n"            /* CDCSTAT */
    "       jsr     0x5F22.w\n"
    "       bcc.b   2f\n"
    "       subq.l  #1,%d7\n"
    "       bne.b   1b\n"
    "       moveq   #-2,%d0\n"
    "       bra.b   9f\n"
    "2:     andi.w  #0xF8FF,0x8004.w\n"       /* CDC destination: Sub-CPU */
    "       ori.w   #0x0300,0x8004.w\n"
    "3:     move.w  #0x008B,%d0\n"            /* CDCREAD */
    "       jsr     0x5F22.w\n"
    "       bcs.b   3b\n"
    "       movea.l 48(%sp),%a0\n"            /* buf */
    "       movea.l 52(%sp),%a1\n"            /* header */
    "       move.w  #0x008C,%d0\n"            /* CDCTRN */
    "       jsr     0x5F22.w\n"
    "       bcc.b   4f\n"
    "       moveq   #-1,%d0\n"
    "       bra.b   9f\n"
    "4:     move.w  #0x008D,%d0\n"            /* CDCACK */
    "       jsr     0x5F22.w\n"
    "       moveq   #0,%d0\n"
    "9:     movem.l (%sp)+,%d2-%d7/%a2-%a6\n"
    "       rts\n"
    "       .popsection\n"
);
static int st_next = -1, st_end;               /* the drive delivers st_next .. st_end - 1 */
static uint32_t st_restarts, st_low, st_high, st_err;   /* diagnostics */
#if OL_ROMPAUSE /* DX-ROMPAUSE begin */
/* ROMPAUSE-1 (Metal Slug CD32X, 3 Nov 2026; ledger section 22b's "pause the data read instead of losing it",
   section 26): when the read-ahead ring (st_buf) is nearly full, the drive is paused with its place kept
   (ROMPAUSEON 0x08, state 0x2B) instead of left to run on: the sectors it delivered meanwhile were lost and the
   next chunk started the read again (a seek).  The ring keeps ST_PAUSE_ROOM slots for the sectors still on their
   way when it pauses; cd_chunk_read lets it go on (ROMPAUSEOFF 0x09, state 0x2A) when the next chunk is asked for.
   Any other use of the drive drops the pause with the stream (st_kill); a sector that does not follow on (the
   headers are checked as always) starts the read again, as before. */
extern void scd_rom_pause(int on);
__asm__(
    "       .pushsection .text\n"
    "       .align  2\n"
    "       .globl  scd_rom_pause\n"
    "scd_rom_pause:\n"
    "       movem.l %d2-%d7/%a2-%a6,-(%sp)\n"
    "       move.w  #0x0009,%d0\n"            /* ROMPAUSEOFF */
    "       tst.l   48(%sp)\n"
    "       beq.b   1f\n"
    "       move.w  #0x0008,%d0\n"            /* ROMPAUSEON */
    "1:     jsr     0x5F22.w\n"
    "       movem.l (%sp)+,%d2-%d7/%a2-%a6\n"
    "       rts\n"
    "       .popsection\n"
);
#define ST_PAUSE_ROOM 8
static int st_paused;
static uint32_t st_pauses;                     /* (diagnostics) */
#define st_kill() (st_next = -1, st_paused = 0)
#define scd_sub_read(p, l, n)  (st_kill(), scd_sub_read(p, l, n))
#define scd_open_file(n)       (st_kill(), scd_open_file(n))
#define scd_bios_call(f, d, a) (st_kill(), scd_bios_call(f, d, a))
#else /* DX-ROMPAUSE else */
#define scd_sub_read(p, l, n)  (st_next = -1, scd_sub_read(p, l, n))
#define scd_open_file(n)       (st_next = -1, scd_open_file(n))
#define scd_bios_call(f, d, a) (st_next = -1, scd_bios_call(f, d, a))
#endif /* DX-ROMPAUSE end */
static int unbcd(int v) { return (v >> 4) * 10 + (v & 15); }
static void cd_stream_read(uint8_t *dst, int lba, int n, int file_end)
{
    int fails = 0;
    while (n > 0) {
        if (lba != st_next || lba >= st_end) {
            int count = file_end - lba;
            if (count < n) count = n;
            scd_stream_start(lba, count);
            st_next = lba; st_end = lba + count;
            st_restarts++;
#if OL_ROMPAUSE /* DX-ROMPAUSE begin */
            st_paused = 0;                               /* (CDCSTOP + ROMREADN: a new read) */
#endif /* DX-ROMPAUSE end */
        }
        uint8_t hdr[4];
        const int r = scd_stream_sector(dst, hdr, 400000);
        const int got = unbcd(hdr[0]) * 4500 + unbcd(hdr[1]) * 75 + unbcd(hdr[2]) - 150;
        if (r == 0 && got < lba && got >= lba - 64) {    /* an earlier sector (the drive's run-up): skip it */
            st_low++;
            continue;
        }
        if (r != 0 || got != lba) {                      /* failed, or sectors were missed */
            if (r != 0) st_err++; else st_high++;
            st_next = -1;                                /* start again at this sector */
            if (++fails > 64) { scd_sub_read(dst, lba, n); return; }   /* the old way */
            continue;
        }
        st_next++; lba++; n--; dst += 2048;
    }
}
#endif

/* 68K VBlank Tick Macro. Sub-CPU accesses Main CPU Comm Registers via 0xFF8000 */
#define GET_TICKS read_long(0xFF801C)

#define SUB_MAIN_FLAG  0xFF800E
#define SUB_SUB_FLAG   0xFF800F

static void fourcc(char *out, unsigned int v)
{
    int i, ok = 1;
    for (i = 0; i < 4; i++) {
        unsigned char c = (v >> (24 - 8 * i)) & 0xFF;
        if (c < 0x20 || c > 0x7E) ok = 0;
    }
    if (ok) {
        for (i = 0; i < 4; i++) out[i] = (v >> (24 - 8 * i)) & 0xFF;
        out[4] = 0;
    } else {
        sprintf(out, "%08X", v);
    }
}

extern int do_md_cmd0(int cmd);
extern int do_md_cmd1(int cmd, int arg1);
extern int do_md_cmd2(int cmd, int arg1, int arg2);
extern int do_md_cmd3(int cmd, int arg1, int arg2, int arg3);

#if OL_STREAM
/* STREAM-1: read-ahead.  While the 68000 copies a chunk to the cart, and while the Sub-CPU waits
   for the next command, the drive goes on delivering sectors; they would be lost (its buffer
   holds only a few), so cd_pump takes each one as it arrives into st_buf - the start of the
   next chunk.  cd_chunk_read uses what st_buf holds and reads the rest. */
#if OL_ZCHUNK /* DX-ZCHUNK begin */
#define ST_RING 96                                   /* ZCHUNK-1: 192 KB - a packed chunk is ~8 sectors; the drive outruns
                                                        the unpacking, and each stop of the drive costs a seek */
#else /* DX-ZCHUNK else */
#define ST_RING 32                                   /* sectors (64 KB): two chunks */
#endif /* DX-ZCHUNK end */
static uint8_t st_buf[ST_RING * 2048] __attribute__((aligned(4)));
static int st_buf_lba = -1, st_buf_n, st_buf_pos;      /* sector st_buf_lba is in slot st_buf_pos */
extern int wait_md_cmd(void);
static void cd_pump(void)
{
#if OL_ROMPAUSE /* DX-ROMPAUSE begin */
    if (!st_paused && st_next >= 0 && st_buf_lba >= 0 && st_next < st_end && st_next == st_buf_lba + st_buf_n &&
        st_buf_n >= ST_RING - ST_PAUSE_ROOM) { scd_rom_pause(1); st_paused = 1; st_pauses++; }
#endif /* DX-ROMPAUSE end */
    if (st_next < 0 || st_buf_lba < 0 || st_buf_n >= ST_RING || st_next >= st_end ||
        st_next != st_buf_lba + st_buf_n)
        return;
    uint8_t hdr[4];
    const int r = scd_stream_sector(st_buf + ((st_buf_pos + st_buf_n) % ST_RING) * 2048, hdr, 1);
    if (r == -2) return;                                 /* no sector yet */
    const int got = unbcd(hdr[0]) * 4500 + unbcd(hdr[1]) * 75 + unbcd(hdr[2]) - 150;
    if (r == 0 && got < st_next && got >= st_next - 64) { st_low++; return; }   /* run-up */
    if (r != 0 || got != st_next) { st_next = -1; return; }                    /* lost: start again when asked */
    st_buf_n++; st_next++;
}
/* every 68000 command from here on keeps the drive's sectors coming (0xFF800E: the 68000 is busy).
   NOT for MD_CMD_DMA_SCREEN (17): send_md_cmd leaves the sub flag set for it and the 68000 waits
   for wait_md_cmd to clear it, so this pumping wait would never end.  main.c does not use it. */
static int md_wait_pumping(void)
{
    while (read_byte(0xFF800E)) cd_pump();
    return wait_md_cmd();
}
static int md_cmd0_p(int c) { send_md_cmd0(c); return md_wait_pumping(); }
static int md_cmd1_p(int c, int a) { send_md_cmd1(c, a); return md_wait_pumping(); }
static int md_cmd2_p(int c, int a, int b) { send_md_cmd2(c, a, b); return md_wait_pumping(); }
static int md_cmd3_p(int c, int a, int b, int d) { send_md_cmd3(c, a, b, d); return md_wait_pumping(); }
static int md_cmd4_p(int c, int a, int b, int d, int e) { send_md_cmd4(c, a, b, d, e); return md_wait_pumping(); }
#define do_md_cmd0 md_cmd0_p
#define do_md_cmd1 md_cmd1_p
#define do_md_cmd2 md_cmd2_p
#define do_md_cmd3 md_cmd3_p
#define do_md_cmd4 md_cmd4_p
static void cd_chunk_read(uint8_t *dst, int lba, int n, int file_end)
{
    int k = 0;
#if OL_ROMPAUSE /* DX-ROMPAUSE begin */
    if (st_paused && (st_next < 0 || !(st_buf_lba >= 0 && lba >= st_buf_lba && lba + n <= st_buf_lba + st_buf_n) ||
                      st_buf_lba + st_buf_n - (lba + n) <= ST_RING / 4)) {
        st_paused = 0;                                   /* (the drive on from its place: when the ring runs low, or */
        if (st_next >= 0) scd_rom_pause(0);              /*  the chunk is not all in it - one seek for many chunks) */
    }
#endif /* DX-ROMPAUSE end */
    if (st_buf_lba >= 0 && lba >= st_buf_lba && lba < st_buf_lba + st_buf_n) {
        const int skip = lba - st_buf_lba;               /* sectors read ahead that are not wanted */
        st_buf_pos = (st_buf_pos + skip) % ST_RING; st_buf_lba = lba; st_buf_n -= skip;
        k = st_buf_n < n ? st_buf_n : n;
        for (int i = 0; i < k; i++)
            memcpy(dst + i * 2048, st_buf + ((st_buf_pos + i) % ST_RING) * 2048, 2048);
        st_buf_pos = (st_buf_pos + k) % ST_RING; st_buf_lba += k; st_buf_n -= k;
    }
    if (k < n) {
        cd_stream_read(dst + k * 2048, lba + k, n - k, file_end);
        st_buf_lba = lba + n; st_buf_n = 0; st_buf_pos = 0;   /* the read-ahead goes on from here */
    }
}
#if OL_ZCHUNK /* DX-ZCHUNK begin */
/* ZCHUNK-1 (Metal Slug CD32X, 3 Nov 2026; ledger section 27): command 100 = CMD 46 from a packed file - its chunks
   (32 KB each) LZ4 blocks, so the drive (150 KB/s at most) reads half as much.  The file: sector 0 = 'MSZ1', the
   number of chunks (u32), then 12 bytes a chunk (big-endian): its first sector (u16), its sectors (u16), the
   packed length (u32; bit 31 = stored as it is), the chunk's length (u32); each chunk from a sector's start, in
   order, so the stream reads on from one to the next.  The block is read into zbuf and unpacked into Word RAM where
   CMD 46 reads its chunk; the read-ahead is pumped every 4 KB of output. */
static uint32_t saved_lba, saved_len;         /* (tentative: CMD 38's, defined below) */
static uint8_t zt[2048] __attribute__((aligned(4)));
static uint8_t zbuf[34 * 1024] __attribute__((aligned(4)));
static uint32_t zt_lba = 0xFFFFFFFFu;
static uint32_t zbe32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
/* the unpacker, in 68000 code (C took ~230 ms a chunk in ares, 140 KB/s: slower than the drive): tokens, literals,
   matches as LZ4's block format, the long ones 16 bytes at a time; every 4 KB of output the read-ahead is pumped
   (zpump).  The blocks are our own (tools/edcd/zpack.py): the output's end is checked every 4 KB and at the end,
   not at each sequence.  -1: a bad block. */
void zpump(void) { cd_pump(); }
extern int lz4_unpack(const uint8_t *s, int slen, uint8_t *d, int dmax);
__asm__(
    "       .pushsection .text\n"
    "       .align  2\n"
    "       .globl  lz4_unpack\n"
    "lz4_unpack:\n"
    "       movem.l %d2-%d7/%a2-%a6,-(%sp)\n"
    "       movea.l 48(%sp),%a0\n"            /* the block */
    "       move.l  52(%sp),%d7\n"
    "       movea.l 56(%sp),%a1\n"            /* the output */
    "       move.l  60(%sp),%d6\n"
    "       lea     0(%a0,%d7.l),%a2\n"       /* the block's end */
    "       movea.l %a1,%a5\n"
    "       lea     0(%a1,%d6.l),%a6\n"       /* the output's end */
    "       lea     4096(%a1),%a4\n"          /* the next pump */
    "       moveq   #15,%d5\n"
    "       moveq   #0,%d0\n"
    "       moveq   #0,%d1\n"
    "       moveq   #0,%d2\n"
    "       moveq   #0,%d3\n"
    "0:     move.b  (%a0)+,%d0\n"             /* the token (d0's upper bytes stay 0) */
    "       move.w  %d0,%d1\n"
    "       lsr.w   #4,%d1\n"                 /* literals */
    "       beq.s   3f\n"
    "       cmp.w   %d5,%d1\n"
    "       beq.s   1f\n"
    "       subq.w  #1,%d1\n"
    "21:    move.b  (%a0)+,(%a1)+\n"          /* (short: byte by byte) */
    "       dbra    %d1,21b\n"
    "       bra.s   3f\n"
    "1:     move.b  (%a0)+,%d2\n"             /* (15 or more) */
    "       add.w   %d2,%d1\n"
    "       not.b   %d2\n"
    "       beq.s   1b\n"
    "       bsr     70f\n"
    "3:     cmpa.l  %a2,%a0\n"
    "       bhs     8f\n"                     /* (the last sequence: literals only) */
    "       move.b  1(%a0),%d3\n"
    "       lsl.w   #8,%d3\n"
    "       move.b  (%a0),%d3\n"              /* the offset (little-endian) */
    "       addq.l  #2,%a0\n"
    "       movea.l %a1,%a3\n"
    "       suba.l  %d3,%a3\n"
    "       and.w   %d5,%d0\n"
    "       cmp.w   %d5,%d0\n"
    "       beq.s   4f\n"
    "       addq.w  #3,%d0\n"                 /* (the length - 1) */
    "51:    move.b  (%a3)+,(%a1)+\n"
    "       dbra    %d0,51b\n"
    "       moveq   #0,%d0\n"
    "       cmpa.l  %a4,%a1\n"
    "       blo.s   0b\n"
    "       bra.s   6f\n"
    "4:     move.b  (%a0)+,%d2\n"             /* (19 or more) */
    "       add.w   %d2,%d0\n"
    "       not.b   %d2\n"
    "       beq.s   4b\n"
    "       addq.w  #4,%d0\n"
    "       move.w  %d0,%d1\n"
    "       exg     %a0,%a3\n"
    "       bsr     70f\n"
    "       exg     %a0,%a3\n"
    "       moveq   #0,%d0\n"
    "       cmpa.l  %a4,%a1\n"
    "       blo     0b\n"
    "6:     lea     4096(%a1),%a4\n"
    "       cmpa.l  %a6,%a1\n"
    "       bhi.s   9f\n"                     /* (past the output's end: a bad block) */
    "       movem.l %d0-%d3/%a0-%a1,-(%sp)\n"
    "       jsr     zpump\n"
    "       movem.l (%sp)+,%d0-%d3/%a0-%a1\n"
    "       bra     0b\n"
    /* d1 bytes (a0)+ -> (a1)+, 16 at a time */
    "70:    move.w  %d1,%d4\n"
    "       lsr.w   #4,%d4\n"
    "       and.w   %d5,%d1\n"
    "       add.w   %d1,%d1\n"
    "       neg.w   %d1\n"
    "       jmp     72f(%pc,%d1.w)\n"
    "71:    move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "       move.b  (%a0)+,(%a1)+\n"
    "72:    dbra    %d4,71b\n"
    "       rts\n"
    "8:     move.l  %a1,%d0\n"
    "       sub.l   %a5,%d0\n"
    "       cmpa.l  %a6,%a1\n"
    "       bls.s   10f\n"
    "9:     moveq   #-1,%d0\n"
    "10:    movem.l (%sp)+,%d2-%d7/%a2-%a6\n"
    "       rts\n"
    "       .popsection\n"
);
/* chunk c of the open file (saved_lba, saved_len) into dst: its length, or -1 */
static int zchunk_read(int c, uint8_t *dst, int file_end)
{
    if (zt_lba != saved_lba) {
        cd_chunk_read(zt, (int)saved_lba, 1, file_end);
        if (zt[0] != 'M' || zt[1] != 'S' || zt[2] != 'Z' || zt[3] != '1') return -1;
        zt_lba = saved_lba;
    }
    if (c < 0 || (uint32_t)c >= zbe32(zt + 4) || 8 + 12 * c + 12 > 2048) return -1;
    const uint8_t *e = zt + 8 + 12 * c;
    int sec = e[0] << 8 | e[1], ns = e[2] << 8 | e[3];
    uint32_t pl = zbe32(e + 4), raw = zbe32(e + 8), plen = pl & 0x7FFFFFFF;
    if (ns < 1 || ns * 2048 > (int)sizeof zbuf || plen > (uint32_t)ns * 2048 || raw > 32768) return -1;
    cd_chunk_read(zbuf, (int)saved_lba + sec, ns, file_end);
    if (pl & 0x80000000u) { memcpy(dst, zbuf, raw); return plen == raw ? (int)raw : -1; }
    return lz4_unpack(zbuf, (int)plen, dst, (int)raw) == (int)raw ? (int)raw : -1;
}
/* the next chunk unpacked ahead, into zout, while the 68000 copies this one to the cart (one command of 32 KB, the
   cartridge-boot listener's m1_xfer: its interrupts masked 1 KB at a time) - the SH2 asks for c + 1 next, mostly */
#ifndef OL_ZMODE1
#define OL_ZMODE1 0
#endif
static uint8_t zout[32768] __attribute__((aligned(4)));
static int zpre_c = -1;
static uint32_t zpre_lba;
static void zprefetch(int c, int file_end)
{
    zpre_c = -1;
    if (zchunk_read(c, zout, file_end) == (int)sizeof zout) { zpre_c = c; zpre_lba = saved_lba; }
}
#endif /* DX-ZCHUNK end */
#endif

#define MD_CMD_CLEAR_B        4
#define MD_CMD_CLEAR_A        8
#define MD_CMD_READ_32X_16    19
#define MD_CMD_WRITE_32X_16   20
#define MD_CMD_READ_32X_32    21
#define MD_CMD_WRITE_32X_32   22
#define MD_CMD_COPY_FROM_CART 24
#define MD_CMD_COPY_WORDS     25
#define MD_CMD_SAFE_CART_XFER 26   
#define MD_CMD_SAFE_CART_XFER_W 27     
#define MD_CMD_CHILLY_MEMCPY  32         
#define MD_CMD_CHILLY_RV_MEMCPY 33       
#define MD_CMD_BRAM_PUT       28         
#define MD_CMD_BRAM_TO_FB     29         
#define MD_CMD_BRAM_GET       30         
#define MD_CMD_INSTALL_EXC    31         /* HW1: 68K exception trap (K29), prints 68K EXC in red */
#define MD_CMD_SET_VDP_REG    34
#define MD_CMD_START_D32XR    35         /* D32X M1: 68K -> D32XR's _start (never returns) */
#if DX_GENPLANE /* DX-GENPLANE begin */
#define MD_CMD_SET_VRAM        5         /* hw_md.s set_vram(addr, word) - Chilly's */
#define MD_CMD_COPY_VRAM       7         /* hw_md.s copy_vram(addr, src, words) */
#define MD_CMD_SET_PALETTE    12         /* hw_md.s set_palette(src, index, count) */
#define MD_CMD_SET_VSRAM      36         /* SuperTux REV2: hw_md.s set_vsram(addr, word) */
#define MD_CMD_COPY_VRAM_COL  37         /* SuperTux REV4: hw_md.s copy_vram_col(addr, src, words) */
#endif /* DX-GENPLANE end */
#if DX_VSPLIT /* DX-VSPLIT begin */
#define MD_CMD_SET_VS_LINE    38         /* VSPLIT: hw_md.s set_vs_line(index, entry) */
#endif /* DX-VSPLIT end */
#if DX_FBVDP /* DX-FBVDP begin */
#define MD_CMD_FB_LIST        39         /* FBVDP: hw_md.s fb_list(op, list) */
#endif /* DX-FBVDP end */
#if OL_TILEP /* DX-TILEP begin */
#define MD_CMD_TILE_COPY      40         /* TILEP: hw_md.s tile_copy(p, off) */
#endif /* DX-TILEP end */
/* KOBO K19: hw_md.s set_vdp_reg - one VDP register word */

/* KOBO K19 - GREEN DIAGNOSTIC TEXT.  1 = printed as before (C on the pad shows/hides it).
   0 = print_diag() prints nothing at all: every print_diag() call stays in the code, so
   setting 1 again brings all the text back.  (Chilly's white 32X start-up messages come
   from hw_md.s and are not affected.) */
#define KOBO_DIAG_TEXT        0          /* D32X M1: on - the boot steps must be visible */

/* K30 - BOOT TEXT.  0 = the Genesis display is switched OFF at the very start of main(), so
   neither the green text nor Chilly's white 32X start-up messages appear (dev request): the
   SEGA logo is followed by the splash.  The text is still written to video RAM, so setting 1
   (or pressing C in the game) shows it.  Set 1 when debugging a start-up problem. */
#ifndef EDCD_QUIET
#define EDCD_QUIET            0          /* EDCD diagnostics (Makefile QUIET=1): quiet from 4E00 */
#endif
#define KOBO_HW_QUIET         0          /* HW9: 1 = once the game reports 4B10, the Sub-CPU stops polling
                                                   the 68K; it only writes its heartbeat twice a second. */
static int sub_quiet = 0;
static int ol_game_running = 0;   /* OL-2b: STATE 4B10 seen - no more per-pass text */
#define KOBO_BOOT_TEXT        0          /* K31: on - Chilly's white 32X start-up messages show
                                                   (green text stays off: KOBO_DIAG_TEXT 0) */

#define LEGACY_68K_CART 0

#ifndef OL_CART_MB
#define OL_CART_MB 0                     /* CART-1: 8 or 16 = cart test build (build.sh CART=8/16) */
#endif

static uint32_t saved_lba = 0;
/* K10: length of the file opened by CMD 38, kept here (like saved_lba) so CMD 50 never has
   to trust COMM8 - in K9 CMD 50 read a wrong length from COMM8 and read 0 sectors. */
static uint32_t saved_len = 0;
static int quiet = 0;      
#define SLIDESHOW_DEBUG 1

static const char *find_sh2_marker(const char *p, int len)
{
    int i;
    for (i = 0; i + 8 <= len; i++)
        if (p[i] == 'S' && p[i+1] == 'H' && p[i+2] == '2' && p[i+3] == '-' && p[i+4] == 'V')
            return p + i + 5;
    return 0;
}

void print_diag(char *temp, const char *msg, int x, int y) {
    if (quiet) return;
#if !KOBO_DIAG_TEXT
    return;                                   /* K19: green text switched off (see top) */
#endif
    strcpy(temp, msg);
    switch_banks();
    do_md_cmd4(MD_CMD_PUT_STR, 0x200000, TEXT_GREEN, x, y);
    switch_banks();
}

/* =====================================================================================
 * KOBO STORAGE MODE - must match d32xr-master/sh2_main.c KOBO_NOCART
 *
 *   1 = NO-CART (development / Fusion): the RAM cart is never touched - not even the
 *       start-up cart check (accessing a missing cart crashes the console).  The SH2 reads
 *       KOBOGFX.BIN straight from CD with CMD 38 (open) + CMD 50 (one 32 KB chunk into the
 *       frame buffer).  Runs in Fusion and on the console, cart plugged in or not.
 *   0 = CART (real hardware + MicroNut99 cart): KOBOGFX.BIN is stored in the cart at
 *       offset 2 MB with CMD 38 + CMD 46 (Chilly's rules, verified), the SH2 reads it at
 *       0x22000000.  Proven on hardware.
 * ===================================================================================== */
#define KOBO_NOCART     0          /* HW1: CART MODE - real hardware WITH the cart plugged in */

#define CART_TARGET     0              /* MicroNut99 cart layout (only used when KOBO_NOCART 0) */
#define KOBO_PACK_NAME  "KOBOGFX.BIN"
#define KOBO_PACK_SLOT  16             /* cart mode: 16 * CART_SLOT = 2 MB into the 4 MB cart */
#define CART_WINDOW_READS    1   
#define CART_VERIFY_READBACK 1   

#define CART_IMG_BASE   0x000000
#define CART_SLOT       0x20000
#define CMD_CHUNK_TO_CART 46
#define CMD_CHUNK_COMPARE 47       /* D32X M1c: CMD 46 without writing - is the chunk already in the cart? */
#define CMD_CART_TO_FB    47             
static int cur_img = 0;

/* BOOTCAMP: the game's file table is in game_files.h (the only Sub-CPU file a game edits):
   file 20 = the game ROM, 100+n / 200+n = data files into the "level" / "resident" slots,
   300+n = movies into the level slot.  The table layout is OpenLara CD32X-REV's. */
#include "game_files.h"
#define GAME_DATA_COUNT   ((int)(sizeof(game_data_files) / sizeof(game_data_files[0])))
#define GAME_MOVIE_COUNT  ((int)(sizeof(game_movie_files) / sizeof(game_movie_files[0])))
static int g_nexc = 0;                   

#define XB(off)   (0x0C0100 + (off))       
#define XM(off)   (0x200100 + (off))       
#define PB(off)   (*(volatile uint8_t *)XB(off))

static void cx(int src, int dst, int len)  
{
    do_md_cmd3(MD_CMD_SAFE_CART_XFER, src, dst, len);
}

static int safe_cart_present(char *temp, char *tbuf)
{
    int ok;
#if CART_TARGET == 1
    uint8_t id, orig, rb;
    PB(0) = 0; PB(1) = 0; PB(2) = 0x5A; PB(3) = 0; PB(4) = 1; PB(5) = 0;
    switch_banks();                              
    cx(0x400001, XM(0), 1);                      
    cx(0x600001, XM(1), 1);                      
    cx(XM(4), 0x7FFFFF, 1);                      
    cx(XM(2), 0x600001, 1);                      
    cx(0x600001, XM(3), 1);                      
    cx(XM(1), 0x600001, 1);                      
    cx(XM(5), 0x7FFFFF, 1);                      
    switch_banks();                              
    id = PB(0); orig = PB(1); rb = PB(3);
    ok = (rb == 0x5A);
    sprintf(tbuf, "BRAM ID:%02X (%uKB) OLD:%02X        ", id, (id & 0x80) ? 0 : (0x2000u << (id & 7)) >> 10, orig);
    print_diag(temp, tbuf, 2, 23);
    sprintf(tbuf, "BRAM @600001: WROTE 5A READ %02X  ", rb);
#else
    uint16_t orig = 0, via9 = 0xFFFF, via4 = 0xFFFF;
    PB(0) = 0x5A; PB(1) = 0xA5; PB(2) = 0; PB(3) = 0;
#if CART_WINDOW_READS
    do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15104, 0);                       
    orig = (uint16_t)do_md_cmd1(MD_CMD_READ_32X_16, 0x900000);
#endif
    switch_banks();
    do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x400000, XM(0), 2);          
    switch_banks();                                                     
    via4 = 0xFFFF;                                                      
#if CART_WINDOW_READS
    via9 = (uint16_t)do_md_cmd1(MD_CMD_READ_32X_16, 0x900000);        
    PB(0) = orig >> 8; PB(1) = orig & 0xFF;
    switch_banks();
    do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x400000, XM(0), 2);          
    switch_banks();
#endif
#if !CART_WINDOW_READS && !CART_VERIFY_READBACK
    ok = 1;                                                             
#else
    ok = (via4 == 0x5AA5 || via9 == 0x5AA5);
#endif
    sprintf(tbuf, "OLD:%04X  VIA 400000:%04X      ", orig, via4);
    print_diag(temp, tbuf, 2, 23);
    sprintf(tbuf, "WROTE 5AA5  VIA 900000:%04X    ", via9);
#endif
    print_diag(temp, tbuf, 2, 24);
    print_diag(temp, ok ? "SAFE CART: FOUND               " : "SAFE CART: NOT FOUND           ", 2, 25);
    return ok;
}

static uint16_t cdda_track_word;       

static int bios_wait_ready(unsigned int max_ticks)
{
    (void)max_ticks;
    while (scd_cd_status() & 0x80000000) {
        bump_fm();
    }
    return 1;
}


/* =====================================================================================
 * K33 SOUND EFFECTS - Sega CD PCM chip (RF5C164), driven directly by the Sub-CPU.
 *   KOBOSFX.BIN (tools/kobo_sfx.py, from Kobo's own rendered .agw sounds): a table + a
 *   ready-made 64 KB wave-RAM image, loaded ONCE at boot by CMD 53 (before the music:
 *   no CD reads after the music starts).
 *   Playing: the SH2 puts an effect number in the upper byte of its pad request
 *   (COMM0 = 48 | id << 8); the fast path below starts it - no extra exchange, no 68K.
 *   Channel 7 = the player's gun (restarted each shot), channels 0-6 = round robin.
 *   Chip registers (odd bytes at 0xFF0001..): ENV volume, PAN, FDL/FDH step (0x800 = 32552
 *   Hz), LSL/LSH loop address, ST start page, CTRL (bit 7 sound on, bit 6: 1 = choose a
 *   channel for the registers above, 0 = choose a 4 KB wave-RAM bank), ONOFF (bit = 1 off).
 *   Wave RAM window: 0xFF2001 + 2 * n, n < 4096, of the bank chosen in CTRL.
 * ===================================================================================== */
#define PCM_ENV    0xFF0001
#define PCM_PAN    0xFF0003
#define PCM_FDL    0xFF0005
#define PCM_FDH    0xFF0007
#define PCM_LSL    0xFF0009
#define PCM_LSH    0xFF000B
#define PCM_ST     0xFF000D
#define PCM_CTRL   0xFF000F   /* K35b FIX: register 7 (was 0xFF0011 = the ON/OFF register!)    */
#define PCM_ONOFF  0xFF0011   /* K35b FIX: register 8 (was 0xFF0013 - nothing there).  Order of
                                 the registers: ENV PAN FDL FDH LSL LSH ST CTRL ON/OFF at
                                 0xFF0001, 03 ... 11.  K33/K35 wrote the channel/bank selects
                                 into ON/OFF: channels switched on/off at random and never
                                 stopped cleanly - the shot kept looping (seen in Fusion). */
#define PCM_WAVE   0xFF2001
#define SFX_MAX    31
typedef struct { uint8_t st, vol; uint16_t fd, loop, len; } sfx_t;
static sfx_t sfx_tab[SFX_MAX + 1];            /* [1..count] */
static int sfx_count = 0;
static uint8_t sfx_map[256];                  /* OL-6: engine sample index -> effect */
static int sfx_has_map = 0;
static uint8_t pcm_onoff = 0xFF;              /* shadow: the ONOFF register cannot be read */
static int pcm_next = 0;

/* SFXVOL-1: vol = 1-64 from the SH2 (64 = full, lower = farther away; 0 = full).  The
   chip's ENV register scales the sample linearly, so ENV = the effect's own level * vol / 64. */
static void pcm_play(int id, int vol) {
    const sfx_t *e;
    int ch;
    if (id < 1 || id > sfx_count) return;
    e = &sfx_tab[id];
    ch = (id == 1) ? 7 : pcm_next;            /* id 1 = player gun: its own channel */
    if (id != 1) pcm_next = pcm_next < 6 ? pcm_next + 1 : 0;
    pcm_onoff |= (uint8_t)(1 << ch);          /* key off (restart from the start page) */
    write_byte(PCM_ONOFF, pcm_onoff);
    write_byte(PCM_CTRL, 0xC0 | ch);          /* sound on + registers of channel ch */
    if (vol <= 0 || vol > 64) vol = 64;
    write_byte(PCM_ENV, (uint8_t)((e->vol * vol) >> 6));
    write_byte(PCM_PAN, 0xFF);
    write_byte(PCM_FDL, e->fd & 0xFF);
    write_byte(PCM_FDH, e->fd >> 8);
    write_byte(PCM_LSL, e->loop & 0xFF);
    write_byte(PCM_LSH, e->loop >> 8);
    write_byte(PCM_ST, e->st);
    pcm_onoff &= (uint8_t)~(1 << ch);         /* key on */
    write_byte(PCM_ONOFF, pcm_onoff);
}

/* CMD 53: KOBOSFX.BIN -> PCM wave RAM.  Returns the number of effects (0 = failed). */
static int pcm_load(const char *name) {           /* OL-6: SFX<level>.BIN */
    int64_t fi = scd_open_file(name);
    uint32_t len, lba, nsec, img, i, b;
    volatile uint8_t *buf = (volatile uint8_t *)0x0C0000;   /* Sub-CPU's Word RAM bank */
    if (fi < 0) return 0;
    len = (uint32_t)(fi >> 32); lba = (uint32_t)(fi & 0xFFFFFFFF);
    nsec = (len + 2047) / 2048;
    if (nsec > 48) nsec = 48;
    scd_sub_read((void *)0x0C0000, lba, nsec);
    if (buf[0] != 'K' || buf[1] != 'S' || buf[2] != 'F' || buf[3] != 'X') return 0;
    sfx_count = (buf[4] << 8) | buf[5];
    img = (uint32_t)((buf[6] << 8) | buf[7]);
    if (img == 0) img = 65536;
    sfx_has_map = 0;                                           /* OL-6: map after the image */
    if (len >= 256 + img + 256) {
        for (i = 0; i < 256; i++) sfx_map[i] = buf[256 + img + i];
        sfx_has_map = 1;
    }
    if (sfx_count > SFX_MAX) sfx_count = SFX_MAX;
    for (i = 0; i < (uint32_t)sfx_count; i++) {
        volatile uint8_t *t = buf + 8 + i * 8;
        sfx_tab[i + 1].st = t[0]; sfx_tab[i + 1].vol = t[1];
        sfx_tab[i + 1].fd = (uint16_t)((t[2] << 8) | t[3]);
        sfx_tab[i + 1].loop = (uint16_t)((t[4] << 8) | t[5]);
        sfx_tab[i + 1].len = (uint16_t)((t[6] << 8) | t[7]);
    }
    pcm_onoff = 0xFF;
    write_byte(PCM_ONOFF, 0xFF);                               /* all channels off */
    for (b = 0; b < 16; b++) {                                 /* 16 banks x 4 KB */
        write_byte(PCM_CTRL, (uint8_t)b);                      /* sound off, wave bank b */
        for (i = 0; i < 4096; i++) {
            uint32_t a = b * 4096 + i;
            write_byte(PCM_WAVE + 2 * i, a < img ? buf[256 + a] : 0xFF);   /* unused = stop */
        }
    }
    write_byte(PCM_CTRL, 0x80);                                /* sound on */
    return sfx_count;
}


/* =====================================================================================
 * K36 BACKUP RAM (Sega CD internal) - high scores + options, one 128-byte file.
 *   BIOS entry _BURAM = 0x5F16: d0 = function, a0/a1 = parameters, carry set = error.
 *   BRMINIT 0 (a0 = 0x640-byte work area, a1 = 12-byte string buffer), BRMREAD 3 (a0 = 11-char
 *   file name, a1 = buffer), BRMWRITE 4 (a0 = name(11) + mode(1: 0 = normal) + blocks(word,
 *   64 bytes each), a1 = data).  All registers are saved around the call.
 *   The SH2 exchanges the 128 bytes one word at a time over COMM (CMD 57 get / 58 put) - no
 *   frame buffer, no Word RAM, only at start-up and after a game.
 * ===================================================================================== */
#define BRAM_BYTES 4096                 /* OL-5: OpenLara settings + saved game */
static uint8_t bram_work[0x640];
static uint8_t bram_str[16];
static uint8_t bram_buf[BRAM_BYTES];
static const char bram_name[12] = GAME_BRAM_NAME;  /* the Sega CD backup RAM file (game_files.h) */
static uint8_t bram_param[14];
static int bram_ready = 0;

static int bram_call(int fn, const void *pa0, void *pa1) {
    register int d0 __asm__("d0") = fn;
    register const void *a0 __asm__("a0") = pa0;
    register void *a1 __asm__("a1") = pa1;
    register int d1 __asm__("d1") = 0;
    __asm__ volatile (
        "movem.l %%d2-%%d7/%%a2-%%a6,-(%%sp)\n\t"
        "jsr 0x5F16.w\n\t"
        "scs %%d1\n\t"                          /* carry (error) -> d1 low byte 0xFF */
        "movem.l (%%sp)+,%%d2-%%d7/%%a2-%%a6"
        : "+d"(d0), "+a"(a0), "+a"(a1), "+d"(d1) : : "cc", "memory");
    return (d1 & 0xFF) ? -1 : d0;
}
static int bram_init(void) {
    if (!bram_ready) bram_ready = bram_call(0, bram_work, bram_str) >= 0 ? 1 : -1;
    return bram_ready > 0;
}
static int bram_load(void) {                    /* 1 = loaded, 0 = no file / no BRAM */
    int i;
    for (i = 0; i < BRAM_BYTES; i++) bram_buf[i] = 0;
    if (!bram_init()) return 0;
    return bram_call(3, bram_name, bram_buf) >= 0 ? 1 : 0;
}
static int bram_save(void) {
    int i;
    if (!bram_init()) return 0;
    for (i = 0; i < 11; i++) bram_param[i] = (uint8_t)bram_name[i];
    bram_param[11] = 0;                         /* normal (not protected) mode */
    bram_param[12] = 0; bram_param[13] = BRAM_BYTES / 64;   /* blocks */
    return bram_call(4, bram_param, bram_buf) >= 0 ? 1 : 0;
}

static int cdda_start(int track, int *st_out, int once)   /* OL-4 play once */
{
    int st = 0;
    bios_wait_ready(0);
    scd_bios_call(0x0089, 0, 0);            
    scd_bios_call(0x0002, 0, 0);            
#if OL_CDVOL /* DX-CDVOL begin */
    scd_bios_call(0x0085, OL_CDVOL & 0x3FF, 0);   /* CDVOL-1: the CD audio's volume (FDRSET, 0-0x3FF; 0x400 full) */
#else /* DX-CDVOL else */
    scd_bios_call(0x0085, 0x0400, 0);       
#endif /* DX-CDVOL end */
    cdda_track_word = (uint16_t)track;
    scd_bios_call(once ? 0x0012 : 0x0013, 0, &cdda_track_word);   /* MSCPLAY1 once / MSCPLAYR repeat */  
    st = scd_cd_status();
    *st_out = st; 
    return 1;
}

int main(void)
{
    write_byte(0x8003, read_byte(0x8003) & ~1);
    while (read_byte(0x8003) & 2);
#if !KOBO_BOOT_TEXT
    do_md_cmd1(MD_CMD_SET_VDP_REG, 0x8134);   /* K30: Genesis display off (md_init_hw left it on) */
#endif

    char *temp = (char *)0x0C0000;
    char tbuf[64];
    uint32_t build_counter = 0;
    int cart_ok = 0;

    print_diag(temp, "BUILD: D32XR CD M1C QUICK CHECK     ", 2, 0);
    print_diag(temp, "LOADING 32XCD ENGINE...           ", 2, 2);

    {
        const char *m = find_sh2_marker(sh2_app_start, sh2_app_length);
        if (m) sprintf(tbuf, "32X PAYLOAD: %d BYTES SH2-V%c%c        ", sh2_app_length, m[0], m[1]);
        else   sprintf(tbuf, "32X PAYLOAD: %d BYTES SH2:NONE       ", sh2_app_length);
        print_diag(temp, tbuf, 2, 4);
    }

    memcpy(temp, sh2_app_start, sh2_app_length);
    switch_banks();
    {   
        int r32 = do_md_cmd2(MD_CMD_INIT_32X, 0x200000, sh2_app_length);
        do_md_cmd0(MD_CMD_INSTALL_EXC);            /* HW1: hook the 68K error vectors (after the 32X is on) */
        g_nexc = 0;
        char a[12], b[12], c[12];
        fourcc(a, read_long(0xFF8014));   
        fourcc(b, read_long(0xFF8018));   
        fourcc(c, read_long(0xFF801C));   
        switch_banks();
        
        sprintf(tbuf, "INIT32X:%d 1ST:%s              ", r32, a);
        print_diag(temp, tbuf, 2, 19);
        sprintf(tbuf, "END C0:%s C4:%s          ", b, c);
        print_diag(temp, tbuf, 2, 20);
        if (r32 < 0) {
            print_diag(temp, "32X INIT FAILED - STOPPED       ", 2, 21);
            while (1) bump_fm();
        }

        do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15124, 0x1234);

        {
            uint16_t ready = 0;
            volatile int wait = 0;
            
            {
                unsigned int t0 = GET_TICKS;
                for (wait = 0; wait < 1000000; wait++) {
                    ready = (uint16_t)do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512A);
                    if (ready == 0x0AAA) break;
                    if ((GET_TICKS - t0) >= 180) break;
                }
            }
            sprintf(tbuf, "SH2 READY FOR ISOLATED TEST: %04X    ", ready);
            print_diag(temp, tbuf, 2, 19);

            if (ready != 0x0AAA) {
                print_diag(temp, "SH2 NEVER SIGNALLED READY - STOPPED", 2, 20);
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512A, 0x0BBB);   
                while (1) bump_fm();
            }
        }

#if KOBO_NOCART
        cart_ok = 0;                                   /* no-cart build: never touch the cart */
        print_diag(temp, "CART: NOT USED (NO-CART BUILD)     ", 2, 25);
#else
        cart_ok = safe_cart_present(temp, tbuf);  
#endif
        (void)cart_ok; 

        do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512A, 0x0BBB);   
    }

    uint16_t init_err = InitCD();

    {
        unsigned char c = read_byte(SUB_MAIN_FLAG);
        sprintf(tbuf, "POST-INITCD COMM: %02X          ", c);
        print_diag(temp, tbuf, 2, 1);
    }

    if (init_err != 0) {
    } else {
        print_diag(temp, "CD DRIVE READY                ", 2, 5);
        print_diag(temp, "BOOTING SUB-CPU...            ", 2, 6);
    }

    {   
        int st = scd_cd_status();
        int t2 = scd_track_info(2);
        sprintf(tbuf, "CD TRACKS %02X-%02X  TRACK2 %08X      ", (st >> 8) & 0xFF, st & 0xFF, t2);
        print_diag(temp, tbuf, 2, 14);
    }

    memcpy(temp, Sub_Start, Sub_End - Sub_Start);

    {
        unsigned char c = read_byte(SUB_MAIN_FLAG);
        sprintf(tbuf, "POST-MEMCPY2 COMM: %02X         ", c);
        print_diag(temp, tbuf, 2, 3);
    }

    print_diag(temp, "INITIALIZING SUB-CPU Sfx...   ", 2, 7);
    send_md_cmd0('I'); 

    {
        unsigned char c = read_byte(SUB_MAIN_FLAG);
        sprintf(tbuf, "POST-SENDMD COMM: %02X          ", c);
        print_diag(temp, tbuf, 2, 18);
    }

    print_diag(temp, "68K LISTENER ACTIVE           ", 2, 8);

    {
        unsigned char boot_comm = read_byte(SUB_MAIN_FLAG);
        sprintf(tbuf, "BOOT COMM PORT: %02X            ", boot_comm);
        print_diag(temp, tbuf, 2, 9);
    }

    print_diag(temp, "(TEXT BLINKS TO SHOW SH2 IS ALIVE)", 2, 26);

    while (1) {
        build_counter++;

        if (build_counter == 1) {
            unsigned char first_iter_comm = read_byte(SUB_MAIN_FLAG);
            sprintf(tbuf, "ITER1 COMM PORT: %02X           ", first_iter_comm);
            print_diag(temp, tbuf, 2, 15);
        }

        bump_fm();

#if KOBO_HW_QUIET || EDCD_QUIET
        if (sub_quiet) {                 /* HW9 QUIET TEST: no 68K traffic except this heartbeat */
            static unsigned int t_last = 0;
            static uint16_t q_count = 0;
            unsigned int now = GET_TICKS;             /* read directly - no 68K command */
            if (now - t_last >= 30) {
                t_last = now;
                q_count++;
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512E,
                           ((read_byte(0xFF8038) & 0x0F) << 12) | (q_count & 0x0FFF));
#if EDCD_QUIET && KOBO_DIAG_TEXT
                {   /* EDCD trace, quiet variant: COMM12 history sampled here only (every 0.5 s) */
                    static uint16_t qh[6]; static int qn = 0; static uint16_t ql = 0xFFFF;
                    uint16_t v = (uint16_t)do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512C);
                    int i, n;
                    if (v != ql) { ql = v; if (qn < 6) qh[qn++] = v; else { for (i = 0; i < 5; i++) qh[i] = qh[i + 1]; qh[5] = v; } }
                    n = sprintf(tbuf, "QHIST:");
                    for (i = 0; i < qn; i++) n += sprintf(tbuf + n, " %04X", qh[i]);
                    while (n < 38) tbuf[n++] = ' ';
                    tbuf[n] = 0;
                    print_diag(temp, tbuf, 2, 23);
                }
#endif
            }
#if !EDCD_QUIET
            continue;                    /* (EDCD_QUIET: commands are still served below) */
#endif
        }
#endif
        {   /* HW1 SUB-CPU HEARTBEAT (K29): every 16 passes the pass counter goes to COMM14.  Only
               the Sub-CPU writes COMM14; the SH2 only reads it (hardware readout "P"). */
            static uint16_t hb_pass = 0;
#if EDCD_QUIET
            if (sub_quiet) goto hb_done;  /* quiet: no per-pass 68000 traffic, no regular heartbeat */
#endif
#if KOBO_DIAG_TEXT
            /* EDCD trace: the history of COMM12 (the SH2's status) - every pass, the last 10
               distinct values, oldest first.  The cartridge boot's restart cycle overwrites
               COMM12 (4D05, 0), so a snapshot never shows the game's last step; this does. */
            static uint16_t st_hist[18], st_last = 0xFFFF; static int st_n = 0;
            {
                uint16_t v = (uint16_t)do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512C);
                if (v != st_last) {
                    st_last = v;
                    if (st_n < 18) st_hist[st_n++] = v;
                    else { int i; for (i = 0; i < 17; i++) st_hist[i] = st_hist[i + 1]; st_hist[17] = v; }
                }
            }
#endif
            /* HW8: top digit = the CD drive's status (CDD status register 0xFF8038, first
               nibble: 0 stopped, 1 playing, 2 seeking, 3 scanning, 4 paused, 5 tray open,
               9 reading TOC, 0xB no disc / error ...), lower 3 digits = pass count / 64 */
            if ((++hb_pass & 63) == 0) {
#if KOBO_DIAG_TEXT
                {   /* 18 values, oldest first, six a row on rows 23-25 */
                    int i, r, n;
                    for (r = 0; r < 3; r++) {
                        n = sprintf(tbuf, r == 0 ? "HIST:" : "     ");
                        for (i = r * 6; i < r * 6 + 6; i++)
                            n += (i < st_n) ? sprintf(tbuf + n, " %04X", st_hist[i]) : sprintf(tbuf + n, "     ");
                        while (n < 38) tbuf[n++] = ' ';
                        tbuf[n] = 0;
                        if (!ol_game_running) print_diag(temp, tbuf, 2, 23 + r);
                    }
                }
#endif
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512E,
                           ((read_byte(0xFF8038) & 0x0F) << 12) | ((hb_pass >> 6) & 0x0FFF));
#if KOBO_DIAG_TEXT   /* OL-5 */
                /* OL-2 DIAG 3: Sub-CPU heartbeat on screen + the command waiting in COMM0 */
                sprintf(tbuf, "S%04X C0:%04X ST:%04X M:%04X SL:%04X", hb_pass,   /* DIAG 4 */
                        do_md_cmd1(MD_CMD_READ_32X_16, 0xA15120),
                        do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512C),
                        do_md_cmd1(MD_CMD_READ_32X_16, 0xA15126),
                        do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512A));
                if (!ol_game_running) print_diag(temp, tbuf, 2, 19);   /* OL-2b */
                /* EDCD trace: the 32X as the 68000 sees it, live.  A15100: bit 15 FM (the SH2 owns the
                   VDP), bit 1 RES, bit 0 ADEN.  A15106: bit 0 RV.  A1510A: FBCTL (FS bit 0, PEN bit
                   13, FEN bit 1) - valid for the 68000 only while FM = 0. */
                sprintf(tbuf, "32X: A15100 %04X RV %04X FBCTL %04X ",
                        do_md_cmd1(MD_CMD_READ_32X_16, 0xA15100),
                        do_md_cmd1(MD_CMD_READ_32X_16, 0xA15106),
                        do_md_cmd1(MD_CMD_READ_32X_16, 0xA1510A));
                if (!ol_game_running) print_diag(temp, tbuf, 2, 22);
#endif
            }
#if EDCD_QUIET
hb_done:    ;
#endif
        }
        /* SFXVOL-1: COMM0 (command) and COMM2 (its parameter) in ONE read - the effect's
           volume comes in COMM2 with the pad request, at no extra round trip through the 68K */
        uint32_t cmd32 = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0xA15120);
        uint16_t cmd = (uint16_t)(cmd32 >> 16);
        uint16_t cmdParam = (uint16_t)(cmd32 & 0xFFFF);
        if (cmd == 0) {                  /* HW6: idle - pause ~0.2 ms before the 68K reads COMM0
                                            again (fewer reads while the SH2 may be writing it) */
            volatile int k;
#if OL_STREAM
            for (k = 0; k < 150; k++) { if ((k & 3) == 0) cd_pump(); }   /* STREAM-1: read ahead */
#else
            for (k = 0; k < 150; k++) { }
#endif
        }

        /* KOBO K5 - PAD FAST PATH.  WHY: the SH2 asks for the pad (CMD 48) every frame and
           only waits a short time.  The rest of this loop (3 more register reads + redrawing
           row 10, each a round trip through the 68K) took longer than that, so the SH2 gave
           up and scrolled with pad = 0.  Answer CMD 48 first and start the next pass. */
        if ((cmd & 0xFF) == 48) {
            uint16_t pad = GET_PAD(0);
            if (cmd >> 8) pcm_play(sfx_has_map ? sfx_map[((cmd >> 8) - 1) & 0xFF] : (cmd >> 8), cmdParam);   /* OL-6 */           /* K33: sound effect requested */
            /* K32: reply = pad (COMM8, upper 16 bits) + the 68K's vblank counter (COMM10, lower
               16 bits) in ONE 32-bit write.  The SH2 uses the counter to run Kobo's logic at
               its fixed 30 ms step, whatever the frame rate.  COMM10 is only used by the
               start-up handshake, long finished by now. */
#if OL_PAD2 /* DX-PAD2 begin */
            /* PAD2-1 (Metal Slug CD32X, 3 Nov 2026): COMM10 = pad 2 (GET_PAD(1), the 68K's vblank copy,
               0xF000 = no pad) instead of the 68K's vblank counter, for a game of two players */
            do_md_cmd2(MD_CMD_WRITE_32X_32, 0xA15128, ((uint32_t)pad << 16) | GET_PAD(1));
#else /* DX-PAD2 else */
            do_md_cmd2(MD_CMD_WRITE_32X_32, 0xA15128, ((uint32_t)pad << 16) | (GET_TICKS & 0xFFFF));
#endif /* DX-PAD2 end */
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);     /* done           */
            continue;
        }

#if KOBO_DIAG_TEXT   /* OL-5: no register reads for text nobody sees */
        uint16_t comm4 = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15124);
        uint16_t sh2_state = do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512C);

        {   /* HW5: redraw row 10 ONLY when the SH2's step code changes, and never again once the
               game runs (0x4B10).  WHY: the old test also watched COMM0 (flips 00/30 every
               frame with the pad request) and COMM8/10 (pad + frame counter since K32), so on
               hardware (text on) the Sub-CPU reprinted a text row EVERY pass - each print swaps
               Word RAM and goes through the 68K - and the pad replies waited: 1-2 fps. */
            static uint16_t l_st = 0xFFFF;
            static int running = 0;
            if (!running && sh2_state != l_st) {
                uint32_t ckpt = do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128);
                l_st = sh2_state;
                sprintf(tbuf, "32X:%X 32XReg:%X STATE:%X CKPT:%04X     ", cmd, comm4, sh2_state, (unsigned int)(ckpt >> 16));
                print_diag(temp, tbuf, 2, 10);
                if (sh2_state == 0x4B10) { running = 1; ol_game_running = 1; sub_quiet = KOBO_HW_QUIET; }   /* HW9 */
#if EDCD_QUIET
                if (sh2_state == 0x4E00) sub_quiet = 1;   /* EDCD diagnostics: quiet from the game's start */
#endif
            }
        }

        if (comm4 == 0x0EEE) {
            print_diag(temp, "SH2 REACHED THE LDC LINE              ", 2, 18); 
        } else if (!ol_game_running && comm4 != 0x1234 && comm4 != 0x0000) {   /* OL-2b: quiet once the game runs */
            print_diag(temp, "SH2 NEVER EVEN REACHED THIS FILE      ", 2, 18); 
        }

#endif

        if (cmd == 38) { 
            int img_idx = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            char img_name[24];
            cur_img = img_idx < 0 ? 0 : img_idx;         
            /* KOBO M1: file number 16 = the Kobo graphics pack (tools/kobo_gfx.py).
               CMD 46 then stores it at cart offset 16 * CART_SLOT = 2 MB. */
            if (img_idx == 16) { strcpy(img_name, KOBO_PACK_NAME); cur_img = KOBO_PACK_SLOT; }
            else if (img_idx == 20) { strcpy(img_name, GAME_ROM_NAME); cur_img = 0; }  /* the game ROM, cart 0 (CDLOADER) */
            else if (img_idx >= GAME_DATA_BASE && img_idx < GAME_DATA_BASE + GAME_DATA_COUNT) {
                strcpy(img_name, game_data_files[img_idx - GAME_DATA_BASE]);
                cur_img = GAME_DATA_SLOT;
            }
            else if (img_idx >= GAME_RESIDENT_BASE && img_idx < GAME_RESIDENT_BASE + GAME_DATA_COUNT) {
                strcpy(img_name, game_data_files[img_idx - GAME_RESIDENT_BASE]);
                cur_img = GAME_RESIDENT_SLOT;
            }
            else if (img_idx >= GAME_MOVIE_BASE && img_idx < GAME_MOVIE_BASE + GAME_MOVIE_COUNT) {
                strcpy(img_name, game_movie_files[img_idx - GAME_MOVIE_BASE]);
                cur_img = GAME_DATA_SLOT;
            }
            else if (img_idx <= 0) strcpy(img_name, "IMAGE.RAW");
            else sprintf(img_name, "IMAGE%d.RAW", img_idx);

            sprintf(tbuf, "OPENING %s...              ", img_name);
            print_diag(temp, tbuf, 2, 11);

            unsigned char entry_comm = read_byte(SUB_MAIN_FLAG);
            if (entry_comm != 0) {
                sprintf(tbuf, "ENTRY COMM FAULT: %02X        ", entry_comm);
                print_diag(temp, tbuf, 2, 12);
            }

            int64_t file_info = scd_open_file(img_name);
            if (file_info < 0) {
                sprintf(tbuf, "ERR: %s NOT FOUND          ", img_name);
                print_diag(temp, tbuf, 2, 12);
                sprintf(tbuf, "TIMEOUT: %d (0=none 1=cmd 2=ack)", scd_last_timeout_code);
                print_diag(temp, tbuf, 2, 13);
                do_md_cmd2(MD_CMD_WRITE_32X_32, 0xA15128, 0); 
                saved_len = 0;
            } else {
                uint32_t length = (uint32_t)(file_info >> 32);
                saved_lba = (uint32_t)(file_info & 0xFFFFFFFF);
                saved_len = length;                                   /* K10: for CMD 50 */
                sprintf(tbuf, "%s %lu BYTES%s          ", img_name, (unsigned long)length,
                        length > 128000 ? " (TOO TALL)" : " OK");
                print_diag(temp, tbuf, 2, 12);
                do_md_cmd2(MD_CMD_WRITE_32X_32, 0xA15128, length); 
            }
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0); 
        }
#if OL_ZCHUNK /* DX-ZCHUNK begin */
        else if (cmd == CMD_CHUNK_TO_CART || cmd == CMD_CHUNK_COMPARE || cmd == 100) {
#else /* DX-ZCHUNK else */
        else if (cmd == CMD_CHUNK_TO_CART || cmd == CMD_CHUNK_COMPARE) {
#endif /* DX-ZCHUNK end */
            int chunk = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            int novfy = chunk & 0x8000;          /* OL-13: level loads skip the read-back */
#if OL_FMVRING
            /* FMV-2 (OL_FMVRING, Makefile FMVRING=1, the default): COMM2 bit 14 = ring mode for a
               movie that plays while it streams: file chunk (COMM2 & 0x1FFF) goes to ring slot
               chunk % 60 of the level slot (60 x 32 KB), or with bit 13 to slot 60 (the copy of
               slot 0 after the ring's end, so a movie chunk across the wrap stays in one piece).
               The 68000 copies 4 KB at a time (short RV stalls for the SH2s playing the movie). */
            int ring = chunk & 0x4000, guard = chunk & 0x2000;
            chunk &= ring ? 0x1FFF : 0x7FFF;
#else
            chunk &= 0x7FFF;
#endif
            int length = do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128);
            int off = chunk * 32768;
            int n = 0, bytes, p, k, bad = 0;
            uint32_t coff = CART_IMG_BASE + (uint32_t)cur_img * CART_SLOT + (uint32_t)off;
            int piece = 0x4000;
#if OL_FMVRING
            if (ring) {
                coff = CART_IMG_BASE + (uint32_t)cur_img * CART_SLOT + (uint32_t)(guard ? 60 : chunk % 60) * 32768;
                piece = 0x1000;
            }
#endif

            if (off < length) n = (length - off + 2047) / 2048;
            if (n > 16) n = 16;
            bytes = n * 2048;

#if CART_TARGET == 1
            if (n > 0) {
                volatile uint8_t *flag = (volatile uint8_t *)0x0DF000;   
                scd_sub_read((void *)0x0C0000, saved_lba + chunk * 16, n);
                flag[0] = 1; flag[1] = 0;
                switch_banks();                                   
                cx(0x21F000, 0x7FFFFF, 1);                        
                for (p = 0; p < bytes; p += 4096)
                    do_md_cmd3(MD_CMD_BRAM_PUT, 0x200000 + p, 0x600001 + 2 * (coff + p),
                               bytes - p < 4096 ? bytes - p : 4096);
                cx(0x21F001, 0x7FFFFF, 1);                        
                for (p = 0; p < bytes; p += 4096)                 
                    do_md_cmd3(MD_CMD_BRAM_GET, 0x600001 + 2 * (coff + p), 0x208000 + p,
                               bytes - p < 4096 ? bytes - p : 4096);
                switch_banks();                                   
                {
                    volatile uint8_t *disc = (volatile uint8_t *)0x0C0000;
                    volatile uint8_t *cart = (volatile uint8_t *)0x0C8000;
                    for (k = 0; !novfy && k < bytes; k++) {
                        if (disc[k] != cart[k]) {
                            sprintf(tbuf, "MISMATCH C%d +%05X DISC %02X CART %02X ", chunk, k,
                                    disc[k], cart[k]);
                            print_diag(temp, tbuf, 2, 17);
                            bad = 1;
                            break;
                        }
                    }
                }
            }
#else
            if (n > 0) {
#if OL_STREAM
#if OL_ZCHUNK /* DX-ZCHUNK begin */
                if (cmd == 100) {                /* ZCHUNK-1: the chunk from its packed block (unpacked ahead: copied) */
                    if (zpre_c == chunk && zpre_lba == saved_lba && bytes == (int)sizeof zout)
                        memcpy((void *)0x0C0000, zout, sizeof zout);
                    else if (zchunk_read(chunk, (uint8_t *)0x0C0000, (int)(saved_lba + (saved_len + 2047) / 2048)) != bytes) bad = 2;
                    zpre_c = -1;
                } else
#endif /* DX-ZCHUNK end */
                cd_chunk_read((uint8_t *)0x0C0000, saved_lba + chunk * 16, n,   /* STREAM-1 */
                              (int)(saved_lba + (saved_len + 2047) / 2048));
#else
                scd_sub_read((void *)0x0C0000, saved_lba + chunk * 16, n);   
#endif
                switch_banks();                                   
#if OL_FMVRING
                /* FMV-2: COMM12 bit 15 while the cart is written in ring mode: the slave SH2 (the
                   movie's sound) does not read the cart then, so RV never holds it */
                if (ring) do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, 0x8000);
#endif
#if OL_CARTW
                /* CARTW-1 (OL_CARTW, Makefile CARTW=1, the default): the 68000 copies words
                   (hw_md.s command 27, safe_cart_xfer_w: 15 cycles a byte) instead of bytes
                   (command 33: 42 cycles a byte).  Every chunk reaches the cart almost 3 times
                   faster, and RV (the SH2s locked out of the cart) is on for a third of the time.
                   Command 27 masks the 68000's interrupts, so the pieces are 4 KB (8 ms, less than
                   a frame: the vblank waits, it is never lost).  Arguments: source, destination. */
                (void)piece;
#if OL_ZCHUNK /* DX-ZCHUNK begin */
                if (cmd == 100 && OL_ZMODE1 && !bad) {   /* ZCHUNK-1: the copy, and meanwhile the next chunk unpacked */
                    send_md_cmd3(MD_CMD_SAFE_CART_XFER_W, 0x200000, 0x400000 + coff, bytes);
                    zprefetch(chunk + 1, (int)(saved_lba + (saved_len + 2047) / 2048));
                    md_wait_pumping();
                } else
#endif /* DX-ZCHUNK end */
                if (cmd != CMD_CHUNK_COMPARE)
                    for (p = 0; p < bytes; p += 0x1000)
                        do_md_cmd3(MD_CMD_SAFE_CART_XFER_W, 0x200000 + p, 0x400000 + coff + p,
                                   bytes - p < 0x1000 ? bytes - p : 0x1000);
#else
                if (cmd != CMD_CHUNK_COMPARE)          /* CMD 47: compare only, no write */
                    for (p = 0; p < bytes; p += piece)
                        do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x400000 + coff + p, 0x200000 + p,
                                   bytes - p < piece ? bytes - p : piece);
#endif
#if OL_ZCHUNK /* DX-ZCHUNK begin */
                if (cmd == 100) { switch_banks(); goto z_done; }   /* (ZCHUNK: no read-back, no window checks, no text -
                                                                       5 listener calls a chunk fewer) */
#endif /* DX-ZCHUNK end */
#if OL_FMVRING
                if (ring) do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, 0);
#endif
#if CART_VERIFY_READBACK
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15104, (int)(coff >> 20) & 3);   
                for (p = 0; !novfy && p < bytes; p += 0x4000)
                    do_md_cmd3(MD_CMD_CHILLY_MEMCPY, 0x208000 + p,
                               0x900000 + ((coff + p) & 0xFFFFF),
                               bytes - p < 0x4000 ? bytes - p : 0x4000);
#endif
                switch_banks();                                   
#if CART_VERIFY_READBACK
                {   
                    volatile uint8_t *disc = (volatile uint8_t *)0x0C0000;
                    volatile uint8_t *cart = (volatile uint8_t *)0x0C8000;
#if OL_STREAM
                    {   /* STREAM-1: four bytes at a time, pumping; the byte loop below finds the place */
                        const volatile uint32_t *dw = (const volatile uint32_t *)disc;
                        const volatile uint32_t *cw = (const volatile uint32_t *)cart;
                        int w;
                        for (w = 0; !novfy && w < bytes / 4; w++) {
                            if ((w & 255) == 0) cd_pump();
                            if (dw[w] != cw[w]) break;
                        }
                        k = novfy ? bytes : w * 4;
                    }
                    for (; !novfy && k < bytes; k++) {
#else
                    for (k = 0; !novfy && k < bytes; k++) {
#endif
                        if (disc[k] != cart[k]) {
                            sprintf(tbuf, "MISMATCH C%d +%05X DISC %02X CART %02X ", chunk, k,
                                    disc[k], cart[k]);
                            print_diag(temp, tbuf, 2, 17);
                            bad = 1;
                            break;
                        }
                    }
                }
#endif
#if CART_WINDOW_READS
                {   
                    int wbad = 0;
                    uint16_t want = 0, got = 0;
                    uint32_t o = 0;
                    do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15104, (int)(coff >> 20) & 3);   
                    for (k = 0; k < 4 && !wbad; k++) {
                        o = (uint32_t)(((bytes - 2) / 3 * k) & ~1);
                        want = *(volatile uint16_t *)(0x0C0000 + o);
                        got  = (uint16_t)do_md_cmd1(MD_CMD_READ_32X_16, 0x900000 + ((coff + o) & 0xFFFFF));
                        if (got != want) wbad = 1;
                    }
                    if (wbad)
                        sprintf(tbuf, "32X WIN C%d +%05lX W%04X R%04X   ", chunk, (unsigned long)o, want, got);
                    else
                        sprintf(tbuf, "32X WIN C%d 900000: OK            ", chunk);
                    print_diag(temp, tbuf, 2, 16);
                }
#else
                print_diag(temp, "32X WIN: OFF (V72 TEST)          ", 2, 16);
#endif
            }
#endif
            sprintf(tbuf, "%s I%d C%d @%06lX %s          ", cmd == CMD_CHUNK_COMPARE ? "CMP " : "CART", cur_img, chunk,
#if CART_TARGET == 1
                    (unsigned long)(0x600001 + 2 * coff),      
#else
                    (unsigned long)(0x400000 + coff),          
#endif
                    n == 0 ? "EMPTY" : bad ? "BAD" : (CART_VERIFY_READBACK ? "OK" : "NOVFY"));
            print_diag(temp, tbuf, 2, 13);
#if OL_ZCHUNK /* DX-ZCHUNK begin */
z_done:
#endif /* DX-ZCHUNK end */
#if OL_FMVRING
            if (ring) bad |= 0x100;              /* FMV-2: "ring mode known" - the SH2 tests it */
#endif
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, bad);   /* HW4: 16-bit COMM12 - COMM14 is the heartbeat */
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == CMD_CART_TO_FB) {
            int slot_chunk = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            int slot = slot_chunk >> 8;
            int chunk = slot_chunk & 0xFF;
            int length = do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128);
            int off = chunk * 32768, bytes = length - off, p;
            uint32_t coff = CART_IMG_BASE + (uint32_t)slot * CART_SLOT + (uint32_t)off;
            
            if (bytes > 32768) bytes = 32768;
            for (p = 0; p < bytes; p += 4096)
                do_md_cmd3(MD_CMD_BRAM_TO_FB, 0x600001 + 2 * (coff + p), 0x840200 + off + p,
                           bytes - p < 4096 ? bytes - p : 4096);
            
            sprintf(tbuf, "CART->FB S%d C%d %d BYTES          ", slot, chunk, bytes > 0 ? bytes : 0);
            print_diag(temp, tbuf, 2, 16);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 60) {
            /* D32X M1: the ROM image is in the cart.  Acknowledge the SH2 FIRST (the 68K will be
               gone after this), then make the 68K jump to D32XR's Genesis code (0x880800 via the
               32X window).  send_md_cmd only (no wait: the 68K never comes back).  The Sub-CPU
               has nothing else to do in M1 (D32XR finds no Sega CD and does not call it). */
            /* M1b: what does the 68000 see?  D32XR's _start at cart offset 0x800 must read
               7000323C through the fixed window 0x880000 (that is where the 68K will jump) -
               compare with the proven bank window 0x900000 (bank 0) and our patch at 0x40. */
            {
                uint32_t w88, w90, w40;
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15104, 0);              /* bank 0 */
                w88 = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0x880800);
                w90 = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0x900800);
                w40 = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0x880040);
                sprintf(tbuf, "880800:%08X 900800:%08X  ", (unsigned)w88, (unsigned)w90);
                print_diag(temp, tbuf, 2, 22);
                sprintf(tbuf, "880040:%08X (PATCH 23FCC000)  ", (unsigned)w40);
                print_diag(temp, tbuf, 2, 23);
                /* M1c: where do the 68000's V-blank / H-blank / external-interrupt vectors
                   point with the 32X on?  RAM slots (FFFDxx) or the cart window (880xxx)? */
                sprintf(tbuf, "V78:%08X V70:%08X V68:%08X ",
                        (unsigned)do_md_cmd1(MD_CMD_READ_32X_32, 0x78),
                        (unsigned)do_md_cmd1(MD_CMD_READ_32X_32, 0x70),
                        (unsigned)do_md_cmd1(MD_CMD_READ_32X_32, 0x68));
                print_diag(temp, tbuf, 2, 24);
            }
            print_diag(temp, "D32XR: STARTING THE GAME FROM THE CART ", 2, 14);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
            send_md_cmd0(MD_CMD_START_D32XR);
            while (1) { }
        }
        else if (cmd == 44) {
#if !SLIDESHOW_DEBUG
            if (!quiet) { do_md_cmd0(MD_CMD_CLEAR_B); do_md_cmd0(MD_CMD_CLEAR_A); quiet = 1; }
#endif
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 50 || cmd == 51) {
            /* K15: CMD 51 = same as CMD 50, but chunk c goes to frame buffer byte
               0x200 + c * 32 KB (a whole picture in order - the splash screen). */
            /* KOBO NO-CART - CMD 50 "file chunk -> frame buffer (raw)".
               COMM2 = chunk number (32 KB each).  File length: saved_len from CMD 38 (K10 -
               no longer read from COMM8).  CD -> Word RAM (Sub-CPU) -> frame buffer at 68K 0x840200 via MD
               command 25 copy_words with flag 0 = plain copy (flag 1 would set bit 15 on every
               word, which is only right for 15-bit pictures).  The SH2 has handed the frame
               buffer to the 68K (FM=0) before sending this. */
            int chunk = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            int length = (int)saved_len;           /* K10: from CMD 38, NOT from COMM8 */
            /* K31: CMD 50 chunks are 16 KB (8 sectors) - the off-screen loading window shrank
               to 16 KB so all 5 tile sets fit in spare frame-buffer memory (no CD reads after
               the music starts: a data read silences the CD audio).  CMD 51 stays 32 KB. */
            int sec = cmd == 51 ? 16 : 8;                   /* sectors per chunk */
            int off = chunk * sec * 2048;
            int n = 0;
            if (off < length) n = (length - off + 2047) / 2048;
            if (n > sec) n = sec;
            uint16_t w0 = 0;
            /* (K13 cleared Word RAM priority mode here; K13 showed it was never on - register 05
               before and after - so the write is removed in K15: no needless pokes at the Word
               RAM mode register on real hardware.  The real K12 fault was zero words not landing
               in the frame buffer - fixed on the SH2 side by clearing the area first.) */
            if (n > 0) {
                scd_sub_read((void *)0x0C0000, saved_lba + chunk * sec, n);  /* CD -> Word RAM */
                switch_banks();                                               /* -> 68K side    */
                do_md_cmd4(MD_CMD_COPY_WORDS, 0x200000,
                           cmd == 51 ? 0x840200 + off : 0x852000, n * 2048, 0);
                /* K30: CMD 50 now lands OFF-SCREEN (frame buffer byte 0x12000, past the
                   visible 320x224 picture which ends at 0x11A00): loading never overwrites
                   what is on screen (the panel stays intact).  SH2 side: FB_CHUNK_BUF. */
                switch_banks();                                               /* -> Sub side    */
                w0 = *(volatile uint16_t *)0x0C0000;   /* first word read from CD (read it
                                                          BEFORE print_diag reuses this buffer) */
            }
            /* K11 reply for the SH2's diagnostic strip (lines 1-3): chunk | sectors | length>>8.
               Written before COMM0 is cleared; the SH2 reads it only after that. */
            /* K14 reply: chunk | sectors | the word in WORD RAM at offset 0x210 of this chunk, read
               after the copy (the Sub-CPU's buffer at 0x0C0000).  For chunk 0 that is the high
               word of the tile set offset and must be 0000.  Correct here but wrong in the
               frame buffer (strip line T) = the 68K -> frame buffer copy loses zero data.
               (K13: priority mode was never on - register read 05 before and after.)        */
            {
                uint16_t wr = (n > 0) ? *(volatile uint16_t *)(0x0C0000 + 0x210) : 0xEEEE;
                do_md_cmd2(MD_CMD_WRITE_32X_32, 0xA1512C, (chunk << 24) | (n << 16) | wr);
            }
            (void)w0;
            sprintf(tbuf, "CD->FB CHUNK %d  %d SECTORS          ", chunk, n);
            print_diag(temp, tbuf, 2, 16);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 54) {
            /* K36 - CMD 54 "music off": BIOS MSCSTOP (the same call cdda_start makes first).
               Music on again = CMD 45 as always. */
            bios_wait_ready(0);
            scd_bios_call(0x0002, 0, 0);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#if OL_CDPAUSE /* DX-CDPAUSE begin */
        else if (cmd == 81) {
            /* CDPAUSE-1 (Metal Slug CD32X, 4 Nov 2026; ledger section 22b's proposal 81, as Rayman CD32X built it in
               its copy): COMM2 = 1 pauses the CD music where it is (BIOS MSCPAUSEON 0x03), 0 plays on from there
               (MSCPAUSEOFF 0x04) - 54 then 45 would start the track from its top. Both are queued BIOS calls: the
               drive is let finish first, as 54 does. A drive paused past the spin-down delay (60 s) goes to standby;
               MSCPAUSEOFF then starts the kept play again with a seek (section 22a). Reply: none. */
            int on = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            bios_wait_ready(0);
            scd_bios_call(on ? 0x0003 : 0x0004, 0, 0);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif /* DX-CDPAUSE end */
        else if (cmd == 56 || cmd == 59) {
            /* K36 - CMD 56 load / CMD 59 save the backup-RAM file.  Reply COMM12: 1 ok, 0 not. */
            int ok = cmd == 56 ? bram_load() : bram_save();
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, ok);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 57 || cmd == 58) {
            /* K36 - CMD 57 get / CMD 58 put one word of the 128-byte buffer: COMM2 = word index,
               COMM8 = value (58: written by the SH2 before the command; 57: our reply). */
            int wi = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & (BRAM_BYTES / 2 - 1);   /* OL-5 */
            if (cmd == 58) {
                int v = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15128);
                bram_buf[wi * 2] = (uint8_t)(v >> 8); bram_buf[wi * 2 + 1] = (uint8_t)v;
#if KOBO_DIAG_TEXT
                sprintf(tbuf, "TRACE: 58 WORD %d = %04X          ", wi, v & 0xFFFF);   /* EDCD trace */
                print_diag(temp, tbuf, 2, 21);
#endif
            } else {
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15128, (bram_buf[wi * 2] << 8) | bram_buf[wi * 2 + 1]);
            }
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 61) {
            /* OL-9: save area - the 4 KB buffer (filled with CMD 58) -> RAM cart at
               0x3F0000 + COMM2 * 4 KB.  The SH2 cannot write the cart; the 68000 can (RV method,
               exactly as CMD 46).  Read back through the 32X window and compared.
               Reply COMM12: 1 ok, 0 failed. */
#if OL_SLOTS
            /* SLOTS-1 (OL_SLOTS, Makefile SLOTS=1, the default): COMM2 bit 8 set = a piece of the
               save slot area, cart 0x060000 + (COMM2 & 31) * 4 KB (OpenLara's 16 save slots, between
               the game and Caves); reply 0x101 instead of 1, so the SH2 knows the slots are there. */
            int piece = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            int slot_area = piece & 0x100;
            uint32_t coff = slot_area ? CART_IMG_BASE + 0x060000 + (uint32_t)(piece & 31) * 4096
                                      : CART_IMG_BASE + 0x3F0000 + (uint32_t)(piece & 15) * 4096;
#else
            int piece = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 15;
            uint32_t coff = CART_IMG_BASE + 0x3F0000 + (uint32_t)piece * 4096;
#endif
            int k, ok = 1;
            volatile uint8_t *wr = (volatile uint8_t *)0x0C0000;
#if KOBO_DIAG_TEXT
            sprintf(tbuf, "TRACE: 61 IN  CART %06lX         ", (unsigned long)coff);   /* EDCD trace */
            print_diag(temp, tbuf, 2, 21);
#endif
            for (k = 0; k < 4096; k++) wr[k] = bram_buf[k];
            switch_banks();
            do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x400000 + coff, 0x200000, 4096);
#if KOBO_DIAG_TEXT
            print_diag(temp, "TRACE: 61 WROTE (68K BACK)      ", 2, 21);
#endif
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15104, (int)(coff >> 20) & 3);
            do_md_cmd3(MD_CMD_CHILLY_MEMCPY, 0x208000, 0x900000 + (coff & 0xFFFFF), 4096);
#if KOBO_DIAG_TEXT
            print_diag(temp, "TRACE: 61 READ BACK (68K BACK)  ", 2, 21);
#endif
            switch_banks();
            {
                volatile uint8_t *cart = (volatile uint8_t *)0x0C8000;
                for (k = 0; k < 4096; k++)
                    if (cart[k] != bram_buf[k]) { ok = 0; break; }
            }
#if KOBO_DIAG_TEXT
            if (ok) print_diag(temp, "TRACE: 61 OK - SAVE VERIFIED    ", 2, 21);
            else {
                sprintf(tbuf, "TRACE: 61 BAD +%04X %02X/%02X        ", k,
                        bram_buf[k], ((volatile uint8_t *)0x0C8000)[k]);
                print_diag(temp, tbuf, 2, 21);
            }
#endif
#if OL_SLOTS
            if (ok && slot_area) ok = 0x101;        /* SLOTS-1 */
#endif
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, ok);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#if OL_CART_MB
        else if (cmd == 68) {
            /* CART-1 (test builds only: CART=8 / CART=16 bash build.sh) - CMD 68 "cart page":
               COMM2 = page 1-7 for window 1 (cart 2-4 MB) of the 16MB-PCB-CD32X.  The 68000
               writes the page as ONE even byte to 0xA130F2 (the board latches /UWR, D8-D10)
               with the existing listener routine 26 (safe_cart_xfer: interrupts off, RV = 1,
               byte copy, RV = 0).  No listener change.  Window 0 (the program) is never paged.
               Reply COMM12: 1 done, 0 refused.  docs: CD32X_SRAM_PAGING_PROPOSAL.md */
            int page = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF;
            int ok = page >= 1 && page <= 7;
            if (ok) {
                PB(0) = (uint8_t)page;
                switch_banks();                                   /* Word RAM -> 68000 */
                cx(XM(0), 0xA130F2, 1);                           /* src, dst, 1 byte */
                switch_banks();
            }
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, ok);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif
#if OL_TILEP /* DX-TILEP begin */
        else if (cmd == 99) {
            /* TILEP-2 (Metal Slug CD32X, 3 Nov 2026; ledger section 25): a tile page's cache.  A game's sprite
               tiles are cut into 2 MB pages (window 1, CMD 68); a page keeps slots for tiles that are in another
               page, and its leaves say, of each tile it does not have, the page q and index j that do.  COMM2 =
               the page window 1 shows (p, 1-6), COMM8L = where the tile part ('MSPG') starts in every page.  The
               tiles themselves the SH2 hands to the listener's routine 40 (hw_md.s tile_copy) one by one
               through COMM4-COMM10 while this command runs (TILEP-1, a command a tile through routine 26, took
               ~10 ms a tile in ares).  Reply COMM12 = the tiles copied, 0xFFFF refused. */
            uint32_t a2 = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF;
            uint32_t a8 = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128);
            int p = (int)(a2 & 7), r = -1;
            if (p >= 1 && p <= 6 && a8 < 0x1FFF00) r = do_md_cmd2(MD_CMD_TILE_COPY, p, (int)a8);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, r < 0 ? 0xFFFF : r & 0xFFFF);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif /* DX-TILEP end */
#if DX_GENPLANE /* DX-GENPLANE begin */
        /* DX-GENPLANE (Dracula X CD32X, GENPLANE=1 bash build.sh): SuperTux CD32X REV's commands 62-67,
           taken unchanged from MicroNut99/SuperTuxCD32X_REV cd32x/subcpu/main.c (REV2: 62-66, REV4: 67).
           Ledger: CD32X_COMMAND_LEDGER_ADDITIONS.md section 1; listener routines 36 / 37 (section 2). */
        /* ---------------------------------------------------------------------------------
           SuperTux REV2 - THE GENESIS PICTURE FOR THE SH2 (commands 62-66).
           The 68000 listener could always write the Genesis VDP (Chilly's set_vram /
           copy_vram / set_palette, Kobo's set_vdp_reg) but no SH2 command reached them: the
           SH2 could only switch the Genesis text on / off (52).  These five let the game use
           the Genesis planes - tiles and maps uploaded from the RAM cart, scrolled by
           register.  They run only when the SH2 sends them, i.e. after the game has started:
           nothing in the boot sequence calls them.  Parameters as the other commands: COMM2
           (16 bits) and COMM8 (32 bits: COMM8 and COMM10), written by the SH2 BEFORE COMM0;
           done = COMM0 cleared.  Cart sources are read by the 68000 through the 32X bank
           window 0x900000 (RV 0 - as CMD 46's read-back), so the SH2 is never locked out.
             62  VRAM word     COMM2 = VRAM byte address, COMM8 (high 16) = the word
             63  cart -> VRAM  COMM2 = VRAM byte address, COMM8L = words << 22 | cart offset
                               (even, < 4 MB; 1..1023 words); reply COMM12 = 1 ok, 0 refused
             64  cart -> CRAM  COMM2 = first colour (0-63), COMM8L = count << 22 | cart offset
                               (1..64 colours); reply COMM12 = 1 ok, 0 refused
             65  VDP register  COMM2 = 0x8000 | reg << 8 | value (registers 0-23 only)
             66  VSRAM word    COMM2 = VSRAM byte address (0 plane A, 2 plane B), COMM8 (high
                               16) = the vertical scroll
           A copy that crosses a 1 MB bank of the window is done in two parts. */
        else if (cmd == 62 || cmd == 66) {
            int addr = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF;
            int word = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15128) & 0xFFFF;
            if (cmd == 62) do_md_cmd2(MD_CMD_SET_VRAM, addr & 0xFFFE, word);
            else do_md_cmd2(MD_CMD_SET_VSRAM, addr & 0x7E, word);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 63 || cmd == 64) {
            int dst = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF;
            uint32_t p = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128);
            uint32_t coff = p & 0x3FFFFF;
            int n = (int)(p >> 22);
            int ok = n > 0 && !(coff & 1) && (cmd == 63 ? !(dst & 1) : (dst < 64 && dst + n <= 64));
            while (ok && n > 0) {
                /* the part inside this 1 MB bank of the window */
                int fit = (int)((0x100000 - (coff & 0xFFFFF)) >> 1);
                int k = n < fit ? n : fit;
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15104, (int)(coff >> 20) & 3);
                if (cmd == 63) do_md_cmd3(MD_CMD_COPY_VRAM, dst, 0x900000 + (coff & 0xFFFFF), k);
                else do_md_cmd3(MD_CMD_SET_PALETTE, 0x900000 + (coff & 0xFFFFF), dst, k);
                n -= k;
                coff += (uint32_t)k * 2;
                dst += cmd == 63 ? k * 2 : k;
            }
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, ok);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        /* SuperTux REV4 - 67  cart -> VRAM in COLUMNS (a plane map 64 cells wide, one map row =
           128 bytes): COMM2 = VRAM byte address of the first column's top word, COMM8L =
           (columns - 1) << 30 | words a column << 22 | cart offset (even; 1-4 columns of 1-255
           words, stored one column after the other).  Column k goes to address + 2 k.  Reply
           COMM12 = 1 ok / 0 refused.  WHY: a plane scrolling sideways needs a new map column
           every 8 pixels - 30 words 128 bytes apart (cmd 63 copies rows). */
        else if (cmd == 67) {
            int dst = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF;
            uint32_t p = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128);
            uint32_t coff = p & 0x3FFFFF;
            int rows = (int)((p >> 22) & 0xFF), cols = (int)(p >> 30) + 1, k;
            int ok = rows > 0 && !(coff & 1) && !(dst & 1)
                     && ((coff & 0xFFFFF) + (uint32_t)(rows * cols * 2) <= 0x100000);   /* one bank */
            if (ok) {
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15104, (int)(coff >> 20) & 3);
                for (k = 0; k < cols; k++)
                    do_md_cmd3(MD_CMD_COPY_VRAM_COL, dst + 2 * k, 0x900000 + (coff & 0xFFFFF) + (uint32_t)(k * rows * 2), rows);
            }
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, ok);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 65) {
            int w = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF;
            if ((w & 0xE000) == 0x8000 && ((w >> 8) & 0x1F) <= 23) do_md_cmd1(MD_CMD_SET_VDP_REG, w);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif /* DX-GENPLANE end */
#if DX_VSPLIT /* DX-VSPLIT begin */
        /* VSPLIT (Dracula X CD32X, D4: stage 8's raster bands; needs GENPLANE) - 78 "V scroll line split":
           the V scroll of planes A and B changed part-way down the screen by the Genesis H interrupt.
             COMM2 = index 0-15: COMM8L = entry (line << 20 | V scroll A << 10 | V scroll B) into the list;
             COMM2 = 0x8000 | count: show that list, its first count entries (0 = off).
           Entry 0 = the top of the picture; the others in line order.  MD 38 set_vs_line.
           D5: two lists - entries go into the one not shown; a commit while the splits run takes effect
           at the end of the picture (the vblank of a 32X swap requested after it). */
        else if (cmd == 78) {
            int a = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF;
            uint32_t e = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128);
            do_md_cmd2(MD_CMD_SET_VS_LINE, a, (int)e);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif /* DX-VSPLIT end */
#if DX_FBVDP /* DX-FBVDP begin */
        /* FBVDP (Dracula X CD32X, docs/60; needs GENPLANE and VSPLIT) - 79 "the Genesis VDP from the 32X frame
           buffer": the words a running game makes each frame (map entries, converted tiles, H scroll tables,
           colours) - the cart commands 63 / 64 / 67 can't carry them (the SH2 can't write the cart), and 62 / 66
           take one word a command (about 0.44 ms each).
             COMM2 = 0: run the list at frame buffer offset COMM8L (even, below 0x20000; the SH2 gave the frame
                        buffer to the 68000 first, FM = 0, and takes it back after);
             COMM2 = 1: the commit - run the part the list kept (after the 32X swap: the vblank's register writes).
           Reply COMM12 = 1 ok, 0 a bad list.  The list's form: hw_md.s fb_list (MD 39). */
        else if (cmd == 79) {
            int op = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF;
            uint32_t off = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128) & 0x1FFFE;
            int r = op <= 3 ? do_md_cmd2(MD_CMD_FB_LIST, op, (int)(0x840000 + off)) : 0;   /* (2 / 3: docs/66) */
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, r & 1);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif /* DX-FBVDP end */
#if OL_CDRING /* DX-CDRING begin */
        /* CDRING-1 (Dracula X CD32X, 3 Nov 2026; ledger section 29) - 91 "sectors of the open file into the cart at
           any place": a game that reads its own disc image while it runs (the PC Engine's data track, 20.8 MB, read
           as its game asks: DraculaX docs/69) keeps a ring of them in the cart, where CMD 46's slots can't go.
             COMM2 = the first sector in the file (CMD 38 opened it); COMM8L = bits 0-23 the cart byte offset (2 KB
             aligned, below the save area), bits 24-27 the sectors - 1 (1-16).
           Reply COMM12 = 0 ok, 1 refused (past the file's end or the cart's).  With STREAM the drive reads ahead as
           for CMD 46; the copy as CMD 46's (CARTW: words). */
        else if (cmd == 91) {
            int sec = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF, bad = 0, p;
            uint32_t a8 = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128);
            uint32_t coff = a8 & 0xFFFFFF; int n = (int)((a8 >> 24) & 15) + 1;
            int last = (int)((saved_len + 2047) / 2048);
            if (sec + n > last) n = last - sec;
            if (n <= 0 || (coff & 2047) || coff + (uint32_t)n * 2048 > 0x3F0000) bad = 1;
            else {
#if OL_STREAM
                cd_chunk_read((uint8_t *)0x0C0000, (int)saved_lba + sec, n, (int)saved_lba + last);
#else
                scd_sub_read((void *)0x0C0000, saved_lba + sec, n);
#endif
                switch_banks();
                for (p = 0; p < n * 2048; p += 0x1000)
#if OL_CARTW
                    do_md_cmd3(MD_CMD_SAFE_CART_XFER_W, 0x200000 + p, 0x400000 + coff + p, n * 2048 - p < 0x1000 ? n * 2048 - p : 0x1000);
#else
                    do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x400000 + coff + p, 0x200000 + p, n * 2048 - p < 0x1000 ? n * 2048 - p : 0x1000);
#endif
                switch_banks();
            }
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, bad);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif /* DX-CDRING end */
#if OL_FBCART /* DX-FBCART begin */
        /* FBCART-1 (Dracula X CD32X, 3 Nov 2026; ledger section 30) - 101 "the 32X frame buffer into the cart": a game
           whose writable memory is bigger than the SH2's keeps it in the cart and writes it back there (the SH2 cannot
           write the cart; DraculaX docs/72: the PC Engine's RAM banks).
             COMM2 = the frame buffer byte offset >> 4 (the 68000 reads it at 0x840000 + offset: the SH2 gives it the
             frame buffer, FM = 0, before the command); COMM8L = bits 0-23 the cart byte offset (even, below the save
             area), bits 24-27 the length in 2 KB - 1 (2-32 KB).
           Reply COMM12 = 0 ok, 1 refused (odd, or past the cart's or the frame buffer's end).  The copy: the
           listener's safe_cart_xfer_w (words, RV on, interrupts off), 4 KB at a time. */
        else if (cmd == 101) {
            uint32_t fo = (uint32_t)(do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF) << 4, p;
            uint32_t a8 = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128);
            uint32_t coff = a8 & 0xFFFFFF, len = (((a8 >> 24) & 15) + 1) * 2048;
            int bad = (coff & 1) || coff + len > 0x3F0000 || fo + len > 0x20000;
            if (!bad)
                for (p = 0; p < len; p += 0x1000)
                    do_md_cmd3(MD_CMD_SAFE_CART_XFER_W, 0x840000 + fo + p, 0x400000 + coff + p, len - p < 0x1000 ? len - p : 0x1000);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, bad);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif /* DX-FBCART end */
        else if (cmd == 53) {
            /* K33 - CMD 53 "load sound effects": KOBOSFX.BIN -> PCM chip.  Reply in COMM12:
               number of effects (0 = failed). */
            char sfx_name[16];                              /* OL-6: COMM2 = level */
            sprintf(sfx_name, "SFX%02d.BIN", do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFF);
            int nfx = pcm_load(sfx_name);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, nfx);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 52) {
            /* KOBO K19 - CMD 52 "Genesis text layer on/off": COMM2 = 1 on, 0 off.
               Switches the Genesis display itself (VDP register 1 bit 6) - off blanks ALL
               Genesis text (green diagnostics and Chilly's white messages) to black and the
               32X picture shows alone; on brings everything back (it stays in video RAM).
               WHY: the 32X priority bit alone did not hide the text reliably (K17/K18). */
            int on = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 1;
            do_md_cmd1(MD_CMD_SET_VDP_REG, on ? 0x8174 : 0x8134);
#if KOBO_DIAG_TEXT
            print_diag(temp, on ? "TRACE: 52 TEXT ON               " : "TRACE: 52 TEXT OFF              ", 2, 21);   /* EDCD trace */
#endif
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 48) {
            /* (K5: normally handled by the fast path at the top of the loop)
               KOBO M1 - CMD 48 "read pad": reply = pad 1 bits (SEGA_CTRL_*) in COMM8.
               WHY a command: the SH2 must never poll a comm register the 68K writes
               (32X manual p.69, and the COMM6 crash).  GET_PAD(0) is the copy the 68K's
               vblank handler keeps in its comm word - no extra 68K work. */
            uint16_t pad = GET_PAD(0);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15128, pad);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 45) {
            int track = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            int once = track & 0x8000;                  /* OL-4: bit 15 = play once */
            track &= 0x7FFF;
            sprintf(tbuf, "CDDA: PLAY TRACK %d (REPEAT)        ", track);
            print_diag(temp, tbuf, 2, 27);
            
            {
                int st = 0;
                int tries = cdda_start(track, &st, once);
                sprintf(tbuf, "CDDA T%d ST %02X TRY %d%s          ", track, (st >> 24) & 0xFF,
                        tries, tries ? "" : " FAILED");
                print_diag(temp, tbuf, 2, 27);
            }
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
    }
    return 0;
}