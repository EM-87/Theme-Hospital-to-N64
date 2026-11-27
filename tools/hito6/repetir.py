#!/usr/bin/env python3
"""Las órdenes de una partida de la ROM, del registro de debugf (isviewer.log)
a la lista de th64sim partida. El contrato de las líneas, en
tools/hito6/README.md («La partida grabada en la ROM») y en
tools/hito6/repetir.sh.

Uso: python3 -I tools/hito6/repetir.py <isviewer.log> <dir_salida>
  Escribe en dir_salida ordenes.tsv (la lista para th64sim partida),
  rom_ordenes_crudo.log (las líneas «TH64 orden» de la ROM, tal cual) y
  rom_ordenes.log (las mismas, como las escribe th64sim partida después de
  leer la lista: el nombre de la orden en vez del número TH_ORD_* y a 0, o «-»
  en def_id, los campos de th_order que esa orden no usa; es lo que se compara
  con su ordenes_rom.log), y saca por la salida
  una línea «<escenario> <semilla> <horas> <huella> <saldo>» (horas, huella y
  saldo «-» si la ROM no llegó a escribir la línea del final). Si el registro
  trae varias partidas, vale la última.
"""
import os
import re
import sys

# TH_ORD_*, en el orden de motor/core/include/th/build.h; los nombres, los de
# bench/lib/ordenes.lua (construir.c, th64sim_nombre_orden).
ORDENES = ["?", "comprar", "colocar", "cerrar", "recoger", "vender", "deshacer", "sala", "puerta", "ventana",
           "quitar_ventana", "fin_ventanas", "comprar_sala", "fin_sala", "atras", "contratar"]
DIRS = ["north", "east", "south", "west"]
# TH_HUM_* (motor/core/include/th/sim.h): la clase de «contratar» va en dir.
CLASES = {1: "doctor", 2: "nurse", 3: "handyman", 4: "receptionist"}

NUEVA = re.compile(r"TH64 partida nueva: escenario (\S+), semilla (\d+)")
FIN = re.compile(r"TH64 partida: (\d+) horas, huella ([0-9a-fA-F]+), saldo (-?\d+)")


def argumentos(orden, x, y, d, qty, w, h, def_id):
    """Los argumentos de la orden en la lista, como los lee th64sim_leer_orden."""
    if orden in ("comprar", "comprar_sala"):
        return [def_id, qty]
    if orden == "colocar":
        return [def_id, x, y, DIRS[int(d) & 3]]
    if orden == "recoger":
        return [x, y]
    if orden == "sala":
        return [def_id, x, y, w, h]
    if orden in ("puerta", "ventana", "quitar_ventana"):
        return [x, y, DIRS[int(d) & 3]]
    if orden == "contratar":
        return [CLASES.get(int(d), str(d)), qty, x, y]
    return []


def canonica(orden, x, y, d, qty, w, h, def_id):
    """Los campos de la línea «TH64 orden» tal como los escribe th64sim partida
    (hito4.c, al_aplicar) para la orden que sale de la lista: lo que
    th64sim_leer_orden (construir.c) no lee se queda a 0 (memset) y def_id a
    «-»; dir, x, y y qty pasan por uint8_t y dir por el nombre (& 3). Devuelve
    (orden, x, y, dir, qty, w, h, def_id), todo en texto."""
    x, y, d, qty, w, h = (int(v) & 255 for v in (x, y, d, qty, w, h))
    if orden in ("comprar", "comprar_sala"):
        c = (0, 0, 0, qty, 0, 0, def_id)
    elif orden == "colocar":
        c = (x, y, d & 3, 0, 0, 0, def_id)
    elif orden == "recoger":
        c = (x, y, 0, 0, 0, 0, "-")
    elif orden == "sala":
        c = (x, y, 0, 0, w, h, def_id)
    elif orden in ("puerta", "ventana", "quitar_ventana"):
        c = (x, y, d & 3, 0, 0, 0, "-")
    elif orden == "contratar" and d in CLASES:
        c = (x, y, d, qty, 0, 0, "-")
    elif orden == "contratar":
        # Una clase que th64sim_leer_orden no conoce: la orden queda en
        # TH_ORD_NONE («?»), con qty, x e y leídos.
        orden, c = "?", (x, y, 0, qty, 0, 0, "-")
    elif orden in ("cerrar", "vender", "deshacer", "fin_ventanas", "fin_sala", "atras"):
        c = (0, 0, 0, 0, 0, 0, "-")
    else:
        orden, c = "?", (0, 0, 0, 0, 0, 0, "-")
    return (orden, *(str(v) for v in c[:6]), c[6])


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    lineas = open(sys.argv[1], encoding="utf-8", errors="replace").read().splitlines()
    inicio = max((i for i, l in enumerate(lineas) if NUEVA.search(l)), default=None)
    if inicio is None:
        sys.exit(f"{sys.argv[1]}: no hay ninguna línea «TH64 partida nueva»")
    escenario, semilla = NUEVA.search(lineas[inicio]).groups()
    horas = huella = saldo = "-"
    tsv = ["tick\torden\targumentos"]
    rom = []
    crudo = []
    distintas = 0
    for linea in lineas[inicio + 1:]:
        fin = FIN.search(linea)
        if fin:
            horas, huella, saldo = fin.groups()
            break
        p = linea.find("TH64 orden\t")
        if p < 0:
            continue
        campos = linea[p:].rstrip("\r\n").split("\t")
        if len(campos) != 11:
            sys.exit(f"línea «TH64 orden» con {len(campos)} campos, en vez de 11: {linea!r}")
        _, tick, orden, x, y, d, qty, w, h, def_id, _motivo = campos
        if not all(v.isdigit() for v in (tick, x, y, d, qty, w, h)):
            sys.exit(f"línea «TH64 orden» con un campo numérico que no es un número: {linea!r}")
        if orden.isdigit():
            orden = ORDENES[int(orden)] if int(orden) < len(ORDENES) else "?"
        crudo.append(campos)
        orden, *resto = canonica(orden, x, y, d, qty, w, h, def_id)
        canon = [campos[0], tick, orden, *resto, _motivo]
        distintas += canon[3:10] != campos[3:10]
        rom.append(canon)
        tsv.append("\t".join([tick, orden, *argumentos(orden, x, y, d, qty, w, h, def_id)]))
    os.makedirs(sys.argv[2], exist_ok=True)
    with open(os.path.join(sys.argv[2], "ordenes.tsv"), "w") as f:
        f.write("\n".join(tsv) + "\n")
    with open(os.path.join(sys.argv[2], "rom_ordenes.log"), "w") as f:
        f.write("".join("\t".join(c) + "\n" for c in rom))
    with open(os.path.join(sys.argv[2], "rom_ordenes_crudo.log"), "w") as f:
        f.write("".join("\t".join(c) + "\n" for c in crudo))
    if distintas:
        print(f"repetir: {distintas} órdenes de la ROM con campos que la orden no usa distintos de 0 (o «-»), "
              "o fuera de rango: no cuentan en la comparación (rom_ordenes_crudo.log)", file=sys.stderr)
    print(escenario, semilla, horas, f"{int(huella, 16):08x}" if huella != "-" else "-", saldo)


if __name__ == "__main__":
    main()
