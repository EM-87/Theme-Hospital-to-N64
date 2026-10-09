#!/usr/bin/env python3
"""Análisis de los gráficos de Theme Hospital para la N64 (fase 2).

Uso: graficos.py <dir_trabajo> <salida.json>
  (lee <dir_trabajo>/raw/, que deja inventario.py, y escribe blobs en
   <dir_trabajo>/blobs/ para medir después la compresión con mkasset)

Hojas de sprites: .TAB (6 B por sprite: posición u32, ancho u8, alto u8) y
.DAT con los píxeles en "chunks" (RLE con 0xFF como transparente), en modo
simple o complejo según la hoja, como en chunk_renderer::decode_chunks de
CorsixTH. Para cada sprite se cuentan los colores distintos y se calcula su
tamaño como textura de la N64:
  CI8: 1 B por píxel, filas alineadas a 8 B (TMEM), paleta global de 256.
  CI4: 0,5 B por píxel, filas alineadas a 8 B, posible si usa <= 15 colores
       (el 16.º es el transparente); paleta propia de 16 entradas (32 B). Se
       usa solo cuando sale más pequeño que CI8.
Bitmaps de pantalla completa (QDATA/*01V.DAT con su .PAL): 640 px de ancho,
8 bpp; se reducen a 320x240 tomando uno de cada 2x2 píxeles.
Las hojas de QDATA (ventanas a pantalla completa) solo existen a 640x480, así
que también se reducen a la mitad del mismo modo para medir su tamaño a
320x240 (DATAM ya trae a baja resolución el panel, el reloj y los menús).
CI4 cuantizado (grupos que usan MPALETTE.DAT): como hizo la versión de PS1,
todo sprite pasa a CI4 con 15 colores propios; los que tienen más se reducen
a sus 15 colores más frecuentes y el resto se sustituye por el más cercano en
RGB. Mide el tamaño de esa opción con pérdida; la calidad se juzga aparte.
"""
import json
import os
import re
import struct
import sys

COMPLEX = re.compile(r"^(PANEL|WATCH|PULLD|MUTTON|REQ\d|\w+02V|AWARD03V)")


def decode(data, w, h, complex_):
    out = bytearray([0xFF]) * (w * h)
    pos, n = 0, w * h
    i, L = 0, len(data)
    # skip_eol de chunk_renderer: un 0 al principio de una línea no rellena nada
    # si lo anterior acabó justo en el final de la línea anterior.
    skip_eol = False

    def fill(amt, col):
        nonlocal pos, skip_eol
        amt = max(0, min(amt, n - pos))
        if amt:
            out[pos:pos + amt] = bytes([col]) * amt
            pos += amt
            skip_eol = True

    def copy(amt):
        nonlocal pos, i, skip_eol
        amt = max(0, min(amt, n - pos, L - i))
        if amt:
            out[pos:pos + amt] = data[i:i + amt]
            pos += amt
            i += amt
            skip_eol = True

    while pos < n and i < L:
        b = data[i]
        i += 1
        if b == 0:
            if pos % w or not skip_eol:
                fill(w - pos % w, 0xFF)
            skip_eol = False
        elif complex_:
            if b < 0x40:
                copy(b)
            elif (b & 0xC0) == 0x80:
                fill(b - 0x80, 0xFF)
            elif b == 0xFF:
                if i + 1 >= L:
                    break
                fill(data[i], data[i + 1])
                i += 2
            else:
                amt = b - 60 - (b & 0x80) // 2
                col = data[i] if i < L else 0
                i += 1
                fill(amt, col)
        else:
            if b < 0x80:
                copy(b)
            else:
                fill(0x100 - b, 0xFF)
    return bytes(out)


def ci8_bytes(w, h):
    return ((w + 7) // 8) * 8 * h


def ci4_bytes(w, h):
    return ((w + 15) // 16) * 8 * h + 32


def quantize15(px, colors, pal):
    """Índices 0-14 por píxel (15 = transparente) con los 15 colores más usados."""
    freq = {}
    for p in px:
        if p != 0xFF:
            freq[p] = freq.get(p, 0) + 1
    keep = sorted(freq, key=lambda c: -freq[c])[:15]
    lut = {c: j for j, c in enumerate(keep)}
    for c in colors:
        if c not in lut:
            r, g, b = pal[c]
            lut[c] = min(range(len(keep)), key=lambda j: (pal[keep[j]][0] - r) ** 2 +
                         (pal[keep[j]][1] - g) ** 2 + (pal[keep[j]][2] - b) ** 2)
    return [lut.get(p, 15) for p in px]


def ci4_rows(idx, w, h):
    pad16 = ((w + 15) // 16) * 16
    nib = []
    for y in range(h):
        nib += idx[y * w:(y + 1) * w] + [15] * (pad16 - w)
    return bytes((nib[j] << 4) | nib[j + 1] for j in range(0, len(nib), 2)) + bytes(32)


def analyze_sheet(raw, d, name, pal=None):
    tab = open(os.path.join(raw, d, name + ".TAB"), "rb").read()
    dat = open(os.path.join(raw, d, name + ".DAT"), "rb").read()
    cx = bool(COMPLEX.match(name))
    res = {"hoja": f"{d}/{name}", "complejo": cx, "dat_bytes": len(dat), "sprites": 0,
           "pixeles": 0, "opacos": 0, "ci8": 0, "ci4_o_ci8": 0, "ci4_posibles": 0, "mitad_ci8": 0,
           "ci4_cuantizado": 0}
    blob_ci8, blob_mix, blob_half, blob_q = bytearray(), bytearray(), bytearray(), bytearray()
    for k in range(0, len(tab) - 5, 6):
        off, w, h = struct.unpack_from("<IBB", tab, k)
        if not w or not h:
            continue
        px = decode(dat[off:], w, h, cx)
        colors = set(px) - {0xFF}
        res["sprites"] += 1
        res["pixeles"] += w * h
        res["opacos"] += sum(1 for p in px if p != 0xFF)
        res["ci8"] += ci8_bytes(w, h)
        pad8 = ((w + 7) // 8) * 8
        rows = b"".join(px[y * w:(y + 1) * w] + b"\xff" * (pad8 - w) for y in range(h))
        blob_ci8 += rows
        hw, hh = max(1, w // 2), max(1, h // 2)
        hpad = ((hw + 7) // 8) * 8
        res["mitad_ci8"] += ci8_bytes(hw, hh)
        blob_half += b"".join(px[(2 * y) * w:(2 * y + 1) * w:2][:hw] + b"\xff" * (hpad - hw) for y in range(hh))
        # CI4 solo si cabe (<= 15 colores) y además sale más pequeño que CI8:
        # en sprites diminutos los 32 B de su paleta lo encarecen.
        if len(colors) <= 15 and ci4_bytes(w, h) < ci8_bytes(w, h):
            res["ci4_posibles"] += 1
            res["ci4_o_ci8"] += ci4_bytes(w, h)
            lut = {c: j for j, c in enumerate(sorted(colors))}
            blob_mix += ci4_rows([lut.get(p, 15) for p in px], w, h)
        else:
            res["ci4_o_ci8"] += ci8_bytes(w, h)
            blob_mix += rows
        if pal:
            res["ci4_cuantizado"] += ci4_bytes(w, h)
            blob_q += ci4_rows(quantize15(px, colors, pal), w, h)
    return res, bytes(blob_ci8), bytes(blob_mix), bytes(blob_half), bytes(blob_q)


def main():
    work, out = sys.argv[1], sys.argv[2]
    raw = os.path.join(work, "raw")
    blobs = os.path.join(work, "blobs")
    os.makedirs(blobs, exist_ok=True)
    sheets = []
    p = open(os.path.join(raw, "DATA", "MPALETTE.DAT"), "rb").read()
    mpal = [tuple(4 * p[3 * i + k] for k in range(3)) for i in range(256)]
    groups = {"V": ("DATA", r"^(VSPR-0|VBLK-0)$"), "M": ("DATAM", r"^(MSPR-0|MBLK-0)$"),
              "UI_DATA": ("DATA", r"^(?!VSPR|VBLK|LANG)"), "UI_DATAM": ("DATAM", r"^(?!MSPR|MBLK)"),
              "QDATA_UI": ("QDATA", r"^(?!FONT)"), "QDATA_FONT": ("QDATA", r"^FONT"),
              "QDATAM": ("QDATAM", r".")}
    summary = {}
    for gname, (d, rx) in groups.items():
        names = sorted({f[:-4] for f in os.listdir(os.path.join(raw, d)) if f.endswith(".TAB")})
        names = [n for n in names if re.search(rx, n) and os.path.exists(os.path.join(raw, d, n + ".DAT"))]
        acc = {"hojas": 0, "sprites": 0, "pixeles": 0, "opacos": 0, "dat_bytes": 0, "ci8": 0,
               "ci4_o_ci8": 0, "ci4_posibles": 0, "mitad_ci8": 0, "ci4_cuantizado": 0}
        b8, bm, bh, bq = bytearray(), bytearray(), bytearray(), bytearray()
        quant = gname == "M"
        for n in names:
            r, x8, xm, xh, xq = analyze_sheet(raw, d, n, mpal if quant else None)
            sheets.append(r)
            acc["hojas"] += 1
            for k in ("sprites", "pixeles", "opacos", "dat_bytes", "ci8", "ci4_o_ci8", "ci4_posibles",
                      "mitad_ci8", "ci4_cuantizado"):
                acc[k] += r[k]
            b8 += x8
            bm += xm
            bh += xh
            bq += xq
        open(os.path.join(blobs, f"sprites_{gname}_ci8.bin"), "wb").write(b8)
        open(os.path.join(blobs, f"sprites_{gname}_mixto.bin"), "wb").write(bm)
        if not quant:
            del acc["ci4_cuantizado"]
            for r in sheets[-len(names):]:
                del r["ci4_cuantizado"]
        if gname == "QDATA_UI":
            open(os.path.join(blobs, f"sprites_{gname}_mitad_ci8.bin"), "wb").write(bh)
        if quant:
            open(os.path.join(blobs, f"sprites_{gname}_ci4q.bin"), "wb").write(bq)
        summary[gname] = acc
        print(f"{gname:10s} {acc['hojas']:3d} hojas {acc['sprites']:5d} sprites {acc['pixeles'] / 1e6:6.2f} Mpx "
              f"RLE {acc['dat_bytes'] / 1e6:5.2f} MB | CI8 {acc['ci8'] / 1e6:5.2f} MB | CI4/CI8 {acc['ci4_o_ci8'] / 1e6:5.2f} MB "
              f"({acc['ci4_posibles']} en CI4)")

    # Bitmaps de pantalla completa: .DAT con .PAL del mismo nombre y sin .TAB.
    bitmaps, small = [], bytearray()
    for d in ("QDATA",):
        for f in sorted(os.listdir(os.path.join(raw, d))):
            base = f[:-4]
            if f.endswith(".DAT") and os.path.exists(os.path.join(raw, d, base + ".PAL")) \
                    and not os.path.exists(os.path.join(raw, d, base + ".TAB")):
                b = open(os.path.join(raw, d, f), "rb").read()
                # Los *01V son de 640x480; MAIN01M (menú de la versión de baja
                # resolución) ya es de 320x200 y se deja tal cual.
                if base.endswith("M"):
                    w = 320
                    h = len(b) // w
                    bitmaps.append({"bitmap": f"{d}/{f}", "ancho": w, "alto": h, "bytes": len(b),
                                    "a_320x240_ci8": len(b)})
                    small += b
                    continue
                w = 640
                h = len(b) // w
                bitmaps.append({"bitmap": f"{d}/{f}", "ancho": w, "alto": h, "bytes": len(b),
                                "a_320x240_ci8": (w // 2) * (h // 2)})
                for y in range(0, h - 1, 2):
                    small += b[y * w:(y + 1) * w:2]
    open(os.path.join(blobs, "pantallas_320_ci8.bin"), "wb").write(small)
    bsum = {"bitmaps": len(bitmaps), "bytes": sum(b["bytes"] for b in bitmaps),
            "a_320x240_ci8": sum(b["a_320x240_ci8"] for b in bitmaps)}
    print(f"pantallas  {bsum['bitmaps']} bitmaps {bsum['bytes'] / 1e6:.2f} MB originales; "
          f"{bsum['a_320x240_ci8'] / 1e6:.2f} MB a 320x240 CI8")
    json.dump({"grupos": summary, "pantallas": bsum, "hojas": sheets, "bitmaps": bitmaps},
              open(out, "w"), indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
