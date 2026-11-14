// map_cfx.sv - krikzz's SSF mapper (map_ssf.sv, unchanged) with the CD32X cart compositor (cfx_core.sv) beside it
//
// map_ssf runs as always; this module only takes over (Rayman CD32X's map_rayfx.sv, the same multiplexers):
//  - the PSRAMs, while the console is not using them (the engine reads layers, tiles, maps and sprites there);
//  - the SRAM (map_ssf never uses it): the engine's two pictures, maps and broken bits; the console reads the shown
//    picture at cart 0x3E0000-0x3EFFFF, straight from the pins;
//  - the console's reads of cart 0x3E0000-0x3FFFFF, once the engine is on (cfx_core.sv: the key).
// Off (power-on, reset, until the key), every output is map_ssf's.
//
// One module for the three builds: KEY1 and ID name the game (hub_<game>.sv sets them).
//
// MIT licence (as krikzz's mega-ed-pub, which this sits beside); CD32X BootCamp cartfx, 2026.

module map_cfx #(
    parameter [14:0] KEY1 = 15'h5848,
    parameter [7:0]  ID   = 8'hC1
)(
    input  MapIn mai,
    output MapOut mout
);

    wire clk = mai.clk;
    CpuBus cpu;
    assign cpu = mai.cpu;

    MapOut mout_ssf;
    map_ssf ssf_inst(mai, mout_ssf);

//************************************************************************************* the engine
    wire        fx_on, fx_area, con_psram, con_sram, fx_shown;
    wire [15:0] fx_status;
    wire        e_rom1, e_rom_oe, e_sram_oe, e_sram_we_lo, e_sram_we_hi;
    wire [22:0] e_rom_addr, e_sram_addr;
    wire [15:0] e_sram_di;

    cfx_core #(.KEY1(KEY1), .ID(ID)) fx_inst(
        .clk(clk),
        .rst(mai.map_rst),
        .a(cpu.addr[21:1]),
        .ce_lo_n(cpu.ce_lo),
        .oe_n(cpu.oe),
        .we_lo_n(cpu.we_lo),
        .we_hi_n(cpu.we_hi),
        .on(fx_on),
        .area(fx_area),
        .status(fx_status),
        .con_psram(con_psram),
        .con_sram(con_sram),
        .shown(fx_shown),
        .e_rom1(e_rom1),
        .e_rom_addr(e_rom_addr),
        .e_rom_oe(e_rom_oe),
        .rom0_do(mai.rom0_do),
        .rom1_do(mai.rom1_do),
        .e_sram_addr(e_sram_addr),
        .e_sram_di(e_sram_di),
        .e_sram_oe(e_sram_oe),
        .e_sram_we_lo(e_sram_we_lo),
        .e_sram_we_hi(e_sram_we_hi),
        .sram_do(mai.sram_do)
    );

//************************************************************************************* the outputs
    wire area_rd = fx_area & !cpu.ce_lo & !cpu.oe;
    wire cmd_area = cpu.addr[21:16] == 6'h3F;

    always_comb begin
        mout = mout_ssf;
        if (fx_on) begin
            if (!con_psram) begin                               // the PSRAMs: the engine's while the console is off them
                mout.rom0.addr  = e_rom_addr;
                mout.rom0.dati  = 16'h0000;
                mout.rom0.oe    = e_rom_oe & !e_rom1;
                mout.rom0.we_lo = 0;
                mout.rom0.we_hi = 0;
                mout.rom1.addr  = e_rom_addr;
                mout.rom1.dati  = 16'h0000;
                mout.rom1.oe    = e_rom_oe & e_rom1;
                mout.rom1.we_lo = 0;
                mout.rom1.we_hi = 0;
            end
            if (con_sram) begin                                 // the console reads the shown picture
                mout.sram.addr  = {6'd0, fx_shown, cpu.addr[15:1], 1'b0};
                mout.sram.dati  = 16'h0000;
                mout.sram.oe    = 1;
                mout.sram.we_lo = 0;
                mout.sram.we_hi = 0;
            end else begin                                      // the engine reads or writes it
                mout.sram.addr  = e_sram_addr;
                mout.sram.dati  = e_sram_di;
                mout.sram.oe    = e_sram_oe;
                mout.sram.we_lo = e_sram_we_lo;
                mout.sram.we_hi = e_sram_we_hi;
            end
            if (area_rd) begin
                mout.map_oe = 1;
                mout.map_do = cmd_area ? fx_status : mai.sram_do;
            end
        end
    end

endmodule
