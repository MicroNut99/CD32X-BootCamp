# 14 – MD+ music from the EverDrive's SD card, and a game with no Sega CD

**Found with OutRun CD32X (MicroNut99/cannonballcd32x, `edcd-14mb`), on the console, 23 Nov 2026.** Ledger
section 40 has the registers and the command; this page is what a port needs to know. Nothing in `stack/` changed:
both parts are scripts that patch a build's **copy** of this kit (`mdplus/`), as `sdcard/subcpu/merge_sdsave.py` does.

## What was measured on the console

| | |
|---|---|
| A `.32X` started from the EverDrive PRO with a `.cue` of the same name beside it | **MD+ is on.** The EverDrive's OS turns it on as it does for a Genesis ROM |
| A 68000 write at cart 0x03F7FA with RV = 1, the 32X on | **Reaches the cart**: MD+ opened, and the cart's RAM read back the word. (MODE1-3's "the cart's first bytes take no write", 25 Oct, was measured at the first bytes; it is not all of window 0) |
| The SH2s' read of 0x2203F7F6 / 0x2203F7F8 | "RATE" (0x5241, 0x5445) while MD+ is open |
| The music | Played from the SD card through the cart's own sound output, heard with the 32X in the chain; the Sega CD's lid open |
| No Sega CD at all (Genesis + 32X + EverDrive) | The game runs (`mdplus/merge_nocd.py`); without it the launcher stops blue |

## MD+ in three lines

MD+ is krikzz's `mapper/lib_base/mdp.sv`; every core built from his FPGA tree with `MDP_ON` has it, a game's own core
(`map_orfx`, `map_dracx`, ...) included. It watches the **bus address**, not the mapper's banks:
write 0xCD54 at **0x03F7FA** to open it, then a command at **0x03F7FE**: `0x11tt` play track tt once, `0x12tt`
repeating, `0x1300` stop. The tracks are the `.cue`'s, track numbers as on the disc (track 1 is the data track).

**The address is five hex digits.** `'h3F7FA` is 0x03F7FA, in window 0 - not 0x3F7FAx. Three OutRun builds wrote the
wrong place before this was read properly; the emulator's memory dump showed where the words had gone.

## What a port does

1. **The files on the SD card, one name:** `<name>.32X` (the packed launcher), `<name>.cue`, and the `.bin` the cue
   names. A disc's own `.cue` / `.bin` work as they are (rename both and the `FILE` line inside the cue).
2. **The build:** after `merge_sdsave.py` (if used), `python3 mdplus/merge_mdp.py <copy>`, then build the copy with
   `MDP=1 M1MDP=1 M1WORDS=1` and `EDCD_SAVE=1` (the route is in the `M1SAVE` branch of `m1_cart`).
3. **The game:** command **105**, COMM2 = the MD+ command, sent like every copy made with RV = 1 (the slave parked,
   the master waiting in SDRAM); COMM12 = 0x4D50 says it was sent. Then read 0x2203F7F6 / F8: "RATE" = MD+ is there,
   play the music with 105 and never send 45 / 54; anything else = fall back to the CD's audio.
4. **Keep 0x03F7F6-0x03FFFF of window 0's bank free.** While MD+ is open those addresses read its registers and its
   data buffer, not the bank's memory.

## No Sega CD

`python3 mdplus/merge_nocd.py <copy>` (needs a packed launcher: `PACKED=1 EDCD_PACKED=1 EDCD_W0=<bank>`, ledger
section 35 - the program and the data come from the SD card). Where `edboot/edcd_md.s` found no Sega CD it now runs a
68000 program from work RAM that makes the listener's start and answers the game's commands itself: 48 (pad), 52, 38
(no file), 53 / 56 / 59 (nothing there), 57 / 58, 74 / 75 (the save bank), 105 (MD+). **It knows these commands
only** - a port that uses others (62-67, 68, 78, 79, 91, ...) adds them there. Lost with the Sega CD: its PCM chip's
effects, CD audio, backup RAM; a boot picture has to come with the launcher (OutRun: `ORSP` in the program's bank).
With a Sega CD attached nothing changes: the launcher starts the Sub-CPU as before.

## In ares

`ares-headless --machine 32x --game <launcher>.32X --ramcart-type everdrive` runs a 32X and the EverDrive with no
disc. MD+'s registers answer when `<launcher>.cue` is beside the launcher or with `ARES_MDPLUS=1`; the commands are
printed (`[MD+] play track 2, repeating`), there is no sound. `ARES_W0_LOG=1` prints the 68000's other writes to
window 0, which the model still drops.

## Not known

Where in window 0 the 32X stops passing the 68000's writes (0x03F7FA does; the first bytes did not). MD+'s other
commands (pause, volume, the SD-card file commands) from a 32X. The no-Sega-CD program's save bank on the console.
