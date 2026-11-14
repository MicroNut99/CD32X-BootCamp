// cfx_core.sv - the CD32X cart compositor (cartfx) on the Mega EverDrive PRO's FPGA: the engine
//
// What it does, to the bit: cartfx/model/cartfx.h (cartfx.c is the reference this is checked against in Verilator:
// cartfx/sim). This file: how, on the cart's memories.
//
//  - The bus front, the key, the command queue and the two memory channels are Rayman CD32X's rfx_core.sv
//    (Rayman_32X_RAM fpga/rtl, console-proven 8 Nov 2026), with Metal Slug CD32X's console fix: a command read's
//    address is taken only once it has held steady two clocks, and a new steady address while the read lasts is a
//    new command.
//  - The console always wins: it gets a memory the moment it reads or writes it (map_cfx.sv's multiplexers, from the
//    bus pins); an engine access the console overlapped, or came into within two clocks after, is done again.
//  - PSRAM: address and OE 5 clocks, the data taken at the 5th (Rayman's). SRAM writes from a queue (address and data
//    a clock, WE 2, a clock off); SRAM reads (maps, broken bits) OE 4 clocks, sampled twice, done again unless both
//    samples agree. Addresses are absolute: PSRAM byte address bits 23-0 (bit 23: the second chip), SRAM word
//    address bits 17-0. Pictures: buffer b at SRAM words b x 0x8000.
//  - A frame is composed a 16 x 8 cell at a time in block RAM (all layers, the sprites, the over layers), then the
//    cell's 64 words go to the SRAM. A one-word cache sits in front of the PSRAM channel.
//
// The clock is the EverDrive's 50 MHz (20 ns).
//
// MIT licence (as krikzz's mega-ed-pub, which this sits beside); CD32X BootCamp cartfx, 2026.

module cfx_core #(
    parameter [14:0] KEY1 = 15'h5848,           // the build: 'XH' Hurrican, 'XJ' Jazz, 'XS' SuperTux
    parameter [7:0]  ID   = 8'hC1
)(
    input             clk,
    input             rst,              // map_rst: the EverDrive menu / no game

    // the cart bus (the pins, not synchronised)
    input      [21:1] a,
    input             ce_lo_n,
    input             oe_n,
    input             we_lo_n,
    input             we_hi_n,

    output            on,               // the engine answers its area
    output            area,             // the console's address is in the engine's area (and it is on)
    output     [15:0] status,           // what a command port read returns (from the address: status or a counter)
    output            con_psram,        // the console is using a PSRAM now (it gets them)
    output            con_sram,         // the console is reading the picture now (it gets the SRAM)
    output            shown,            // the buffer the window shows

    output reg        e_rom1,           // the engine's PSRAM: 0 = PSRAM 0 (banks 0-15), 1 = PSRAM 1
    output reg [22:0] e_rom_addr,
    output reg        e_rom_oe,
    input      [15:0] rom0_do,
    input      [15:0] rom1_do,

    output reg [22:0] e_sram_addr,
    output reg [15:0] e_sram_di,
    output reg        e_sram_oe,
    output reg        e_sram_we_lo,
    output reg        e_sram_we_hi,
    input      [15:0] sram_do
);
    localparam [14:0] KEY0 = 15'h4346, KEY2 = 15'h0C01;

//***************************************************************************************** the bus, the key, the queue
    wire cart_cyc = !ce_lo_n & (!oe_n | !we_lo_n | !we_hi_n);
    wire in_area  = a[21:17] == 5'b11111;                       // 0x3E0000-0x3FFFFF
    wire cmd_rd   = !ce_lo_n & !oe_n & a[21:16] == 6'h3F;
    reg  on_r, shown_r, err;
    assign on        = on_r;
    assign shown     = shown_r;
    assign area      = on_r & in_area;
    assign con_psram = cart_cyc & !(on_r & in_area);
    assign con_sram  = on_r & !ce_lo_n & !oe_n & a[21:16] == 6'h3E;

    // a command read: the address taken once it has held steady two clocks (Metal Slug CD32X's fix)
    reg  [1:0] rd_ff;
    reg [14:0] a1, a2, last_a, cmd_w;
    reg        taken, cmd_ev;
    always @(posedge clk) begin
        rd_ff <= {rd_ff[0], cmd_rd};
        a1 <= a[15:1]; a2 <= a1;
        cmd_ev <= 0;
        if (!rd_ff[1]) taken <= 0;
        else if (a1 == a2 && (!taken || a2 != last_a)) begin
            taken <= 1; last_a <= a2; cmd_w <= a2; cmd_ev <= 1;
        end
    end

    reg [1:0] p_ff, s_ff;                                       // the console's use of the memories, synchronised
    always @(posedge clk) begin
        p_ff <= {p_ff[0], con_psram};
        s_ff <= {s_ff[0], con_sram};
    end
    wire con_p = p_ff[1] | con_psram;
    wire con_s = s_ff[1] | con_sram;

    reg [14:0] q [0:1023];
    reg [10:0] q_wr, q_rd;
    wire [10:0] q_n = q_wr - q_rd;
    wire q_full = q_n >= 11'd768;
    reg  [1:0] key;
    reg        exec;
    wire       mem_quiet;
    reg        off_now;
    reg [15:0] cnt_frames, cnt_cells, cnt_spr, cnt_time, cnt_predo, cnt_sredo, cnt_lost;
    reg  [6:0] st;                                              // (the state machine's state, below)
    wire       busy = (q_n != 0) | exec | (st != 7'd0) | !mem_quiet;   // (st: a word between the queue and its run)

    reg [15:0] cnt_mux;
    always @* begin
        case (a[4:1])
        4'd1: cnt_mux = cnt_frames;
        4'd2: cnt_mux = cnt_cells;
        4'd3: cnt_mux = cnt_spr;
        4'd4: cnt_mux = cnt_time;
        4'd5: cnt_mux = cnt_predo;
        4'd6: cnt_mux = cnt_sredo;
        4'd7: cnt_mux = cnt_lost;
        4'd8: cnt_mux = {1'b0, KEY2};
        default: cnt_mux = 16'd0;
        endcase
    end
    wire cnt_rd = a[15:5] == 11'd0 && a[4:1] != 4'd0;
    assign status = cnt_rd ? cnt_mux : {ID, 4'd0, err, shown_r, q_full, busy};

    reg [14:0] q_out;
    always @(posedge clk) begin
        if (rst) begin
            on_r <= 0; key <= 0; q_wr <= 0; cnt_lost <= 0;
        end else begin
            if (cmd_ev) begin
                if (!on_r) begin
                    if (key == 0 && cmd_w == KEY0) key <= 1;
                    else if (key == 1 && cmd_w == KEY1) key <= 2;
                    else if (key == 2 && cmd_w == KEY2) begin key <= 0; on_r <= 1; end
                    else key <= cmd_w == KEY0 ? 2'd1 : 2'd0;
                end else if (cmd_w[14] | cmd_w[13]) begin
                    if (q_n != 11'd1024) begin
                        q[q_wr[9:0]] <= cmd_w;
                        q_wr <= q_wr + 1'd1;
                    end else cnt_lost <= cnt_lost + 1'd1;
                end
            end
            if (off_now) begin on_r <= 0; key <= 0; end
        end
    end
    always @(posedge clk) q_out <= q[q_rd[9:0]];

//***************************************************************************************** PSRAM channel
    // Words, one after another: whenever the channel is free, or ends a read the console did not touch, it starts
    // the word w_a (w_v: the state machine wants one; registered there a clock before). Address and OE 5 clocks, the
    // data taken at the 5th, then a clock with OE off (Rayman's read, back to back: a word every 6 clocks). A read the
    // console overlapped, or came into within two clocks after, is done again. p_vld (a clock): p_data is word p_vld_a.
    reg        w_v;
    reg [22:0] w_a;                                             // word addresses: bit 22 the chip
    reg        p_vld;
    reg [22:0] p_cur, p_vld_a;
    reg [15:0] p_data;
    reg  [2:0] p_cnt;
    reg        p_busy, p_hit;
    always @(posedge clk) begin
        p_vld <= 0;
        if (rst) begin p_busy <= 0; e_rom_oe <= 0; cnt_predo <= 0; end
        else begin
            reg start;
            start = 0;
            if (!p_busy) start = w_v;
            else begin
                p_cnt <= p_cnt + 1'd1;
                if (con_p) p_hit <= 1;
                if (p_cnt == 3'd4) begin p_data <= e_rom1 ? rom1_do : rom0_do; e_rom_oe <= 0; end
                if (p_cnt == 3'd5) begin
                    if (p_hit | con_p) begin p_cnt <= 0; p_hit <= 0; e_rom_oe <= 1; cnt_predo <= cnt_predo + 1'd1; end
                    else begin p_busy <= 0; p_vld <= 1; p_vld_a <= p_cur; start = w_v; end
                end
            end
            if (start) begin
                p_busy <= 1; p_cnt <= 0; p_hit <= 0;
                p_cur <= w_a;
                e_rom1 <= w_a[22];
                e_rom_addr <= {w_a[21:0], 1'b0};
                e_rom_oe <= 1;
            end
        end
    end

//***************************************************************************************** SRAM channel
    // s_push (a clock, when !s_full): write s_in_d to word s_in_a. sr_go (a clock): read word sr_addr; sr_vld (a
    // clock): sr_data is it. A read goes before the queued writes (they are picture words, never a word read).
    reg        s_push;
    reg [17:0] s_in_a;
    reg [15:0] s_in_d;
    (* ramstyle = "M9K" *) reg [33:0] sq [0:127];             // the writes waiting (block RAM: read a clock late)
    reg [33:0] sq_q;                                            // the word at sq_rd, as read a clock before
    reg  [6:0] sq_qa;
    reg        sq_qok;                                          // it had been written before that read
    reg  [7:0] sq_wr, sq_rd;
    wire [7:0] sq_n = sq_wr - sq_rd;
    wire       s_full = sq_n >= 8'd120;
    wire       s_room64 = sq_n <= 8'd60;                      // room for a whole cell's 64 words
    wire       s_empty = sq_n == 0;
    reg        sr_go, sr_vld, sr_pend;
    reg [17:0] sr_addr;
    reg [15:0] sr_data, sr_s1;
    reg  [2:0] s_cnt;
    reg        s_busy, s_rd, s_hit;
    always @(posedge clk) begin
        if (s_push) sq[sq_wr[6:0]] <= {s_in_a, s_in_d};
        sq_q <= sq[sq_rd[6:0]];
        sq_qa <= sq_rd[6:0];
        sq_qok <= sq_wr != sq_rd;
    end
    wire s_head = sq_qok && sq_qa == sq_rd[6:0];               // sq_q is the first word waiting
    always @(posedge clk) begin
        sr_vld <= 0;
        if (rst) begin
            sq_wr <= 0; sq_rd <= 0; s_busy <= 0; sr_pend <= 0;
            e_sram_we_lo <= 0; e_sram_we_hi <= 0; e_sram_oe <= 0; cnt_sredo <= 0;
        end else begin
            if (s_push) sq_wr <= sq_wr + 1'd1;
            if (!s_busy) begin
                if (sr_pend) begin                              // a read first
                    sr_pend <= 0;
                    s_rd <= 1; s_busy <= 1; s_cnt <= 0; s_hit <= 0;
                    e_sram_addr <= {4'd0, sr_addr, 1'b0}; e_sram_oe <= 1;
                end else if (s_head) begin                      // address and data a clock before WE
                    s_rd <= 0; sq_rd <= sq_rd + 1'd1;
                    e_sram_addr <= {4'd0, sq_q[33:16], 1'b0}; e_sram_di <= sq_q[15:0];
                    s_busy <= 1; s_cnt <= 0; s_hit <= 0;
                end
            end else if (s_rd) begin
                s_cnt <= s_cnt + 1'd1;
                if (con_s) s_hit <= 1;
                if (s_cnt == 3) sr_s1 <= sram_do;
                if (s_cnt == 4) begin sr_data <= sram_do; e_sram_oe <= 0; end
                if (s_cnt == 5) begin
                    if (s_hit | con_s | sr_data != sr_s1) begin
                        s_cnt <= 0; s_hit <= 0; e_sram_oe <= 1; cnt_sredo <= cnt_sredo + 1'd1;
                    end else begin s_busy <= 0; sr_vld <= 1; end
                end
            end else begin
                s_cnt <= s_cnt + 1'd1;
                if (con_s) s_hit <= 1;
                e_sram_we_hi <= s_cnt == 0 || s_cnt == 1;
                e_sram_we_lo <= s_cnt == 0 || s_cnt == 1;
                if (s_cnt == 3) begin
                    if (s_hit | con_s) begin s_cnt <= 0; s_hit <= 0; cnt_sredo <= cnt_sredo + 1'd1; end   // again
                    else s_busy <= 0;
                end
            end
            if (sr_go) sr_pend <= 1;                            // (after the start: a request is never lost)
        end
    end
    assign mem_quiet = s_empty & !s_busy & !s_push & !sr_pend & !sr_go & !p_busy & !w_v;

//***************************************************************************************** block RAMs
    // the sprite list: {seen 1, cells x0 5, x1 5, y0 5, y1 5, flags 3, x 13, y 13, w 9, h 9, stride 13, src 24,
    // offset 8} (seen: its rectangle, clipped to the picture, is not empty; the cells: that rectangle's)
    reg [112:0] spr_mem [0:255];
    reg  [7:0] spr_ra, spr_wa;
    reg [112:0] spr_q, spr_wd;
    reg        spr_we;
    always @(posedge clk) begin
        if (spr_we) spr_mem[spr_wa] <= spr_wd;
        spr_q <= spr_mem[spr_ra];
    end

    // the marked cells, a row a word
    (* ramstyle = "M9K" *) reg [20:0] mark_mem [0:31];
    reg  [4:0] mk_ra, mk_wa;
    reg [20:0] mk_q, mk_wd;
    reg        mk_we;
    always @(posedge clk) begin
        if (mk_we) mark_mem[mk_wa] <= mk_wd;
        mk_q <= mark_mem[mk_ra];
    end

    // the cell being composed: even and odd pixels, a byte a pixel ({row, pixel / 2}), and which pixels are drawn
    (* ramstyle = "M9K" *) reg [7:0] bufe [0:63];
    (* ramstyle = "M9K" *) reg [7:0] bufo [0:63];
    reg  [5:0] bw_a, br_a;
    reg  [7:0] bw_d, bre_q, bro_q;
    reg        bw_we, bw_odd;
    always @(posedge clk) begin
        if (bw_we & !bw_odd) bufe[bw_a] <= bw_d;
        if (bw_we & bw_odd) bufo[bw_a] <= bw_d;
        bre_q <= bufe[br_a];
        bro_q <= bufo[br_a];
    end

    // the shade table (OP_LUT): light l, colour c -> byte l x 256 + c
    reg  [7:0] shade_mem [0:4095];
    reg [11:0] sh_ra, sh_wa;
    reg  [7:0] sh_q, sh_wd;
    reg        sh_we;
    always @(posedge clk) begin
        if (sh_we) shade_mem[sh_wa] <= sh_wd;
        sh_q <= shade_mem[sh_ra];
    end

//***************************************************************************************** the divider
    // dv_go (a clock): dv_q = dv_n / dv_d, dv_r = dv_n % dv_d, 7 clocks later (dv_busy low); dv_d > 0. Two bits a clock.
    reg        dv_go, dv_busy;
    reg [13:0] dv_n, dv_sh, dv_q;
    reg [18:0] dv_d, dv_r;
    reg  [2:0] dv_i;
    always @(posedge clk) begin
        if (rst) dv_busy <= 0;
        else if (dv_go) begin dv_busy <= 1; dv_i <= 3'd7; dv_r <= 0; dv_sh <= dv_n; dv_q <= 0; end
        else if (dv_busy) begin
            reg [19:0] t1, t2;
            reg [18:0] r1, r2;
            reg q1, q2;
            t1 = {dv_r, dv_sh[13]};
            if (t1 >= {1'b0, dv_d}) begin r1 = t1[18:0] - dv_d; q1 = 1; end else begin r1 = t1[18:0]; q1 = 0; end
            t2 = {r1, dv_sh[12]};
            if (t2 >= {1'b0, dv_d}) begin r2 = t2[18:0] - dv_d; q2 = 1; end else begin r2 = t2[18:0]; q2 = 0; end
            dv_r <= r2; dv_q <= {dv_q[11:0], q1, q2};
            dv_sh <= {dv_sh[11:0], 2'b00};
            dv_i <= dv_i - 1'd1;
            if (dv_i == 3'd1) dv_busy <= 0;
        end
    end

//***************************************************************************************** the state
    localparam OP_BEGIN = 4'd1, OP_SPRITE = 4'd2, OP_LAYER = 4'd3, OP_SCROLL = 4'd4, OP_CELLS = 4'd5, OP_POKE = 4'd6,
               OP_UPLOAD = 4'd7, OP_LUT = 4'd8, OP_SETUP = 4'd9, OP_SHOW = 4'd10, OP_GO = 4'd11, OP_OFF = 4'd15;

    reg [12:0] c [1:10];                                        // the command's arguments
    reg  [3:0] c_op;
    reg  [9:0] c_arg;
    reg  [3:0] c_n, c_need;

    reg  [9:0] L_fl  [0:3];                                     // the layers (OP_LAYER, OP_SCROLL)
    reg [23:0] L_pix [0:3];
    reg [23:0] L_map [0:3];
    reg [12:0] L_w   [0:3];
    reg [12:0] L_h   [0:3];
    reg  [7:0] L_off [0:3];
    reg        L_brk [0:3];
    reg  [5:0] L_t   [0:3];
    reg [12:0] L_x   [0:3];
    reg [12:0] L_y   [0:3];
    reg [18:0] L_wt  [0:3];                                     // the wrap: pixels (bitmap), cells x tile (tile map)
    reg [18:0] L_ht  [0:3];
    reg [11:0] L_tt  [0:3];                                     // tile x tile

    reg  [4:0] cols, rows;
    reg [23:0] blend;
    reg [17:0] brk;
    reg  [8:0] nspr;
    reg        full;
    wire [8:0] pw = {cols, 4'd0};                               // the picture's width and height
    wire [7:0] ph = {rows, 3'd0};
    wire [20:0] colmask = (21'd1 << cols) - 21'd1;

    // direct PSRAM reads (UPLOAD, LUT, map entries, the blend table)
    reg [23:0] f_addr;
    reg  [7:0] f_byte;
    reg [15:0] f_word;
    reg  [6:0] f_ret;
    reg        f_seq;                                           // UPLOAD, LUT: the next word read ahead too

    reg        l_tiles, l_4bpp, l_opq, l_over, l_shade, l_hfl, l_msram, l_brk;   // the layer being drawn

    // the pixel stream (layers, sprites) and the direct reads: four PSRAM words held (by the address's low two bits);
    // the word the pixel (or the direct read) needs, then up to two after it in the run, read ahead
    reg [23:0] ba;                                              // the pixel's byte
    reg        nib;                                             // 4 bits a pixel: the low nibble
    reg        dirm;                                            // the stream runs backwards (mirrored)
    reg  [5:0] rem;                                             // pixels left in the run (this one too)
    reg [22:0] wb_a [0:3];
    reg [15:0] wb_d [0:3];
    reg  [3:0] wb_v;
    wire       ps_active = st == L_BPX || st == L_TPX || st == S_PX;
    wire       lk_f = st == F_REQ;
    wire       lk_dir = !lk_f && dirm;
    wire [22:0] lk_wa = lk_f ? f_addr[23:1] : ba[23:1];
    wire [22:0] lk_n1 = lk_dir ? lk_wa - 23'd1 : lk_wa + 23'd1;
    wire [22:0] lk_n2 = lk_dir ? lk_wa - 23'd2 : lk_wa + 23'd2;
    wire        ps_hit = wb_v[lk_wa[1:0]] && wb_a[lk_wa[1:0]] == lk_wa;
    wire        h1 = wb_v[lk_n1[1:0]] && wb_a[lk_n1[1:0]] == lk_n1;
    wire        h2 = wb_v[lk_n2[1:0]] && wb_a[lk_n2[1:0]] == lk_n2;
    wire        f0 = (p_busy && p_cur == lk_wa) || (p_vld && p_vld_a == lk_wa);
    wire        f1 = (p_busy && p_cur == lk_n1) || (p_vld && p_vld_a == lk_n1);
    wire        f2 = (p_busy && p_cur == lk_n2) || (p_vld && p_vld_a == lk_n2);
    wire [15:0] ps_hd = wb_d[lk_wa[1:0]];
    wire  [7:0] ps_byte = ba[0] ? ps_hd[7:0] : ps_hd[15:8];
    wire  [3:0] ps_nibv = nib ? ps_byte[3:0] : ps_byte[7:4];
    // the words after this one the run still needs: the pixels left past this word's
    wire        px4 = (st == L_BPX || st == L_TPX) && l_4bpp;
    wire  [1:0] pos_f = px4 ? {ba[0], nib} : {1'b0, ba[0]};    // this pixel's place in its word, in the run's order
    wire  [1:0] pos = !dirm ? pos_f : px4 ? 2'd3 - pos_f : {1'b0, !ba[0]};
    wire  [6:0] reach = {1'b0, rem} + {5'd0, pos};             // > q: the next word is needed; > 2q: the one after
    wire        need1 = lk_f ? f_seq : px4 ? reach > 7'd4 : reach > 7'd2;
    wire        need2 = lk_f ? f_seq : px4 ? reach > 7'd8 : reach > 7'd4;
    wire        n0 = (ps_active || lk_f) && !ps_hit && !f0;
    wire        n1 = (ps_active || lk_f) && need1 && !h1 && !f1;
    wire        n2 = (ps_active || lk_f) && need2 && !h2 && !f2;
    always @(posedge clk) begin
        w_v <= !rst && (n0 | n1 | n2);
        w_a <= n0 ? lk_wa : n1 ? lk_n1 : lk_n2;
    end

    // the pixel writes into the cell: two stages (the shade table's read between)
    reg        px1_v, px1_sh, px2_v, px2_sh;
    reg  [6:0] px1_idx, px2_idx;
    reg  [7:0] px1_val, px2_val;
    wire       px_quiet = !px1_v && !px2_v && !bw_we;

    // GO
    reg        b;                                               // the buffer composed into
    reg  [4:0] cx, cy;
    reg [20:0] row_m;
    reg  [8:0] ncells;
    reg [31:0] t_go;
    reg  [8:0] x0;                                              // the cell's first pixel
    reg  [7:0] y0;
    reg  [2:0] lyr;
    reg        ph_over;                                         // the pass: 0 the layers under, 1 the over layers
    reg [127:0] bvalid;                                         // the cell's pixels drawn (the others are 0)
    reg  [6:0] k;
    reg [15:0] wrow;                                            // the write-out: this row's first word
    reg [15:0] wrow_k;                                          // the word pushed
    reg  [6:0] wk;
    // a layer pass
    reg  [2:0] r;
    reg  [3:0] i;
    reg [23:0] l_pix, l_map;
    reg [12:0] l_w, l_h;
    reg  [5:0] l_t;
    reg [18:0] l_ht;
    reg [11:0] l_tt;
    reg  [7:0] l_off;
    reg [18:0] lx, ly, lx0;
    reg [12:0] mx, my, mx0;
    reg  [5:0] u, v, u0;
    reg [26:0] rowbase;                                         // bitmap: ly x w; tile map: my x w (cells)
    reg [23:0] row_ba;                                          // bitmap: the row's first byte (lx = 0)
    reg        row_nib;
    reg [11:0] vt;
    // the tile map's entries: four held (cell, entry, broken, tile x t x t)
    reg [26:0] ec_cell [0:3];
    reg [15:0] ec_ent  [0:3];
    reg        ec_brk  [0:3];
    reg [23:0] ec_tb   [0:3];
    reg  [3:0] ec_v;
    reg  [1:0] ec_nx;
    reg [26:0] e_cell;
    reg [15:0] cur_ent;
    reg        cur_brk;
    reg [23:0] cur_tb;
    // a sprite
    reg  [8:0] s;
    reg  [2:0] sf;
    reg signed [15:0] sx, sy;
    reg  [8:0] sw, sh;
    reg [12:0] sstride;
    reg [23:0] ssrc;
    reg  [7:0] soff;
    reg signed [15:0] di0, di1, dj1, di, dj;
    reg [21:0] sprod;
    reg  [3:0] bi;
    reg  [2:0] br;
    reg  [7:0] pcol;
    // marking
    reg  [4:0] mcy, mcy1;
    reg [20:0] mmask;
    // UPLOAD / LUT / BEGIN
    reg [12:0] n_left;
    reg [23:0] up_src;
    reg [17:0] up_dst;
    reg [12:0] lut_i;

    localparam S_IDLE = 7'd0, S_GATHER = 7'd1, S_RUN = 7'd2, S_DONE = 7'd3,
               S_BEGIN = 7'd4, S_SPR0 = 7'd5, S_MK0 = 7'd7, S_MK1 = 7'd8, S_MK2 = 7'd9,
               S_CELLS1 = 7'd10, S_CELLS2 = 7'd11, S_POKE = 7'd12, S_UP0 = 7'd13, S_UP1 = 7'd14,
               S_LUT0 = 7'd15, S_LUT1 = 7'd16, S_LAYER1 = 7'd17, S_SHOW = 7'd18,
               F_REQ = 7'd20,
               G_START = 7'd24, G_ROW = 7'd25, G_ROW1 = 7'd26, G_ROW2 = 7'd27, G_CELL = 7'd28, G_MAP0 = 7'd29,
               G_MAP1 = 7'd30, G_END = 7'd31,
               C_START = 7'd32, C_LAYER = 7'd33, L_START = 7'd34, L_DIV1 = 7'd35, L_DIV2 = 7'd36, L_DIV3 = 7'd37,
               L_DIV4 = 7'd38, L_ROW = 7'd39, L_ROW2 = 7'd40, L_BPX = 7'd41,
               L_TENT = 7'd42, L_TE_S = 7'd43, L_TE_P = 7'd44, L_TE_B = 7'd45, L_TE_B2 = 7'd46, L_TE_T = 7'd47,
               L_TSPAN = 7'd48, L_TPX = 7'd49, L_TSKIP = 7'd50, L_NEXT = 7'd51,
               C_SPR = 7'd56, C_SPR1 = 7'd57, C_SPR2 = 7'd58, C_SPR3 = 7'd59, S_ROW = 7'd60, S_ROW2 = 7'd61,
               S_PX = 7'd62, S_BL0 = 7'd63, S_BL1 = 7'd64, S_BL2 = 7'd65, S_BL3 = 7'd66, S_ADV = 7'd67,
               C_WR0 = 7'd70, C_WR = 7'd71, C_DONE = 7'd73;

    wire [1:0] ly2 = lyr[1:0];

    always @(posedge clk) begin
        s_push <= 0;
        sr_go <= 0;
        dv_go <= 0;
        off_now <= 0;
        spr_we <= 0;
        mk_we <= 0;
        bw_we <= 0;
        sh_we <= 0;
        px1_v <= 0;
        t_go <= t_go + 1'd1;
        if (rst) begin
            st <= S_IDLE; exec <= 0; q_rd <= 0; c_n <= 0; err <= 0; shown_r <= 0;
            wb_v <= 0; px2_v <= 0; ec_v <= 0; dirm <= 0; f_seq <= 0; rem <= 0;
            cols <= 5'd20; rows <= 5'd22; nspr <= 0; full <= 0; blend <= 0; brk <= 0;
            cnt_frames <= 0; cnt_cells <= 0; cnt_spr <= 0; cnt_time <= 0;
            for (integer j = 0; j < 4; j = j + 1) begin
                L_fl[j] <= 0; L_w[j] <= 0; L_h[j] <= 0; L_t[j] <= 0; L_wt[j] <= 0; L_ht[j] <= 0; L_tt[j] <= 0;
                L_x[j] <= 0; L_y[j] <= 0; L_off[j] <= 0; L_brk[j] <= 0; L_pix[j] <= 0; L_map[j] <= 0;
            end
        end else begin
            // ---- a PSRAM word arrives: held
            if (p_vld) begin wb_a[p_vld_a[1:0]] <= p_vld_a; wb_d[p_vld_a[1:0]] <= p_data; wb_v[p_vld_a[1:0]] <= 1; end
            // ---- the pixel writes: stage 1 (the shade table read) -> stage 2 -> the cell
            px2_v <= px1_v; px2_sh <= px1_sh; px2_idx <= px1_idx; px2_val <= px1_val;
            if (px2_v) begin
                bw_a <= {px2_idx[6:4], px2_idx[3:1]}; bw_odd <= px2_idx[0]; bw_d <= px2_sh ? sh_q : px2_val; bw_we <= 1;
                bvalid[px2_idx] <= 1;
            end

            case (st)
            S_IDLE: if (q_n != 0) begin q_rd <= q_rd + 1'd1; st <= S_GATHER; end   // (q_out takes the word at q_rd)
            S_GATHER: begin
                if (q_out[14]) begin
                    c_op <= q_out[13:10]; c_arg <= q_out[9:0]; c_n <= 1;
                    case (q_out[13:10])
                    OP_SPRITE: c_need <= 4'd9;
                    OP_LAYER:  c_need <= 4'd11;
                    OP_SCROLL: c_need <= 4'd3;
                    OP_CELLS:  c_need <= 4'd3;
                    OP_POKE:   c_need <= 4'd5;
                    OP_UPLOAD: c_need <= 4'd6;
                    OP_LUT:    c_need <= 4'd3;
                    OP_SETUP:  c_need <= 4'd7;
                    default:   c_need <= 4'd1;
                    endcase
                    case (q_out[13:10])
                    OP_SPRITE, OP_LAYER, OP_SCROLL, OP_CELLS, OP_POKE, OP_UPLOAD, OP_LUT, OP_SETUP: st <= S_IDLE;
                    default: begin exec <= 1; st <= S_RUN; end
                    endcase
                end else if (c_n != 0) begin
                    c[c_n] <= q_out[12:0]; c_n <= c_n + 1'd1;
                    if (c_n + 1'd1 == c_need) begin exec <= 1; st <= S_RUN; end else st <= S_IDLE;
                end else begin err <= 1; st <= S_IDLE; end         // an argument with no command: dropped
            end
            S_RUN: begin
                c_n <= 0;
                case (c_op)
                OP_BEGIN: begin
                    nspr <= 0; full <= c_arg[0]; err <= 0; wb_v <= 0; mcy <= 0;
                    st <= S_BEGIN;
                end
                OP_SPRITE: begin
                    if (nspr == 9'd256) begin err <= 1; st <= S_DONE; end
                    else if (c[3][8:0] == 0 || c[4][8:0] == 0) st <= S_DONE;
                    else begin
                        sx <= {{3{c[1][12]}}, c[1]}; sy <= {{3{c[2][12]}}, c[2]};
                        sw <= c[3][8:0]; sh <= c[4][8:0];
                        st <= S_SPR0;
                    end
                end
                OP_LAYER: begin
                    L_fl[c_arg[1:0]] <= c_arg;
                    L_pix[c_arg[1:0]] <= {c[1][10:0], c[2]};
                    L_w[c_arg[1:0]] <= c[3]; L_h[c_arg[1:0]] <= c[4];
                    L_off[c_arg[1:0]] <= c[5][7:0]; L_brk[c_arg[1:0]] <= c[5][8];
                    L_t[c_arg[1:0]] <= c[6][5:0];
                    L_map[c_arg[1:0]] <= c_arg[7] ? {6'd0, c[7][4:0], c[8]} : {c[7][10:0], c[8]};
                    L_x[c_arg[1:0]] <= c[9]; L_y[c_arg[1:0]] <= c[10];
                    st <= S_LAYER1;
                end
                OP_SCROLL: begin L_x[c_arg[1:0]] <= c[1]; L_y[c_arg[1:0]] <= c[2]; st <= S_DONE; end
                OP_CELLS: begin
                    if (c_arg < 10'd24) begin mk_ra <= c_arg[4:0]; st <= S_CELLS1; end
                    else st <= S_DONE;
                end
                OP_POKE: st <= S_POKE;
                OP_UPLOAD: begin
                    up_src <= {c[1][10:0], c[2][12:1], 1'b0}; up_dst <= {c[3][4:0], c[4]}; n_left <= c[5];
                    wb_v <= 0; dirm <= 0; st <= S_UP0;
                end
                OP_LUT: begin up_src <= {c[1][10:0], c[2]}; lut_i <= 0; wb_v <= 0; dirm <= 0; st <= S_LUT0; end
                OP_SETUP: begin
                    cols <= c[1] == 0 ? 5'd1 : c[1] > 13'd21 ? 5'd21 : c[1][4:0];
                    rows <= c[2] == 0 ? 5'd1 : c[2] > 13'd24 ? 5'd24 : c[2][4:0];
                    blend <= {c[3][10:0], c[4]};
                    brk <= {c[5][4:0], c[6]};
                    st <= S_DONE;
                end
                OP_SHOW: st <= S_SHOW;
                OP_GO: st <= G_START;
                OP_OFF: begin off_now <= 1; st <= S_DONE; end
                default: begin err <= 1; st <= S_DONE; end
                endcase
            end
            S_DONE: begin exec <= 0; st <= S_IDLE; end

            // ---- BEGIN: no cell marked
            S_BEGIN: begin
                mk_wa <= mcy; mk_wd <= 0; mk_we <= 1;
                if (mcy == 5'd31) st <= S_DONE; else mcy <= mcy + 1'd1;
            end

            // ---- SPRITE: into the list with the cells its rectangle (clipped to the picture) touches; those marked
            S_SPR0: begin
                reg signed [15:0] X0, X1, Y0, Y1;
                reg seen;
                X0 = sx < 0 ? 16'sd0 : sx;
                Y0 = sy < 0 ? 16'sd0 : sy;
                X1 = sx + $signed({7'd0, sw}) - 16'sd1;
                Y1 = sy + $signed({7'd0, sh}) - 16'sd1;
                if (X1 > $signed({7'd0, pw}) - 16'sd1) X1 = $signed({7'd0, pw}) - 16'sd1;
                if (Y1 > $signed({8'd0, ph}) - 16'sd1) Y1 = $signed({8'd0, ph}) - 16'sd1;
                seen = !(X0 > X1 || Y0 > Y1);
                spr_wa <= nspr[7:0];
                spr_wd <= {seen, X0[8:4], X1[8:4], Y0[7:3], Y1[7:3],
                           c_arg[2:0], c[1], c[2], c[3][8:0], c[4][8:0], c[5], c[6][10:0], c[7], c[8][7:0]};
                spr_we <= 1;
                nspr <= nspr + 1'd1;
                if (!seen) st <= S_DONE;
                else begin
                    mcy <= Y0[7:3]; mcy1 <= Y1[7:3];
                    mmask <= ((21'd2 << X1[8:4]) - 21'd1) & ~((21'd1 << X0[8:4]) - 21'd1);
                    st <= S_MK0;
                end
            end
            S_MK0: begin mk_ra <= mcy; st <= S_MK1; end
            S_MK1: st <= S_MK2;
            S_MK2: begin
                mk_wa <= mcy; mk_wd <= mk_q | mmask; mk_we <= 1;
                if (mcy == mcy1) st <= S_DONE; else begin mcy <= mcy + 1'd1; st <= S_MK0; end
            end

            // ---- CELLS
            S_CELLS1: st <= S_CELLS2;
            S_CELLS2: begin
                mk_wa <= c_arg[4:0]; mk_wd <= mk_q | {c[2][7:0], c[1]}; mk_we <= 1;
                st <= S_DONE;
            end

            // ---- LAYER: its wrap and t x t
            S_LAYER1: begin
                if (L_fl[c_arg[1:0]][3]) begin
                    L_wt[c_arg[1:0]] <= L_w[c_arg[1:0]] * L_t[c_arg[1:0]];
                    L_ht[c_arg[1:0]] <= L_h[c_arg[1:0]] * L_t[c_arg[1:0]];
                end else begin
                    L_wt[c_arg[1:0]] <= {6'd0, L_w[c_arg[1:0]]};
                    L_ht[c_arg[1:0]] <= {6'd0, L_h[c_arg[1:0]]};
                end
                L_tt[c_arg[1:0]] <= L_t[c_arg[1:0]] * L_t[c_arg[1:0]];
                st <= S_DONE;
            end

            // ---- POKE
            S_POKE: if (!s_full) begin
                s_in_a <= {c[1][4:0], c[2]}; s_in_d <= {c[3][7:0], c[4][7:0]}; s_push <= 1;
                st <= S_DONE;
            end

            // ---- UPLOAD: n words, PSRAM to SRAM
            S_UP0: begin
                if (n_left == 0) st <= S_DONE;
                else if (!s_full) begin f_addr <= up_src; f_ret <= S_UP1; f_seq <= 1; st <= F_REQ; end
            end
            S_UP1: begin
                s_in_a <= up_dst; s_in_d <= f_word; s_push <= 1;
                up_src <= up_src + 24'd2; up_dst <= up_dst + 18'd1; n_left <= n_left - 1'd1;
                st <= S_UP0;
            end

            // ---- LUT: 4096 bytes
            S_LUT0: begin
                if (lut_i == 13'd4096) st <= S_DONE;
                else begin f_addr <= up_src + {11'd0, lut_i}; f_ret <= S_LUT1; f_seq <= 1; st <= F_REQ; end
            end
            S_LUT1: begin
                sh_wa <= lut_i[11:0]; sh_wd <= f_byte; sh_we <= 1;
                lut_i <= lut_i + 1'd1; st <= S_LUT0;
            end

            // ---- SHOW: when the console is not reading the picture
            S_SHOW: if (!con_s) begin shown_r <= c_arg[0]; st <= S_DONE; end

            // ---- a direct read: f_byte / f_word of byte address f_addr (the words held; what is not there is wanted)
            F_REQ: if (ps_hit) begin f_byte <= f_addr[0] ? ps_hd[7:0] : ps_hd[15:8]; f_word <= ps_hd; st <= f_ret; end

            // ================================================================================ GO
            G_START: if (mem_quiet) begin
                b <= !shown_r; cy <= 0; ncells <= 0; t_go <= 0;
                st <= G_ROW;
            end
            G_ROW: begin
                if (cy == 5'd24) st <= G_END;
                else begin mk_ra <= cy; st <= G_ROW1; end
            end
            G_ROW1: st <= G_ROW2;
            G_ROW2: begin
                row_m <= cy < rows ? (full ? colmask : mk_q & colmask) : 21'd0;
                cx <= 0; st <= G_CELL;
            end
            G_CELL: begin
                if (cx == 5'd21) st <= G_MAP0;
                else if (row_m[cx]) st <= C_START;
                else cx <= cx + 1'd1;
            end
            G_MAP0: if (!s_full) begin
                s_in_a <= {2'd0, b, 15'h7E00 | {9'd0, cy, 1'b0}}; s_in_d <= row_m[15:0]; s_push <= 1;
                st <= G_MAP1;
            end
            G_MAP1: if (!s_full) begin
                s_in_a <= {2'd0, b, 15'h7E00 | {9'd0, cy, 1'b1}}; s_in_d <= {11'd0, row_m[20:16]}; s_push <= 1;
                cy <= cy + 1'd1; st <= G_ROW;
            end
            G_END: begin
                cnt_frames <= cnt_frames + 1'd1; cnt_cells <= {7'd0, ncells}; cnt_spr <= {7'd0, nspr};
                cnt_time <= t_go[31:22] != 0 ? 16'hFFFF : t_go[21:6];
                st <= S_DONE;
            end

            // ---- a cell: 0s, the layers under, the sprites, the over layers, then its 64 words out
            C_START: begin
                bvalid <= 0; x0 <= {cx, 4'd0}; y0 <= {cy, 3'd0};
                lyr <= 0; ph_over <= 0;
                st <= C_LAYER;
            end
            C_LAYER: begin
                if (lyr == 3'd4) begin
                    if (!ph_over) begin s <= 0; st <= C_SPR; end
                    else st <= C_WR0;
                end else if (L_fl[ly2][2] && L_fl[ly2][4] == ph_over) st <= L_START;
                else lyr <= lyr + 1'd1;
            end
            L_START: begin
                l_tiles <= L_fl[ly2][3]; l_over <= L_fl[ly2][4]; l_opq <= L_fl[ly2][5]; l_4bpp <= L_fl[ly2][6];
                l_msram <= L_fl[ly2][7]; l_shade <= L_fl[ly2][8] & L_fl[ly2][3]; l_hfl <= L_fl[ly2][9];
                l_brk <= L_brk[ly2];
                l_pix <= L_pix[ly2]; l_map <= L_map[ly2]; l_w <= L_w[ly2]; l_h <= L_h[ly2]; l_t <= L_t[ly2];
                l_ht <= L_ht[ly2]; l_tt <= L_tt[ly2]; l_off <= L_off[ly2];
                ec_v <= 0;
                if (L_wt[ly2] == 0 || L_ht[ly2] == 0 || (L_fl[ly2][3] && L_t[ly2] == 0)) st <= L_NEXT;
                else begin
                    dv_n <= {5'd0, x0} + {1'd0, L_x[ly2]}; dv_d <= L_wt[ly2]; dv_go <= 1;
                    st <= L_DIV1;
                end
            end
            L_DIV1: if (!dv_busy && !dv_go) begin               // lx0 = (x0 + x) mod the wrap
                lx0 <= {5'd0, dv_r[13:0]};
                dv_n <= {6'd0, y0} + {1'd0, L_y[ly2]}; dv_d <= l_ht; dv_go <= 1;
                st <= L_DIV2;
            end
            L_DIV2: if (!dv_busy && !dv_go) begin               // ly = (y0 + y) mod the wrap
                ly <= {5'd0, dv_r[13:0]};
                if (l_tiles) begin dv_n <= lx0[13:0]; dv_d <= {13'd0, l_t}; dv_go <= 1; st <= L_DIV3; end
                else begin r <= 0; st <= L_ROW; end
            end
            L_DIV3: if (!dv_busy && !dv_go) begin               // mx0, u0 = lx0 / t, lx0 % t
                mx0 <= dv_q[12:0]; u0 <= dv_r[5:0];
                dv_n <= ly[13:0]; dv_d <= {13'd0, l_t}; dv_go <= 1;
                st <= L_DIV4;
            end
            L_DIV4: if (!dv_busy && !dv_go) begin               // my, v = ly / t, ly % t
                my <= dv_q[12:0]; v <= dv_r[5:0];
                r <= 0; st <= L_ROW;
            end
            L_ROW: begin                                        // a row: its base
                if (l_tiles) begin rowbase <= my * l_w; vt <= v * l_t; end
                else rowbase <= ly[12:0] * l_w;
                i <= 0;
                st <= L_ROW2;
            end
            L_ROW2: begin
                if (l_tiles) begin mx <= mx0; u <= u0; st <= L_TENT; end
                else begin
                    reg [26:0] nb0;
                    nb0 = rowbase + {8'd0, lx0};
                    ba <= l_4bpp ? l_pix + nb0[24:1] : l_pix + nb0[23:0]; nib <= nb0[0]; dirm <= 0;
                    row_ba <= l_4bpp ? l_pix + rowbase[24:1] : l_pix + rowbase[23:0]; row_nib <= rowbase[0];
                    lx <= lx0; rem <= 6'd16;
                    st <= L_BPX;
                end
            end
            // ---- a bitmap's row: a pixel a clock while the words are there
            L_BPX: if (ps_hit) begin
                reg [7:0] val;
                val = l_4bpp ? {4'd0, ps_nibv} : ps_byte;
                if (val != 0 || (l_opq && !l_over)) begin px1_v <= 1; px1_sh <= 0; px1_idx <= {r, i}; px1_val <= val + l_off; end
                if (lx + 1'd1 == {6'd0, l_w}) begin lx <= 0; ba <= row_ba; nib <= row_nib; end
                else begin
                    lx <= lx + 1'd1;
                    if (!l_4bpp) ba <= ba + 1'd1;
                    else if (nib) begin ba <= ba + 1'd1; nib <= 0; end
                    else nib <= 1;
                end
                i <= i + 1'd1; rem <= rem - 1'd1;
                if (i == 4'd15) begin
                    ly <= ly + 1'd1 == {6'd0, l_h} ? 19'd0 : ly + 1'd1;
                    if (r == 3'd7) st <= L_NEXT; else begin r <= r + 1'd1; st <= L_ROW; end
                end
            end
            // ---- a tile map's row: the map cell's entry (four held), then its pixels
            L_TENT: begin
                reg [26:0] cc;
                reg h;
                reg [1:0] hk;
                cc = rowbase + {14'd0, mx};
                e_cell <= cc;
                h = 0; hk = 0;
                for (integer j = 0; j < 4; j = j + 1) if (ec_v[j] && ec_cell[j] == cc) begin h = 1; hk = j[1:0]; end
                if (h) begin cur_ent <= ec_ent[hk]; cur_brk <= ec_brk[hk]; cur_tb <= ec_tb[hk]; st <= L_TSPAN; end
                else if (l_msram) begin sr_addr <= l_map[17:0] + cc[17:0]; sr_go <= 1; st <= L_TE_S; end
                else begin f_addr <= l_map + {cc[22:0], 1'b0}; f_ret <= L_TE_P; f_seq <= 0; st <= F_REQ; end
            end
            L_TE_S: if (sr_vld) begin cur_ent <= sr_data; st <= L_TE_B; end
            L_TE_P: begin cur_ent <= f_word; st <= L_TE_B; end
            L_TE_B: begin
                if (l_brk) begin sr_addr <= brk + e_cell[21:4]; sr_go <= 1; st <= L_TE_B2; end
                else begin cur_brk <= 0; st <= L_TE_T; end
            end
            L_TE_B2: if (sr_vld) begin cur_brk <= sr_data[e_cell[3:0]]; st <= L_TE_T; end
            L_TE_T: begin
                reg [23:0] tb;
                tb = cur_ent[11:0] * l_tt;
                cur_tb <= tb;
                ec_cell[ec_nx] <= e_cell; ec_ent[ec_nx] <= cur_ent; ec_brk[ec_nx] <= cur_brk; ec_tb[ec_nx] <= tb;
                ec_v[ec_nx] <= 1; ec_nx <= ec_nx + 1'd1;
                st <= L_TSPAN;
            end
            L_TSPAN: begin                                      // the span of this map cell in the row
                reg [5:0] a16, at;
                a16 = 6'd16 - {2'd0, i};
                at = l_t - u;
                rem <= a16 < at ? a16 : at;
                if (cur_ent[11:0] == 0 || cur_brk) st <= L_TSKIP;
                else begin
                    reg [5:0] uu;
                    reg [24:0] nn;
                    reg mir;
                    mir = l_hfl && cur_ent[12];
                    uu = mir ? l_t - 6'd1 - u : u;
                    nn = {1'b0, cur_tb} + {13'd0, vt} + {19'd0, uu};
                    ba <= l_4bpp ? l_pix + nn[24:1] : l_pix + nn[23:0]; nib <= nn[0]; dirm <= mir;
                    st <= L_TPX;
                end
            end
            L_TPX: if (ps_hit) begin
                reg [7:0] val;
                val = l_4bpp ? {4'd0, ps_nibv} : ps_byte;
                if (val != 0 || (l_opq && !l_over)) begin
                    px1_v <= 1; px1_sh <= l_shade; px1_idx <= {r, i}; px1_val <= val + l_off;
                    sh_ra <= {cur_ent[15:12], val + l_off};
                end
                if (!l_4bpp) ba <= dirm ? ba - 1'd1 : ba + 1'd1;
                else if (!dirm) begin if (nib) begin ba <= ba + 1'd1; nib <= 0; end else nib <= 1; end
                else begin if (!nib) begin ba <= ba - 1'd1; nib <= 1; end else nib <= 0; end
                i <= i + 1'd1; rem <= rem - 1'd1;
                if (u + 1'd1 == l_t) begin u <= 0; mx <= mx + 1'd1 == l_w ? 13'd0 : mx + 1'd1; end
                else u <= u + 1'd1;
                if (i == 4'd15) begin
                    if (v + 1'd1 == l_t) begin v <= 0; my <= my + 1'd1 == l_h ? 13'd0 : my + 1'd1; end
                    else v <= v + 1'd1;
                    if (r == 3'd7) st <= L_NEXT; else begin r <= r + 1'd1; st <= L_ROW; end
                end else if (u + 1'd1 == l_t) st <= L_TENT;
            end
            L_TSKIP: begin                                      // nothing in this span: past it at once
                reg [4:0] ni;
                reg [6:0] nu;
                ni = {1'b0, i} + rem[4:0];
                nu = {1'b0, u} + {1'b0, rem};
                if (nu == {1'b0, l_t}) begin u <= 0; mx <= mx + 1'd1 == l_w ? 13'd0 : mx + 1'd1; end
                else u <= nu[5:0];
                i <= ni[3:0];
                if (ni == 5'd16) begin
                    if (v + 1'd1 == l_t) begin v <= 0; my <= my + 1'd1 == l_h ? 13'd0 : my + 1'd1; end
                    else v <= v + 1'd1;
                    if (r == 3'd7) st <= L_NEXT; else begin r <= r + 1'd1; st <= L_ROW; end
                end else st <= L_TENT;                          // (the span ended with the map cell)
            end
            L_NEXT: begin lyr <= lyr + 1'd1; st <= C_LAYER; end

            // ---- the sprites, in the order they came
            C_SPR: begin
                if (s == nspr) begin ph_over <= 1; lyr <= 0; st <= C_LAYER; end
                else begin spr_ra <= s[7:0]; st <= C_SPR1; end
            end
            C_SPR1: begin spr_ra <= s[7:0] + 8'd1; st <= C_SPR2; end   // (the list's read; the next one asked)
            C_SPR2: begin                                       // sprite s (spr_q), s + 1 being read: one a clock
                if (!spr_q[112] || cx < spr_q[111:107] || cx > spr_q[106:102] || cy < spr_q[101:97] || cy > spr_q[96:92])
                begin                                           // not in this cell
                    s <= s + 1'd1; spr_ra <= s[7:0] + 8'd2;
                    if (s + 1'd1 == nspr) begin ph_over <= 1; lyr <= 0; st <= C_LAYER; end
                end else begin
                    sf <= spr_q[91:89];
                    sx <= {{3{spr_q[88]}}, spr_q[88:76]}; sy <= {{3{spr_q[75]}}, spr_q[75:63]};
                    sw <= spr_q[62:54]; sh <= spr_q[53:45]; sstride <= spr_q[44:32];
                    ssrc <= spr_q[31:8]; soff <= spr_q[7:0];
                    st <= C_SPR3;
                end
            end
            C_SPR3: begin                                       // the sprite's pixels in this cell
                reg signed [15:0] dlo, dhi, elo, ehi;
                dlo = $signed({7'd0, x0}) - sx; dhi = dlo + 16'sd15;
                elo = $signed({8'd0, y0}) - sy; ehi = elo + 16'sd7;
                di0 <= dlo < 0 ? 16'sd0 : dlo;
                di1 <= dhi > $signed({7'd0, sw}) - 16'sd1 ? $signed({7'd0, sw}) - 16'sd1 : dhi;
                dj <= elo < 0 ? 16'sd0 : elo;
                dj1 <= ehi > $signed({7'd0, sh}) - 16'sd1 ? $signed({7'd0, sh}) - 16'sd1 : ehi;
                st <= S_ROW;
            end
            S_ROW: begin
                reg [8:0] sj;
                sj = sf[1] ? sh - 9'd1 - dj[8:0] : dj[8:0];
                sprod <= sj * sstride;
                st <= S_ROW2;
            end
            S_ROW2: begin
                reg signed [15:0] tb, tr;
                reg [8:0] si0;
                si0 = sf[0] ? sw - 9'd1 - di0[8:0] : di0[8:0];
                ba <= ssrc + {2'd0, sprod} + {15'd0, si0}; dirm <= sf[0]; nib <= 0;
                di <= di0; rem <= di1[5:0] - di0[5:0] + 6'd1;
                tb = sx + di0 - $signed({7'd0, x0});
                tr = sy + dj - $signed({8'd0, y0});
                bi <= tb[3:0]; br <= tr[2:0];
                st <= S_PX;
            end
            S_PX: if (ps_hit) begin
                if (ps_byte != 0 && sf[2]) begin pcol <= ps_byte + soff; st <= S_BL0; end     // blended: by itself
                else begin
                    if (ps_byte != 0) begin px1_v <= 1; px1_sh <= 0; px1_idx <= {br, bi}; px1_val <= ps_byte + soff; end
                    ba <= dirm ? ba - 1'd1 : ba + 1'd1; bi <= bi + 1'd1; rem <= rem - 1'd1;
                    if (di == di1) begin
                        if (dj == dj1) begin s <= s + 1'd1; st <= C_SPR; end
                        else begin dj <= dj + 16'sd1; st <= S_ROW; end
                    end else di <= di + 16'sd1;
                end
            end
            S_BL0: if (px_quiet) begin br_a <= {br, bi[3:1]}; st <= S_BL1; end   // the pixel under it, once written
            S_BL1: st <= S_BL2;
            S_BL2: begin
                reg [7:0] dst;
                dst = bvalid[{br, bi}] ? (bi[0] ? bro_q : bre_q) : 8'd0;
                f_addr <= blend + {8'd0, pcol, dst};
                f_ret <= S_BL3; f_seq <= 0; st <= F_REQ;
            end
            S_BL3: begin px1_v <= 1; px1_sh <= 0; px1_idx <= {br, bi}; px1_val <= f_byte; st <= S_ADV; end
            S_ADV: begin
                ba <= dirm ? ba - 1'd1 : ba + 1'd1; bi <= bi + 1'd1; rem <= rem - 1'd1;
                if (di == di1) begin
                    if (dj == dj1) begin s <= s + 1'd1; st <= C_SPR; end
                    else begin dj <= dj + 16'sd1; st <= S_ROW; end
                end else begin di <= di + 16'sd1; st <= S_PX; end
            end

            // ---- the cell's 64 words into the buffer, a word a clock (the queue has room for them all)
            C_WR0: if (px_quiet && s_room64) begin
                reg [15:0] w0;
                w0 = ({8'd0, y0} * {7'd0, pw} + {7'd0, x0}) >> 1;
                wrow <= w0; wrow_k <= w0;
                k <= 0; wk <= 0;
                st <= C_WR;
            end
            C_WR: begin
                if (k < 7'd64) begin br_a <= k[5:0]; k <= k + 1'd1; end
                if (k >= 7'd2) begin                            // the word read two clocks ago
                    s_in_a <= {2'd0, b, wrow_k[14:0]};
                    s_in_d <= {bvalid[{wk[5:3], wk[2:0], 1'b0}] ? bre_q : 8'd0, bvalid[{wk[5:3], wk[2:0], 1'b1}] ? bro_q : 8'd0};
                    s_push <= 1;
                    if (wk[2:0] == 3'd7) begin wrow <= wrow + {8'd0, pw[8:1]}; wrow_k <= wrow + {8'd0, pw[8:1]}; end
                    else wrow_k <= wrow_k + 1'd1;
                    wk <= wk + 1'd1;
                    if (wk == 7'd63) st <= C_DONE;
                end
            end
            C_DONE: begin ncells <= ncells + 1'd1; cx <= cx + 1'd1; st <= G_CELL; end

            default: st <= S_IDLE;
            endcase
        end
    end
endmodule
