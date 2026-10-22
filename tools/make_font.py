#!/usr/bin/env python3
"""make_font.py - an 8x8 font (ASCII 32-127) as a C header, rasterised from a monospace TrueType font.

    python3 tools/make_font.py /usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf hello/src/font8x8.h

One byte per row, bit 7 = leftmost pixel.  Needs Pillow."""
import sys
from PIL import Image, ImageDraw, ImageFont

def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    font = ImageFont.truetype(sys.argv[1], 9)
    rows = []
    for c in range(32, 128):
        im = Image.new("L", (8, 8), 0)
        d = ImageDraw.Draw(im)
        bb = font.getbbox("M")
        d.text((0, -bb[1]), chr(c) if c < 127 else " ", fill=255, font=font)
        glyph = []
        for y in range(8):
            v = 0
            for x in range(8):
                if im.getpixel((x, y)) > 110:
                    v |= 0x80 >> x
            glyph.append(v)
        if c == ord('_') and not any(glyph):
            glyph[7] = 0xFC                 # the font draws '_' below the 8x8 cell: put it on the last row
        rows.append(glyph)
    with open(sys.argv[2], "w") as f:
        f.write("/* font8x8.h - ASCII 32-127, 8 x 8, bit 7 = left (tools/make_font.py, %s) */\n" % sys.argv[1].split("/")[-1])
        f.write("static const unsigned char font8x8[96][8] = {\n")
        for c, g in zip(range(32, 128), rows):
            note = " (drawn on the last row: the font put it below the cell)" if c == ord('_') else ""
            f.write("    {%s},   /* %r%s */\n" % (", ".join("0x%02X" % b for b in g), chr(c) if c < 127 else "DEL", note))
        f.write("};\n")

if __name__ == "__main__":
    main()
