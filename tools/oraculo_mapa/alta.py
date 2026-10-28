#!/usr/bin/env python3
"""Convierte los gráficos de alta resolución de DATA (los que usa CorsixTH a
640 × 480) a los formatos del motor, para dibujar con th64sim lo mismo que
CorsixTH y compararlo con sus capturas. La N64 usa los de DATAM, que convierte
tools/conv/conv.py.

Uso: python3 -I tools/oraculo_mapa/alta.py <dir_salida>   (lee $TH_DATA_DIR)

Escribe en dir_salida:
  vblk-0.tsh       bloques del mapa (VBLK-0), más el 226 de las vistas giradas
                   (hojas.con_sombra_oeste)
  vspr-0.tsh       sprites de personas y objetos (VSPR-0)
  animaciones.tan  tablas de animación (VSTART/VFRA/VLIST/VELE-1)
  mpalette.dat     DATA/MPALETTE.DAT descomprimido (768 B, 6 bits por
                   componente), que th64sim acepta como paleta de CorsixTH
  panel02v.tsh     el panel inferior (DATA/PANEL02V)
  mpointer.tsh     los punteros (DATA/MPOINTER)
  font05v.tsh, font16v.tsh, font01v.tsh, font124v.tsh, font00v.tsh
                   las fuentes del panel y de las descripciones (QDATA)
  pref01v.pal      la paleta con la que CorsixTH dibuja las descripciones
"""
import os
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, "..", "conv"))
import anim  # noqa: E402
import conv  # noqa: E402
import hojas  # noqa: E402


def hoja(d, nombre, directorio="DATA"):
    sprites = hojas.leer(d.leer(directorio, nombre + ".TAB"), d.leer(directorio, nombre + ".DAT"),
                         hojas.es_compleja(nombre))
    if nombre == "VBLK-0":  # más la sombra sobre una pared oeste de las vistas giradas (motor/vista/girar.h)
        sprites = hojas.con_sombra_oeste(sprites, 32)
    tsh = hojas.escribir(sprites)
    if hojas.leer_tsh(tsh) != sprites:
        sys.exit(f"ERROR: la hoja {nombre} no se lee igual que se escribió")
    return tsh, len(sprites)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    salida = sys.argv[1]
    os.makedirs(salida, exist_ok=True)
    d = conv.Datos(os.environ.get("TH_DATA_DIR", ""))
    for nombre in ("VBLK-0", "VSPR-0"):
        tsh, n = hoja(d, nombre)
        with open(os.path.join(salida, nombre.lower() + ".tsh"), "wb") as f:
            f.write(tsh)
        print(f"{nombre}: {n} sprites, {len(tsh)} B")
    tablas = anim.leer(*[d.leer("DATA", f"V{t}-1.ANI") for t in ("START", "FRA", "LIST", "ELE")])
    tan = anim.escribir(*tablas)
    with open(os.path.join(salida, "animaciones.tan"), "wb") as f:
        f.write(tan)
    print(f"animaciones: {len(tablas[0])} animaciones, {len(tan)} B")
    for directorio, nombre in (("DATA", "PANEL02V"), ("DATA", "MPOINTER"), ("QDATA", "FONT05V"),
                               ("QDATA", "FONT16V"), ("QDATA", "FONT01V"), ("QDATA", "FONT124V"),
                               ("QDATA", "FONT00V")):
        tsh, n = hoja(d, nombre, directorio)
        with open(os.path.join(salida, nombre.lower() + ".tsh"), "wb") as f:
            f.write(tsh)
        print(f"{nombre}: {n} sprites, {len(tsh)} B")
    with open(os.path.join(salida, "pref01v.pal"), "wb") as f:
        f.write(d.leer("QDATA", "PREF01V.PAL"))
    pal = d.leer("DATA", "MPALETTE.DAT")
    with open(os.path.join(salida, "mpalette.dat"), "wb") as f:
        f.write(pal)
    print(f"paleta: {len(pal)} B")


if __name__ == "__main__":
    main()
