#!/usr/bin/env python3
"""Clasifica las líneas de código Lua de CorsixTH en datos, andamiaje y funciones.

Criterio (por línea física con al menos un token que no sea comentario):
  * "funcion": la línea cae dentro del rango [linedefined, lastlinedefined] de
    algún prototipo de función que no sea el chunk principal, según el
    compilador de Lua (luac5.4 -p -l). Es código con comportamiento.
  * "andamiaje": línea del chunk principal que solo declara la clase o el
    módulo: `class "X" (Y)`, `local X = _G["X"]`, `local object = {}`,
    `return object`, `corsixth.require(...)`, `local anim_mgr = TheApp...`.
  * "datos": el resto del chunk principal: asignaciones de campos, tablas
    constructoras, constantes y llamadas que registran datos (marcadores de
    animación). Son declaraciones que se ejecutan una vez al cargar.
Además, dentro de "funcion" se separan las funciones auxiliares que solo
generan datos al cargar (`copy_north_to_south`, `initPatient` de las
enfermedades), para no contarlas como lógica de simulación.

Uso: python3 -I datos_codigo.py <dir Lua de CorsixTH> <salida.json>
"""
import json
import os
import re
import subprocess
import sys

DIRS = ["objects", "rooms", "diseases", "diagnosis", "humanoid_actions",
        "entities/humanoids"]


def code_lines(src):
    """Devuelve el conjunto de líneas (1-based) con algún token que no es comentario."""
    lines = set()
    i, n, line = 0, len(src), 1

    def long_bracket(pos):
        m = re.match(r"\[(=*)\[", src[pos:])
        return m.group(1) if m else None

    while i < n:
        c = src[i]
        if c == "\n":
            line += 1
            i += 1
        elif c in " \t\r":
            i += 1
        elif src.startswith("--", i):
            lvl = long_bracket(i + 2)
            if lvl is not None:
                end = src.find("]" + lvl + "]", i)
                end = n if end < 0 else end + len(lvl) + 2
                line += src.count("\n", i, end)
                i = end
            else:
                end = src.find("\n", i)
                i = n if end < 0 else end
        elif c == "[" and long_bracket(i) is not None:
            lvl = long_bracket(i)
            end = src.find("]" + lvl + "]", i)
            end = n if end < 0 else end + len(lvl) + 2
            for ln in range(line, line + src.count("\n", i, end) + 1):
                lines.add(ln)
            line += src.count("\n", i, end)
            i = end
        elif c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            lines.add(line)
            line += src.count("\n", i, j)
            i = j + 1
        else:
            lines.add(line)
            i += 1
    return lines


FUNC_RE = re.compile(r"^function <[^:]+:(\d+),(\d+)>")


SCAFFOLD_RE = re.compile(
    r"^\s*(class\s*[\"'(]|local\s+\w+\s*=\s*_G\[|---@|local\s+(object|room|disease|action)\s*=\s*\{\s*\}\s*$|"
    r"return\s+\w+\s*$|corsixth\.require|local\s+anim_mgr\s*=|local\s+\w+\s*=\s*TheApp)")
DATAGEN_FUNCS = re.compile(r"^\s*(local\s+function\s+copy_north_to_south|disease\.initPatient\s*=\s*function)")


def classify(path):
    src = open(path, encoding="utf-8", errors="replace").read()
    text = src.splitlines()
    code = code_lines(src)
    out = subprocess.run(["luac5.4", "-p", "-l", path], capture_output=True,
                         text=True, check=True).stdout
    ranges = []
    for l in out.splitlines():
        m = FUNC_RE.match(l)
        if m:
            ranges.append((int(m.group(1)), int(m.group(2))))
    in_func = set()
    datagen = set()
    for a, b in ranges:
        rng = set(range(a, b + 1))
        in_func |= rng
        if a - 1 < len(text) and DATAGEN_FUNCS.match(text[a - 1]):
            datagen |= rng
    res = {"datos": 0, "andamiaje": 0, "funcion": 0, "funcion_generadora_datos": 0}
    for ln in code:
        if ln in in_func:
            res["funcion"] += 1
            if ln in datagen:
                res["funcion_generadora_datos"] += 1
        elif SCAFFOLD_RE.match(text[ln - 1]):
            res["andamiaje"] += 1
        else:
            res["datos"] += 1
    res["total"] = len(code)
    res["funciones"] = len([r for r in ranges])
    res["lineas_fisicas"] = len(text)
    return res


def main():
    base, outp = sys.argv[1], sys.argv[2]
    report = {"criterio": __doc__.split("Uso:")[0].strip(), "grupos": {}, "ficheros": {}}
    tot = {}
    for d in DIRS:
        g = {}
        for root, _, files in os.walk(os.path.join(base, d)):
            for f in sorted(files):
                if not f.endswith(".lua"):
                    continue
                p = os.path.join(root, f)
                r = classify(p)
                report["ficheros"][os.path.relpath(p, base)] = r
                for k, v in r.items():
                    g[k] = g.get(k, 0) + v
                g["ficheros"] = g.get("ficheros", 0) + 1
        report["grupos"][d] = g
        for k, v in g.items():
            tot[k] = tot.get(k, 0) + v
    report["total"] = tot
    json.dump(report, open(outp, "w"), indent=1, ensure_ascii=False)
    hdr = f"{'grupo':22} {'fich':>4} {'fisicas':>7} {'codigo':>6} {'datos':>6} {'andam':>6} {'func':>6} {'f.gen':>6} {'%datos':>6} {'%func':>6}"
    print(hdr)
    for d, g in list(report["grupos"].items()) + [("TOTAL", tot)]:
        print(f"{d:22} {g['ficheros']:4} {g['lineas_fisicas']:7} {g['total']:6} {g['datos']:6} {g['andamiaje']:6} "
              f"{g['funcion']:6} {g['funcion_generadora_datos']:6} {100*g['datos']/g['total']:6.1f} {100*g['funcion']/g['total']:6.1f}")


if __name__ == "__main__":
    main()
