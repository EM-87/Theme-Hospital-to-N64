#!/usr/bin/env python3
"""Convierte la hoja de bloques del mapa de DATA (VBLK-0, la de CorsixTH a
640 × 480) al formato TSH1 del motor, para dibujar con th64sim vista lo mismo
que CorsixTH y compararlo con sus capturas. La N64 usa la de DATAM (MBLK-0),
que convierte tools/conv/conv.py.

Uso: python3 -I tools/oraculo_mapa/vblk.py <salida.tsh> <salida_paleta.dat>   (lee $TH_DATA_DIR)

Escribe también DATA/MPALETTE.DAT descomprimido (768 B, 6 bits por componente),
que es lo que th64sim vista acepta como paleta de CorsixTH.
"""
import os
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, "..", "conv"))
import conv  # noqa: E402
import hojas  # noqa: E402


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    d = conv.Datos(os.environ.get("TH_DATA_DIR", ""))
    sprites = hojas.leer(d.leer("DATA", "VBLK-0.TAB"), d.leer("DATA", "VBLK-0.DAT"), False)
    tsh = hojas.escribir(sprites)
    if hojas.leer_tsh(tsh) != sprites:
        sys.exit("ERROR: la hoja no se lee igual que se escribió")
    with open(sys.argv[1], "wb") as f:
        f.write(tsh)
    pal = d.leer("DATA", "MPALETTE.DAT")
    with open(sys.argv[2], "wb") as f:
        f.write(pal)
    print(f"VBLK-0: {len(sprites)} sprites, {len(tsh)} B -> {sys.argv[1]}; paleta {len(pal)} B -> {sys.argv[2]}")


if __name__ == "__main__":
    main()
