| SEGA MegaCD support code
| by Chilly Willy

| MD Hardware Calls - keep in sync with the enums in hw_md.h

        .equ    MD_CMD_INIT_HW, 1
        .equ    MD_CMD_SET_SR, 2
        .equ    MD_CMD_GET_PAD, 3
        .equ    MD_CMD_CLEAR_B, 4
        .equ    MD_CMD_SET_VRAM, 5
        .equ    MD_CMD_NEXT_VRAM, 6
        .equ    MD_CMD_COPY_VRAM, 7
        .equ    MD_CMD_CLEAR_A, 8
        .equ    MD_CMD_PUT_STR, 9
        .equ    MD_CMD_PUT_CHR, 10
        .equ    MD_CMD_DELAY, 11
        .equ    MD_CMD_SET_PALETTE, 12
        .equ    MD_CMD_Z80_BUSREQUEST, 13
        .equ    MD_CMD_Z80_RESET, 14
        .equ    MD_CMD_Z80_MEMCLR, 15
        .equ    MD_CMD_Z80_MEMCPY, 16
        .equ    MD_CMD_DMA_SCREEN, 17
        .equ    MD_CMD_INIT_32X, 18
        .equ    MD_CMD_COPY_FROM_CART, 24
        .equ    MD_CMD_COPY_WORDS, 25
        .equ    MD_CMD_SAFE_CART_XFER, 26
.ifdef DX_VSPLIT                            | DX-VSPLIT begin
.ifdef DX_FBVDP                             | DX-FBVDP begin
        .equ    MD_CMD_END, 40              /* + fb_list (39): FBVDP, the Genesis VDP from the 32X frame buffer */
.else                                       | DX-FBVDP else
        .equ    MD_CMD_END, 39              /* + set_vs_line (38): VSPLIT, V scroll changed part-way down */
.endif                                      | DX-FBVDP end
.else                                       | DX-VSPLIT else
.ifdef DX_GENPLANE                          | DX-GENPLANE begin
        .equ    MD_CMD_END, 38              /* + set_vsram (36), copy_vram_col (37): SuperTux REV2 / REV4 */
.else                                       | DX-GENPLANE else
        .equ    MD_CMD_END, 36              /* KOBO K19: + set_vdp_reg (34); D32X M1: + start_d32xr (35) */
.endif                                      | DX-GENPLANE end
.endif                                      | DX-VSPLIT end
.ifdef DX_TILEP                             | DX-TILEP begin
        .equ    MD_CMD_END, 41              /* + tile_copy (40): TILEP, a tile page's cache (36-39 cmd_nop if off) */
.endif                                      | DX-TILEP end

| MODE1-1 (Makefile MODE1=1, docs/12): the listener for a CARTRIDGE boot (the Sega CD's "mode 1"), started
| by edboot/EDCD.32X from a Mega EverDrive PRO.  There the cart is at 0x000000 and the Sega CD at 0x400000
| (Word RAM at 0x600000), the reverse of a CD boot.  The Sub-CPU program is unchanged: the listener moves
| each address argument it is given (Word RAM 0x2xxxxx -> 0x6xxxxx, the cart 0x4xxxxx-0x7xxxxx ->
| 0x0xxxxx-0x3xxxxx), keeps interrupts off while RV = 1 (the 68000's vectors are then the cart's), and
| starts the 32X the cartridge way.  Not set: this file assembles exactly as before.
        .ifndef OL_MODE1
        .equ    OL_MODE1, 0
        .endif
| MODE1-2 (Makefile M1WORDS=1, with MODE1=1): every copy to or from the cart whose source, destination and
| length are even goes in words, as edtest wrote the EverDrive (byte writes to it with the 32X on were
| never tried on a console).
        .ifndef OL_M1WORDS
        .equ    OL_M1WORDS, 0
        .endif
| MODE1-4 (diagnostics, EDCD only): M1MINVB=1 - the vblank handler does only the Sub-CPU's level 2
| interrupt (no pads, no gate-array writes); M1NOVINT=1 - the vblank handler skips the Sub-CPU's level 2
| interrupt (its BIOS then gets none: CD reads will stall later, which is expected).
        .ifndef OL_M1MINVB
        .equ    OL_M1MINVB, 0
        .endif
| MODE1-5: M1W0BANK=<n> - the EverDrive bank that window 0 shows (EDCD_W0): the window-1 route to
| window 0's addresses maps that bank, not bank 0.
        .ifndef OL_M1W0BANK
        .equ    OL_M1W0BANK, 0
        .endif
| MODE1-7 (M1SAVE=1, with MODE1=1): the EverDrive's save bank.  Bank 31 is the EverDrive PRO's battery
| RAM, which its OS keeps on the SD card (MEGA/gamedata/<launcher>/bram.srm, 256 KB) when the launcher's
| header declares save RAM; the 16 MB of banks 0-29 are PSRAM, which the OS wipes every time it returns
| to its menu (edsave, console, 25 Oct 2026).  A copy whose cart side is 0x800000-0x83FFFF goes to or
| from that bank: window 1 = bank 31 for the block, the copy at 0x080000 + (address - 0x800000),
| window 1 = bank 1 again - the window-1 route of MODE1-3 with another bank.  Only the first 256 KB:
| the OS keeps its own things above.  Not set: m1_cart assembles exactly as before.
        .ifndef OL_M1SAVE
        .equ    OL_M1SAVE, 0
        .endif
| MODE1-7b (EDSAVE-3 tests): M1SAVEBANK=<n> the bank the saves go to (31; 27 = a PSRAM bank, for a test:
| nothing persists), M1SAVEWIN=<1-7> the window the bank is shown in for the block (1).  Defaults = MODE1-7.
        .ifndef OL_M1SAVEBANK
        .equ    OL_M1SAVEBANK, 31
        .endif
        .ifndef OL_M1SAVEWIN
        .equ    OL_M1SAVEWIN, 1
        .endif
| MODE1-9 (M1PAGE=1, with MODE1=1): the paged cart's page registers on the EverDrive.  The paged games
| (OpenJazz, Hurrican, SuperTux, Sonic CD) switch 2 MB pages by one byte written with RV = 1 to 0xA130F2
| (window 1 = cart 0x200000-0x3FFFFF) or 0xA130F0 (window 0), through the listener's safe_cart_xfer.  On
| the EverDrive those addresses are CTRL0/CTRL1 and a page is four of its 512 KB banks: page p in
| window 1 = CTRL4..7 = 4p..4p+3, in window 0 = CTRL0's bank + CTRL1..3.  m1_cart catches the write
| and does that instead; m1_wb keeps what each EverDrive window shows, so the window-1 routes
| (MODE1-3, MODE1-7) put the right bank back.  Page 7 holds bank 31 (the OS's): a game's EverDrive
| build must never ask for it (the games' CART_ED probes stop at page 6).  Not set: nothing assembles.
        .ifndef OL_M1PAGE
        .equ    OL_M1PAGE, 0
        .endif
| MODE1-6: the EverDrive's write-enable bit (CTRL0 bit 13, W) is OFF except inside a copy INTO the
| cart.  Console, 25 Oct 2026 (edw0): with W on, some of the 68000's writes to OTHER addresses (work
| RAM, the VDP, the gate array - what a listener does all the time) land in the cart, and the SH2's
| reads of the cart come back as zeros meanwhile; with W off, none of that.  So W goes on after the
| call's own stack push, with RV already holding the SH2 off the cart, and off again before any other
| write.  M1CTRL0 = the launcher's CTRL0 without W (default 0x8000: P; 0xC000 with the X bit).
        .ifndef OL_M1CTRL0
        .equ    OL_M1CTRL0, 0x8000
        .endif
        .equ    OL_M1CTRL0_W1, OL_M1CTRL0 + 0x2000 + OL_M1W0BANK   /* + not |: | starts a comment here */
        .equ    OL_M1CTRL0_W0, OL_M1CTRL0 + OL_M1W0BANK
        .ifndef OL_M1NOVINT
        .equ    OL_M1NOVINT, 0
        .endif


        .text

        .align  2

| int do_md_cmd0(int cmd);
        .global do_md_cmd0
do_md_cmd0:
        move.l  4(sp),d0
        bsr     send_md_cmd
        bra     wait_md_cmd

| int do_md_cmd1(int cmd, int arg1);
        .global do_md_cmd1
do_md_cmd1:
        move.l  4(sp),d0
        move.l  8(sp),0x8020.w
        bsr     send_md_cmd
        bra     wait_md_cmd

| int do_md_cmd2(int cmd, int arg1, int arg2);
        .global do_md_cmd2
do_md_cmd2:
        move.l  4(sp),d0
        move.l  8(sp),0x8020.w
        move.l  12(sp),0x8024.w
        bsr     send_md_cmd
        bra     wait_md_cmd

| int do_md_cmd3(int cmd, int arg1, int arg2, int arg3);
        .global do_md_cmd3
do_md_cmd3:
        move.l  4(sp),d0
        move.l  8(sp),0x8020.w
        move.l  12(sp),0x8024.w
        move.l  16(sp),0x8028.w
        bsr     send_md_cmd
        bra     wait_md_cmd

| int do_md_cmd4(int cmd, int arg1, int arg2, int arg3, int arg4);
        .global do_md_cmd4
do_md_cmd4:
        move.l  4(sp),d0
        move.l  8(sp),0x8020.w
        move.l  12(sp),0x8024.w
        move.l  16(sp),0x8028.w
        move.l  20(sp),0x802C.w
        bsr     send_md_cmd
        bra     wait_md_cmd

| void send_md_cmd0(int cmd);
        .global send_md_cmd0
send_md_cmd0:
        move.l  4(sp),d0
        bra.b   send_md_cmd

| void send_md_cmd1(int cmd, int arg1);
        .global send_md_cmd1
send_md_cmd1:
        move.l  4(sp),d0
        move.l  8(sp),0x8020.w
        bra.b   send_md_cmd

| void send_md_cmd2(int cmd, int arg1, int arg2);
        .global send_md_cmd2
send_md_cmd2:
        move.l  4(sp),d0
        move.l  8(sp),0x8020.w
        move.l  12(sp),0x8024.w
        bra.b   send_md_cmd

| void send_md_cmd3(int cmd, int arg1, int arg2, int arg3);
        .global send_md_cmd3
send_md_cmd3:
        move.l  4(sp),d0
        move.l  8(sp),0x8020.w
        move.l  12(sp),0x8024.w
        move.l  16(sp),0x8028.w
        bra.b   send_md_cmd

| void send_md_cmd4(int cmd, int arg1, int arg2, int arg3, int arg4);
        .global send_md_cmd4
send_md_cmd4:
        move.l  4(sp),d0
        move.l  8(sp),0x8020.w
        move.l  12(sp),0x8024.w
        move.l  16(sp),0x8028.w
        move.l  20(sp),0x802C.w

send_md_cmd:
        tst.b   0x800E.w
        bne.b   send_md_cmd
        move.b  d0,0x800F.w
0:
        cmp.b   0x800E.w,d0
        bne.b   0b
        cmpi.b  #MD_CMD_DMA_SCREEN,d0
        beq.b   1f
        move.b  #0,0x800F.w
1:
        rts

| int wait_md_cmd(void);
        .global wait_md_cmd
wait_md_cmd:
        move.b  #0,0x800F.w
0:
        tst.b   0x800E.w
        bne.b   0b
        move.l  0x8010.w,d0
        rts

| void switch_banks(void);
| Switch 1M Banks
        .global switch_banks
switch_banks:
        bchg    #0,0x8003.w
0:
        btst    #1,0x8003.w
        bne.b   0b
        rts

| Initialize the SCD hardware & MD code and hardware (called from crt0)
        .global init_hardware
init_hardware:
        lea     md_init_start(pc),a0
        lea     0x080000,a1
        lea     md_init_end(pc),a2
1:
        move.l  (a0)+,(a1)+
        cmpa.l  a0,a2
        bhi.b   1b
        bset    #0,0x8003.w
        move.b  #'M,0x800F.w
2:
        cmpi.b  #'M,0x800E.w
        bne.b   2b
        move.b  #0,0x800F.w
3:
        tst.b   0x800E.w
        bne.b   3b

        bset    #2,0x8003.w
        move    #0x2000,sr
        rts


| Copied to Word RAM for MD to execute
        .align  4
md_init_start:
        move.b  #'M,0xA1200E
        lea     md_start(pc),a0
        lea     0xFF1000,a1
        lea     md_init_end(pc),a2
1:
        move.l  (a0)+,(a1)+
        cmpa.l  a0,a2
        bhi.b   1b
        jmp     0xFF1000

        .align  4
md_start:
        move.l  #0,0xA12010
        move.l  #0,0xA12014
        move.l  #0,0xA12018
        move.l  #0,0xA1201C
        bsr.w   md_init_hw
        moveq   #0,d7
cmd_done:
        tst.b   0xA1200F
        bne.b   cmd_done
        tst.b   0xA1200F                /* HW10: read twice - a torn read must not end the */
        bne.b   cmd_done                /*       handshake early                            */
        move.b  #0,0xA1200E

        /* main loop */
        moveq   #0,d0
cmd_loop:
        tst.b   d7
        beq.b   md_check
        move.w  0xA15120,d0
        beq.b   md_check

mars_done:
        moveq   #0,d0
        move.w  d0,0xA15120
md_check:
        move.b  0xA1200F,d0
        beq.b   cmd_loop
        cmp.b   0xA1200F,d0             /* HW10: the command byte must read the same twice.  */
        bne.b   md_check                /* WHY (console, 68K EXC V09 SR=A71E, args 0091FFFE): */
                                        /* a read overlapping the Sub-CPU's write returned 2  */
                                        /* instead of 19 -> set_sr(0x0091FFFE): trace on,     */
                                        /* interrupts off; or a silent deadlock (wrong echo). */
        move.b  d0,0xA1200E

        /* push args on stack */
        .if OL_MODE1
        move.l  0xA1202C,d1             /* MODE1-1: each argument moved to the cartridge-boot map */
        bsr     m1_arg
        move.l  d1,-(sp)
        move.l  0xA12028,d1
        bsr     m1_arg
        move.l  d1,-(sp)
        move.l  0xA12024,d1
        bsr     m1_arg
        move.l  d1,-(sp)
        move.l  0xA12020,d1
        bsr     m1_arg
        move.l  d1,-(sp)
        .else
        move.l  0xA1202C,-(sp)
        move.l  0xA12028,-(sp)
        move.l  0xA12024,-(sp)
        move.l  0xA12020,-(sp)
        .endif

        cmpi.b  #'M,d0
        bne.b   2f
1:
        btst    #0,0xA12003
        beq.b   1b
        .if OL_MODE1
        jmp     0x600000                /* MODE1-1: Word RAM in a cartridge boot */
        .else
        jmp     0x200000
        .endif
2:
        move.l  #-1,0xA12010
        cmpi.b  #MD_CMD_END,d0
        bhs.b   3f
.ifdef DX_VSPLIT                            | DX-VSPLIT begin
        move.w  #0x2400,d1              | VSPLIT: the routines that touch the VDP run with the H interrupt
        cmpi.b  #3,d0                   | masked (the handler's VSRAM write would move the VDP's address
        beq.b   4f                      | mid-copy; a split due meanwhile runs late). D5: the ones that
        cmpi.b  #11,d0                  | don't - get_pad (3), delay (11) and the 32X register reads and
        beq.b   4f                      | writes (19-22) every command makes - run with it on, so they
        cmpi.b  #19,d0                  | never hold a split back
        blo.b   5f
        cmpi.b  #22,d0
        bhi.b   5f
4:
        move.w  #0x2000,d1
5:
.endif                                      | DX-VSPLIT end
        add.w   d0,d0
        lea     cmd_table(pc),a0
        move.w  0(a0,d0.w),d0
.ifdef DX_VSPLIT                            | DX-VSPLIT begin
        move.w  d1,sr
        jsr     0(a0,d0.w)
        move.w  #0x2000,sr
.else                                       | DX-VSPLIT else
        jsr     0(a0,d0.w)
.endif                                      | DX-VSPLIT end
        move.l  d0,0xA12010
3:
        lea     16(sp),sp
        bra     cmd_done

        .if OL_MODE1
| MODE1-1: d1 = an argument of command d0, as the Sub-CPU gives it (CD-boot addresses) -> d1 for a
| cartridge boot.  Word RAM 0x200000-0x3FFFFF -> 0x600000-0x7FFFFF; the cart 0x400000-0x7FFFFF ->
| 0x000000-0x3FFFFF (with RV = 1, as every cart routine here sets it).  Commands 19-22 (32X register
| reads and writes) are left alone: their values are data (pads, lengths, chunk numbers), not addresses.
m1_arg:
        cmpi.b  #19,d0
        blo.b   1f
        cmpi.b  #22,d0
        bls.b   9f
1:
        cmpi.l  #0x200000,d1
        blo.b   9f
        cmpi.l  #0x800000,d1
        bhs.b   9f
        cmpi.l  #0x400000,d1
        bhs.b   2f
        addi.l  #0x400000,d1            /* Word RAM */
        rts
2:
        subi.l  #0x400000,d1            /* the cart */
9:
        rts
        .endif

cmd_table:
        .word   cmd_done - cmd_table
        .word   md_init_hw - cmd_table
        .word   cmd_nop - cmd_table      /* 2: HW10 set_sr disabled - never sent by the game; a
                                            garbled command 2 must not touch the status register */
        .word   get_pad - cmd_table
        .word   clear_b - cmd_table
        .word   set_vram - cmd_table
        .word   next_vram - cmd_table
        .word   copy_vram - cmd_table
        .word   clear_a - cmd_table
        .word   put_str - cmd_table
        .word   put_chr - cmd_table
        .word   delay - cmd_table
        .word   set_palette - cmd_table
        .word   z80_busrequest - cmd_table
        .word   z80_reset - cmd_table
        .word   z80_memclr - cmd_table
        .word   z80_memcpy - cmd_table
        .word   dma_screen - cmd_table
        .word   init_32x - cmd_table
        .word   read_32x_16 - cmd_table         /* 19 */
        .word   write_32x_16 - cmd_table        /* 20 */
        .word   read_32x_32 - cmd_table         /* 21 */
        .word   write_32x_32 - cmd_table        /* 22 */
        .word   copy_to_cart - cmd_table        /* 23 */
        .word   copy_from_cart - cmd_table      /* 24 */
        .word   copy_words - cmd_table          /* 25 */
        .word   safe_cart_xfer - cmd_table      /* 26 */
        .word   safe_cart_xfer_w - cmd_table    /* 27 */
        .word   bram_put - cmd_table            /* 28 */
        .word   bram_to_fb - cmd_table          /* 29 */
        .word   bram_get - cmd_table            /* 30 */
        .word   install_exc - cmd_table         /* 31 */
        .word   chilly_memcpy - cmd_table       /* 32 */
        .word   chilly_rv_memcpy - cmd_table    /* 33 */
        .word   set_vdp_reg - cmd_table         /* 34  KOBO K19 */
        .word   start_d32xr - cmd_table         /* 35  D32X M1 */
.ifdef DX_TILEP                             | DX-TILEP begin
.ifndef DX_GENPLANE
        .word   cmd_nop - cmd_table             /* 36  (its switch off: TILEP keeps the numbers) */
        .word   cmd_nop - cmd_table             /* 37  (its switch off: TILEP keeps the numbers) */
        .word   cmd_nop - cmd_table             /* 38  (its switch off: TILEP keeps the numbers) */
        .word   cmd_nop - cmd_table             /* 39  (its switch off: TILEP keeps the numbers) */
        .word   tile_copy - cmd_table           /* 40  TILEP */
.endif
.endif                                      | DX-TILEP end
.ifdef DX_GENPLANE                          | DX-GENPLANE begin
        .word   set_vsram - cmd_table           /* 36  SuperTux REV2 */
        .word   copy_vram_col - cmd_table       /* 37  SuperTux REV4 */
.ifdef DX_TILEP                             | DX-TILEP begin
.ifndef DX_VSPLIT
        .word   cmd_nop - cmd_table             /* 38  (its switch off: TILEP keeps the numbers) */
        .word   cmd_nop - cmd_table             /* 39  (its switch off: TILEP keeps the numbers) */
        .word   tile_copy - cmd_table           /* 40  TILEP */
.endif
.endif                                      | DX-TILEP end
.ifdef DX_VSPLIT                            | DX-VSPLIT begin
        .word   set_vs_line - cmd_table         /* 38  VSPLIT */
.ifdef DX_FBVDP                             | DX-FBVDP begin
        .word   fb_list - cmd_table             /* 39  FBVDP (needs VSPLIT) */
.endif                                      | DX-FBVDP end
.ifdef DX_TILEP                             | DX-TILEP begin
.ifndef DX_FBVDP
        .word   cmd_nop - cmd_table             /* 39  (its switch off: TILEP keeps the numbers) */
.endif
        .word   tile_copy - cmd_table           /* 40  TILEP */
.endif                                      | DX-TILEP end

| VSPLIT (needs GENPLANE): the V scroll of planes A and B changed part-way down the screen, from a list of
| up to 16 entries, by the Genesis H interrupt (level 4, every line).  Mega-CD manual: on H-INT the main
| CPU jumps to (the high word of the vector at $70) | (the word in $A12006), so $A12006 = the low word of
| hint_vs (this listener runs in work RAM at $FF1000).
|
| void set_vs_line(int index, int entry): index 0-15 = put entry (line << 20 | V scroll A << 10 | V scroll B)
| into the list being filled; index 0x8000 | count = show that list, its first count entries (0 = off: H
| interrupt disabled at once).  Entry 0 is the top of the picture (its line is ignored); the others must be
| in line order.
| D5 (31 Oct 2026): two lists.  Entries go into the one not shown, so the picture on screen never changes
| under it; a commit while the splits run only marks the filled list pending, and hint_vs switches to it
| at the end of the picture (line 224: the same vblank at which the 32X shows a swapped frame).  The first
| commit (splits off until then) shows the list at once.
| Runs in the command loop with level 4 masked (the dispatcher), so nothing changes under hint_vs.
set_vs_line:
        move.l  4(sp),d0
        move.l  8(sp),d1
        lea     vs_vars(pc),a1
        btst    #15,d0
        bne.b   1f
        andi.w  #15,d0
        lsl.w   #2,d0
        add.w   4(a1),d0
        eori.w  #64,d0                  | the list not shown
        move.l  d1,8(a1,d0.w)
        moveq   #0,d0
        rts
1:
        andi.w  #31,d0
        cmpi.w  #16,d0
        bls.b   2f
        moveq   #16,d0
2:
        move.w  #-1,6(a1)               | nothing pending
        tst.w   d0
        beq.b   4f
        tst.w   (a1)
        beq.b   5f
        move.w  d0,6(a1)                | the splits run: switch at the end of this picture
        moveq   #0,d0
        rts
5:
        move.w  d0,(a1)                 | the first list: shown now
        eori.w  #64,4(a1)
        move.w  #1,2(a1)                | vs_pos: the next entry after the top one
        lea     0xC00004,a0
        move.w  #0x8F02,(a0)
        move.l  #0x40000010,(a0)        | VSRAM 0: the top entry now
        move.w  4(a1),d0
        move.l  8(a1,d0.w),d1
        move.l  d1,d0
        lsr.l   #8,d0
        lsr.l   #2,d0
        andi.w  #0x3FF,d0
        move.w  d0,0xC00000             | plane A
        andi.w  #0x3FF,d1
        move.w  d1,0xC00000             | plane B
        lea     hint_vs(pc),a1
        move.w  a1,0xA12006             | the H-INT vector's low word -> hint_vs
        move.w  #0x8A00,(a0)            | H interrupt every line
        move.w  #0x8014,(a0)            | register 0: IE1 (H interrupt) on
        moveq   #0,d0
        rts
4:
        move.w  d0,(a1)                 | count 0
        move.w  #0x8004,0xC00004        | register 0: H interrupt off
        moveq   #0,d0
        rts

| the H interrupt, at the end of every line: set up the next line if an entry starts there; after the last
| line switch to a pending list, and put the top entry back for the next picture
hint_vs:
        movem.l d0-d2/a0-a1,-(sp)
        lea     vs_vars(pc),a1
        move.w  0xC00008,d0
        lsr.w   #8,d0                   | V counter: the line just drawn
        addq.w  #1,d0                   | the line about to be drawn
        cmpi.w  #224,d0
        blo.b   1f
        move.w  6(a1),d1                | the picture is over: a pending list?
        bmi.b   2f
        move.w  d1,(a1)
        move.w  #-1,6(a1)
        eori.w  #64,4(a1)               | it is shown from the next picture
2:
        move.w  #1,2(a1)                | back to the top entry
        moveq   #0,d2
        bra.b   3f
1:
        move.w  2(a1),d2                | vs_pos
        cmp.w   (a1),d2
        bhs.b   9f
        lsl.w   #2,d2
        move.w  d2,d1
        add.w   4(a1),d1
        move.l  8(a1,d1.w),d1
        swap    d1
        lsr.w   #4,d1                   | its line
        cmp.w   d1,d0
        blo.b   9f                      | not yet
        addq.w  #1,2(a1)
3:
        add.w   4(a1),d2
        move.l  8(a1,d2.w),d1
        lea     0xC00004,a0
        move.w  #0x8F02,(a0)
        move.l  #0x40000010,(a0)
        move.l  d1,d0
        lsr.l   #8,d0
        lsr.l   #2,d0
        andi.w  #0x3FF,d0
        move.w  d0,0xC00000
        andi.w  #0x3FF,d1
        move.w  d1,0xC00000
9:
        movem.l (sp)+,d0-d2/a0-a1
        rte

        .align  2
vs_vars:
        .word   0, 1, 0, -1             | vs_count, vs_pos, the shown list (0 / 64 bytes), pending count (-1 none)
        .long   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0     | list 0
        .long   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0     | list 1
.endif                                      | DX-VSPLIT end

| DX-GENPLANE: SuperTux CD32X REV's two listener routines, unchanged (MicroNut99/SuperTuxCD32X_REV
| cd32x/subcpu/hw_md.s).
| SuperTux REV2: void set_vsram(int addr, int word);
| One word into the VDP's vertical scroll RAM (VSRAM) at byte address addr (0 = plane A,
| 2 = plane B in full-screen mode; up to 0x4E in 2-cell column mode).  WHY a command of its
| own: VSRAM is not video RAM - set_vram's address command always targets VRAM (0x4000....),
| a VSRAM write needs the code 0x40000010.  Same shape as set_vram: auto-increment 2, the
| 32-bit address command in ONE move.l (an interrupt cannot split it), then the data word.
| Runs in the command loop, never inside an interrupt.
set_vsram:
        lea     0xC00000,a1
        move.w  #0x8F02,4(a1)
        move.l  4(sp),d1
        andi.l  #0x0000007E,d1
        swap    d1
        ori.l   #0x40000010,d1
        move.l  d1,4(a1)
        move.l  8(sp),d0
        move.w  d0,(a1)
        moveq   #0,d0
        rts

| SuperTux REV4: void copy_vram_col(int addr, short *src, int words);
| copy_vram with the VDP's auto-increment at 128 bytes instead of 2: the words go DOWN one
| column of a 64-cell-wide plane map (one map row is 64 words).  WHY: a plane map is stored row
| by row, so a new column scrolling in is 28-30 words 128 bytes apart - with copy_vram that would
| be one command a word.  The increment is put back to 2 afterwards (every other routine sets it
| itself too).  Runs in the command loop, never inside an interrupt.
copy_vram_col:
        lea     0xC00000,a1
        move.w  #0x8F80,4(a1)
        move.l  4(sp),d1
        lsl.l   #2,d1
        lsr.w   #2,d1
        swap    d1
        ori.l   #0x40000000,d1
        move.l  d1,4(a1)
        movea.l 8(sp),a0
        move.l  12(sp),d0
        subq.w  #1,d0
0:
        move.w  (a0)+,(a1)
        dbra    d0,0b
        move.w  #0x8F02,4(a1)
        moveq   #0,d0
        rts

.endif                                      | DX-GENPLANE end
.ifdef DX_FBVDP                             | DX-FBVDP begin
| FBVDP (needs GENPLANE and VSPLIT; DraculaX_32X docs/60): int fb_list(int op, short *list);
| The Genesis VDP written from a list the SH2 put in the 32X frame buffer - the one memory both reach. The SH2
| gives the frame buffer to the 68000 (FM = 0) before the command and takes it back after; nothing here
| touches FM. The list is big-endian words, each run a header and its words:
|   0x0000          the end
|   0x1nnn + n      n words to the VDP's control port (address commands as two words, register writes)
|   0x2nnn + n      n words to the data port
|   0x3nnn + n      kept here (fb_keep, up to FB_KEEP words in all) to run at the commit: runs of 1 and 2 only
|   0x4nnn + n      n / 2 pairs (word index, word) kept (fb_jrn, up to FB_JRN_N words) and written into the frame
|                   buffer at the commit, after its runs (DraculaX docs/65: the SH2 sees only the buffer not shown,
|                   so the tables it keeps in both go into the other one once the swap has been; the SH2 gives the
|                   68000 the frame buffer for the commit then)
| op 0: run the list at 'list' (68000 address: 0x840000 + the SH2's frame buffer offset); op 1: the commit -
| run what was kept, then write the pairs, then forget both. Ops 2 / 3 (DraculaX docs/66): run the list as op 0,
| then show 32X frame buffer 0 / 1 (FBCTL FS, written here: the 32X swaps at the next vblank) and run the kept
| runs from vblank_int at that vblank - the scroll and colours change at the line the 32X's picture does, however
| late the Sub-CPU is. A list that ends in a vertical blank waits for the display first (DraculaX docs/68: a swap at
| once in a blank came before the SH2's colours for it); if the swap is shown already all the same, the kept runs and
| the pairs now.
| COMM10 = 0xFB00 | the buffer once FS is written (COMM8L, the list's offset, is read before). Op 1 then writes
| the pairs (the SH2 sends it once it sees the swap). Kept runs still armed when a command
| comes (vblank_int held off) are run first. Returns 1, or 0 (a header it doesn't know, or no room to keep: the rest
| is not run). The control and data runs set the VDP's address and increment themselves: nothing is assumed
| of the VDP's state, and the increment is left as the list leaves it. Runs with level 4 masked (the
| dispatcher: hint_vs writes VSRAM), so nothing moves the VDP's address under it.
        .equ    FB_KEEP, 256
        .equ    FB_JRN, 0xFFC000                | (68000 work RAM, 4 KB: nothing else of the stack uses it)
        .equ    FB_JRN_N, 2048
fb_list:
        movem.l d2-d3/a2-a3,-(sp)
        movea.l 24(sp),a0               | (the arguments past the 4 registers saved)
        move.l  20(sp),d3               | op
        bsr.w   fb_overdue              | (runs kept for a vblank whose interrupt has not run them: now)
        cmpi.l  #1,d3
        bne.w   1f                      | ops 0, 2, 3: a list
        lea     fb_keep(pc),a0          | the commit: what was kept, then empty
        move.w  -2(a0),d0
        clr.w   -2(a0)
        add.w   d0,d0
        clr.w   0(a0,d0.w)              | (its end)
1:
        lea     0xC00000,a2
        lea     0xC00004,a1
2:
        move.w  (a0)+,d0
        move.w  d0,d1
        andi.w  #0x0FFF,d1              | the count
        andi.w  #0xF000,d0
        beq.w   8f                      | 0: the end
        subq.w  #1,d1
        bmi.w   2b                      | (no words)
        cmpi.w  #0x1000,d0
        beq.w   3f
        cmpi.w  #0x2000,d0
        beq.w   4f
        cmpi.w  #0x4000,d0
        beq.w   10f
        cmpi.w  #0x3000,d0
        bne.w   9f
        cmpi.l  #1,d3
        beq.w   9f                      | (kept runs hold no keep)
        lea     fb_keep(pc),a3          | 0x3nnn: kept
        move.w  -2(a3),d0
        move.w  d0,d2
        add.w   d1,d2
        cmpi.w  #FB_KEEP-2,d2           | (room for its words and the end word)
        bhs.w   9f
        addq.w  #1,d2
        move.w  d2,-2(a3)
        add.w   d0,d0
        adda.w  d0,a3
5:
        move.w  (a0)+,(a3)+
        dbra    d1,5b
        bra.w   2b
3:
        move.w  (a0)+,(a1)              | 0x1nnn: control
        dbra    d1,3b
        bra.w   2b
10:
        cmpi.l  #1,d3
        beq.w   9f                      | (no pairs inside the commit's runs)
        lea     fb_jrn_n(pc),a3         | 0x4nnn: pairs kept for the commit
        move.w  (a3),d0
        move.w  d0,d2
        add.w   d1,d2
        cmpi.w  #FB_JRN_N-1,d2
        bhs.w   9f
        addq.w  #1,d2
        move.w  d2,(a3)
        lea     FB_JRN,a3
        add.w   d0,d0
        adda.w  d0,a3
11:
        move.w  (a0)+,(a3)+
        dbra    d1,11b
        bra.w   2b
4:
        move.w  (a0)+,(a2)              | 0x2nnn: data
        dbra    d1,4b
        bra.w   2b
8:
        cmpi.l  #1,d3
        beq.w   13f                     | op 1: the pairs
        cmpi.l  #2,d3
        blo.w   12f                     | op 0: done
16:
        btst    #3,0xC00005             | (the list ended in a vertical blank: wait for the display first, so the
        bne.b   16b                     | swap is at the next vblank and the SH2 sees it there, as always)
        move.w  sr,-(sp)                | ops 2 / 3: the 32X to show frame buffer op - 2 (it swaps at the next
        move.w  #0x2700,sr              | vblank, at once in a blank), the kept runs at that vblank
        moveq   #1,d0
        and.w   d3,d0
        move.w  d0,0xA1518A             | (FBCTL FS: the frame buffer is the 68000's, FM = 0)
        move.w  d0,d1
        ori.w   #0xFB00,d1
        move.w  d1,0xA1512A             | (COMM10 = 0xFB00 | the buffer: the swap is asked for - the SH2's vblank
                                        | interrupt knows the swap is at this blank before the command's end)
        moveq   #7,d1
14:
        dbra    d1,14b
        move.w  0xA1518A,d1
        eor.w   d0,d1
        btst    #0,d1
        bne.b   15f
        bsr.w   fb_vrun                 | shown already (the list ended in a blank): the kept runs now,
        move.w  (sp)+,sr                | and the pairs (the buffer not shown is the one drawn in next)
        bra.w   13f
15:
        lea     fb_armed(pc),a3         | not yet: vblank_int runs them
        move.w  #1,(a3)
        move.w  (sp)+,sr
        bra.w   12f
13:
        lea     fb_jrn_n(pc),a3         | the commit, after its runs: the pairs kept into the frame buffer (the
        move.w  (a3),d0                 | one the SH2 draws in next: the swap has been)
        beq.w   12f
        clr.w   (a3)
        lea     FB_JRN,a2
        lsr.w   #1,d0
        subq.w  #1,d0
6:
        moveq   #0,d1
        move.w  (a2)+,d1
        add.l   d1,d1
        movea.l d1,a1
        adda.l  #0x840000,a1
        move.w  (a2)+,(a1)
        dbra    d0,6b
12:
        moveq   #1,d0
        bra.w   7f
9:
        moveq   #0,d0
7:
        movem.l (sp)+,d2-d3/a2-a3
        rts

| fb_vrun: the kept runs now, then nothing kept or armed (called with interrupts masked, or from vblank_int)
fb_vrun:
        movem.l d0-d1/a0-a2,-(sp)
        lea     fb_armed(pc),a0
        clr.w   (a0)
        lea     fb_keep(pc),a0
        move.w  -2(a0),d0
        clr.w   -2(a0)
        add.w   d0,d0
        clr.w   0(a0,d0.w)              | (their end)
        lea     0xC00000,a2
        lea     0xC00004,a1
1:
        move.w  (a0)+,d0
        move.w  d0,d1
        andi.w  #0x0FFF,d1
        andi.w  #0xF000,d0
        beq.b   9f
        subq.w  #1,d1
        bmi.b   1b
        cmpi.w  #0x1000,d0
        beq.b   3f
2:
        move.w  (a0)+,(a2)              | (data)
        dbra    d1,2b
        bra.b   1b
3:
        move.w  (a0)+,(a1)              | (control)
        dbra    d1,3b
        bra.b   1b
9:
        movem.l (sp)+,d0-d1/a0-a2
        rts

| fb_overdue: runs armed for a vblank whose interrupt did not run them (interrupts off meanwhile): now
fb_overdue:
        move.w  fb_armed(pc),d0
        beq.b   1f
        move.w  sr,-(sp)
        move.w  #0x2700,sr
        move.w  fb_armed(pc),d0
        beq.b   2f
        bsr.w   fb_vrun
2:
        move.w  (sp)+,sr
1:
        rts

        .align  2
fb_armed:
        .word   0                       | 1: the kept runs wait for the vblank (ops 2 / 3)
fb_jrn_n:
        .word   0                       | fb_jrn's count, in words
        .word   0                       | fb_keep's count, in words
fb_keep:
        .space  FB_KEEP*2
.endif                                      | DX-FBVDP end

| D32X M1: void start_d32xr(void) - never returns.
| D32XR's ROM is in the RAM cart.  Interrupts off, cart bank 0, RV off (the SH2 must reach the
| cart), then jump to D32XR's Genesis entry _start (0x880800 = cart offset 0x800 through the
| 32X window).  D32XR clears work RAM (this listener included) and, thanks to the CD-boot patch
| in its ROM (tools/patch_d32xr_cdboot.py), rebuilds the BIOS interrupt jumps itself.
start_d32xr:
        move.w  #0x2700,sr
        move.l  #0xC0000000,0xC00004    /* M1b checkpoint: backdrop (CRAM 0) RED = the 68K */
        move.w  #0x000E,0xC00000        /* got this command and is about to jump            */
        move.w  #0,0xA15104             /* cart bank select = 0 */
        move.b  #0,0xA15107             /* RV off */
        jmp     0x880800

| KOBO K19: void set_vdp_reg(int word);
| Writes one VDP register word (0x8Rvv) to the VDP control port.  Used to switch the
| Genesis display on/off (register 1: 0x8174 = display on, 0x8134 = display off - same
| value md_init_hw uses, only bit 6 "display enable" differs; bit 5 (V-int) stays ON so
| the vblank handler keeps reading the pads).  Runs in the command loop, never inside an
| interrupt, so it cannot split another routine's VDP address write.
set_vdp_reg:
        move.l  4(sp),d0
        move.w  d0,0xC00004
        moveq   #0,d0
        rts

| void md_init_hw(void);
| initialize MD hardware
md_init_hw:
        movem.l d2-d7/a2-a5,-(sp)
        move.w  #0x2700,sr

| init joyports
        lea     0xA10000,a5
        move.b  #0x40,0x09(a5)
        move.b  #0x40,0x0B(a5)
        move.b  #0x40,0x03(a5)
        move.b  #0x40,0x05(a5)

        lea     0xC00000,a3
        lea     0xC00004,a4

| wait on VDP DMA
        move.w  #0x8114,(a4)
0:
        move.w  (a4),d0
        btst    #1,d0
        bne.b   0b

        moveq   #0,d0
        move.w  #0x8000,d5
        move.w  #0x0100,d7

| Set VDP registers
        lea     InitVDPRegs(pc),a5
        moveq   #18,d1
1:
        move.b  (a5)+,d5
        move.w  d5,(a4)
        add.w   d7,d5
        dbra    d1,1b

| clear VRAM
        move.w  #0x8F02,(a4)
        move.l  #0x40000000,(a4)
        move.w  #0x7FFF,d1
2:
        move.w  d0,(a3)
        dbra    d1,2b

| Clear CRAM
        lea     InitVDPRAM(pc),a5
        move.l  (a5)+,(a4)
        move.l  (a5)+,(a4)
        moveq   #31,d3
3:
        move.l  d0,(a3)
        dbra    d3,3b

| Clear VSRAM
        move.l  (a5)+,(a4)
        moveq   #19,d4
4:
        move.l  d0,(a3)
        dbra    d4,4b

| halt Z80 and init FM chip
        move.w  #0x100,0xA11100
        move.w  #0x100,0xA11200

| reset YM2612
        lea     FMReset(pc),a5
        lea     0xA00000,a0
        move.w  #0x4000,d1
        moveq   #26,d2
5:
        move.b  (a5)+,d1
        move.b  (a5)+,0(a0,d1.w)
        nop
        nop
        dbra    d2,5b

        moveq   #0x30,d0
        moveq   #0x5F,d2
6:
        move.b  d0,0x4000(a0)
        nop
        nop
        move.b  #0xFF,0x4001(a0)
        nop
        nop
        move.b  d0,0x4002(a0)
        nop
        nop
        move.b  #0xFF,0x4003(a0)
        nop
        nop
        addq.b  #1,d0
        dbra    d2,6b

| reset PSG
        lea     PSGReset(pc),a5
        lea     0xC00000,a0
        move.b  (a5)+,0x0011(a0)
        move.b  (a5)+,0x0011(a0)
        move.b  (a5)+,0x0011(a0)
        move.b  (a5),0x0011(a0)

| load font tile data
        move.w  #0x8F02,(a4)
        move.l  #0x40000000,(a4)
        lea     font_data(pc),a0
        move.w  #0x6B*8-1,d2
7:
        move.l  (a0)+,d0
        move.l  d0,d1
        not.l   d1
        andi.l  #0x11111111,d0
        andi.l  #0x00000000,d1
        or.l    d1,d0
        move.l  d0,(a3)
        dbra    d2,7b

| set default palette
        move.l  #0xC0000000,(a4)
        move.l  #0x00000CCC,(a3)
        move.l  #0xC0200000,(a4)
        move.l  #0x000000A0,(a3)        /* D32X M1: green text VISIBLE again (HW13 had it black)
                                           = BLACK - any green line is invisible.  White (line 0)
                                           and the red crash text (line 2) are unchanged. */
        move.l  #0xC0400000,(a4)
        move.l  #0x0000000A,(a3)

        lea     vblank_int(pc),a0
        move.l  a0,0xFFFD08

        .ifdef  OL_DISPON
        move.w  #0x8174,(a4)            | MODE1-8 (DIAG=1 launcher): the Genesis display stays ON for the green text
        .else
        move.w  #0x8134,(a4)            | OL-13: display OFF (V-int on) - no white boot text
        .endif
        movem.l (sp)+,d2-d7/a2-a5
        move    #0x2000,sr
        rts

InitVDPRegs:
        .byte   0x04, 0x14, 0x30, 0x2C, 0x07, 0x54, 0x00, 0x00
        .byte   0x00, 0x00, 0x00, 0x00, 0x81, 0x2B, 0x00, 0x01
        .byte   0x01, 0x00, 0x00

        .align  2

InitVDPRAM:
        .word   0x8104, 0x8F01
        .word   0xC000, 0x0000
        .word   0x4000, 0x0010

FMReset:
        .byte   0,0x22, 1,0x00
        .byte   0,0x27, 1,0x00
        .byte   0,0x28, 1,0x00, 1,0x04, 1,0x01, 1,0x05, 1,0x02, 1,0x06
        .byte   0,0x2A, 1,0x80, 0,0x2B, 1,0x00
        .byte   0,0xB4, 1,0x00, 0,0xB5, 1,0x00, 0,0xB6, 1,0x00
        .byte   2,0xB4, 3,0x00, 2,0xB5, 3,0x00, 2,0xB6, 3,0x00

PSGReset:
        .byte   0x9f, 0xbf, 0xdf, 0xff

        .align  4

        .include "font.s"

        .align  4

| HW10: harmless stand-in for commands the game never sends at runtime
cmd_nop:
        moveq   #0,d0
        rts

set_sr:
        moveq   #0,d0
        move.w  sr,d0
        move.l  4(sp),d1
        move.w  d1,sr
        rts

get_pad:
        move.l  d2,-(sp)
        move.l  8(sp),d0
        cmpi.w  #1,d0
        bhi     no_pad
        add.w   d0,d0
        addi.l  #0xA10003,d0
        movea.l d0,a0
        bsr.b   get_input
        move.w  d0,d1
        andi.w  #0x0C00,d0
        bne.b   no_pad
        bsr.b   get_input
        bsr.b   get_input
        move.w  d0,d2
        bsr.b   get_input
        andi.w  #0x0F00,d0
        cmpi.w  #0x0F00,d0
        beq.b   common
        move.w  #0x010F,d2
common:
        lsl.b   #4,d2
        lsl.w   #4,d2
        andi.w  #0x303F,d1
        move.b  d1,d2
        lsr.w   #6,d1
        or.w    d1,d2
        eori.w  #0x1FFF,d2
        move.w  d2,d0
        move.l  (sp)+,d2
        rts

no_pad:
        .ifdef  HAS_SMS_PAD
        move.b  (a0),d0
        andi.w  #0x003F,d0
        eori.w  #0x003F,d0
        .else
        move.w  #0xF000,d0
        .endif
        move.l  (sp)+,d2
        rts

get_input:
        move.b  #0x00,(a0)
        nop
        nop
        move.b  (a0),d0
        move.b  #0x40,(a0)
        lsl.w   #8,d0
        move.b  (a0),d0
        rts

clear_b:
        moveq   #0,d0
        lea     0xC00000,a0
        move.w  #0x8F02,4(a0)
        move.l  #0x60000003,d1
        move.l  d1,4(a0)
        move.w  #64*32-1,d1
1:
        move.w  d0,(a0)
        dbra    d1,1b
        rts

set_vram:
        lea     0xC00000,a1
        move.w  #0x8F02,4(a1)
        move.l  4(sp),d1
        lsl.l   #2,d1
        lsr.w   #2,d1
        swap    d1
        ori.l   #0x40000000,d1
        move.l  d1,4(a1)
        move.l  8(sp),d0
        move.w  d0,(a1)
        rts

next_vram:
        move.l  4(sp),d0
        move.w  d0,0xC00000
        rts

copy_vram:
        lea     0xC00000,a1
        move.w  #0x8F02,4(a1)
        move.l  4(sp),d1
        lsl.l   #2,d1
        lsr.w   #2,d1
        swap    d1
        ori.l   #0x40000000,d1
        move.l  d1,4(a1)
        movea.l 8(sp),a0
        move.l  12(sp),d0
        subq.w  #1,d0
0:
        move.w  (a0)+,(a1)
        dbra    d0,0b
        rts

clear_a:
        moveq   #0,d0
        lea     0xC00000,a0
        move.w  #0x8F02,4(a0)
        move.l  #0x40000003,d1
        move.l  d1,4(a0)
        move.w  #64*32-1,d1
1:
        move.w  d0,(a0)
        dbra    d1,1b
        rts

put_str:
        movea.l 4(sp),a0
        move.l  8(sp),d0
        lea     0xC00000,a1
        move.w  #0x8F02,4(a1)
        move.l  16(sp),d1
        lsl.l   #6,d1
        or.l    12(sp),d1
        add.w   d1,d1
        swap    d1
        ori.l   #0x40000003,d1
        move.l  d1,4(a1)
1:
        move.b  (a0)+,d0
        subi.b  #0x20,d0
        move.w  d0,(a1)
        tst.b   (a0)
        bne.b   1b
        rts

put_chr:
        movea.l 4(sp),a0
        move.l  8(sp),d0
        lea     0xC00000,a1
        move.w  #0x8F02,4(a1)
        move.l  16(sp),d1
        lsl.l   #6,d1
        or.l    12(sp),d1
        add.w   d1,d1
        swap    d1
        ori.l   #0x40000003,d1
        move.l  d1,4(a1)
        move.l  a0,d1
        move.b  d1,d0
        subi.b  #0x20,d0
        move.w  d0,(a1)
        rts

delay:
        move.l  4(sp),d0
        add.l   0xA1201C,d0
0:
        cmp.l   0xA1201C,d0
        bgt.b   0b
        rts

set_palette:
        movea.l 4(sp),a0
        move.l  8(sp),d0
        move.l  12(sp),d1
        add.w   d0,d0
        swap    d0
        ori.l   #0xC0000000,d0
        subq.w  #1,d1
        lea     0xC00000,a1
        move.w  #0x8F02,4(a1)
        move.l  d0,4(a1)
0:
        move.w  (a0)+,(a1)
        dbra    d1,0b
        rts

z80_busrequest:
        move.l  4(sp),d0
        andi.w  #0x0100,d0
        move.w  d0,0xA11100
0:
        move.w  0xA11100,d1
        and.w   d0,d1
        bne.b   0b
        rts

z80_reset:
        move.l  4(sp),d0
        andi.w  #0x0100,d0
        move.w  d0,0xA11200
        rts

z80_memclr:
        movea.l 4(sp),a1
        move.l  8(sp),d0
        subq.w  #1,d0
        moveq   #0,d1
0:
        move.b  d1,(a1)+
        dbra    d0,0b
        rts

z80_memcpy:
        movea.l 4(sp),a1
        movea.l 8(sp),a0
        move.l  12(sp),d0
        subq.w  #1,d0
0:
        move.b  (a0)+,d1
        move.b  d1,(a1)+
        dbra    d0,0b
        rts

dma_screen:
        move.l  4(sp),d0
        move.l  8(sp),d1
        movem.l d2-d7/a2-a6,-(sp)
        move.w  #0x2700,sr
        move.l  #0x93E0948C,d2
        tst.b   d1
        beq.b   0f
        move.l  #0x934094AD,d2
0:
        move.l  #0x96009500,d3
        lsr.l   #1,d0
        move.b  d0,d3
        swap    d3
        lsr.l   #8,d0
        move.b  d0,d3
        move.l  #0x81149700,d4
        lsr.l   #8,d0
        andi.w  #0x007F,d0
        move.b  d0,d4
        swap    d4
        move.l  #0xC0000080,d5
        moveq   #0,d0
        lea     0xC00000,a2
        lea     0xC00004,a3
        moveq   #31,d1
        move.l  #0xC0000000,(a3)
1:
        move.l  d0,(a2)
        dbra    d1,1b
        move.w  #0x8F00,(a3)
        cmpi.l  #0x934094AD,d2
        beq.b   dma_wide
        move.w  #0x8C00,(a3)
        bra.w   dma_narrow

dma_wide:
        moveq   #0,d0
        move.w  #0x8154,(a3)
        move.l  #0x40000000,(a3)
1:
        btst    #3,1(a3)
        beq.b   1b
2:
        btst    #3,1(a3)
        bne.b   2b

        move.l  d0,(a2)
        move.l  d0,(a2)
        move.l  d0,(a2)
        move.l  d0,(a2)
        move.l  d0,(a2)
        move.l  d0,(a2)
        move.w  d0,(a2)
        nop
        nop
        nop
        nop
        nop
        nop
        move.l  d2,(a3)
        nop
        nop
        move.l  d3,(a3)
        nop
        nop
        move.l  d4,(a3)
        nop
        nop
        move.l  d5,(a3)
        btst    #7,0xA1200F
        beq.b   4f
        bset    #1,0xA12003
3:
        btst    #1,0xA12003
        bne.b   3b
4:
        pea     0.w
        bsr.w   get_pad
        addq.l  #4,sp
        move.w  d0,0xA12018
        pea     1.w
        bsr.w   get_pad
        addq.l  #4,sp
        move.w  d0,0xA1201A
        move.l  0xA1201C,d0
        addq.l  #1,d0
        move.l  d0,0xA1201C
        move.w  0xA12000,d0
        ori.w   #0x0100,d0
        move.w  d0,0xA12000
        moveq   #0x7F,d0
        and.b   0xA1200F,d0
        cmpi.b  #MD_CMD_DMA_SCREEN,d0
        bne.b   exit_wide
        bra.w   dma_wide
exit_wide:
        bsr.w   md_init_hw
        movem.l (sp)+,d2-d7/a2-a6
        rts

dma_narrow:
        moveq   #0,d0
        move.w  #0x8154,(a3)
        move.l  #0x40000000,(a3)
1:
        btst    #3,1(a3)
        beq.b   1b
2:
        btst    #3,1(a3)
        bne.b   2b

        move.w  d0,(a2)
        move.w  d0,(a2)
        move.w  d0,(a2)
        move.w  d0,(a2)
        move.w  d0,(a2)
        move.w  d0,(a2)
        move.w  d0,(a2)
        move.w  d0,(a2)
        move.w  d0,(a2)
        move.w  d0,(a2)
        move.w  d0,(a2)
        move.w  d0,(a2)
        move.w  d0,(a2)
        nop
        nop
        nop
        nop
        nop
        nop
        move.l  d2,(a3)
        nop
        nop
        move.l  d3,(a3)
        nop
        nop
        move.l  d4,(a3)
        nop
        nop
        move.l  d5,(a3)
        btst    #7,0xA1200F
        beq.b   4f
        bset    #1,0xA12003
3:
        btst    #1,0xA12003
        bne.b   3b
4:
        pea     0.w
        bsr.w   get_pad
        addq.l  #4,sp
        move.w  d0,0xA12018
        pea     1.w
        bsr.w   get_pad
        addq.l  #4,sp
        move.w  d0,0xA1201A
        move.l  0xA1201C,d0
        addq.l  #1,d0
        move.l  d0,0xA1201C
        move.w  0xA12000,d0
        ori.w   #0x0100,d0
        move.w  d0,0xA12000
        moveq   #0x7F,d0
        and.b   0xA1200F,d0
        cmpi.b  #MD_CMD_DMA_SCREEN,d0
        bne.b   exit_narrow
        bra.w   dma_narrow
exit_narrow:
        bsr.w   md_init_hw
        movem.l (sp)+,d2-d7/a2-a6
        rts

init_32x:
        pea     19.l
        pea     12.l
        pea     0.l
        pea     message0(pc)
        bsr     put_str
        lea     16(sp),sp

        move.l  0xA130EC,d0
        cmpi.l  #0x4D415253,d0
        beq.b   0f
        moveq   #-2,d0
        rts
0:
        pea     20.l
        pea     12.l
        pea     0.l
        pea     message1(pc)
        bsr     put_str
        lea     16(sp),sp

        .if OL_MODE1
| MODE1-1: the cartridge boot already started the 32X: its boot ROMs loaded this loader from the cart
| (EDCD.32X's 32X header) and wrote M_OK / S_OK.  No SH2 reset here: after one the boot ROMs would
| look for the cartridge's start-up again.
        move.w  #0x2700,sr
        .else
        lea     0xA15000,a0
        move.l  #500000,d1
1:
        btst    #7,0x0101(a0)
        bne.b   2f
        subq.l  #1,d1
        bne.b   1b
        moveq   #-5,d0
        rts
2:
        move.w  #0x2700,sr
        move.b  #1,0x0101(a0)
        move.w  #19170,d1
1:
        dbra    d1,1b
        .endif

        pea     21.l
        pea     12.l
        pea     0.l
        pea     message2(pc)
        bsr     put_str
        lea     16(sp),sp
        lea     0xA15000,a0

        moveq   #0,d0
        .if OL_MODE1 == 0                   /* MODE1-1: M_OK / S_OK stay; the 32X is on already */
        move.l  d0,0x0120(a0)
        move.l  d0,0x0124(a0)
        move.b  #3,0x0101(a0)
        .endif
2:
        bclr    #7,0x0100(a0)
        bne.b   2b
        move.w  d0,0x0102(a0)
        move.w  d0,0x0104(a0)
        move.w  d0,0x0106(a0)
        move.l  d0,0x0108(a0)
        move.l  d0,0x010C(a0)
        move.w  d0,0x0110(a0)
        move.w  d0,0x0130(a0)
        move.w  d0,0x0132(a0)
        move.w  d0,0x0138(a0)
        move.w  d0,0x0180(a0)
        move.w  d0,0x0182(a0)
3:
        bclr    #0,0x018B(a0)
        bne.b   3b
        moveq   #-1,d1
        lea     0x840000,a1
4:
        move.w  d0,(a1)+
        dbra    d1,4b
5:
        bset    #0,0x018B(a0)
        beq.b   5b
        moveq   #-1,d1
        lea     0x840000,a1
6:
        move.w  d0,(a1)+
        dbra    d1,6b
11:
        bclr    #0,0x018B(a0)
        bne.b   11b

        moveq   #0x7F,d1
        lea     0xA15200,a1
7:
        move.l  d0,(a1)+
        dbra    d1,7b

        pea     22.l
        pea     12.l
        pea     0.l
        pea     message3(pc)
        bsr     put_str
        lea     16(sp),sp
        lea     0xA15000,a0

        .if OL_MODE1 == 0                   /* MODE1-1: no CD boot table, no "_CD_" */
        move.l  0x0120(a0),d0
        cmpi.l  #0x53444552,d0
        bne.b   8f
        move.w  #0x2000,sr
        moveq   #-3,d0
        rts
8:
        pea     23.l
        pea     12.l
        pea     0.l
        pea     message4(pc)
        bsr     put_str
        lea     16(sp),sp
        lea     0xA15000,a0

        movea.l 4(sp),a0
        move.l  8(sp),d0
        lea     0x840000,a1
9:
        move.l  (a0)+,(a1)+
        subq.l  #4,d0
        bgt.b   9b

        pea     24.l
        pea     12.l
        pea     0.l
        pea     0x840000.l
        bsr     put_str
        lea     16(sp),sp

        lea     0xA15000,a0
        .endif
10:
        bset    #7,0x0100(a0)
        beq.b   10b

        .if OL_MODE1 == 0
        move.l  #0x5F43445F,0x0120(a0)
        .endif

        pea     25.l
        pea     12.l
        pea     0.l
        pea     message5(pc)
        bsr     put_str
        lea     16(sp),sp
        lea     0xA15000,a0

        clr.w   -(sp)
        move.l  #1000000,d1
0:
        move.l  0x0120(a0),d0
        cmpi.l  #0x4D5F4F4B,d0
        beq.b   1f
        subq.l  #1,d1
        bne.b   0b
        ori.w   #1,(sp)
1:
        move.l  #250000,d1
2:
        move.l  0x0124(a0),d0
        cmpi.l  #0x535F4F4B,d0
        beq.b   3f
        subq.l  #1,d1
        bne.b   2b
        ori.w   #2,(sp)
3:
        pea     26.l
        pea     12.l
        pea     0.l
        pea     message6(pc)
        bsr     put_str
        lea     16(sp),sp
        lea     0xA15000,a0

        moveq   #0,d7
        moveq   #0,d0
        move.l  d0,0x0120(a0)
        move.l  d0,0x0124(a0)

        move.w  #0x2000,sr
        moveq   #0,d0
        move.w  (sp)+,d0
        rts
15:
        addq.l  #2,sp
        move.l  d0,0xA12018
        move.w  #0x2000,sr
        moveq   #-3,d0
        rts

read_32x_16:
        movea.l 4(sp),a0
        moveq   #0,d0
        move.w  (a0),d0
        rts

write_32x_16:
        movea.l 4(sp),a0
        move.l  8(sp),d0
        move.w  d0,(a0)
        rts

read_32x_32:
        movea.l 4(sp),a0
        move.l  (a0),d0
        rts

write_32x_32:
        movea.l 4(sp),a0
        move.l  8(sp),d0
        move.l  d0,(a0)
        rts

copy_to_cart:
        movea.l 4(sp),a0
        movea.l 8(sp),a1
        move.l  12(sp),d0
        subq.l  #1,d0
        bmi.b   2f
1:
        move.b  (a0)+,(a1)
        addq.l  #2,a1
        dbra    d0,1b
2:
        moveq   #0,d0
        rts

safe_cart_xfer:
        .if OL_MODE1
        movem.l d3,-(sp)                /* MODE1-1: (src, dst, len), bytes, through m1_xfer */
        movea.l 8(sp),a0
        movea.l 12(sp),a1
        move.l  16(sp),d0
        moveq   #0,d3
        bsr     m1_xfer
        movem.l (sp)+,d3
        moveq   #0,d0
        rts
        .endif
        movem.l d2-d3/a2,-(sp)
        movea.l 16(sp),a2
        move.l  20(sp),d2
        move.l  24(sp),d3

        move.w  sr,-(sp)
        move.w  #0x2700,sr

        bset    #0,0xA15107

        movea.l a2,a0
        movea.l d2,a1
        move.l  d3,d0
        beq.b   3f
2:
        move.b  (a0)+,(a1)+
        subq.l  #1,d0
        bne.b   2b
3:
        bclr    #0,0xA15107

        move.w  (sp)+,sr

        moveq   #0,d0
        movem.l (sp)+,d2-d3/a2
        rts

safe_cart_xfer_w:
        .if OL_MODE1
        movem.l d3,-(sp)                /* MODE1-1: (src, dst, len), words, through m1_xfer */
        movea.l 8(sp),a0
        movea.l 12(sp),a1
        move.l  16(sp),d0
        moveq   #1,d3
        bsr     m1_xfer
        movem.l (sp)+,d3
        moveq   #0,d0
        rts
        .endif
        movem.l d2-d3/a2,-(sp)
        movea.l 16(sp),a2
        move.l  20(sp),d2
        move.l  24(sp),d3

        move.w  sr,-(sp)
        move.w  #0x2700,sr
        bset    #0,0xA15107

        movea.l a2,a0
        movea.l d2,a1
        move.l  d3,d0
        ble.b   3f
2:
        move.w  (a0)+,(a1)+
        subq.l  #2,d0
        bgt.b   2b
3:
        bclr    #0,0xA15107
        move.w  (sp)+,sr
        moveq   #0,d0
        movem.l (sp)+,d2-d3/a2
        rts

bram_put:
        .if OL_MODE1
        moveq   #-1,d0                  /* MODE1-1: not in a cartridge boot (Word RAM with RV = 1) */
        rts
        .endif
        movem.l d2-d3/a2,-(sp)
        movea.l 16(sp),a2
        move.l  20(sp),d2
        move.l  24(sp),d3
        move.w  sr,-(sp)
        move.w  #0x2700,sr
        bset    #0,0xA15107
        movea.l a2,a0
        movea.l d2,a1
        move.l  d3,d0
        ble.b   3f
2:
        move.b  (a0)+,(a1)
        addq.l  #2,a1
        subq.l  #1,d0
        bne.b   2b
3:
        bclr    #0,0xA15107
        move.w  (sp)+,sr
        moveq   #0,d0
        movem.l (sp)+,d2-d3/a2
        rts

bram_get:
        .if OL_MODE1
        moveq   #-1,d0                  /* MODE1-1: not in a cartridge boot (Word RAM with RV = 1) */
        rts
        .endif
        movem.l d2-d3/a2,-(sp)
        movea.l 16(sp),a2
        move.l  20(sp),d2
        move.l  24(sp),d3
        move.w  sr,-(sp)
        move.w  #0x2700,sr
        bset    #0,0xA15107
        movea.l a2,a0
        movea.l d2,a1
        move.l  d3,d0
        ble.b   3f
2:
        move.b  (a0),(a1)+
        addq.l  #2,a0
        subq.l  #1,d0
        bne.b   2b
3:
        bclr    #0,0xA15107
        move.w  (sp)+,sr
        moveq   #0,d0
        movem.l (sp)+,d2-d3/a2
        rts

bram_to_fb:
        movem.l d2-d3/a2,-(sp)
        movea.l 16(sp),a2
        move.l  20(sp),d2
        move.l  24(sp),d3
        move.w  sr,-(sp)
        move.w  #0x2700,sr
        bset    #0,0xA15107
        movea.l a2,a0
        movea.l d2,a1
        move.l  d3,d0
        lsr.l   #1,d0
        beq.b   3f
        moveq   #0,d1
2:
        move.b  (a0),d1
        addq.l  #2,a0
        lsl.w   #8,d1
        move.b  (a0),d1
        addq.l  #2,a0
        ori.w   #0x8000,d1
        move.w  d1,(a1)+
        subq.l  #1,d0
        bne.b   2b
3:
        bclr    #0,0xA15107
        move.w  (sp)+,sr
        moveq   #0,d0
        movem.l (sp)+,d2-d3/a2
        rts

copy_from_cart:
        movea.l 4(sp),a0
        movea.l 8(sp),a1
        move.l  12(sp),d0
        lsr.l   #1,d0
        beq.b   2f
        moveq   #0,d1
1:
        move.b  (a0),d1
        addq.l  #2,a0
        lsl.w   #8,d1
        move.b  (a0),d1
        addq.l  #2,a0
        ori.w   #0x8000,d1
        move.w  d1,(a1)+
        subq.l  #1,d0
        bne.b   1b
2:
        moveq   #0,d0
        rts

copy_words:
        movea.l 4(sp),a0
        movea.l 8(sp),a1
        move.l  12(sp),d0
        lsr.l   #1,d0
        beq.b   2f
        subq.l  #1,d0
        move.l  16(sp),d1
        tst.l   d1
        bne.b   3f
1:
        move.w  (a0)+,(a1)+
        dbra    d0,1b
2:
        moveq   #0,d0
        rts
3:
        move.w  (a0)+,d1
        or.w    #0x8000,d1
        move.w  d1,(a1)+
        dbra    d0,3b
        moveq   #0,d0
        rts

message0:
        .if OL_MODE1
        .asciz  "Init 32X M1"           /* MODE1-1: the cartridge-boot listener */
        .else
        .asciz  "Init 32X V2.9"
        .endif
message1:
        .asciz  "Found 32X"
message2:
        .asciz  "Activated 32X"
message3:
        .asciz  "Cleared 32X"
message4:
        .asciz  "32X Waiting"
message5:
        .asciz  "32X Handshake"
message6:
        .asciz  "32X GO!"

        .align  4

vblank_int:
        movem.l d0-d1/a0-a1,-(sp)
.ifdef DX_FBVDP                             | DX-FBVDP begin
        lea     fb_armed(pc),a0         | FBVDP ops 2 / 3: the kept runs at the vblank the 32X shows their picture
        tst.w   (a0)
        beq.b   0f
        bsr.w   fb_vrun
0:
.endif                                      | DX-FBVDP end
        .if OL_MODE1 && OL_M1MINVB
        bra.b   vb_ifl2                 /* MODE1-4: level 2 to the Sub-CPU only */
        .endif

        pea     0.w
        bsr.w   get_pad
        addq.l  #4,sp
        move.w  d0,0xA12018

        pea     1.w
        bsr.w   get_pad
        addq.l  #4,sp
        move.w  d0,0xA1201A

        move.l  0xA1201C,d0
        addq.l  #1,d0
        move.l  d0,0xA1201C
        btst    #0,0xA15101             /* HW7: only once the 32X is on (ADEN bit) */
        beq.b   1f
        nop                     | OpenLara: COMM6 belongs to the renderer (was: move.w d0,0xA15126)             /* HW7: 68K heartbeat -> 32X COMM6 (SH2 reads it 1x/s) */
1:
vb_ifl2:
        .if OL_MODE1 && OL_M1NOVINT
        bra.b   vb_out                  /* MODE1-4: no level 2 to the Sub-CPU */
        .endif

        move.w  0xA12000,d0
        ori.w   #0x0100,d0
        move.w  d0,0xA12000
vb_out:

        movem.l (sp)+,d0-d1/a0-a1
        rte

chilly_memcpy:
        movea.l 4(sp),a1
        movea.l 8(sp),a0
        move.l  12(sp),d0
0:
        move.b  (a0)+,d1
        move.b  d1,(a1)+
        subi.l  #1,d0
        bgt.b   0b
        moveq.l #0,d0
        rts

chilly_rv_memcpy:
        .if OL_MODE1
        movem.l d3,-(sp)                /* MODE1-1: (dst, src, len), bytes, through m1_xfer */
        movea.l 8(sp),a1
        movea.l 12(sp),a0
        move.l  16(sp),d0
        moveq   #0,d3
        bsr     m1_xfer
        movem.l (sp)+,d3
        moveq.l #0,d0
        rts

| MODE1-1: m1_xfer - a copy to or from the cart for the cartridge boot.  a0 = src, a1 = dst, d0 = bytes,
| d3 = 0 bytes / 1 words.  The cart (0x000000-0x3FFFFF) and its registers (0xA130xx) take RV = 1; the
| Sega CD (0x400000-0x7FFFFF: Word RAM) may not be touched while RV = 1.  Console, 25 Oct 2026: copies
| from Word RAM made with RV = 1 came out garbled in cartridge mode (the game "03" became "01", 5AA5
| read back 4880), the same copies with RV = 0 (the splash) did not.  So every block of up to 1 KB
| goes through a buffer in work RAM: the Sega CD side with RV = 0, the cart side with RV = 1 and
| interrupts off (about 5 ms: a vblank waits at most that long).
| MODE1-3: the cart's window 0 (0x000000-0x07FFFF) is reached through window 1.  Console, 25 Oct 2026:
| copies to the cart's first bytes never landed (chunk 0 +6: 03 read back 01, 5AA5 read back 4880; the
| same with bytes, words, bank 8 in window 0, the buffer).  With the 32X on, its adapter answers the
| 68000 there.  edtest only ever wrote 0x080000 and up, so: window 1 = bank 0 for the block, the copy at
| 0x080000 + offset, window 1 = bank 1 again (EDCD's w0_move does the same).  d4: 0 the cart is a1.
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
        .else
        bset    #0,0xA15107
        movea.l 4(sp),a1
        movea.l 8(sp),a0
        move.l  12(sp),d0
0:
        move.b  (a0)+,d1
        move.b  d1,(a1)+
        subi.l  #1,d0
        bgt.b   0b
        bclr    #0,0xA15107
        moveq.l #0,d0
        rts
        .endif

.ifdef DX_TILEP                             | DX-TILEP begin
| TILEP-2 (Metal Slug CD32X, 3 Nov 2026; ledger section 25): int tile_copy(int p, int off) - CMD 99's tiles
| copied by the 68000 alone, the SH2 handing them over through the 32X COMM registers (one command for all
| a picture's tiles; TILEP-1 took some 15 listener calls a tile, ~10 ms).  Window 1 shows page p; off = where
| the tile part ('MSPG') starts in every page.  For entry k (0, 1, ...) the routine puts 0x8000 | k (12 bits)
| in COMM12; the SH2 then puts the leaf's offset in the page / 4 (COMM4, and COMM6 bits 0-2 its bits 16-18),
| the page q holding the tile (COMM6 bits 8-10), its index j in q (COMM8) and the slot s (COMM10), and last
| 0x4000 | k in COMM2; 0x7FFF in COMM2 = no more.  Each tile with RV = 1 and no interrupt: window 1 = page q
| (CTRL4-7 = 4q..4q+3), its rows (128 bytes) and flags (4) to m1_buf; window 1 = what m1_wb says (page p);
| then with W on and no other write: the slot's last leaf gets its value back (the slot table: that leaf's
| offset, its value before), the rows and flags into slot s (index: the header's first slot + s), the
| table = this leaf, 0x80000000 | q << 24 | j, and last the leaf = index << 16 | 0xFFFE.  Returns the
| tiles done, -1 refused (page p, off, or no 'MSPG').  Needs MODE1 and M1PAGE (else refused).
tile_copy:
        movem.l d2-d7/a2-a6,-(sp)
        moveq   #-1,d0
        .if OL_MODE1
        .if OL_M1PAGE
        move.l  48(sp),d6               | p
        move.l  52(sp),d7               | off
        lea     0xA15100,a6
        cmpi.l  #1,d6
        blo     9f
        cmpi.l  #6,d6
        bhi     9f
        cmpi.l  #0x1FFF00,d7
        bhs     9f
        movea.l d7,a5
        adda.l  #0x200000,a5            | the part, window 1
        move.w  sr,-(sp)
        move.w  #0x2700,sr
        bset    #0,7(a6)                | RV = 1: page p's header
        lea     tc_hdr(pc),a4
        moveq   #-1,d5
        cmpi.l  #0x4D535047,(a5)
        bne.b   1f
        move.l  28(a5),(a4)             | rows
        move.l  24(a5),4(a4)            | flags
        move.l  36(a5),8(a4)            | the first slot's index
        move.l  44(a5),12(a4)           | the slot table
        moveq   #0,d5
1:      bclr    #0,7(a6)
        move.w  (sp)+,sr
        tst.l   d5
        bmi     9f
        tst.l   8(a4)
        beq     9f
2:      move.w  d5,d0                   | entry k: ready
        andi.w  #0x0FFF,d0
        move.w  d0,d1
        ori.w   #0x8000,d0
        ori.w   #0x4000,d1
        move.w  d0,0x2C(a6)
        move.l  #0x80000,d0             | (bounded: ~2 s)
3:      move.w  0x22(a6),d2
        cmp.w   d1,d2
        beq.b   4f
        cmpi.w  #0x7FFF,d2
        beq.b   8f
        subq.l  #1,d0
        bne.b   3b
        bra.b   8f
4:      moveq   #0,d3
        move.w  0x24(a6),d3             | the leaf / 4, low
        moveq   #0,d4
        move.w  0x26(a6),d4             | its high bits, q
        moveq   #0,d2
        move.w  0x28(a6),d2             | j
        moveq   #0,d1
        move.w  0x2A(a6),d1             | s
        bsr     tc_one
        addq.l  #1,d5
        bra.b   2b
8:      move.l  d5,d0
9:
        .endif
        .endif
        movem.l (sp)+,d2-d7/a2-a6
        rts

        .if OL_MODE1
        .if OL_M1PAGE
| one tile: d3/d4 the leaf (COMM4, COMM6), d2 j, d1 s; a4 page p's header values, a5 the part, a6 0xA15100
tc_one:
        movem.l d5/a2-a3,-(sp)
        move.w  d4,d0
        lsr.w   #8,d0
        andi.l  #7,d0                   | q
        beq     9f
        cmpi.w  #6,d0
        bhi     9f
        andi.l  #7,d4
        swap    d4
        move.w  d3,d4
        add.l   d4,d4
        add.l   d4,d4                   | the leaf's offset in the page
        cmpi.l  #0x200000,d4
        bhs     9f
        move.l  d0,d3                   | (the table's value: 0x80000000 | q << 24 | j)
        ror.l   #8,d3
        or.l    d2,d3
        bset    #31,d3
        lsl.w   #2,d0                   | bank 4q
        move.w  sr,-(sp)
        move.w  #0x2700,sr
        bset    #0,7(a6)                | RV = 1
        lea     0xA130F8,a0             | window 1 = page q
        move.w  d0,(a0)+
        addq.w  #1,d0
        move.w  d0,(a0)+
        addq.w  #1,d0
        move.w  d0,(a0)+
        addq.w  #1,d0
        move.w  d0,(a0)+
        lea     m1_buf(pc),a1
        cmpi.l  #0x4D535047,(a5)
        bne.b   5f                      | (not a page part: nothing copied)
        move.l  d2,d5
        lsl.l   #7,d5
        movea.l a5,a0
        adda.l  28(a5),a0
        adda.l  d5,a0                   | q's rows of tile j
        .rept   8                       | (16 bytes a step: movem, ~25 % fewer clocks than a long at a time)
        movem.l (a0)+,d0/d5/a2-a3
        movem.l d0/d5/a2-a3,(a1)
        lea     16(a1),a1
        .endr
        move.l  d2,d5
        lsl.l   #2,d5
        movea.l a5,a0
        adda.l  24(a5),a0
        move.l  0(a0,d5.l),(a1)         | its flags (m1_buf + 128)
        moveq   #0,d0
        bra.b   6f
5:      moveq   #-1,d0
6:      lea     m1_wb+8(pc),a0          | window 1 = page p again
        lea     0xA130F8,a2
        move.w  (a0)+,(a2)+
        move.w  (a0)+,(a2)+
        move.w  (a0)+,(a2)+
        move.w  (a0)+,(a2)+
        tst.l   d0
        bne     7f
        move.l  d1,d5
        lsl.l   #3,d5
        movea.l a5,a3
        adda.l  12(a4),a3
        adda.l  d5,a3                   | slot s's entry in the table
        move.l  8(a4),d2
        add.l   d1,d2                   | the slot's index
        move.l  (a3),d0                 | the slot's last leaf
        lea     m1_buf(pc),a1
        move.l  d2,d5
        lsl.l   #7,d5
        movea.l a5,a0
        adda.l  (a4),a0
        adda.l  d5,a0                   | p's rows of the slot
        move.l  d2,d5
        lsl.l   #2,d5
        movea.l a5,a2
        adda.l  4(a4),a2
        adda.l  d5,a2                   | its flags
        swap    d2
        move.w  #0xFFFE,d2              | the leaf's value
        move.w  #OL_M1CTRL0_W1,0xA130F0 | W on: from here only writes into the cart
        tst.l   d0
        beq.b   2f
        cmpi.l  #0x200000,d0
        bhs.b   2f
        movea.l d0,a1
        adda.l  #0x200000,a1
        move.l  4(a3),(a1)              | the slot's last tile: its leaf back
        lea     m1_buf(pc),a1
2:      .rept   8                       | (16 bytes a step, as above; d7 is free here: tile_copy saved it and
        movem.l (a1)+,d0-d1/d5/d7       |  no write but the cart's while W is on - no push)
        movem.l d0-d1/d5/d7,(a0)
        lea     16(a0),a0
        .endr
        move.l  (a1),(a2)
        move.l  d4,(a3)+
        move.l  d3,(a3)
        movea.l d4,a0
        adda.l  #0x200000,a0
        move.l  d2,(a0)                 | last: the leaf, once its rows are there
        move.w  #OL_M1CTRL0_W0,0xA130F0 | W off
7:      bclr    #0,7(a6)
        move.w  (sp)+,sr
9:      movem.l (sp)+,d5/a2-a3
        rts
        .align  2
tc_hdr: .long   0, 0, 0, 0              | page p's rows, flags, first slot, slot table (offsets in the part)
        .endif
        .endif
.endif                                      | DX-TILEP end
| =============================================================================
| 68K EXCEPTION TRAP (V71, restored in K29) - DIAGNOSTIC, changes nothing unless the 68K
| faults.  Hooks vectors 2..11 (bus, address, illegal, div0, CHK, TRAPV, privilege, trace,
| line-A, line-F): where a vector points into RAM (the BIOS jump table), "jmp <our stub>" is
| written there; vectors pointing to ROM are left alone.  Returns how many were hooked.
| On a fault: row 21 "68K EXC Vnn  FRAME:", row 22 = first 7 words of the exception frame
|   bus/address error (V02/V03): FC  ADDR-HI ADDR-LO  IR  SR  PC-HI PC-LO
|   all others:                  SR  PC-HI PC-LO  (rest = stack contents)
| then the 68K stops (the message stays on screen).  Needs the Genesis display ON to be seen.
| =============================================================================
install_exc:
        movem.l d2/a2-a3,-(sp)
        lea     exc_stubs(pc),a3
        moveq   #0,d1                   /* hooked count */
        moveq   #2,d2                   /* vector number 2..11 */
1:
        move.l  d2,d0
        lsl.l   #2,d0
        movea.l d0,a0
        movea.l (a0),a2                 /* where this vector goes */
        cmpa.l  #0xFF0000,a2
        blo.b   2f                      /* not RAM - leave it */
        move.w  #0x4EF9,(a2)            /* jmp abs.l */
        move.l  a3,2(a2)                /*     -> our stub */
        addq.l  #1,d1
2:
        addq.l  #8,a3                   /* next stub (8 bytes each) */
        addq.l  #1,d2
        cmpi.l  #12,d2
        bne.b   1b
        move.l  d1,d0
        movem.l (sp)+,d2/a2-a3
        rts

        .align  2
exc_stubs:                              /* each: move.w #vec,-(sp) / bra.w exc_common = 8 bytes */
        move.w  #2,-(sp)
        bra.w   exc_common
        move.w  #3,-(sp)
        bra.w   exc_common
        move.w  #4,-(sp)
        bra.w   exc_common
        move.w  #5,-(sp)
        bra.w   exc_common
        move.w  #6,-(sp)
        bra.w   exc_common
        move.w  #7,-(sp)
        bra.w   exc_common
        move.w  #8,-(sp)
        bra.w   exc_common
        move.w  #9,-(sp)
        bra.w   exc_common
        move.w  #10,-(sp)
        bra.w   exc_common
        move.w  #11,-(sp)
        bra.w   exc_common

exc_common:
        move.w  #0x2700,sr
        move.w  (sp)+,d6                /* vector number */
        movea.l sp,a5                   /* exception frame */
        move.w  #0x8174,0xC00004        /* K29: Genesis display ON so the message shows */
        lea     exc_buf(pc),a0
        lea     exc_txt1(pc),a1
3:      move.b  (a1)+,(a0)+             /* "68K EXC V" */
        bne.b   3b
        subq.l  #1,a0
        move.l  d6,d0
        moveq   #1,d1                   /* 2 hex digits */
        bsr     exc_hex
        lea     exc_txt2(pc),a1
4:      move.b  (a1)+,(a0)+
        bne.b   4b
        pea     21.l
        pea     2.l
        pea     0x4000.l                /* red */
        pea     exc_buf(pc)
        bsr     put_str
        lea     16(sp),sp

        lea     exc_buf(pc),a0
        moveq   #6,d5                   /* 7 words */
5:      move.w  (a5)+,d0
        moveq   #3,d1                   /* 4 hex digits */
        bsr     exc_hex
        move.b  #' ',(a0)+
        dbra    d5,5b
        clr.b   (a0)
        pea     22.l
        pea     2.l
        pea     0x4000.l
        pea     exc_buf(pc)
        bsr     put_str
        lea     16(sp),sp
6:      bra.b   6b                      /* stop here - leave the message on screen */

| d0 = value, d1 = digits-1 (low digits of d0), a0 = output -> advanced
exc_hex:
        move.l  d1,d3
        addq.l  #1,d3
        lsl.l   #2,d3                   /* bits to print */
        ror.l   d3,d0                   /* bring them to the top */
7:      rol.l   #4,d0
        move.b  d0,d4
        andi.b  #15,d4
        addi.b  #'0',d4
        cmpi.b  #'9',d4
        bls.b   8f
        addq.b  #7,d4
8:      move.b  d4,(a0)+
        dbra    d1,7b
        rts

exc_txt1:
        .asciz  "68K EXC V"
exc_txt2:
        .asciz  "  FRAME:                    "
        .align  2
exc_buf:
        .space  48

        .align  4
md_init_end:
