#!/usr/bin/env python3
"""make_test_track.py - a short CD audio test track (a few tones), 44.1 kHz 16-bit stereo WAV.

    python3 tools/make_test_track.py hello/music/Track02.wav

So the example's "A = music" has something to play.  Replace it with your own music: disc
track N = what CMD 45 plays for N (track 1 is the data track)."""
import math, struct, sys, wave

def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    rate, notes = 44100, [262, 330, 392, 523, 392, 330]
    frames = bytearray()
    for n in notes * 3:
        for i in range(int(rate * 0.25)):
            env = min(1.0, i / 500.0) * max(0.0, 1.0 - i / (rate * 0.25))
            v = int(9000 * env * math.sin(2 * math.pi * n * i / rate))
            frames += struct.pack("<hh", v, v)
    w = wave.open(sys.argv[1], "wb")
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(rate)
    w.writeframes(bytes(frames)); w.close()
    print("==> %s (%.1f s)" % (sys.argv[1], len(frames) / 4 / rate))

if __name__ == "__main__":
    main()
