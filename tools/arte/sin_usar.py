#!/usr/bin/env python3
"""Animaciones de los datos de TH que CorsixTH no usa: las que tienen
fotogramas propios y cuyo número no aparece en su Lua, quitando las que solo
repiten fotogramas de una usada. Es un tope: alguna puede usarse por un camino
indirecto (un número calculado). Con --hoja, una hoja de contacto con el
fotograma central de cada una, para mirarlas.

Uso: python3 -I tools/arte/sin_usar.py <salida.tsv> [--hoja hoja.png]
Salida: número de animación y fotogramas, una por línea.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from comun import LUA, Animaciones  # noqa: E402


def numeros_del_lua():
    out = set()
    for raiz, _, ficheros in os.walk(LUA):
        if os.sep + "languages" in raiz:
            continue
        for f in ficheros:
            if f.endswith(".lua"):
                for linea in open(os.path.join(raiz, f), errors="replace"):
                    linea = linea.split("--")[0]
                    out |= {int(x) for x in re.findall(r"(?<![\w.])(\d{1,4})(?![\w.])", linea)}
    return out


def main():
    an = Animaciones()
    usados = numeros_del_lua()
    validas = [a for a in range(len(an)) if not an.vacia(a)]
    alcanzados = {f for a in validas if a in usados for f in an.cadena(a)}
    no = [a for a in validas if a not in usados and not set(an.cadena(a)) <= alcanzados]
    spr_usados = an.sprites(alcanzados)
    propios = {f for a in no for f in an.cadena(a)} - alcanzados
    nuevos = an.sprites(propios) - spr_usados
    print(f"animaciones {len(an)}, con fotogramas propios {len(validas)}, sin usar {len(no)}: "
          f"{len(propios)} fotogramas que solo salen en ellas y {len(nuevos)} sprites que no usa ninguna usada")
    with open(sys.argv[1], "w") as o:
        for a in no:
            o.write(f"{a}\t{len(an.cadena(a))}\n")
    if "--hoja" in sys.argv:
        hoja(an, no, sys.argv[sys.argv.index("--hoja") + 1])


def hoja(an, lista, ruta, columnas=12, lado=128):
    import numpy as np
    from PIL import Image, ImageDraw
    from comun import leer
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "conv"))
    import hojas
    import paleta
    spr = hojas.leer(leer("DATA", "VSPR-0.TAB"), leer("DATA", "VSPR-0.DAT"), hojas.es_compleja("VSPR-0.DAT"))
    pal = np.array([tuple(paleta.a_8bit(c) for c in col)
                    for col in paleta.colores_6bit(leer("DATA", "MPALETTE.DAT"))], np.uint8)

    def pinta(f):
        img = np.full((300, 300, 3), 40, np.uint8)
        els = an.elementos_de(f)
        elegida = {}
        for _, _, _, capa, _, cid in els:
            if cid:
                elegida.setdefault(capa, cid)
        for s, x, y, capa, flags, cid in els:
            w, h, px = spr[s]
            if (cid and elegida.get(capa) != cid) or not w:
                continue
            a = np.frombuffer(px, np.uint8).reshape(h, w)
            if flags & 1:
                a = a[:, ::-1]
            if flags & 2:
                a = a[::-1, :]
            x0, y0 = x - 141 + 150, y - 186 + 230  # el desplazamiento de alta resolución
            for yy in range(h):
                for xx in range(w):
                    Y, X = y0 + yy, x0 + xx
                    if a[yy, xx] != 0xFF and 0 <= Y < 300 and 0 <= X < 300:
                        c = pal[a[yy, xx]]
                        img[Y, X] = img[Y, X] // 2 + c // 2 if flags & 0xC else c
        return Image.fromarray(img).resize((lado, lado))

    filas = (len(lista) + columnas - 1) // columnas
    out = Image.new("RGB", (columnas * lado, filas * (lado + 12)))
    d = ImageDraw.Draw(out)
    for k, a in enumerate(lista):
        c = an.cadena(a)
        x, y = (k % columnas) * lado, (k // columnas) * (lado + 12)
        out.paste(pinta(c[len(c) // 2]), (x, y + 12))
        d.text((x + 2, y), f"{a} ({len(c)})", fill=(255, 255, 0))
    out.save(ruta)


if __name__ == "__main__":
    main()
