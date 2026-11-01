# timeprobe: does /TIME reach the cart through the 32X?

The 16 MB paged cart switches pages on a write to `0xA130F0` / `0xA130F2`, and its GreenPAK only
takes one when the cart edge's **/TIME** pin (B31) is low while **/UWR** (B29) falls. Nobody has
yet seen /TIME at the 32X's top slot on a console:

- the 4 MB RAM cart has nothing on /TIME, so command 68's console run (19 Oct 2026) proved only that
  the command is answered;
- the Mega EverDrive PRO decodes `0xA130xx` from the address lines and /AS, not from the pin
  (krikzz/mega-ed-pub `fpga/mapper/lib_base/everdrive.sv:84`; its FPGA pin list has no /TIME), so its
  page writes working through the 32X say nothing about B31.

Software can't see the pin. This disc makes it move, the way the board's page switch will, and you
look at the cart edge.

## Build

```
cd /mnt/s/KoboPort/sega-toolchain-12.1/sega/CD32X-BootCamp
GAME=timeprobe CART=16 DISC_NAME=CD32X_TIMEPROBE bash build.sh 2>&1 | tee build_log.txt
```

`CART=16` puts command 68 into the Sub-CPU (stack/subcpu/Makefile, CART-1). Without it the screen
says "CMD 68 NOT ANSWERED" after 10 s. **Not with `EDCD=1`:** the EverDrive build turns page writes
into EverDrive register writes. Nothing in `stack/` changes; the disc uses command 68 and the
listener's routine 26 as they are.

## What it does

Each write is command 68 with COMM2 = 1: the 68000 listener's `safe_cart_xfer` (interrupts off,
RV = 1) writes the byte `0x01` to `0xA130F2`. That is one 68000 write cycle with /TIME low, /UWR low,
/LWR high, D8 high and D9-D15 low. Page 1 is what window 1 holds after reset, so the write changes
nothing on any cart (the 4 MB cart ignores it; a 16 MB board re-takes page 1).

The writes come in bursts: **WRITING** for one second, **QUIET** for one second, with a green block on
screen while writing. START switches to continuous writes, which is easier on a scope or analyzer.
The screen also shows the writes per second (a few hundred: each one is a round trip through the
Sub-CPU) and the total.

## What to measure

Boot it with the 4 MB RAM cart in the 32X as usual. The probe points are on the cart edge's B row,
counted as on the edge footprint of the 4 MB cart's board files (check which face is B there before
probing):

| Pin | Signal | Expect while WRITING | Expect while QUIET |
|---|---|---|---|
| B31 | /TIME | short low pulses, about 0.5 µs each | high, no pulses |
| B29 | /UWR  | low pulses at the same moments as B31 | pulses only from other cart writes, never inside B31 pulses |
| B28 | /LWR  | **no** pulse inside the B31 pulses (even byte) | - |
| A21 | D8    | high during each B31 pulse | - |
| A01 | GND   | probe ground | |

The cart fingers are inside the slot, so use a cart extender or riser, or tack thin wires onto the
4 MB cart's B31, B29 and B28 fingers where they meet the board, and pass them out of the shell.

- **Logic probe with a pulse LED** (pulse-stretching): put it on B31. It should blink once a second,
  in step with the green block, and stay dark in QUIET.
- **Logic analyzer** (any 8-channel 24 MHz USB one): B31, B29, B28, A21 and GND, continuous mode,
  trigger on B31 falling. You should see /UWR low inside the /TIME pulse, /LWR high, D8 high.
- **Scope:** the same on two channels, B31 and B29. Read the low and high levels too. The GreenPAK
  runs at 5 V and its inputs need about 3.5 V for a high in the standard input mode; write the levels
  down.

## Reading the result

- **B31 pulses with B29 low inside them:** the 32X passes /TIME and /UWR on these writes. The board's
  paging can work as designed.
- **B31 stays high, no pulses:** the 32X doesn't pass /TIME with the adapter on. Paging won't work on
  the board as drawn; the board still works as a 4 MB cart (pages stay 0 and 1).
- **B31 floats (wobbles or sits between levels):** the pin isn't connected through the 32X. The
  GreenPAK's /TIME input then needs a pull-up (internal, set in the `.gp6`, or a resistor), or noise
  could switch pages during ordinary cart writes.
- **B31 pulses but /UWR doesn't:** the 32X blocks the strobe on /TIME cycles. Same outcome as no
  /TIME.

A photo of the analyzer or scope trace, plus the levels, is the record to keep.
