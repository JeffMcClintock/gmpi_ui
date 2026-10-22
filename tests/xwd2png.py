#!/usr/bin/env python3
"""xwd (X Window Dump, 32bpp ZPixmap) to PNG. Usage: xwd2png.py in.xwd out.png"""
import struct, sys
from PIL import Image
src, dst = sys.argv[1], sys.argv[2]
with open(src, "rb") as f:
    data = f.read()
hdr = struct.unpack(">25I", data[:100])
header_size, _, _, depth, width, height, _, byte_order, _, _, _, bpp, bytes_per_line, _, rmask, gmask, bmask, _, _, ncolors = hdr[:20]
off = header_size + ncolors * 12
img = Image.new("RGB", (width, height))
px = img.load()
for y in range(height):
    row = data[off + y * bytes_per_line: off + y * bytes_per_line + width * 4]
    for x in range(width):
        b0, b1, b2, b3 = row[x*4:x*4+4]
        v = (b0 << 24 | b1 << 16 | b2 << 8 | b3) if byte_order == 1 else (b3 << 24 | b2 << 16 | b1 << 8 | b0)
        def chan(mask):
            if not mask: return 0
            shift = (mask & -mask).bit_length() - 1
            return (v & mask) >> shift
        px[x, y] = (chan(rmask), chan(gmask), chan(bmask))
img.save(dst)
