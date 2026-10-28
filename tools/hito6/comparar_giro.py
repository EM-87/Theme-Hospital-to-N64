#!/usr/bin/env python3
"""Compara la captura de CorsixTH con el hospital girado (captura_girada.lua)
con la vista girada del motor (th64sim escena4 --giro=r), píxel a píxel.

Uso: python3 -I tools/hito6/comparar_giro.py <captura.png> <vista.ppm> <entidades.pgm> <mezcla.pgm> [diferencias.png]

Separa tres zonas:
  - las entidades (objetos) del motor y dos píxeles alrededor: CorsixTH
    dibuja las animaciones con filtrado lineal, así que difieren un poco por
    dentro y en el borde (hito 3: 5 o menos por dentro y 7 o menos en el
    borde);
  - las sombras (los píxeles en los que lo último que se pintó era
    semitransparente): el redondeo de la mezcla, 5 o menos donde se cruzan dos;
  - el resto del mapa: suelos, paredes y decorado. Debe ser igual, salvo
    algún píxel a 5 o menos en el borde de una sombra.
Escribe una línea por zona: cuántos píxeles, iguales, a 2 o menos, de 3 a 7
y a más de 7, y la diferencia máxima. Termina con 1 si algo se pasa.
"""
import sys

import numpy as np
from PIL import Image


def main():
    if len(sys.argv) < 5:
        print(__doc__)
        return 2
    a = np.asarray(Image.open(sys.argv[1]).convert("RGB"), dtype=np.int16)
    b = np.asarray(Image.open(sys.argv[2]).convert("RGB"), dtype=np.int16)
    ent = np.asarray(Image.open(sys.argv[3]).convert("L")) > 0
    mezcla = np.asarray(Image.open(sys.argv[4]).convert("L")) > 0
    if a.shape != b.shape:
        print(f"tamaños distintos: {a.shape} y {b.shape}")
        return 1
    d = np.abs(a - b).max(axis=2)
    # Dos píxeles alrededor de las entidades: el borde filtrado.
    zona = ent.copy()
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            zona |= np.roll(np.roll(ent, dy, axis=0), dx, axis=1)
    malo = 0
    limites = {"mapa": 5, "sombras": 5, "entidades": 7}
    for nombre, m in (("mapa", ~zona & ~mezcla), ("sombras", ~zona & mezcla), ("entidades", zona)):
        v = d[m]
        n = int(v.size)
        cuentas = [int((v == 0).sum()), int(((v > 0) & (v <= 2)).sum()), int(((v > 2) & (v <= 7)).sum()),
                   int((v > 7).sum())]
        print(f"{nombre}: {n} píxeles; {cuentas[0]} iguales, {cuentas[1]} a 2 o menos, {cuentas[2]} de 3 a 7, "
              f"{cuentas[3]} a más de 7 (máxima {int(v.max()) if n else 0})")
        malo += int((v > limites[nombre]).sum())
    if len(sys.argv) > 5:
        img = (a // 3).astype(np.uint8)
        img[d > 2] = (255, 0, 0)
        Image.fromarray(img).save(sys.argv[5])
    return 1 if malo else 0


if __name__ == "__main__":
    sys.exit(main())
