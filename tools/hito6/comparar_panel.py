#!/usr/bin/env python3
"""Compara las capturas de CorsixTH del panel inferior (bench/scenarios/
captura_panel.lua) con lo que dibuja el motor en el PC (th64sim panel), donde
pinta la interfaz del motor (panel_N_ui.pgm: el panel entero, las letras y
el recuadro de las descripciones). El mapa ya se comparó en el hito 2.

CorsixTH dibuja las piezas de las ventanas con filtrado lineal: cada píxel
sale mezclado con sus vecinos y se desvía unas unidades del color de la
paleta. Las letras (bitmap_font, thdf_nearest) y los rectángulos van sin
filtrar. Así que:
  - letras y rectángulos: tienen que ser iguales;
  - piezas: se cuentan por la mayor diferencia de un componente (0, 1–2,
    3–7, más de 7). Una pieza mal colocada da diferencias grandes en bloque.

Uso: python3 -I tools/hito6/comparar_panel.py <dir> [--dif]
  dir: el de las capturas (capturas.txt, panel_N.png, panel_N.ppm, panel_N_ui.pgm).
  --dif: escribe panel_N_dif.png, con lo que difiere en más de 7 en rojo
  (en amarillo las letras o rectángulos que difieren en algo).
"""
import os
import sys

from PIL import Image

PANEL = 3                  # 1 + TH_SHEET_PANEL
LETRAS = range(5, 10)      # 1 + TH_SHEET_FONT_*
RECTANGULO = 255


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 1:
        sys.exit(__doc__)
    d = args[0]
    dif = "--dif" in sys.argv
    n = 0
    tot = {"letras": [0, 0], "piezas": [0, 0, 0, 0]}
    for linea in open(os.path.join(d, "capturas.txt")):
        if not linea.strip():
            continue
        n += 1
        a = Image.open(os.path.join(d, f"panel_{n}.png")).convert("RGB").load()
        b = Image.open(os.path.join(d, f"panel_{n}.ppm")).convert("RGB").load()
        m = Image.open(os.path.join(d, f"panel_{n}_ui.pgm")).load()
        w, h = Image.open(os.path.join(d, f"panel_{n}_ui.pgm")).size
        img = Image.new("RGB", (w, h)) if dif else None
        letras = [0, 0]
        piezas = [0, 0, 0, 0]
        for y in range(h):
            for x in range(w):
                hoja = m[x, y]
                if not hoja:
                    continue
                dmax = max(abs(p - q) for p, q in zip(a[x, y], b[x, y]))
                if hoja in LETRAS or hoja == RECTANGULO:
                    letras[0] += 1
                    letras[1] += dmax > 0
                    malo = dmax > 0
                    color = (255, 255, 0)
                else:
                    piezas[0 if dmax == 0 else 1 if dmax <= 2 else 2 if dmax <= 7 else 3] += 1
                    malo = dmax > 7
                    color = (255, 0, 0)
                if img:
                    img.putpixel((x, y), color if malo else tuple(v // 3 for v in a[x, y]))
        for k in range(2):
            tot["letras"][k] += letras[k]
        for k in range(4):
            tot["piezas"][k] += piezas[k]
        if img:
            img.save(os.path.join(d, f"panel_{n}_dif.png"))
        print(f"captura {n}: letras y rectángulos {letras[0]} píxeles, {letras[1]} distintos; piezas "
              f"{sum(piezas)} píxeles: {piezas[0]} iguales, {piezas[1]} a 1–2, {piezas[2]} a 3–7, {piezas[3]} a más de 7")
    l, p = tot["letras"], tot["piezas"]
    print(f"total: {n} capturas; letras y rectángulos {l[0]} píxeles, {l[1]} distintos; piezas {sum(p)} píxeles: "
          f"{p[0]} iguales, {p[1]} a 1–2, {p[2]} a 3–7, {p[3]} a más de 7")
    return 0 if l[1] == 0 and p[3] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
