# sdcard/ – game saves on the Mega EverDrive PRO's SD card

A CD32X game started from the EverDrive (its `EDCD_<GAME>.32X` launcher) can keep its saves on the
EverDrive's SD card. They survive the EverDrive menu, a power-off and a relaunch. This folder has
everything needed: the SH2 code for the game, the two Sega CD commands, a build script, a demo disc and a
tool to look inside the save file.

It is the method OpenLara CD32X-REV EDCD proved on a console (26 Oct 2026: save, load, relaunch, load) and
Rayman CD32X uses, made into one kit. **The boot stack is not changed**: the commands go into a copy.

## How it works, in one paragraph

The EverDrive keeps its **bank 31** (battery RAM) on the SD card as
`MEGA/gamedata/<launcher name>/bram.srm` (256 KB) – but only when the launcher's header says the game has
save RAM. `EDCD_SAVE=1` puts that in the header. The 32X can't see bank 31, so the game asks the Sega CD
to copy 4 KB at a time between bank 31 and a buffer. While a copy runs, both SH2s wait in their own RAM
(the console gives an SH2 garbage if it touches the cartridge then).

## Try it first: the demo

```bash
bash sdcard/demo/build_demo.sh out/
```

You get `out/SDSAVE_DEMO.cue/.bin` (burn it) and `out/EDCD_SDSAVEDEMO.32X` (copy it to the SD card).

1. Start `EDCD_SDSAVEDEMO.32X` from the EverDrive menu with the disc in the Sega CD.
2. The screen shows **BOOTS: 1**, **SAVED**, **SD BANK: YES**, `LD 6` (no save yet), `SV 0` (saved).
3. Go back to the EverDrive menu and start it again: **BOOTS: 2**, `LD 0`. START saves again.
4. The card now has `MEGA/gamedata/EDCD_SDSAVEDEMO/bram.srm`. Look inside:
   `python3 sdcard/tools/srm_dump.py bram.srm` → `record at 0x00000: magic 53445631 'SDV1' ... 00000002`.

Booted from the disc alone, the demo says **SD BANK: NO**, `LD 1 SV 1`, **SAVE FAILED** – correct: only
the launcher has the bank.

## Adding it to your game

1. **SH2 code.** Add `sdcard/sh2/sdsave.c` and `sdsave.h` to your game and call:

   ```c
   #include "sdsave.h"
   if (sdsave_present()) {                                  /* 1 = started from the launcher */
       if (sdsave_load_record(0, 0x4D594731 /* "MYG1" */, &save, sizeof save) != SDSAVE_OK)
           memset(&save, 0, sizeof save);                   /* no save yet */
       ...
       sdsave_save_record(0, 0x4D594731, &save, sizeof save); /* SDSAVE_OK = written and checked */
   }
   ```

2. **Slave SH2.** In the slave's main loop, add (it must not clear COMM4 itself):

   ```c
   if ((uint16_t)MARS_SYS_COMM4 == SDSAVE_CMD_PARK) sdsave_slave_park();
   ```

   If your slave already uses COMM4 for its own commands, add `SDSAVE_CMD_PARK` as one more command
   (`0x7A60`; change it with `-DSDSAVE_CMD_PARK=...` if that number is taken).

3. **Build.** `bash sdcard/build_sdsave.sh out/ MYGAME` makes the disc's Sega CD program
   (`CDROMPlayer_SDSAVE.bin`: if your disc build takes the stack's `CDROMPlayer.bin`, use this one) and the launcher
   (`EDCD_MYGAME.32X`). If your game builds with the BootCamp's `build.sh`, do what the demo's script
   does: copy the BootCamp, run `python3 sdcard/subcpu/merge_sdsave.py <copy> .`, then in the copy
   `SDSAVE=1 EDCD=1 EDCD_SAVE=1 GAME=<yours> bash build.sh`.

Use a **unique launcher name** per game and keep it: the name is the folder on the SD card, so a renamed
launcher starts with an empty save (the old one stays under the old name).

## What the numbers mean

| Code | Name | Meaning |
|---|---|---|
| 0 | `SDSAVE_OK` | Done (a write was read back and compared) |
| 1 | `SDSAVE_E_NOBANK` | Not started from the launcher, or the launcher was built without `EDCD_SAVE=1` |
| 2 | `SDSAVE_E_CMD` | The Sega CD did not answer – its program lacks the commands (step 3) |
| 3 | `SDSAVE_E_PARK` | The slave SH2 did not park – step 2 is missing |
| 4 | `SDSAVE_E_VERIFY` | Written, but read back differently – the save is not good |
| 5 | `SDSAVE_E_RANGE` | Past the end of the bank (256 KB) |
| 6 | `SDSAVE_E_EMPTY` | No save there yet (normal the first time) |
| 7 | `SDSAVE_E_BAD` | A save is there but damaged (cut off, or another layout) |

The functions return them negative (`-1` … `-7`); the demo shows them without the sign.

## Files

| File | What it is |
|---|---|
| `sh2/sdsave.c`, `sh2/sdsave.h` | The game's side (master SH2, plus the slave's park call) |
| `subcpu/merge_sdsave.py` | Puts the two Sega CD commands into a **copy** of the stack, behind `SDSAVE=1` |
| `build_sdsave.sh` | Both Sega CD programs and the launcher, built in a temporary copy |
| `demo/main.c`, `demo/build_demo.sh` | The demo: `hello/` with its boot counter on the SD card |
| `tools/srm_dump.py` | Lists what is in a `bram.srm` (or in bank 31 of an ares RAM-cart file) |
| `tools/ares_test.sh` | The demo's three ares runs (launch, relaunch, disc boot) and a contact sheet |
| `README_AI.md` | The same for an AI: the protocol, the rules and how to check the work |

## Tested so far

In ares (`ares-headless`, the EverDrive board, 27 Oct 2026), with the demo built by `build_demo.sh`:
first launch BOOTS 1 / SD BANK YES / saved; banks 0-30 wiped as the menu does (bank 31 kept), second
launch BOOTS 2 / LD 0; disc boot SD BANK NO without hanging. **Not yet run on a console** – the method
underneath is the console-proven OpenLara one; this kit's own console test is the demo above.
