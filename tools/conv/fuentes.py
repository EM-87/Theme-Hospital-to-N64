"""Fuentes de baja resolución de Theme Hospital (QDATAM) a BMFont (.fnt de
texto y atlas .png) para que mkfont las pase al formato font64 de libdragon
(lo hace el Makefile del motor).

Una fuente de TH es una hoja de sprites normal: el carácter c (CP437) es el
sprite c − 31 (bitmap_font, th_gfx_font.cpp de CorsixTH). Los textos del
motor van en UTF-8, así que cada carácter se identifica por su código
Unicode, con la misma tabla de CP437 a Unicode que CorsixTH
(Src/cp437_table.h; no es CP437 puro: 0x92 pasa a «?»). Si dos bytes dan el
mismo código, se queda el primero (el «?» de verdad es 0x3F).

El avance de cada carácter es su ancho: CorsixTH suma la separación entre
letras al dibujar (setSeparation), y en el motor la pone quien dibuja.
"""
import io
import os
import re

FUENTES = ("FONT100M", "FONT101M", "FONT130M", "FONT131M", "FONT132M")


def tabla_cp437(corsixth_dir):
    """Los 128 códigos Unicode de 0x80–0xFF, leídos de cp437_table.h."""
    src = open(os.path.join(corsixth_dir, "CorsixTH", "Src", "cp437_table.h"), encoding="utf-8").read()
    cuerpo = re.search(r"cp437_to_unicode_table\s*\{(.*?)\}", src, re.S).group(1)
    cuerpo = re.sub(r"/\*.*?\*/", "", cuerpo, flags=re.S)
    cuerpo = re.sub(r"//[^\n]*", "", cuerpo)
    tabla = [int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]+)", cuerpo)]
    if len(tabla) != 128:
        raise ValueError(f"cp437_to_unicode_table tiene {len(tabla)} entradas")
    return tabla


def unicode_de(byte, tabla):
    return byte if byte < 0x80 else tabla[byte - 0x80]


def convertir(nombre, sprites, rgb, tabla):
    """Devuelve (texto .fnt, PNG, {código: byte}) de una hoja de fuente."""
    from PIL import Image
    glifos = {}
    for s, (w, h, px) in enumerate(sprites):
        c = s + 31
        if 32 <= c <= 255 and w and h:
            u = unicode_de(c, tabla)
            if u not in glifos:
                glifos[u] = (c, w, h, px)
    alto = max(g[2] for g in glifos.values())
    ancho_atlas, x, y = 256, 0, 0
    sitio = {}
    for u, (c, w, h, px) in sorted(glifos.items(), key=lambda kv: kv[1][0]):
        if x + w > ancho_atlas:
            x, y = 0, y + alto + 1
        sitio[u] = (x, y)
        x += w + 1
    alto_atlas = y + alto
    img = Image.new("RGBA", (ancho_atlas, alto_atlas), (0, 0, 0, 0))
    for u, (c, w, h, px) in glifos.items():
        gx, gy = sitio[u]
        for j in range(h):
            for i in range(w):
                v = px[j * w + i]
                if v != 255:
                    img.putpixel((gx + i, gy + j), tuple(rgb[v]) + (255,))
    png = io.BytesIO()
    img.save(png, "PNG")
    base = nombre.lower()
    lineas = [
        f'info face="{base}" size={alto} bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=0 aa=1 '
        "padding=0,0,0,0 spacing=1,1",
        f"common lineHeight={alto} base={alto} scaleW={ancho_atlas} scaleH={alto_atlas} pages=1 packed=0",
        f'page id=0 file="{base}.png"',
        f"chars count={len(glifos)}",
    ]
    for u, (c, w, h, px) in sorted(glifos.items(), key=lambda kv: kv[1][0]):
        gx, gy = sitio[u]
        lineas.append(f"char id={u} x={gx} y={gy} width={w} height={h} xoffset=0 yoffset=0 xadvance={w} "
                      "page=0 chnl=15")
    return "\n".join(lineas) + "\n", png.getvalue(), {u: g[0] for u, g in glifos.items()}


def cobertura(textos, glifos):
    """Caracteres (no de control) de los textos que no tiene la fuente, con
    cuántas veces aparecen."""
    falta = {}

    def recorrer(v):
        if isinstance(v, dict):
            for x in v.values():
                recorrer(x)
        elif isinstance(v, list):
            for x in v:
                recorrer(x)
        elif isinstance(v, str):
            for ch in v:
                if ord(ch) >= 32 and ord(ch) not in glifos:
                    falta[ch] = falta.get(ch, 0) + 1

    recorrer(textos)
    return falta
