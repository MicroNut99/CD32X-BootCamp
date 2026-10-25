# 10 – Mega EverDrive PRO as a 16 MB RAM cart: the test

**The question:** can a CD32X game use the EverDrive PRO's 16 MB of memory instead of the 4 MB RAM cart?

**What is known.** Booting the diagnostic disc through the EverDrive's "Run Mega-CD" (Sega CD + 32X, EverDrive
in the 32X slot) found **no writable memory** at the cart:

- `OLD:0000 VIA 400000:FFFF`
- `WROTE 5AA5 VIA 900000:0000`
- `SAFE CART: NOT FOUND`
- the first ROM chunk did not stick (`CMP I0 C0 @400000 BAD`)

So in that mode the EverDrive is not a RAM cart.

**What this tests.** The EverDrive's **extended SSF mapper**. Per Krikzz's notes, it can make its memory
writable and has a 32X mode, but it only turns on for a ROM whose header says `SEGA SSF`, started from the
EverDrive menu.

| Register | Bits | Use |
|---|---|---|
| `0xA130F0` CTRL0 | `P X W L C . . . . . . R R R R R` | P protection (must be set), X 32X mode, W write enable, L LED, C #CART (Pro), R the bank in window 0 |
| `0xA130F2`–`0xA130FE` CTRL1–7 | `. . . . . . . . . . . R R R R R` | the bank in 512 KB windows 1–7 (cart `0x080000`–`0x3FFFFF`) |

Banks are 512 KB each, 0–31. Banks 28 (saves) and 30–31 (the EverDrive OS; 31 is SRAM on the Pro) must not
be touched.

## The two ROMs

| ROM | CTRL0 | From |
|---|---|---|
| `EDTEST_X.32X` | `0xE000` = P + X + W | the X-series notes, which list an X (32X) bit |
| `EDTEST_W.32X` | `0xA000` = P + W | the Pro write-up, which shows no X bit |

Build them with `bash edtest/build.sh` (`ROOTDIR=` / `SEGA_TOOLCHAIN=` as for the main build).

## How to run it

1. Copy both `.32X` files to the EverDrive's SD card.
2. Genesis + **32X** with the EverDrive PRO in the 32X slot. The Sega CD is not needed: take it off, or leave
   it, it is not used.
3. Start `EDTEST_X.32X` from the EverDrive menu. Leave it for at least one full pass (`PASS DONE`, about 3–5
   minutes), better two or three. Photograph the screen.
4. Power off, then do the same with `EDTEST_W.32X`.

## What it does

Each pass:

1. Writes EverDrive banks 1–27 (13.5 MB) with the 68000, with RV = 1, as the CD32X stack writes its RAM cart.
   Each bank gets its own pattern, mapped seven at a time into windows 1–7. Each bank is checked right after
   it is written.
2. After **all** banks are written, maps each one in again and checks it three ways: the 68000, the master
   SH2 (first half) and the slave SH2 (second half). The SH2s read cache-through at `0x22000000`, as a game
   reads its cart.

Every other pass the pattern's bits are inverted. Bank 0 (the test program itself) and banks 28–31 are never
written.

## Reading the screen

| Per bank | Meaning |
|---|---|
| `OK` | written, kept, and read back the same by the 68000 and both SH2s |
| `W` | not writable, or not mapped: it did not read back right after writing |
| `A` | written, but lost or overwritten after the other banks were written: the banks share memory, or the mapper does not switch |
| `S` | the 68000 reads it right, an SH2 does not |

- **`BAD WORDS 68K / SH2`:** totals over all passes.
- **`FIRST ERROR`:** the bank, the offset in it, the word expected, the word read, and who read it.
- **The verdict line** sums it up.

| Verdict | What it means for the games |
|---|---|
| `ALL TESTED BANKS OK` (after several passes, in one of the ROMs) | the EverDrive can be a 16 MB cart for a 32X program in **cartridge mode**. Next: boot the CD stack from it (a separate step, since the CD boot needs the cart to step aside). |
| `NOT WRITABLE: NO RAM CART HERE` | that CTRL0 value does not enable writing with the 32X. Try the other ROM. |
| `BANKS LOSE DATA OR ALIAS: MAPPER?` | the writes land, but the mapper's banks do not switch under the 32X. The `GOT` word tells which bank's pattern was found. |
| `SH2 READS DIFFER FROM 68000` | the memory works, but the SH2s see something else: timing or the 32X's cart path |
| `68000 NOT ANSWERING` | the 68000 side stopped. It is stuck in a command. |

In ares this ROM shows `W` for every bank: ares does not emulate the EverDrive's mapper. That is expected.
