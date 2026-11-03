ifdef $(GENDEV)
ROOTDIR = $(GENDEV)
else
ROOTDIR = /opt/toolchains/sega
endif

LDSCRIPTSDIR = $(ROOTDIR)/ldscripts

BOOTBLOCKDIR = $(ROOTDIR)/bootblocks
BOOTBLOCK = $(BOOTBLOCKDIR)/US_BOOT.BIN
#BOOTBLOCK = $(BOOTBLOCKDIR)/EU_BOOT.BIN
#BOOTBLOCK = $(BOOTBLOCKDIR)/JP_BOOT.BIN

LIBPATH = -L$(ROOTDIR)/m68k-elf/lib -L$(ROOTDIR)/m68k-elf/lib/gcc/m68k-elf/4.6.2 -L$(ROOTDIR)/m68k-elf/m68k-elf/lib
INCPATH = -I. -I../../include -I$(ROOTDIR)/m68k-elf/include -I$(ROOTDIR)/m68k-elf/m68k-elf/include

CCFLAGS = -m68000 -Wall -Ofast -c -fomit-frame-pointer
HWFLAGS = -m68000 -Wall -O1 -c -fomit-frame-pointer
# CART-1: make cd CART=8 / CART=16 - the cart test build (CMD 68 in main.c, which is built with
# HWFLAGS).  Without CART: unchanged.
ifdef CART
CCFLAGS += -DOL_CART_MB=$(CART)
HWFLAGS += -DOL_CART_MB=$(CART)
endif
# BOOTCAMP: the four switches below default to 0 here.  With all four at 0 this main.c compiles to the
# Sub-CPU code OpenLara CD32X ran on the console (v1.0-v1.4), apart from the file table (game_files.h).
# Each one is tested in emulation only: turn it on (e.g. STREAM=1 bash build.sh) after testing it.
# STREAM-1: the drive read-ahead for CMD 46 / 47 (main.c, OL_STREAM).  OpenLara's default is on;
# 0 builds main.c exactly as before.
STREAM ?= 0
HWFLAGS += -DOL_STREAM=$(STREAM)
# SLOTS-1: CMD 61 also writes OpenLara's save slot area (cart 0x060000-0x07FFFF). OpenLara's default is on;
# it writes OpenLara's cart layout (inside this kit's ROM area): leave it off.
SLOTS ?= 0
HWFLAGS += -DOL_SLOTS=$(SLOTS)
# FMV-2: CMD 46 ring mode, the big movies play while they stream from the CD. OpenLara's default is on;
# 0 builds CMD 46 as before.
FMVRING ?= 0
HWFLAGS += -DOL_FMVRING=$(FMVRING)
# CARTW-1: CMD 46 copies the chunks to the cart in words (hw_md.s command 27), almost 3 times faster
# than bytes (command 33).  OpenLara's default is on; 0 builds the copy as before.
CARTW ?= 0
HWFLAGS += -DOL_CARTW=$(CARTW)
# DX-GENPLANE begin
# GENPLANE=1: SuperTux CD32X REV's Genesis picture commands 62-67 (main.c, built with HWFLAGS) and their
# listener routines 36 / 37 (hw_md.s).  Without it: unchanged.  Ledger section 1 / 2.
GENPLANE ?= 0
HWFLAGS += -DDX_GENPLANE=$(GENPLANE)
ifeq ($(GENPLANE),1)
ASFLAGS_GENPLANE = --defsym DX_GENPLANE=1
endif
# DX-GENPLANE end
# DX-VSPLIT begin
# VSPLIT=1 (needs GENPLANE=1): command 78, the V scroll changed part-way down the screen by the Genesis H
# interrupt (hw_md.s set_vs_line / hint_vs, MD 38).  Without it: unchanged.
VSPLIT ?= 0
HWFLAGS += -DDX_VSPLIT=$(VSPLIT)
ifeq ($(VSPLIT),1)
ASFLAGS_GENPLANE += --defsym DX_VSPLIT=1
endif
# DX-VSPLIT end
# DX-FBVDP begin
# FBVDP=1 (needs GENPLANE=1 VSPLIT=1): command 79, the Genesis VDP written from a list in the 32X frame buffer
# (hw_md.s fb_list, MD 39: map words, converted tiles, H scroll tables a running game makes).  Without it:
# unchanged.  Ledger section 24.
FBVDP ?= 0
HWFLAGS += -DDX_FBVDP=$(FBVDP)
ifeq ($(FBVDP),1)
ASFLAGS_GENPLANE += --defsym DX_FBVDP=1
endif
# DX-FBVDP end
# DX-PAD2 begin
# PAD2=1: CMD 48's reply has pad 2 in COMM10 (main.c, the pad fast path) instead of the 68K's vblank
# counter: a game of two players (Metal Slug CD32X).  Without it: unchanged.  Ledger section 10.
PAD2 ?= 0
HWFLAGS += -DOL_PAD2=$(PAD2)
# DX-PAD2 end
# DX-TILEP begin
# TILEP=1: command 99, a tile page's cache - tiles copied from one window-1 page into the slots of another
# (main.c, and the listener's routine 40 tile_copy, hw_md.s, with MODE1-9's page registers): Metal Slug
# CD32X.  Without it: unchanged.  Ledger section 25.
TILEP ?= 0
HWFLAGS += -DOL_TILEP=$(TILEP)
ifeq ($(TILEP),1)
ASFLAGS_GENPLANE += --defsym DX_TILEP=1
endif
# DX-TILEP end
LDFLAGS = -T $(LDSCRIPTSDIR)/cd.ld -Wl,-Map=output.map -nostdlib
ASFLAGS = -m68000 --register-prefix-optional
# DX-GENPLANE begin
ASFLAGS += $(ASFLAGS_GENPLANE)
# DX-GENPLANE end
# MODE1-1: make CDROMPlayer.bin MODE1=1 - the listener for a cartridge boot (hw_md.s, OL_MODE1), the
# program edboot/EDCD.32X loads into the Sega CD itself.  Without MODE1: unchanged.  build.sh EDCD=1
# builds it in a copy of this folder; the disc always gets the normal program.
ifeq ($(MODE1),1)
ASFLAGS += --defsym OL_MODE1=1
# MODE1-2: M1WORDS=1 - the cartridge-boot listener writes the cart in words wherever it can
ifeq ($(M1WORDS),1)
ASFLAGS += --defsym OL_M1WORDS=1
endif
# MODE1-4 (diagnostics): M1MINVB=1 vblank = level 2 only; M1NOVINT=1 no 68000 interrupts; QUIET=1 the
# Sub-CPU stops its 68000 traffic once the game starts (4E00), history + heartbeat every 0.5 s only
ifeq ($(M1MINVB),1)
ASFLAGS += --defsym OL_M1MINVB=1
endif
ifeq ($(M1NOVINT),1)
ASFLAGS += --defsym OL_M1NOVINT=1
endif
ifeq ($(QUIET),1)
HWFLAGS += -DEDCD_QUIET=1
endif
ifneq ($(M1W0BANK),)
ASFLAGS += --defsym OL_M1W0BANK=$(M1W0BANK)
endif
ifneq ($(M1CTRL0),)
ASFLAGS += --defsym OL_M1CTRL0=$(M1CTRL0)
endif
# MODE1-7: M1SAVE=1 - cart 0x800000-0x83FFFF = the EverDrive's save bank (31, kept on the SD card)
ifeq ($(M1SAVE),1)
ASFLAGS += --defsym OL_M1SAVE=1
ifdef M1SAVEBANK
ASFLAGS += --defsym OL_M1SAVEBANK=$(M1SAVEBANK)   # MODE1-7b: another bank (27 = PSRAM, a test)
endif
ifdef M1SAVEWIN
ASFLAGS += --defsym OL_M1SAVEWIN=$(M1SAVEWIN)     # MODE1-7b: through another window (1-7)
endif
endif
# MODE1-9: M1PAGE=1 the paged games' page-register writes (a byte to 0xA130F0/F2) become the EverDrive's
# window registers (page p in window 1 = CTRL4..7 = 4p..4p+3)
ifeq ($(M1PAGE),1)
ASFLAGS += --defsym OL_M1PAGE=1
endif
endif

PREFIX = $(ROOTDIR)/m68k-elf/bin/m68k-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJC = $(PREFIX)objcopy

ASMZ80 = $(ROOTDIR)/bin/zasm
FLAGSZ80 = -vb2

DD = dd
RM = rm -rf

TARGET = CDROMPlayer
# ---------------------------------------------------------------------------------------------
# KOBO: the SH2 program is built in ../sh2 as D32XR.32x.  It is copied (renamed) to
# ROMS/$(SH2_PAYLOAD) on EVERY build, files.s builds it into the program from there (relative
# path - the old files.s pointed at the J: folder, so S: builds used the old SH2 program), and
# it is also put on the disc.  Keep KOBOGFX.BIN in ROMS/ too: "make clean" deletes *.bin here.
# If you rename SH2_PAYLOAD, change the .incbin line in files.s to match.
# ---------------------------------------------------------------------------------------------
SH2_BUILD   = ../sh2/CDLOADER.32x
SH2_PAYLOAD = CDLOADER.BIN
# FMV-1: *.ROQ (the movies) on the disc too
# BOOTCAMP: everything in ROMS/ goes on the disc (the game ROM, IMAGE.RAW, data files, SFXnn.BIN, movies)
IMAGE_FILES = $(SH2_PAYLOAD) $(filter-out $(SH2_PAYLOAD),$(notdir $(wildcard ROMS/*)))
LIBS = $(LIBPATH) -lc -lgcc -lnosys
OBJS = crt0.o main.o hw_md.o scd.o scd_fs.o cd.o kos.o cdfs.o sub_stubs.o
FILES = files.o

# "cd" is also the name of the temp folder, so make would otherwise say "cd is up to date" and skip the copy step
.PHONY: cd clean FORCE

FORCE:

# always refresh the renamed SH2 program (FORCE), then rebuild files.o from it
ROMS/$(SH2_PAYLOAD): FORCE
	@test -f $(SH2_BUILD) || { echo "*** $(SH2_BUILD) missing - build ../d32xr-master (Makefile.cdloader) first ***"; exit 1; }
	mkdir -p ROMS
	cp -v $(SH2_BUILD) ROMS/$(SH2_PAYLOAD)

files.o: files.s ROMS/$(SH2_PAYLOAD)
	$(AS) $(ASFLAGS) $(INCPATH) files.s -o files.o

all: src-cd32x/SRAMTest32X.bin $(TARGET).bin

src-cd32x/SRAMTest32X.bin:
	make -C src-cd32x

$(TARGET).bin: $(TARGET).elf
	$(OBJC) -O binary $< temp.bin
	$(DD) if=temp.bin of=$@ bs=2048 conv=sync

$(TARGET).elf: $(OBJS) $(FILES)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) $(FILES) -o $(TARGET).elf

%.o80: %.s80
	$(ASMZ80) $(FLAGSZ80) -o $@ $<

main.o: main.c game_files.h
	$(CC) $(HWFLAGS) $(INCPATH) $< -o $@

%.o: %.c
	$(CC) $(CCFLAGS) $(INCPATH) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $(INCPATH) $< -o $@

crt.elf: crt.o sub_stubs.o
	$(LD) -Ttext 0x6000 crt.o sub_stubs.o -o crt.elf

crt.bin: crt.elf
	$(OBJC) -O binary crt.elf crt.bin

cd.o: cd.s crt.bin
	$(AS) $(ASFLAGS) $(INCPATH) cd.s -o cd.o

cd: $(TARGET).bin
	rm -rf cd
	mkdir -p cd
	cp $(TARGET).bin cd/APP.BIN
	@# K10: ROMS/ FIRST.  "make clean" only removes lowercase *.bin, so an old Demo/KOBOGFX.BIN
	@# survived and was put on the disc instead of ROMS/KOBOGFX.BIN.
	for f in $(IMAGE_FILES); do if [ -f ROMS/$$f ]; then cp -v ROMS/$$f cd/$$f; elif [ -f $$f ]; then cp -v $$f cd/$$f; echo "note: $$f taken from Demo/, not ROMS/"; else echo "note: $$f not found in ROMS/ or this folder"; fi; done
	@echo "cd folder now holds:"; ls -1 cd
	genisoimage -sysid "SEGA SEGACD" -volid "CD32XGAME" -generic-boot $(BOOTBLOCK) -full-iso9660-filenames -o $(TARGET).iso cd

clean:
	if [ -d src-cd32x ]; then make clean -C src-cd32x; fi
	$(RM) *.o *.o80 *.bin *.elf *.map *.log *.iso cd