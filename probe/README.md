# probe: the CD32X timing probe

What each kind of 32X access costs on a real console, measured one kind at a time by the master
SH2's own free-running timer, so that the CD32X build of ares can be set to match the console
(the "console timing" mode). One screen of numbers: photograph it.

```
cd /mnt/s/KoboPort/sega-toolchain-12.1/sega/CD32X-BootCamp
GAME=probe DISC_NAME=CD32X_PROBE_V1 EDCD=1 bash build.sh 2>&1 | tee build_log.txt
```

- Disc boot (RAM cart): burn `disc/CD32X_PROBE_V1.cue` and start the console with it.
- Mega EverDrive PRO: the same disc, with `edboot/EDCD.32X` (copy it to the SD card as
  `EDCD_PROBE_V1.32X`) started from the EverDrive's menu.
- The numbers appear after about two seconds. **A** measures again (the run number is top right).
  Photograph the screen once per boot method.

## Why it exists

The 32X's bus arbitration is not fully documented: Sega's hardware manual gives ranges (cart reads
6-15 wait states, frame buffer writes through a 4-word FIFO), not how the two SH2s, the 68000 and the
VDP share the bus and what one costs the other. Those timings come from **our own measurements on
the console, which are not published anywhere else**.

Tuning ares to the games' frame rates alone did not work. Jazz's open sky and its caves make the
same memory accesses per picture, yet run at 10 and 18-20 fps on the console. The probe measures each
cost by itself.

The data so far (Jazz, Hurrican and OpenLara on the console beside ares, the manuals' figures,
ares's access counts) and the state of the ares model are in the CD32X ares build's
`README-CD32X.md`, "Console timing".

## What the numbers are

SH2 clocks (23.01 MHz NTSC) per access, one decimal; the lines marked *clocks* are whole clocks.
Each test is a loop of 16 identical instructions run 128 times (2048 accesses) with interrupts off;
the loop's own cost (DT + BF) is measured with NOPs and taken off; each test runs three times and
the middle result is shown. Addresses are the cache-through ones unless the test is about the cache.

| Line | What runs |
|---|---|
| NOP, ADD, MUL.L, BRA+NOP | the instruction itself (MUL.L back to back; BRA with its delay slot) |
| LD+USE SD | a cached SDRAM load whose value is used by the next instruction (the load-use stall), minus the ADD |
| CART RD.W / RD.L / RD.B | cart reads at 0x22100000 (the RAM cart or the EverDrive's PSRAM) |
| CART HIT | a cached cart read that hits the cache |
| CART LINE | a cached cart read that misses: one cache line (16 bytes) fill each time, minus the ADD |
| SDRM RD.W / RD.L / WR.W / WR.L | SDRAM at 0x26020000, cache-through |
| SDRM HIT, SDRM LINE | SDRAM through the cache: a hit; a line fill each time |
| FB RD.W / WR.W / WR.L | the frame buffer at 0x24008000, back to back (writes fill the VDP's write FIFO) |
| FB WR+6NOP | one frame buffer write, then 6 NOPs (the FIFO has time to empty), minus the NOPs |
| OVR WR.W | the overwrite image (0x24028000) |
| REG RD COM / WR COM / RD VDP | 32X registers: COMM10, the VDP's status |
| FB WR VBL / DISP, FB RD VBL / DISP | 256 frame buffer writes / reads started in the vertical blank / 40 lines into the picture |
| FILL256 VBL / DSP | the VDP's fill of 256 words, from the data write until FEN clears (clocks) |
| FRAME CLK, LINE CLK | SH2 clocks per frame and per line (clocks) |
| CD CMD CLK | one Sega CD command's round trip (CMD 48, the pad), as games use it every frame (clocks) |
| xx/yyyy | the shared bus: the master's access xx (CA cart read, SD SDRAM read, FB frame buffer write) while the slave SH2 runs yyyy without a break (IDLE in its cache, CART reads, SDRM reads, FB W frame buffer writes, SD W SDRAM writes) |

The FRT: the 32X boot ROM leaves it cleared at every compare match A with OCRA = 1, so it never
counts past 1. The probe lets it run free while it measures and puts that set-up back afterwards.

## ares (as released, upstream timing), for comparison

Disc boot, 4 MB RAM cart: NOP 1.0, MUL.L 1.0, CART RD.W 7.0, RD.L 13.0, CART LINE 60.1, SDRM RD.W 1.0,
SDRM LINE 13.1, FB RD.W 6.0, FB WR.W 5.0, FB WR.L 9.0, FILL256 384 / 400, FRAME CLK 384000, LINE CLK
1465, CD CMD CLK 71112 (EverDrive: 9128); no bus contention (CA/IDLE = CA/CART = 7.0).
