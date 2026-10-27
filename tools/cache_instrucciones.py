#!/usr/bin/env python3
"""Choques en la caché de instrucciones de la VR4300 entre funciones que se
llaman unas a otras en los bucles calientes del dibujo.

La caché de instrucciones tiene 16 KiB, líneas de 32 B y correspondencia
directa: dos funciones cuyas direcciones coinciden módulo 16 KiB se echan una a
otra de la caché en cada llamada. libdragon va detrás de todo el código del
motor, así que su posición relativa cambia cada vez que el motor crece. En la
fase 5 (hito 4), un choque así duplicó el tiempo de dibujar los suelos (de 2,7
a 5,2 ms por fotograma).

Uso: python3 -I tools/cache_instrucciones.py <nm> <rom.elf> [--relleno]
  <nm>: mips64-elf-nm. Con --relleno, busca además cuántos bytes de relleno
  entre el motor y libdragon (RELLENO_CACHE en motor/Makefile) quitan los choques.
Devuelve 1 si hay choques.
"""
import subprocess
import sys

CACHE, LINEA = 16384, 32

# Grupos de funciones que se llaman en cada vuelta de un bucle: del motor y de
# libdragon. Solo se miran los choques entre unas y otras, que son los que
# cambian al crecer el motor (los de libdragon entre sí no los mueve nada).
GRUPOS = {
    # Un rectángulo por sprite (suelos, paredes y entidades) y, en casi cada
    # uno, la carga de su textura (cargar, si no está dentro de dibujar).
    "dibujo": (["dibujar", "mapa_n64_dibujar", "cargar"],
               ["__rdpq_texture_rectangle", "__rdpq_texture_rectangle_offline",
                "__rdpq_texture_rectangle_scaled_offline", "__rdpq_fixup_write8_syncchange",
                "__rdpq_write8_syncchangeuse", "__rdpq_write8_syncchange", "__rdpq_autosync_change",
                "__rdpq_write8"]),
}


def simbolos(nm, elf):
    out = subprocess.run([nm, "-S", "--defined-only", elf], capture_output=True, text=True, check=True).stdout
    tabla = {}
    for linea in out.splitlines():
        p = linea.split()
        if len(p) == 4 and p[2] in "tT":
            tabla[p[3]] = (int(p[0], 16), int(p[1], 16))
    return tabla


def lineas(dir_, tam):
    return {(a // LINEA) % (CACHE // LINEA) for a in range(dir_ & ~(LINEA - 1), dir_ + tam, LINEA)}


def choques(tabla, relleno=0):
    res = []
    for grupo, (motor, libdragon) in GRUPOS.items():
        for a in (n for n in motor if n in tabla):
            for b in (n for n in libdragon if n in tabla):
                (da, ta), (db, tb) = tabla[a], tabla[b]
                # El relleno mueve libdragon respecto al motor.
                comun = lineas(da, ta) & lineas(db + relleno, tb)
                if comun:
                    res.append((grupo, a, b, len(comun)))
    return res


def main():
    args = sys.argv[1:]
    buscar = "--relleno" in args
    args = [a for a in args if a != "--relleno"]
    if len(args) != 2:
        sys.exit(__doc__)
    tabla = simbolos(args[0], args[1])
    c = choques(tabla)
    for grupo, a, b, n in c:
        print(f"{grupo}: {a} y {b} comparten {n} líneas de la caché de instrucciones")
    if not c:
        print("sin choques en la caché de instrucciones")
    if buscar and c:
        for r in range(LINEA, CACHE, LINEA):
            if not choques(tabla, r):
                print(f"con {r} B más de relleno no habría choques")
                break
    return 1 if c else 0


if __name__ == "__main__":
    sys.exit(main())
