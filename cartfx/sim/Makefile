# cartfx/sim: cfx_core in Verilator against the reference model.  make && ./obj/Vcfx_core 12
VFLAGS = -Wno-fatal -Wno-BLKSEQ -Wno-WIDTH -Wno-UNUSED -Wno-CASEINCOMPLETE --x-assign 0 --x-initial 0
all:
	verilator $(VFLAGS) --cc ../rtl/cfx_core.sv --exe tb_cfx.cpp ../model/cartfx.c \
	    -CFLAGS "-I$(CURDIR)/../model -O2" --Mdir obj --build -j 4
clean:
	rm -rf obj
