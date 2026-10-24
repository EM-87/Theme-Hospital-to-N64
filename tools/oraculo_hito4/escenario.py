#!/usr/bin/env python3
"""Escenario del hito 4: lo que bench/scenarios/exportar_escenario.lua saca
de una partida de CorsixTH, pasado a TRG1 (tools/conv/reglas.py) para que el
motor lo cargue (motor/core/include/th/scenario.h).

De las casillas solo van las que difieren del mapa que deja el motor al
cargar el nivel (th64sim mapa --volcar), con las banderas como bits
TH_TILE_* (motor/core/include/th/map.h). El resto del documento va tal cual.

Uso: python3 -I tools/oraculo_hito4/escenario.py <dir_exportado> <motor.tsv> <salida.trg>
  dir_exportado: el celdas.tsv y el escenario.json de exportar_escenario.lua
"""
import csv
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "conv"))
import reglas  # noqa: E402

# Columna de celdas.tsv → bit TH_TILE_*. Las de sombra no van: el motor las
# recalcula.
BITS = {
    "passable": 0, "hospital": 1, "buildable": 2, "buildableNorth": 3, "buildableEast": 4,
    "buildableSouth": 5, "buildableWest": 6, "travelNorth": 7, "travelEast": 8, "travelSouth": 9,
    "travelWest": 10, "tallNorth": 14, "tallWest": 15, "room": 16, "doorNorth": 17, "doorWest": 18,
    "doNotIdle": 19,
}


def leer_celdas(ruta):
    return {(int(r["x"]), int(r["y"])): r for r in csv.DictReader(open(ruta), delimiter="\t")}


def casilla(r):
    capas = [int(r["suelo"]), int(r["norte"]), int(r["oeste"])]
    if any(not 0 <= c < 256 for c in capas):
        raise ValueError(f"({r['x']},{r['y']}): capa con banderas de dibujo {capas}, el motor aún no las lleva")
    banderas = sum(1 << b for col, b in BITS.items() if r[col] == "1")
    return [int(r["x"]), int(r["y"]), *capas, banderas, int(r["sala"]), int(r["thob"])]


def convertir(dir_exportado, motor_tsv):
    corsix = leer_celdas(os.path.join(dir_exportado, "celdas.tsv"))
    motor = leer_celdas(motor_tsv)
    if corsix.keys() != motor.keys():
        raise ValueError("los dos mapas no tienen las mismas casillas")
    if "parcela" in corsix[(0, 0)]:
        distintas = [p for p in corsix if corsix[p]["parcela"] != motor[p]["parcela"]]
        if distintas:
            raise ValueError(f"{len(distintas)} casillas con otra parcela; el escenario no cambia parcelas")
    doc = json.load(open(os.path.join(dir_exportado, "escenario.json")))
    doc["celdas"] = [casilla(corsix[p]) for p in sorted(corsix, key=lambda p: (p[1], p[0]))
                     if casilla(corsix[p]) != casilla(motor[p])]
    return doc


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    doc = convertir(sys.argv[1], sys.argv[2])
    datos = reglas.escribir(doc)
    if not reglas.igual(reglas.leer(datos), doc):
        sys.exit("el TRG1 no se relee igual")
    open(sys.argv[3], "wb").write(datos)
    print(f"escenario: {len(doc['celdas'])} casillas, {len(doc['objetos'])} objetos, {len(doc['salas'])} salas, "
          f"{len(doc['personal'])} en plantilla, {len(datos)} B")


if __name__ == "__main__":
    main()
