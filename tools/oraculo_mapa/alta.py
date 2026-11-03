#!/usr/bin/env python3
"""Convierte los gráficos de alta resolución de DATA (los que usa CorsixTH a
640 × 480) a los formatos del motor, para dibujar con th64sim lo mismo que
CorsixTH y compararlo con sus capturas. La N64 usa los de DATAM, que convierte
tools/conv/conv.py.

Uso: python3 -I tools/oraculo_mapa/alta.py <dir_salida> [--arte]   (lee $TH_DATA_DIR)

Con --arte, la hoja de sprites y las animaciones llevan además el arte nuevo
(tools/arte/importar.py), con los mismos índices que en motor/assets. Sin él,
el motor dibuja con el espejo de CorsixTH, como el oráculo: las reglas de
motor/assets nombran animaciones que estas tablas no tienen, y la vista las
ignora.

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

sys.path.insert(0, os.path.join(AQUI, "..", "arte"))
import importar  # noqa: E402


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
    if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != "--arte"):
        sys.exit(__doc__)
    salida = sys.argv[1]
    con_arte = len(sys.argv) == 3
    os.makedirs(salida, exist_ok=True)
    d = conv.Datos(os.environ.get("TH_DATA_DIR", ""))
    esc, nuevas = importar.aplicar(d, "alta") if con_arte else (None, [])
    for nombre in ("VBLK-0", "VSPR-0"):
        if nombre == "VSPR-0" and esc:
            tsh, n = hojas.escribir(esc.sprites), len(esc.sprites)
        else:
            tsh, n = hoja(d, nombre)
        with open(os.path.join(salida, nombre.lower() + ".tsh"), "wb") as f:
            f.write(tsh)
        print(f"{nombre}: {n} sprites, {len(tsh)} B")
    tablas = anim.leer(*[d.leer("DATA", f"V{t}-1.ANI") for t in ("START", "FRA", "LIST", "ELE")])
    if esc:
        tablas = (esc.prim, esc.fot, esc.lista, esc.ele)
        for n in nuevas:
            print(f"arte nuevo: {n['objeto']} {n['orientacion']} -> animación {n['animacion']}, sprite {n['sprite']}")
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
