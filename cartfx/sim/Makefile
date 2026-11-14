# cartfx/sim: cfx_core in Verilator against the reference model.  make && ./obj/Vcfx_core 12
# make t8: the engine with PSRAM_T 8 (Dracula X's read) in obj8/; run it with PSRAM_T=8 (the bench's latency)
VFLAGS = -Wno-fatal -Wno-BLKSEQ -Wno-WIDTH -Wno-UNUSED -Wno-CASEINCOMPLETE --x-assign 0 --x-initial 0
all:
	verilator $(VFLAGS) --cc ../rtl/cfx_core.sv --exe tb_cfx.cpp ../model/cartfx.c \
	    -CFLAGS "-I$(CURDIR)/../model -O2" --Mdir obj --build -j 4
t8:
	verilator $(VFLAGS) -GPSRAM_T=4\'d8 --cc ../rtl/cfx_core.sv --exe tb_cfx.cpp ../model/cartfx.c \
	    -CFLAGS "-I$(CURDIR)/../model -O2" --Mdir obj8 --build -j 4
clean:
	rm -rf obj obj8
