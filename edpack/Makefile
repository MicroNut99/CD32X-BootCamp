# edpack - does the EverDrive PRO menu load a 14 MB .32X whole?  edtest's 68000 side, a read-only SH2 program
#   make ROOTDIR=/opt/toolchains/sega PACK_NONCE=0x1234   -> edpack.32x (512 KB: bank 0 of the probe)
#   edpack/build.sh then pads it to 14 MB with tools/pad_markers.py (the same nonce) -> EDPACK.32X
ROOTDIR ?= /opt/toolchains/sega
SHROOT  ?= $(ROOTDIR)
MDROOT  ?= $(ROOTDIR)

TARGET  = edpack
BUILD   = build

SHPREFIX = $(SHROOT)/sh-elf/bin/sh-elf-
SHCC     = $(SHPREFIX)gcc
SHAS     = $(SHPREFIX)as
SHOBJC   = $(SHPREFIX)objcopy
CC_VER  := $(shell $(SHCC) -dumpversion)

LIBPATH  = -L$(SHROOT)/sh-elf/lib -L$(SHROOT)/sh-elf/lib/gcc/sh-elf/$(CC_VER) -L$(SHROOT)/sh-elf/sh-elf/lib
INCPATH  = -Isrc -I$(SHROOT)/sh-elf/include -I$(SHROOT)/sh-elf/sh-elf/include
CCFLAGS  = -D__32X__ -m2 -mb -O2 -Wall -c -fomit-frame-pointer -std=gnu99
# PACK_NONCE: mixed into every bank's pattern by tools/pad_markers.py; the probe must know it
PACK_NONCE ?= 0x1234
CCFLAGS += -DPACK_NONCE=$(PACK_NONCE)
LDFLAGS  = -T $(SHROOT)/ldscripts/mars.ld -Wl,-Map=$(BUILD)/output.map -nostdlib
ASFLAGS  = --big
LIBS     = $(LIBPATH) -lc -lgcc -lgcc-Os-4-200 -lnosys

# crt0.o must be first: it holds the ROM header, the 32X header and the SH2 start-up
OBJS = $(BUILD)/crt0.o $(BUILD)/main.o

all: $(TARGET).32x

$(TARGET).32x: $(TARGET).elf
	$(SHOBJC) -O binary $< $(BUILD)/temp.bin
	dd if=$(BUILD)/temp.bin of=$@ bs=512K conv=sync status=none

$(TARGET).elf: src-md/m68k.bin $(OBJS)
	$(SHCC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

# the 68000 side of the cartridge (never run on the CD32X: the 68000 runs the CD listener there)
src-md/m68k.bin:
	$(MAKE) -C src-md ROOTDIR=$(MDROOT)

$(BUILD)/crt0.o: src/crt0.s src-md/m68k.bin | $(BUILD)
	$(SHAS) $(ASFLAGS) -I. -o $@ $<

$(BUILD)/%.o: src/%.c src/32x.h | $(BUILD)
	$(SHCC) $(CCFLAGS) $(INCPATH) -o $@ $<

$(BUILD):
	mkdir -p $@

clean:
	$(MAKE) -C src-md clean ROOTDIR=$(MDROOT)
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).32x
