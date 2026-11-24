#!/usr/bin/env python3
"""merge_nocd.py <the build's COPY of the BootCamp> - OutRun CD32X without a Sega CD (a 32X and the EverDrive alone)

    python3 tools/merge_nocd.py build/edcd/bootcamp          (edcd/build_edcd.sh runs it)

Patches the COPY's edboot/edcd_md.s (the launcher's 68000 part; the BootCamp itself is never edited). Where the
launcher found no Sega CD it stopped with a blue screen. Now, with a packed launcher (ED_W0, ED_PACKED: the game's
program and data came from the SD card), it goes on alone: a small 68000 program in work RAM (0xFF9000) does what
the Sub-CPU and its listener do for this game, answering the SH2s' commands in COMM0 itself:

  the start       the Genesis VDP (the stack's register set, display off), window 0 = the program's bank, the 32X
                  registers as the listener's init leaves them, COMM4 = 0x1234, COMM10 0x0AAA -> 0x0BBB
  48              the pad (COMM8), COMM10 = 0; an effect number in the high byte is dropped (no PCM chip)
  52              the Genesis display on / off (VDP register 1)
  38              COMM8 (long) = -1: no file (no disc)
  53 56 59        COMM12 = 0: no effects pack, no backup RAM
  57 58           a word of the 4 KB buffer (work RAM 0xFFA000), as the stack's
  74 75           the buffer <-> a 4 KB piece of the EverDrive's save bank (31, through window 1, RV = 1, W on only
                  inside the write; read back and compared); COMM12 = 0x201, 0 failed; 75 with 0x8000: 0x201
  105             the MD+ command (tools/merge_mdp.py): CD54, 0000, COMM2 at cart 0x03F7FA; COMM12 = 0x4D50
  anything else   COMM0 cleared (45, 54: the CD's music - nothing)
No interrupts (SR 0x2700 throughout). STATE (COMM12) ED10 at its start. Not run on a console when written.
"""
import os, subprocess, sys

BEGIN, END = "NOCD {", "} NOCD"
A1 = """        cmpi.l  #0x53454741,0x400100    /* "SEGA": its BIOS at 0x400000 */
        beq.b   3f
"""
B1 = """        .if ED_W0 * ED_PACKED           /* NOCD { no Sega CD: on without it (tools/merge_nocd.py) */
        bra     nocd_go
        .endif                          /* } NOCD */
"""
A2 = """        .align  2
bios_list:
"""
B2 = r"""| NOCD { (OutRun CD32X tools/merge_nocd.py)
        .if ED_W0 * ED_PACKED
nocd_go:
        lea     nocd_blob(pc),a0
        lea     0xFF9000,a1
        move.w  #(nocd_blob_end-nocd_blob)/2-1,d0
1:      move.w  (a0)+,(a1)+
        dbra    d0,1b
        jmp     0xFF9000
        .align  2
| ---- from here: position independent, run at 0xFF9000 ----
nocd_blob:
        move.w  #0x2700,sr
        lea     0xFFFD00,sp
        move.w  #0xED10,0xA1512C
        lea     0xC00004,a4
        lea     0xC00000,a3
        tst.w   (a4)
        lea     nocd_vdp(pc),a5
        move.w  #0x8000,d5
        moveq   #18,d1
1:      move.b  (a5)+,d5
        move.w  d5,(a4)
        addi.w  #0x0100,d5
        dbra    d1,1b
        moveq   #0,d0
        move.w  #0x8F02,(a4)
        move.l  #0x40000000,(a4)        /* VRAM */
        move.w  #0x7FFF,d1
2:      move.w  d0,(a3)
        dbra    d1,2b
        move.l  #0xC0000000,(a4)        /* CRAM */
        moveq   #63,d1
3:      move.w  d0,(a3)
        dbra    d1,3b
        move.l  #0x40000010,(a4)        /* VSRAM */
        moveq   #39,d1
4:      move.w  d0,(a3)
        dbra    d1,4b
        lea     0xC00011,a5             /* PSG off */
        move.b  #0x9F,(a5)
        move.b  #0xBF,(a5)
        move.b  #0xDF,(a5)
        move.b  #0xFF,(a5)
        lea     0xA10000,a5             /* the pad ports */
        move.b  #0x40,0x09(a5)
        move.b  #0x40,0x0B(a5)
        move.b  #0x40,0x03(a5)
        move.b  #0x40,0x05(a5)
| window 0 = the program's bank (EDCD's w0_move for a packed launcher), with RV = 1
        move.b  #1,0xA15107
        cmpi.l  #0x4544504B,0x0001E0    /* "EDPK" in this ROM's header */
        beq.b   5f
        move.b  #0,0xA15107
        move.w  #0x0E00,0xC00000        /* not packed: blue, as before */
        move.w  #0x8700,(a4)
        move.l  #0xC0000000,(a4)
        move.w  #0x0E00,(a3)
9:      bra.b   9b
5:      move.w  #1,0xA130F2
        move.w  #ED_CTRL0+ED_W0,0xA130F0
        move.b  #0,0xA15107
| the 32X registers as the listener's init leaves them (hw_md.s, MODE1)
        lea     0xA15000,a0
        moveq   #0,d0
6:      bclr    #7,0x0100(a0)
        bne.b   6b
        move.w  d0,0x0102(a0)
        move.w  d0,0x0104(a0)
        move.w  d0,0x0106(a0)
        move.l  d0,0x0108(a0)
        move.l  d0,0x010C(a0)
        move.w  d0,0x0110(a0)
        move.w  d0,0x0130(a0)
        moveq   #0x7F,d1
        lea     0xA15200,a1
7:      move.l  d0,(a1)+
        dbra    d1,7b
8:      bset    #7,0x0100(a0)
        beq.b   8b
        move.l  d0,0x0120(a0)
        move.l  d0,0x0124(a0)
        lea     0xFFA000,a1             /* the 4 KB buffer, cleared */
        move.w  #2047,d1
1:      move.w  d0,(a1)+
        dbra    d1,1b
        move.w  #0x1234,0x0124(a0)
2:      cmpi.w  #0x0AAA,0x012A(a0)
        bne.b   2b
        move.w  #0x0BBB,0x012A(a0)
| ---- the commands ----
nocd_loop:
        lea     0xA15000,a0
        move.w  0x0120(a0),d7
        bne.b   1f
        moveq   #40,d0                  /* idle: a short pause before COMM0 is read again */
0:      dbra    d0,0b
        bra.b   nocd_loop
1:      cmp.w   0x0120(a0),d7           /* the same twice (the SH2 may be writing it) */
        bne.b   nocd_loop
        move.w  0x0122(a0),d6           /* COMM2 */
        moveq   #0,d0
        move.b  d7,d0
        cmpi.b  #48,d0
        beq     nc_pad
        cmpi.b  #52,d0
        beq     nc_text
        cmpi.b  #38,d0
        beq     nc_nofile
        cmpi.b  #53,d0
        beq     nc_zero12
        cmpi.b  #56,d0
        beq     nc_zero12
        cmpi.b  #59,d0
        beq     nc_zero12
        cmpi.b  #57,d0
        beq     nc_get
        cmpi.b  #58,d0
        beq     nc_put
        cmpi.b  #74,d0
        beq     nc_save
        cmpi.b  #75,d0
        beq     nc_save
        cmpi.b  #105,d0
        beq     nc_mdp
nc_done:
        move.w  #0,0xA15120
        bra     nocd_loop

nc_pad:                                 /* the stack's get_pad (hw_md.s), port 1 */
        lea     0xA10003,a1
        bsr     nc_in
        move.w  d0,d1
        andi.w  #0x0C00,d0
        bne.b   2f
        bsr     nc_in
        bsr     nc_in
        move.w  d0,d2
        bsr     nc_in
        andi.w  #0x0F00,d0
        cmpi.w  #0x0F00,d0
        beq.b   1f
        move.w  #0x010F,d2
1:      lsl.b   #4,d2
        lsl.w   #4,d2
        andi.w  #0x303F,d1
        move.b  d1,d2
        lsr.w   #6,d1
        or.w    d1,d2
        eori.w  #0x1FFF,d2
        bra.b   3f
2:      move.w  #0xF000,d2              /* no pad */
3:      swap    d2
        clr.w   d2
        move.l  d2,0x0128(a0)           /* COMM8 = the pad, COMM10 = 0 */
        bra     nc_done
nc_in:
        move.b  #0x00,(a1)
        nop
        nop
        move.b  (a1),d0
        move.b  #0x40,(a1)
        lsl.w   #8,d0
        move.b  (a1),d0
        rts

nc_text:
        move.w  #0x8134,d0
        btst    #0,d6
        beq.b   1f
        move.w  #0x8174,d0
1:      move.w  d0,0xC00004
        bra     nc_done
nc_nofile:
        move.l  #0xFFFFFFFF,0x0128(a0)
        bra     nc_done
nc_zero12:
        move.w  #0,0x012C(a0)
        bra     nc_done
nc_get:
        andi.w  #0x07FF,d6
        add.w   d6,d6
        lea     0xFFA000,a1
        move.w  (a1,d6.w),0x0128(a0)
        bra     nc_done
nc_put:
        andi.w  #0x07FF,d6
        add.w   d6,d6
        lea     0xFFA000,a1
        move.w  0x0128(a0),(a1,d6.w)
        bra     nc_done

nc_save:                                /* d0 = 74 out / 75 in, d6 = the piece */
        moveq   #0,d5                   /* the answer */
        cmpi.b  #75,d0
        bne.b   1f
        cmpi.w  #0x8000,d6
        beq     7f
1:      cmpi.w  #64,d6
        bhs     8f
        moveq   #0,d1
        move.w  d6,d1
        moveq   #12,d2
        lsl.l   d2,d1
        addi.l  #0x080000,d1
        movea.l d1,a2                   /* the piece, through window 1 */
        lea     0xFFA000,a1
        move.b  #1,0xA15107             /* RV on: the cart at 0x000000, its registers answer */
        move.w  #31,0xA130F2            /* window 1 = the save bank */
        cmpi.b  #75,d0
        beq.b   4f
        movea.l a2,a3
        move.w  #2047,d1
        move.w  #ED_CTRL0+ED_W0+0x2000,0xA130F0   /* W on: no other write until it is off */
2:      move.w  (a1)+,(a3)+
        dbra    d1,2b
        move.w  #ED_CTRL0+ED_W0,0xA130F0
        lea     0xFFA000,a1             /* read back */
        move.w  #2047,d1
3:      cmpm.w  (a1)+,(a2)+
        dbne    d1,3b
        bne.b   6f
        bra.b   5f
4:      move.w  #2047,d1
41:     move.w  (a2)+,(a1)+
        dbra    d1,41b
5:      move.w  #0x0201,d5
6:      move.w  #1,0xA130F2             /* window 1 = bank 1 again */
        move.b  #0,0xA15107
        bra.b   8f
7:      move.w  #0x0201,d5
8:      move.w  d5,0xA1512C
        bra     nc_done

nc_mdp:                                 /* MD+: its registers are these bus addresses (mdp.sv) */
        move.b  #1,0xA15107
        move.w  #ED_CTRL0+ED_W0+0x2000,0xA130F0
        move.w  #0xCD54,0x03F7FA
        move.w  #0x0000,0x03F7FC
        move.w  d6,0x03F7FE
        move.w  #ED_CTRL0+ED_W0,0xA130F0
        move.b  #0,0xA15107
        move.w  #0x4D50,0xA1512C
        bra     nc_done

nocd_vdp:
        .byte   0x04, 0x14, 0x30, 0x2C, 0x07, 0x54, 0x00, 0x00
        .byte   0x00, 0x00, 0x00, 0x00, 0x81, 0x2B, 0x00, 0x01
        .byte   0x01, 0x00, 0x00
        .align  2
nocd_blob_end:
        .endif
| } NOCD
"""

def die(m): sys.exit("*** merge_nocd: " + m)

def insert(text, anchor, block, after=True):
    if text.count(anchor) != 1: die("the anchor is not in the file exactly once:\n" + anchor)
    i = text.index(anchor)
    return text[:i + len(anchor)] + block + text[i + len(anchor):] if after else text[:i] + block + text[i:]

def main():
    if len(sys.argv) != 2: sys.exit(__doc__)
    root = os.path.realpath(sys.argv[1])
    if os.path.exists(os.path.join(root, ".git")):
        die("that is the BootCamp itself - run it on a copy (the stack is never edited)")
    p = os.path.join(root, "edboot", "edcd_md.s")
    t = open(p, newline="").read()
    if BEGIN in t: die("already merged")
    t = insert(insert(t, A1, B1), A2, B2, after=False)
    open(p, "w", newline="").write(t)
    print("    merge_nocd: the launcher goes on without a Sega CD (edboot/edcd_md.s in the copy)")

if __name__ == "__main__":
    main()
