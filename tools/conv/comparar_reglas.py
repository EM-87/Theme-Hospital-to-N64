#!/usr/bin/env python3
"""Compara las reglas que saca el extractor del conversor (tools/conv/reglas.lua,
fuera del juego y con stubs) con las que tiene CorsixTH en memoria
(bench/scenarios/volcar_reglas.lua).

Uso: python3 -I tools/conv/comparar_reglas.py <reglas.json> <dir del oráculo>
  reglas.json: el que deja `conv.py --vistas DIR reglas` en DIR

Reglas de la comparación:
  * una cadena de idioma del extractor ({"$ref": "_S..."}) vale lo que haya en
    CorsixTH, que allí ya es el texto traducido;
  * el aspecto de los pacientes («aspecto») solo lo tiene el extractor: es su
    traducción de initPatient, que se compara aparte (está la función);
  * todo lo demás tiene que ser idéntico, y un entero no es igual a un real.
"""
import json
import os
import sys


def diferencias(a, b, ruta, out, limite=20):
    """Añade a out las rutas donde a (extractor) y b (CorsixTH) difieren."""
    if len(out) >= limite:
        return
    if isinstance(a, dict) and set(a) == {"$ref"}:
        return
    if type(a) is not type(b):
        out.append(f"{ruta}: {json.dumps(a)[:60]} frente a {json.dumps(b)[:60]}")
        return
    if isinstance(a, dict):
        for k in sorted(set(a) | set(b)):
            if k == "aspecto" and ruta.startswith("enfermedades."):
                continue
            if k not in b:
                out.append(f"{ruta}.{k}: solo en el extractor")
            elif k not in a:
                out.append(f"{ruta}.{k}: solo en CorsixTH")
            else:
                diferencias(a[k], b[k], f"{ruta}.{k}", out, limite)
    elif isinstance(a, list):
        if len(a) != len(b):
            out.append(f"{ruta}: {len(a)} elementos frente a {len(b)}")
            return
        for i, (x, y) in enumerate(zip(a, b)):
            diferencias(x, y, f"{ruta}[{i}]", out, limite)
    elif a != b:
        out.append(f"{ruta}: {json.dumps(a)[:60]} frente a {json.dumps(b)[:60]}")


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    nuestro = json.load(open(sys.argv[1]))
    oraculo = sys.argv[2]
    fallos = 0
    defs = json.load(open(os.path.join(oraculo, "definiciones.json")))
    for parte in ("objetos", "salas", "enfermedades", "diagnosticos"):
        iguales, malas = 0, []
        for k in sorted(set(nuestro[parte]) | set(defs[parte])):
            d = []
            if k not in defs[parte] or k not in nuestro[parte]:
                d.append("falta en uno de los dos")
            else:
                diferencias(nuestro[parte][k], defs[parte][k], f"{parte}.{k}", d)
            if d:
                malas.append((k, d))
            else:
                iguales += 1
        print(f"{parte:<13} {iguales} de {iguales + len(malas)} iguales")
        for k, d in malas:
            fallos += 1
            print(f"   {k}: " + "; ".join(d[:5]))
    for dif, lista in sorted(nuestro["niveles"].items()):
        iguales, malas = 0, []
        for n, cfg in enumerate(lista, 1):
            ruta = os.path.join(oraculo, f"nivel_{dif}{n:02d}.json")
            if not os.path.exists(ruta):
                continue
            d = []
            diferencias(cfg, json.load(open(ruta)), f"{dif}{n:02d}", d)
            if d:
                malas.append((n, d))
            else:
                iguales += 1
        print(f"niveles {dif:<5} {iguales} de {iguales + len(malas)} iguales")
        for n, d in malas:
            fallos += 1
            print(f"   nivel {n}: " + "; ".join(d[:5]))
    sys.exit(1 if fallos else 0)


if __name__ == "__main__":
    main()
