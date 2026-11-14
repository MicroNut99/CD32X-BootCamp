// krikzz's mega-ed-pub fpga/mapper/map_ssf/hub.sv (MIT), the SSF mapper replaced by map_cfx (the CD32X cart
// compositor for Jazz CD32X: map_ssf unchanged inside it, the engine beside it). Nothing else changed.


`include "../lib_base/defs.sv"


module map_hub(

	input  MapIn mai,
	output MapOut mout
);


	assign mout =
	mai.cfg.map_idx == `MAP_SSF ? mout_ssf :
										   mout_nom;


	MapOut mout_nom;
	map_nom nom_inst(mai, mout_nom);

	MapOut mout_ssf;
	map_cfx #(.KEY1(15'h584A), .ID(8'hC2)) ssf_inst(mai, mout_ssf);	// Jazz: map_ssf with the compositor (map_cfx.sv)



endmodule
