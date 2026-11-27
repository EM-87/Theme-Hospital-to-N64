#!/usr/bin/env python3
"""Resumen de una campaña del oráculo del hito 4: las cifras de cada semilla
(lo que escribe bench/scenarios/traza_hito4.lua o th64sim hito4) y su media y
desviación típica entre semillas.

Uso: python3 -I tools/oraculo_hito4/resumen.py <dir_semilla>... [--json salida.json] [--demo]

Por semilla:
  llegadas, recepcion, consulta, diagnosticados, a_casa: pacientes que
    llegaron a cada paso (a_casa: que se fueron por cualquier motivo);
  cobros_gp: cobros de la consulta (directos o por seguro), dinero_gp: su
    importe; sueldos: lo pagado en sueldos; saldo: saldo final menos inicial;
  t_*: horas medias de cada fase, de los pacientes que la completaron;
  h_vomit, h_pee: horas-paciente vomitando y orinando (acciones.tsv).
Con --demo (la campaña de la demo, tools/hito6/campana_demo.sh), además:
  kicked: los que se fueron a casa echados (motivo kicked de pacientes.tsv);
  primera_llegada: la hora de llegada del primer paciente;
  contratar: lo cobrado al contratar (Hire Staff en CorsixTH, hire_staff en
    el motor), la suma de los sueldos de los contratados;
  valor, reputacion, vomitos, orinadas, basura: de final.tsv (el valor del
    hospital y la reputación al final, los vómitos y las orinadas que
    empezaron y la basura que queda en el suelo).
"""
import csv
import json
import os
import statistics
import sys

FASES = [
    ("t_llegar", "llegada", "cola_recepcion"),
    ("t_recepcion", "cola_recepcion", "recepcion"),
    ("t_espera_gp", "recepcion", "entra_gp"),
    ("t_consulta", "entra_gp", "sale_gp"),
    ("t_tras_gp", "sale_gp", "a_casa"),
    ("t_salir", "a_casa", "desaparece"),
]


def extra_demo(d, pac, out):
    out["kicked"] = sum(1 for r in pac if r["motivo"] == "kicked")
    llegadas = [int(r["llegada"]) for r in pac if int(r["llegada"]) >= 0]
    out["primera_llegada"] = min(llegadas) if llegadas else float("nan")
    contratar = 0
    for r in csv.DictReader(open(os.path.join(d, "dinero.tsv")), delimiter="\t"):
        if "Hire Staff" in r["motivo"] or r["motivo"].startswith("hire_staff"):
            contratar -= int(r["importe"])
    out["contratar"] = contratar
    final = {r["clave"]: int(r["valor"]) for r in csv.DictReader(open(os.path.join(d, "final.tsv")), delimiter="\t")}
    for k in ("valor", "reputacion", "vomitos", "orinadas", "basura"):
        out[k] = final[k]


def semilla(d, demo=False):
    pac = list(csv.DictReader(open(os.path.join(d, "pacientes.tsv")), delimiter="\t"))
    i = lambda r, k: int(r[k])
    out = {
        "llegadas": sum(1 for r in pac if i(r, "llegada") >= 0),
        "recepcion": sum(1 for r in pac if i(r, "recepcion") >= 0),
        "consulta": sum(1 for r in pac if i(r, "visitas_gp") > 0),
        "diagnosticados": sum(1 for r in pac if i(r, "diagnostico") >= 0),
        "a_casa": sum(1 for r in pac if i(r, "a_casa") >= 0),
    }
    for nombre, a, b in FASES:
        v = [i(r, b) - i(r, a) for r in pac if i(r, a) >= 0 and i(r, b) >= 0]
        out[nombre] = statistics.mean(v) if v else float("nan")
    cobros = dinero = sueldos = 0
    for r in csv.DictReader(open(os.path.join(d, "dinero.tsv")), delimiter="\t"):
        imp, motivo = int(r["importe"]), r["motivo"]
        if "General Practice" in motivo or "(pendiente)" in motivo:
            cobros += 1
            dinero += imp
        elif "Wages" in motivo:
            sueldos -= imp
    out["cobros_gp"], out["dinero_gp"], out["sueldos"] = cobros, dinero, sueldos
    dias = list(csv.DictReader(open(os.path.join(d, "dias.tsv")), delimiter="\t"))
    out["saldo"] = int(dias[-1]["saldo"]) - int(dias[0]["saldo"])
    out["h_vomit"] = out["h_pee"] = 0
    ruta = os.path.join(d, "acciones.tsv")
    if os.path.exists(ruta):
        for r in csv.DictReader(open(ruta), delimiter="\t"):
            if r["accion"] in ("vomit", "pee"):
                out["h_" + r["accion"]] += int(r["horas_paciente"])
    if demo:
        extra_demo(d, pac, out)
    return out


def main():
    args = sys.argv[1:]
    salida = None
    if "--json" in args:
        k = args.index("--json")
        salida = args[k + 1]
        del args[k:k + 2]
    demo = "--demo" in args
    if demo:
        args.remove("--demo")
    if not args:
        sys.exit(__doc__)
    filas = [(os.path.basename(d.rstrip("/")), semilla(d, demo)) for d in sorted(args)]
    claves = list(filas[0][1])
    print("semilla      " + " ".join(f"{k[:11]:>11}" for k in claves))
    for nombre, f in filas:
        print(f"{nombre:<12} " + " ".join(f"{f[k]:>11.0f}" for k in claves))
    medias = {}
    for k in claves:
        v = [f[k] for _, f in filas if f[k] == f[k]]
        if not v:
            medias[k] = {"media": float("nan"), "desv": 0.0, "n": 0}
            continue
        m = statistics.mean(v)
        s = statistics.stdev(v) if len(v) > 1 else 0.0
        medias[k] = {"media": m, "desv": s, "n": len(v)}
    print("media        " + " ".join(f"{medias[k]['media']:>11.1f}" for k in claves))
    print("desv. típica " + " ".join(f"{medias[k]['desv']:>11.1f}" for k in claves))
    if salida:
        json.dump({"semillas": dict(filas), "resumen": medias}, open(salida, "w"), indent=1)


if __name__ == "__main__":
    main()
