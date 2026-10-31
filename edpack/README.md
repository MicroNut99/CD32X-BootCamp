# edpack - does the EverDrive PRO menu load a 14 MB `.32X` into its PSRAM whole?

**The question** (`manuals/MegaEverDrivePro_SDCARD.md` §4.0): a CD32X launcher `EDCD_<GAME>.32X` is a file
the EverDrive menu copies from the card into the PSRAM before starting it. If that file carries the game's
data behind its first 512 KB - at the cart offsets the game's own CMD 38 / 46 would fill - the menu has
preloaded the cart for free, with no EDIO code and no change to the stack. That works only if the OS loads
a multi-megabyte `.32X` in full in its 32X mode, which nobody has tried: every `.32X` run on it so far was
tiny, and krikzz's public source has no 32X mapper to read the answer from.

**The probe.** `edtest`'s 68000 side with a read-only SH2 program. `tools/pad_markers.py` pads the ROM to
14 MB (banks 0-27; 28-31 are the EverDrive's) and fills banks 1-27 with `edtest`'s pattern, one seed per
bank (word 0 of a bank is its seed). At run time nothing is written: CTRL0 = P only (`W` off, as a launcher
runs), each bank is mapped into window 1 and read by the 68000 (RV = 1) and both SH2s (cache-through).

```
bash edpack/build.sh          # -> edpack/EDPACK_14MB.32X (14 MB)
```

Copy it to the card, start it from the menu with the 32X attached, no disc. Time the menu's load with a
watch: that is the cost per launch of a packed launcher of this size.

| Screen | Meaning |
|---|---|
| `LOADED 14 OF 14 MB`, every bank `OK` | the OS loads the whole file: a packed launcher works as §4.0 describes |
| `LOADED n OF 14 MB`, then `=Bk` | the file arrived up to bank 2n; above it the PSRAM shows bank k's pattern: a mirror / a size mask at n MB |
| `LOADED n OF 14 MB`, then a hex word | the file arrived up to bank 2n; above it is whatever the wipe left: the OS stopped loading there |
| `SH2` on a bank | the 68000 reads the pattern, an SH2 does not (new: report it) |
| `MAPPER: NO CHANGE` / `DOES NOT RESPOND` | the SSF mapper is not on for this ROM (the header is `SEGA SSF`; was it started from the menu?) |
| `68000 NOT ANSWERING` + COMM dump | as in `edtest` |

Photograph the screen; the nonce top right (`RUN xxxx` in edtest's place) ties it to the build.

**Console, 31 Oct 2026** (Genesis + 32X + Mega EverDrive PRO, nonce 17E7): `LOADED 14 OF 14 MB`, banks 1-27 all `OK`, 0 bad words from the 68000 and both SH2s. The OS loads the whole file. ares cannot answer this one (its EverDrive board loads any size file); the console is the only test.

