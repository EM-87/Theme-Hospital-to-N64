#!/usr/bin/env python3
"""Compara dos resúmenes de resumen.py --json (CorsixTH y el motor): media y
desviación típica de cada cifra, la diferencia y su error típico
(sqrt(s1²/n1 + s2²/n2)), y cuántos errores típicos son (t de Welch).

Uso: python3 -I tools/oraculo_hito4/comparar.py <corsixth.json> <motor.json> [--json salida.json]
"""
import json
import math
import sys


def main():
    args = sys.argv[1:]
    salida = None
    if "--json" in args:
        k = args.index("--json")
        salida = args[k + 1]
        del args[k:k + 2]
    if len(args) != 2:
        sys.exit(__doc__)
    a = json.load(open(args[0]))["resumen"]
    b = json.load(open(args[1]))["resumen"]
    filas = []
    print(f"{'cifra':<14} {'CorsixTH':>18} {'motor':>18} {'diferencia':>11} {'%':>7} {'t':>6}")
    for k in a:
        if k not in b:
            continue
        x, y = a[k], b[k]
        dif = y["media"] - x["media"]
        et = math.sqrt(x["desv"] ** 2 / x["n"] + y["desv"] ** 2 / y["n"])
        t = dif / et if et > 0 else (0.0 if dif == 0 else float("inf"))
        pct = 100 * dif / x["media"] if x["media"] else float("nan")
        filas.append({"cifra": k, "corsixth": x, "motor": y, "diferencia": dif, "porcentaje": pct, "t": t})
        print(f"{k:<14} {x['media']:>9.1f} ± {x['desv']:<6.1f} {y['media']:>9.1f} ± {y['desv']:<6.1f} "
              f"{dif:>+11.1f} {pct:>+6.1f}% {t:>+6.2f}")
    fuera = [f["cifra"] for f in filas if abs(f["t"]) >= 2]
    print(f"\n{len(filas)} cifras; con |t| >= 2: {len(fuera)} {fuera}")
    if salida:
        json.dump(filas, open(salida, "w"), indent=1)


if __name__ == "__main__":
    main()
