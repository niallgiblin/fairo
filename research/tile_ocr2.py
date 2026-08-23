#!/usr/bin/env python3
"""Tile-OCR a schematic image with macOS Vision, printing REAL pixel coordinates."""
import subprocess, sys, os, re
from PIL import Image

IMG = 'pharaoh_schematic_3x.png'
im = Image.open(IMG)
W, H = im.size
print(f'image {W}x{H}', file=sys.stderr)

cols, rows = 6, 2
pad = 30
tiles = []
for r in range(rows):
    for c in range(cols):
        x0 = max(0, c * W // cols - (pad if c > 0 else 0))
        x1 = min(W, (c + 1) * W // cols + (pad if c < cols - 1 else 0))
        y0 = max(0, r * H // rows - (pad if r > 0 else 0))
        y1 = min(H, (r + 1) * H // rows + (pad if r < rows - 1 else 0))
        tiles.append((c, r, x0, x1, y0, y1))

os.makedirs('tiles', exist_ok=True)
for (c, r, x0, x1, y0, y1) in tiles:
    t = im.crop((x0, y0, x1, y1))
    tw, th = t.size
    t = t.resize((tw * 2, th * 2), Image.LANCZOS)
    fn = f'tiles/tile_{c}_{r}.png'
    t.save(fn)
    out = subprocess.run(['swift', 'ocr.swift', fn],
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        m = re.match(r'\[y=([\d.]+) x=([\d.]+)\] (.*)', line)
        if not m:
            continue
        ty, tx, text = float(m.group(1)), float(m.group(2)), m.group(3).strip()
        # normalized coords are relative to the RESIZED (2x) tile
        rx = x0 + tx * (x1 - x0)
        ry = y0 + ty * (y1 - y0)
        print(f'T{c},{r} [y={ry:5.0f} x={rx:5.0f}] {text}')
