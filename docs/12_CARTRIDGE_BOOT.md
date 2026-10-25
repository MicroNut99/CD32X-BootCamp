# 12 – Cartridge boot: a CD32X disc started from the Mega EverDrive PRO

**Why.** A CD32X disc normally boots from the Sega CD with *no* cartridge in the slot (the CD boot, the Sega
CD's "mode 2"): BIOS at `0x000000`, Word RAM at `0x200000`, the RAM cart at `0x400000`. The EverDrive PRO
is a cartridge and stays one. On the console (25 Oct 2026, `edboot/EDPROBE.BIN`), writing CTRL0's C bit
changed nothing: the EverDrive stayed at `0x000000` and the Sega CD at `0x400000`, even after a 68000
RESET and a second write. So the console stays in **cartridge mode** ("mode 1"), the map reversed:

| | CD boot (normal) | Cartridge boot (EverDrive) |
|---|---|---|
| `0x000000` | Sega CD BIOS | **the cart** (EverDrive, 4 MB window of its 16 MB) |
| `0x200000` | Word RAM | the cart |
| `0x400000` | **the cart** | Sega CD BIOS + PRG-RAM window |
| `0x600000` | the cart | Word RAM |

**How.** `edboot/EDCD.32X`, a 32X cartridge ROM with a `SEGA SSF` header, does what the Sega CD BIOS and
the disc's boot block do in a CD boot, then hands over to the **same** stack:

1. The 32X starts the cartridge way: the cartridge's 32X header (`0x3C0`) has the boot ROMs load the
   stack's SH2 loader (CDLOADER). It waits for the listener, as in a CD boot.
2. The EverDrive's mapper as a 4 MB RAM cart: banks 0–7 in windows 0–7, writes on (`0xA000`).
3. The RAM jump table a CD boot's BIOS leaves at `0xFFFD00`. The cartridge's 68000 jump table (`0x200`)
   points there, so the listener's vblank works as in a CD boot.
4. The Sub-CPU: reset, its BIOS unpacked from the Sega CD's ROM into PRG-RAM (Chilly Willy's mode 1
   InitCD), a small start-up program at `0x6000` (`edcd_sp.s`) and the stack's Sub-CPU program at
   `0x8000`, then run. EDCD gives the Sub-CPU BIOS its level 2 interrupt every frame meanwhile.
5. The boot block's main-CPU program: the `I` / `B` / `M` handshake, then into Word RAM (`0x600000`).

From there the stack runs unchanged except for **one switch**, `MODE1=1` (`stack/subcpu/Makefile`,
`hw_md.s`). It only changes the 68000 listener:
- every address argument it gets from the Sub-CPU is moved to the cartridge-boot map (Word RAM
  `0x2xxxxx` → `0x6xxxxx`, the cart `0x4xxxxx`–`0x7xxxxx` → `0x0xxxxx`–`0x3xxxxx`); 32X register
  values (commands 19–22) are left alone;
- interrupts are off while RV = 1 (the 68000's vectors are then the cart's);
- the 32X is not reset and gets no CD boot table (it is running already).

Without `MODE1`, `hw_md.s` assembles to the same bytes as before (checked: the Sub-CPU program's md5 is
unchanged), so `stack/BOOT_FILES.md5` was updated for the new source only.

**The disc.** `EDCD=1 bash build.sh` builds the normal disc plus `edboot/EDCD.32X`. The disc's game ROM
gets one change (`tools/prep_rom.py --mode1`): its 68000 jump table points at the RAM table. A CD boot
never uses that table (its vectors are the BIOS's), so **the same disc boots both ways**: the normal
way with a RAM cart, and from the EverDrive with `EDCD.32X`. The Sub-CPU program inside `EDCD.32X` is a
second build of the stack with `MODE1=1`; the disc keeps the normal one.

## Run it

Genesis + Sega CD + 32X, the EverDrive PRO in the 32X slot, the disc in the Sega CD. Copy `EDCD.32X` to
the SD card and start it from the EverDrive menu (not "Run Mega-CD").

| You see | Meaning |
|---|---|
| The game | It works |
| **Blue** screen | no Sega CD found |
| **Magenta** | no Sub-CPU BIOS found in the Sega CD's ROM |
| **Red** | the Sub-CPU did not start (10 s) |
| **Yellow** | the stack's Sub-CPU program did not hand over (30 s) |
| Black, nothing | the stack is running but stuck: try the diagnostic disc (`DIAG=1 EDCD=1 bash build.sh`) |

While EDCD runs, the 32X's COMM12 holds its progress: `ED01` Sega CD check, `ED02` Sub-CPU BIOS found,
`ED03` Sub-CPU started, `ED04` its start-up ran, `ED05` handing over. CDLOADER's own codes follow.

## Tested

**ares** (the CD32X build with the EverDrive cart, `--ramcart-type everdrive --ramcart-boot
edboot/EDCD.32X`): the BootCamp disc boots, the game runs (`STATE:4B10`), the pad and CD music work, the
data file loads from the CD into the cart, saves work. The same disc still boots the normal way with a
4 MB RAM cart.

ares quirk: with the 32X on, ares sends 68000 accesses to `0x000000`–`0x0000FF` to the 32X even with
RV = 1, so the diagnostic disc's cart probe (it writes cart offset 0) says `SAFE CART: NOT FOUND` there.
The game's first 256 bytes are the same as EDCD's (both from Chilly Willy's template), so loading is not
affected.

**Console:** not yet.
