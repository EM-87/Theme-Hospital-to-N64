#!/usr/bin/env python3
"""Imagen de comparación: sprites de DATAM en CI8 original frente a CI4 cuantizado.

Uso: muestra_ci4.py <dir_trabajo> <salida.png> [n_sprites]

Toma de MSPR-0 los sprites con más de 15 colores (los únicos que cambian al
cuantizar), repartidos por toda la hoja, y los dibuja a 3x: arriba el
original, abajo la versión de 15 colores de graficos.quantize15.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from graficos import decode, quantize15  # noqa: E402
from PIL import Image  # noqa: E402

SCALE, GAP, BG = 3, 6, (96, 96, 112)


def main():
    work, out = sys.argv[1], sys.argv[2]
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 24
    raw = os.path.join(work, "raw")
    p = open(os.path.join(raw, "DATA", "MPALETTE.DAT"), "rb").read()
    pal = [tuple(4 * p[3 * i + k] for k in range(3)) for i in range(256)]
    tab = open(os.path.join(raw, "DATAM", "MSPR-0.TAB"), "rb").read()
    dat = open(os.path.join(raw, "DATAM", "MSPR-0.DAT"), "rb").read()
    picks = []
    for k in range(0, len(tab) - 5, 6):
        off, w, h = struct.unpack_from("<IBB", tab, k)
        if w < 16 or h < 24:
            continue
        px = decode(dat[off:], w, h, False)
        colors = set(px) - {0xFF}
        if len(colors) > 15:
            picks.append((w, h, px, colors))
    picks = [picks[i * len(picks) // count] for i in range(count)]
    width = sum(w for w, *_ in picks) * SCALE + GAP * (len(picks) + 1)
    hmax = max(h for _, h, *_ in picks) * SCALE
    img = Image.new("RGB", (width, 2 * hmax + 3 * GAP), BG)
    x = GAP
    for w, h, px, colors in picks:
        q = quantize15(px, colors, pal)
        freq = {}
        for c in px:
            if c != 0xFF:
                freq[c] = freq.get(c, 0) + 1
        keep = sorted(freq, key=lambda c: -freq[c])[:15]
        for row, idx in ((0, px), (1, [keep[i] if i < 15 else 0xFF for i in q])):
            s = Image.new("RGB", (w, h), BG)
            s.putdata([pal[c] if c != 0xFF else BG for c in idx])
            img.paste(s.resize((w * SCALE, h * SCALE), Image.NEAREST),
                      (x, GAP + row * (hmax + GAP) + hmax - h * SCALE))
        x += w * SCALE + GAP
    img.save(out)
    print(f"{len(picks)} sprites -> {out} ({img.size[0]}x{img.size[1]})")


if __name__ == "__main__":
    main()
