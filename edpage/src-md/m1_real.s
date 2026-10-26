| m1_real.s - the REAL listener code from the CD32X BootCamp's stack/subcpu/hw_md.s (m1_xfer,
| m1_cart, m1_page, m1_cpy, m1_rvcpy, M1_WINBLOCK, m1_wb, m1_buf), lifted unchanged so the probe can
| run what a launcher runs.  It is PC-relative throughout, so the probe copies the block into work
| RAM and calls it there - in a launcher the listener lives in the Genesis's work RAM (the Sub-CPU
| puts it there), where m1_buf and m1_wb are writable.
        .equ    OL_MODE1, 1
        .equ    OL_M1WORDS, 1
        .equ    OL_M1CTRL0_W0, 0x8000
        .equ    OL_M1CTRL0_W1, 0x8000
        .equ    OL_M1W0BANK, 0
        .equ    OL_M1SAVE, 1
        .equ    OL_M1SAVEBANK, 31
        .equ    OL_M1SAVEWIN, 1
        .equ    OL_M1PAGE, 1

        .global m1_blk_start, m1_blk_end
m1_blk_start:
m1_xfer:
        movem.l d2/d4/a2-a4,-(sp)
        .if OL_M1WORDS
        tst.w   d3                      /* MODE1-2: bytes, but all even -> words */
        bne.b   0f
        move.l  a0,d1
        move.l  a1,d2
        or.l    d2,d1
        or.l    d0,d1
        btst    #0,d1
        bne.b   0f
        moveq   #1,d3
0:
        .endif
        lea     m1_buf(pc),a4
1:
        tst.l   d0
        ble     9f
        move.l  d0,d2
        cmpi.l  #1024,d2
        bls.b   2f
        move.l  #1024,d2
2:
        sub.l   d2,d0
        cmpa.l  #0x400000,a0
        blo.b   4f
        cmpa.l  #0x800000,a0
        bhs.b   4f
        movea.l a1,a2                   /* the source is the Sega CD: to the buffer with RV = 0 */
        movea.l a4,a1
        bsr     m1_cpy
        movea.l a2,a1
        movea.l a0,a2
        movea.l a4,a0                   /* then the buffer to the cart side with RV = 1 */
        moveq   #0,d4
        bsr     m1_cart
        movea.l a2,a0
        bra.b   1b
4:
        cmpa.l  #0x400000,a1
        blo.b   6f
        cmpa.l  #0x800000,a1
        bhs.b   6f
        movea.l a1,a2                   /* the destination is the Sega CD: the cart to the buffer */
        movea.l a4,a1                   /* with RV = 1 ... */
        moveq   #1,d4
        bsr     m1_cart
        movea.l a2,a1
        movea.l a0,a2
        movea.l a4,a0                   /* ... then the buffer there with RV = 0 */
        bsr     m1_cpy
        movea.l a2,a0
        bra.b   1b
6:
        moveq   #0,d4
        bsr     m1_cart                 /* neither side is the Sega CD: straight, RV = 1 */
        bra.b   1b
9:
        movem.l (sp)+,d2/d4/a2-a4
        rts

| the cart side of a block: d4 = 0 the cart is a1, else a0.  In window 0 -> through window 1.
| MODE1-7b: one block through a window: \reg = the window's bank register, \back = its bank again.
| a3 = what to add to the address for the copy, d1 = the bank the window shows meanwhile.
        .macro  M1_WINBLOCK reg, back
        move.w  sr,-(sp)
        move.w  #0x2700,sr
        bset    #0,0xA15107
        move.w  d1,\reg                 /* the window = that bank */
        tst.w   d4
        bne.b   5f
        adda.l  a3,a1
        bsr     m1_cpy_w                /* a write into the cart: W on inside */
        suba.l  a3,a1
        bra.b   6f
5:      adda.l  a3,a0
        bsr     m1_cpy                  /* a read from the cart: W stays off */
        suba.l  a3,a0
6:
        .if OL_M1PAGE
        move.w  m1_wb+2*(\back)(pc),\reg /* the window = what it showed (a page may have changed it) */
        .else
        move.w  #\back,\reg             /* the window = its own bank again */
        .endif
        bclr    #0,0xA15107
        move.w  (sp)+,sr
        rts
        .endm
m1_cart:
        .if OL_M1PAGE
        tst.w   d4                      /* MODE1-9: one byte written to a page register? */
        bne.b   m1_np
        cmpi.l  #1,d2
        bne.b   m1_np
        cmpa.l  #0xA130F0,a1
        beq     m1_page
        cmpa.l  #0xA130F2,a1
        beq     m1_page
m1_np:
        .endif
        .if OL_M1SAVE
        tst.w   d4
        bne.b   1f
        move.l  a1,d1
        bra.b   2f
1:      move.l  a0,d1
2:      cmpi.l  #0x800000,d1
        blo.b   3f
        cmpi.l  #0x840000,d1
        bhs     m1_rvcpy                /* above the save bank's 256 KB: never sent */
        movea.l #(OL_M1SAVEWIN*0x080000)-0x800000,a3   /* the save bank, through window OL_M1SAVEWIN */
        moveq   #OL_M1SAVEBANK,d1
        M1_WINBLOCK 0xA130F0+2*OL_M1SAVEWIN, OL_M1SAVEWIN
3:      cmpi.l  #0x080000,d1
        bhs     m1_rvcpy
        movea.l #0x080000,a3            /* window 0, through window 1 (MODE1-3) */
        move.w  #OL_M1W0BANK,d1
        M1_WINBLOCK 0xA130F2, 1
        .else
        tst.w   d4
        bne.b   1f
        cmpa.l  #0x080000,a1
        bra.b   2f
1:      cmpa.l  #0x080000,a0
2:      bhs     m1_rvcpy
        move.w  sr,-(sp)
        move.w  #0x2700,sr
        bset    #0,0xA15107
        move.w  #OL_M1W0BANK,0xA130F2   /* window 1 = the bank window 0 shows (bank 0, or EDCD_W0) */
        tst.w   d4
        bne.b   3f
        adda.l  #0x080000,a1
        bsr.b   m1_cpy_w                /* a write into the cart: W on inside */
        suba.l  #0x080000,a1
        bra.b   4f
3:      adda.l  #0x080000,a0
        bsr.b   m1_cpy                  /* a read from the cart: W stays off */
        suba.l  #0x080000,a0
4:      move.w  #1,0xA130F2             /* window 1 = bank 1 again */
        bclr    #0,0xA15107
        move.w  (sp)+,sr
        rts
        .endif

| d2 bytes (a0)+ -> (a1)+, in bytes (d3 = 0) or words (d3 = 1); with RV = 1 and no interrupt.
| d4 = 0: the cart is the destination (W on inside the copy); else a read of the cart (W off).
m1_rvcpy:
        move.w  sr,-(sp)
        move.w  #0x2700,sr
        bset    #0,0xA15107
        tst.w   d4
        bne.b   1f
        bsr.b   m1_cpy_w
        bra.b   2f
1:      bsr.b   m1_cpy
2:      bclr    #0,0xA15107
        move.w  (sp)+,sr
        rts

| MODE1-6: m1_cpy with the EverDrive's W on for the copy only - no other write by the 68000 between
| the two CTRL0 writes (the call's push is before, the return is a read).  RV = 1 already.
m1_cpy_w:
        move.w  #OL_M1CTRL0_W1,0xA130F0
        bsr.b   m1_cpy
        move.w  #OL_M1CTRL0_W0,0xA130F0
        rts
| d2 bytes (a0)+ -> (a1)+, in bytes (d3 = 0) or words (d3 = 1, d2 rounded up to a word)
m1_cpy:
        move.l  d2,d1
        tst.w   d3
        bne.b   2f
        subq.l  #1,d1
1:
        move.b  (a0)+,(a1)+
        dbra    d1,1b
        rts
2:
        addq.l  #1,d1
        lsr.l   #1,d1
        subq.l  #1,d1
3:
        move.w  (a0)+,(a1)+
        dbra    d1,3b
        rts

        .if OL_M1PAGE
| MODE1-9: the page byte is at (a0) (the buffer: the game wrote it into Word RAM).  a1 = the register.
m1_page:
        move.l  a2,-(sp)
        lea     m1_wb(pc),a2
        moveq   #0,d1
        move.b  (a0),d1
        lsl.w   #2,d1                   /* page p -> bank 4p */
        move.w  sr,-(sp)
        move.w  #0x2700,sr
        bset    #0,0xA15107
        cmpa.l  #0xA130F0,a1
        beq.b   1f
        lea     0xA130F8,a3             /* window 1 (0x200000): CTRL4..7 = 4p..4p+3 */
        addq.l  #8,a2
        bra.b   3f
1:      move.w  d1,d2                   /* window 0 (0x000000): CTRL0's bank + CTRL1..3 */
        ori.w   #OL_M1CTRL0_W0,d2
        move.w  d2,0xA130F0
        move.w  d1,(a2)+
        lea     0xA130F2,a3
        addq.w  #1,d1
        moveq   #3-1,d2
        bra.b   4f
3:      moveq   #4-1,d2
4:      move.w  d1,(a3)+
        move.w  d1,(a2)+
        addq.w  #1,d1
        dbra    d2,4b
        bclr    #0,0xA15107
        move.w  (sp)+,sr
        movea.l (sp)+,a2
        rts
        .align  2
m1_wb:  .word   0, 1, 2, 3, 4, 5, 6, 7  /* what EverDrive window n shows (the launcher's linear start) */
        .endif

        .align  2
m1_buf:
        .space  1024
m1_blk_end:
        .equ    m1_xfer_off, m1_xfer - m1_blk_start
        .global m1_xfer_off
        .equ    m1_blk_len, m1_blk_end - m1_blk_start
        .global m1_blk_len
