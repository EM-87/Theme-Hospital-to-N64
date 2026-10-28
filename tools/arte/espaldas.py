#!/usr/bin/env python3
"""El arte que falta para ver de espaldas los objetos que TH solo dibujó de
cara a la cámara (orientaciones norte y este): por objeto, cuántas animaciones,
fotogramas y sprites distintos usan en norte y este, contando las de uso (la
gente que los usa) y las de reposo. Es lo que habría que dibujar para sur y
oeste. Lee las definiciones de objetos de CorsixTH (Lua/objects).

Uso: python3 -I tools/arte/espaldas.py [salida.tsv]
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from comun import LUA, Animaciones  # noqa: E402


def bloque(texto, nombre):
    """El cuerpo de la tabla object.<nombre> = { ... }."""
    m = re.search(r"^object\." + nombre + r"\s*=\s*[^{\n]*\{", texto, re.M)
    if not m:
        return ""
    i, nivel = m.end(), 1
    while nivel and i < len(texto):
        nivel += {"{": 1, "}": -1}.get(texto[i], 0)
        i += 1
    return texto[m.end():i]


def numeros_de(cuerpo, clave):
    """Los números bajo «clave = { ... }» o «clave = n» (cualquier profundidad)."""
    out = set()
    for m in re.finditer(r"\b" + clave + r"\s*=\s*", cuerpo):
        j = m.end()
        if j < len(cuerpo) and cuerpo[j] == "{":
            i, nivel = j + 1, 1
            while nivel:
                nivel += {"{": 1, "}": -1}.get(cuerpo[i], 0)
                i += 1
            out |= {int(x) for x in re.findall(r"(?<![\w.])(\d+)(?![\w.])", cuerpo[j:i])}
        else:
            n = re.match(r"(\d+)", cuerpo[j:])
            if n:
                out.add(int(n.group(1)))
    return out


def main():
    an = Animaciones()
    carpeta = os.path.join(LUA, "objects")
    nombres = sorted(f for f in os.listdir(carpeta) if f.endswith(".lua"))
    nombres += ["machines/" + f for f in sorted(os.listdir(os.path.join(carpeta, "machines"))) if f.endswith(".lua")]
    filas, todos = [], set()
    for f in nombres:
        texto = open(os.path.join(carpeta, f)).read()
        orient = set(re.findall(r"^  (north|east|south|west)\s*=", bloque(texto, "orientations"), re.M))
        if orient != {"north", "east"}:
            continue
        anims = set()
        for tabla in ("idle_animations", "usage_animations", "multi_usage_animations", "show_animation"):
            cuerpo = bloque(texto, tabla)
            anims |= numeros_de(cuerpo, "north") | numeros_de(cuerpo, "east")
        anims = {a for a in anims if 0 < a < len(an) and not an.vacia(a)}
        fot = {f for a in anims for f in an.cadena(a)}
        spr = an.sprites(fot)
        todos |= spr
        filas.append((os.path.basename(f)[:-4], len(anims), len(fot), len(spr)))
    filas.sort(key=lambda r: -r[3])
    salida = open(sys.argv[1], "w") if len(sys.argv) > 1 else None
    for nombre, a, f, s in filas:
        print(f"{nombre:20s} animaciones {a:4d}  fotogramas {f:5d}  sprites {s:5d}")
        if salida:
            salida.write(f"{nombre}\t{a}\t{f}\t{s}\n")
    print(f"total: {len(filas)} objetos, {sum(r[1] for r in filas)} animaciones, "
          f"{sum(r[2] for r in filas)} fotogramas, {len(todos)} sprites distintos")


if __name__ == "__main__":
    main()
