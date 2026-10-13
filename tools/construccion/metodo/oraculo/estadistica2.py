#!/usr/bin/env python3
"""Dispersión del oráculo entre semillas, a horas fijas.

Para contadores acumulados (curados, muertes, visitas, no curados, saldo) usa
el incremento desde la hora 0 de la partida; para el resto, el valor. Da media,
desviación típica, mínimo, máximo y coeficiente de variación entre N
ejecuciones, y la diferencia mínima detectable entre dos grupos de N
(aprox. 2,8 * sd * sqrt(2/N): alfa 0,05 bilateral, potencia 0,8).

Uso: python3 -I estadistica2.py <salida.json> <horas,separadas> <volcado.jsonl>...
"""
import json
import math
import statistics
import sys

DELTA = ["num_cured", "num_deaths", "num_visitors", "not_cured", "balance", "sodas_sold"]
LEVEL = ["reputation", "patients", "queue_total", "ent_litter", "happiness_mean", "thirst_mean",
         "warmth_mean", "value", "staff"]


def main():
    outp, hours = sys.argv[1], [int(h) for h in sys.argv[2].split(",")]
    runs = []
    for p in sys.argv[3:]:
        rows = {r["hora"]: r for r in (json.loads(l) for l in open(p))}
        runs.append(rows)
    n = len(runs)
    res = {"n": n, "horas": {}}
    for h in hours:
        res["horas"][h] = {}
        for f in DELTA + LEVEL:
            vals = []
            for r in runs:
                if h not in r or r[h].get(f) is None:
                    continue
                v = r[h][f] - r[0][f] if f in DELTA else r[h][f]
                vals.append(v)
            if len(vals) < 2:
                continue
            mu, sd = statistics.mean(vals), statistics.stdev(vals)
            res["horas"][h][("Δ" if f in DELTA else "") + f] = {
                "media": round(mu, 4), "sd": round(sd, 4), "min": min(vals), "max": max(vals),
                "cv": round(sd / abs(mu), 4) if mu else None,
                "dif_detectable_N": round(2.8 * sd * math.sqrt(2 / len(vals)), 4)}
    json.dump(res, open(outp, "w"), indent=1, ensure_ascii=False)
    print(f"N = {n}")
    for h in hours:
        print(f"-- hora {h}")
        for k, s in res["horas"][h].items():
            print(f"   {k:18} media {s['media']:>12} sd {s['sd']:>10} [{s['min']} .. {s['max']}] "
                  f"cv {s['cv']} dif.det {s['dif_detectable_N']}")


if __name__ == "__main__":
    main()
