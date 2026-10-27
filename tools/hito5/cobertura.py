#!/usr/bin/env python3
"""Cuánta lógica de CorsixTH hay ya en el núcleo del motor, para estimar el
tamaño del código del juego completo. Es una cota gruesa:
  * las funciones Lua que el núcleo nombra en sus comentarios («Clase:método»)
    cuentan enteras, aunque el motor solo haga una parte;
  * las acciones de humanoide que nombra («walk.lua») cuentan con su fichero.
Cuenta líneas de código (sin vacías ni comentarios), como
tools/arquitectura/lua_modulos.py.

Uso: python3 -I tools/hito5/cobertura.py <motor/core> <CorsixTH/Lua>
"""
import os
import re
import sys


def lineas_codigo(lineas):
    n, en_bloque = 0, False
    for l in lineas:
        s = l.strip()
        if en_bloque:
            if "]]" in s:
                en_bloque = False
            continue
        if s.startswith("--[["):
            en_bloque = "]]" not in s
            continue
        if s and not s.startswith("--"):
            n += 1
    return n


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    core, lua = sys.argv[1], sys.argv[2]
    texto = ""
    for raiz, _, ficheros in os.walk(core):
        for f in ficheros:
            if f.endswith((".c", ".h")):
                texto += open(os.path.join(raiz, f), encoding="utf-8").read()
    metodos = set(re.findall(r"\b([A-Z][A-Za-z]+):([a-z_][A-Za-z_]+)\b", texto))
    acciones = set(re.findall(r"\b([a-z_]+)\.lua\b", texto))
    encontrados, lineas_metodos, lineas_acciones, ficheros_accion = set(), 0, 0, []
    for raiz, _, ficheros in os.walk(lua):
        for f in ficheros:
            if not f.endswith(".lua"):
                continue
            ruta = os.path.join(raiz, f)
            l = open(ruta, encoding="utf-8", errors="replace").read().splitlines()
            if "humanoid_actions" in ruta and f[:-4] in acciones:
                lineas_acciones += lineas_codigo(l)
                ficheros_accion.append(f)
                continue
            i = 0
            while i < len(l):
                m = re.match(r"^function ([A-Z][A-Za-z]+):([a-z_][A-Za-z_]+)\s*\(", l[i])
                if m and (m[1], m[2]) in metodos:
                    j = i + 1
                    while j < len(l) and not re.match(r"^end\b", l[j]):
                        j += 1
                    lineas_metodos += lineas_codigo(l[i:j + 1])
                    encontrados.add((m[1], m[2]))
                    i = j
                i += 1
    print(f"métodos nombrados en el núcleo: {len(metodos)}; encontrados en CorsixTH: {len(encontrados)}; "
          f"{lineas_metodos} líneas")
    print(f"acciones de humanoide ({len(ficheros_accion)}: {', '.join(sorted(ficheros_accion))}): {lineas_acciones} líneas")
    print(f"total: {lineas_metodos + lineas_acciones} líneas de Lua")
    faltan = sorted(f"{c}:{m}" for c, m in metodos - encontrados)
    if faltan:
        print(f"sin encontrar ({len(faltan)}): {' '.join(faltan)}")


if __name__ == "__main__":
    main()
