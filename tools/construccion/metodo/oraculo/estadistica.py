#!/usr/bin/env python3
"""Dispersión del oráculo entre semillas: para cada campo, media y desviación
típica entre N ejecuciones al final de cada mes (cada 1.500 horas aprox., se
toma la última hora de cada mes de juego), y la banda media ± 3·sd que tendría
que respetar el motor nuevo con la misma partida inicial.

Uso: python3 -I estadistica.py <salida.json> <volcado1.jsonl> <volcado2.jsonl> ...
"""
import json
import statistics
import sys

FIELDS = ["num_visitors", "num_cured", "not_cured", "num_deaths", "balance", "reputation",
          "patients", "queue_total", "ent_litter", "happiness_mean", "thirst_mean", "warmth_mean"]


def month_ends(rows):
    out = {}
    for r in rows:
        out[r["fecha"][:4]] = r  # año-mes: queda la última hora del mes
    return out


def main():
    outp, files = sys.argv[1], sys.argv[2:]
    runs = [month_ends([json.loads(l) for l in open(p)]) for p in files]
    months = sorted(set.intersection(*[set(r) for r in runs]))
    res = {"n": len(runs), "meses": {}}
    for m in months:
        res["meses"][m] = {}
        for f in FIELDS:
            vals = [r[m][f] for r in runs if r[m].get(f) is not None]
            if len(vals) < 2:
                continue
            mu, sd = statistics.mean(vals), statistics.stdev(vals)
            res["meses"][m][f] = {"media": mu, "sd": sd, "min": min(vals), "max": max(vals),
                                  "cv": sd / mu if mu else None}
    json.dump(res, open(outp, "w"), indent=1)
    print(f"N = {len(runs)} semillas")
    for m in months:
        row = res["meses"][m]
        print(m, " ".join(f"{f}={row[f]['media']:.1f}±{row[f]['sd']:.1f}[{row[f]['min']}-{row[f]['max']}]"
                          for f in ["num_visitors", "num_cured", "not_cured", "num_deaths", "balance", "reputation", "patients"] if f in row))


if __name__ == "__main__":
    main()
