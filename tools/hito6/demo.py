#!/usr/bin/env python3
"""El escenario de la demo jugable (docs/06_plan.md, «La demo jugable»): una
partida nueva del nivel, como la deja CorsixTH v0.70.1 al crear el mundo
(World:new, Hospital:new), sin CorsixTH en marcha: todo sale de las reglas
convertidas del nivel y de las definiciones (th64sim reglas), del mapa que
deja el motor al cargar el nivel (th64sim mapa --volcar) y de los puntos de
aparición que calcula el motor (th64sim aparicion). Escribe escenario.json en
el formato de bench/lib/escenario.lua, que tools/oraculo_hito4/escenario.py
pasa a TRG1.

Lo que sale de cada sitio ($CORSIXTH_DIR/CorsixTH/Lua):
- fecha: Date() (world.lua:73; date.lua): año 1, mes 1, día 1, hora 0;
- mundo: hours_per_tick 1 (la velocidad Normal; el escenario no lleva la
  pausa), spawn_rate = monthly_spawn_increase = popn[0].Change
  (world.lua:151-152), popn del nivel, sin spawn_dates ni spawn_hours (en CorsixTH
  las calcula World:new con updateSpawnDates, world.lua:154-156; el núcleo de
  hoy no: load_scenario, sim.c, solo las lee y, si faltan, quedan a 0, así que
  el primer mes no trae ninguna llegada programada hasta que entre la pieza 1
  del plan, «las llegadas del primer mes cuando el escenario no las trae»; la
  clave se deja ausente, no {}, para que entonces signifique «calcúlalas»),
  los puntos de aparición (World:calculateSpawnTiles, world.lua:326-357) y las
  enfermedades del nivel (World:initLevel, world.lua:216-247: las que no son
  pseudo, con visuals/non_visuals distinto de 0 y todas sus salas de
  tratamiento disponibles según World:getAvailableRooms, world.lua:282-313,
  en el orden de TheApp.diseases, que es el de lfs.dir sobre Lua/diseases,
  app.lua:352-353 y 1646-1675);
- hospital: saldo = towns[nivel].StartCash, reputación = StartRep acotada a
  [0, 1000] (hospital.lua:33-48, 86-89); valor = LandCostPerTile × casillas de
  la parcela 1 + 20.000 (hospital.lua:50, map.lua:782-794); población 0,25
  (hospital.lua:135); abierto: la decisión 1 del plan (en CorsixTH empieza
  cerrado, hospital.lua:171, y lo abre el reloj); políticas de una partida
  nueva (hospital.lua:193-200, goto_staffroom = gbv.Tired / 1000) y el saldo de
  los seguros a 0 (hospital.lua:222); el casebook de Hospital:initDiseases
  (hospital.lua:225-262): las enfermedades del nivel y los diagnósticos
  (pseudo), descubierta si expertise[expertise_id].Known == 1, eficacia
  gbv.StartRating si la primera sala de tratamiento es la farmacia (si no,
  100), precio 1 y reputación 500 las que no son pseudo;
- parcelas: la 1, del jugador; objetos: solo los que crea el mapa
  (supports_creation_for_map: las puertas de la entrada), con su sitio del
  volcado (thob) y orientación norte; personal, listas, basura y salas: nada.

Uso: python3 -I tools/hito6/demo.py <nivel.json> <definiciones.json> <motor.tsv>
       <aparicion.json> <dir_enfermedades_corsixth> <número de nivel> <salida.json>
  nivel.json y definiciones.json: th64sim reglas del nivel y de definiciones.trg
"""
import csv
import json
import os
import sys


def clave(n):
    return f"#{n}"


def lua(tabla, k):
    """tabla[k] en Lua: el JSON canónico escribe como lista la tabla con
    claves 1..n (la posición k − 1) y como mapa «#k» la que empieza en 0."""
    if isinstance(tabla, list):
        return tabla[k - 1] if 1 <= k <= len(tabla) else {}
    return tabla.get(clave(k), {})


def disponibles(nivel, defs):
    """Las salas que World:getAvailableRooms deja construir en el nivel."""
    objs_nivel = nivel["objects"]
    salas = set()
    for sid, sala in defs["salas"].items():
        ok = True
        for obj in sala.get("objects_needed", {}):
            spec = lua(objs_nivel, defs["objetos"][obj]["thob"])
            if spec.get("AvailableForLevel", 1) == 0:
                ok = False
        if ok:
            salas.add(sid)
    return salas


def enfermedades(nivel, defs, dir_enf):
    """World:initLevel: las enfermedades del nivel, en el orden de TheApp.diseases."""
    orden = [f[:-4] for f in os.listdir(dir_enf) if f.endswith(".lua")]
    salas = disponibles(nivel, defs)
    vis, novis = nivel.get("visuals", {}), nivel.get("non_visuals", {})
    lista = []
    for eid in orden:
        e = defs["enfermedades"].get(eid)
        if e is None or e.get("pseudo"):
            continue
        vid, nid = e.get("visuals_id"), e.get("non_visuals_id")
        v = 1
        if vis and ((vid is not None and clave(vid) in vis) or (nid is not None and clave(nid) in novis)):
            v = vis[clave(vid)]["Value"] if vid is not None else novis[clave(nid)]["Value"]
        if v != 0 and any(t not in salas for t in e.get("treatment_rooms", [])):
            v = 0
        if v != 0:
            lista.append(eid)
    return lista


def casebook(nivel, defs, lista):
    """Hospital:initDiseases (hospital.lua:225-262)."""
    expertise, gbv = nivel["expertise"], nivel["gbv"]
    cb = {}
    todas = {**defs["enfermedades"], **defs["diagnosticos"]}
    for eid, e in todas.items():
        pseudo = bool(e.get("pseudo"))
        if eid not in lista and not pseudo:
            continue
        conocida = lua(expertise, e["expertise_id"]).get("Known") == 1
        farmacia = (e.get("treatment_rooms") or [None])[0] == "pharmacy"
        c = {"precio": 1.0, "descubierta": conocida, "eficacia": gbv["StartRating"] if farmacia else 100,
             "pseudo": pseudo}
        if not pseudo:
            c["reputacion"] = 500
        cb[eid] = c
    return cb


def diferencias(a, b, ruta=""):
    """Las hojas distintas de dos documentos JSON (los números, como números)."""
    if isinstance(a, dict) and isinstance(b, dict):
        for k in sorted(set(a) | set(b)):
            if k not in a or k not in b:
                yield f"{ruta}{k}", a.get(k, "(falta)"), b.get(k, "(falta)")
            else:
                yield from diferencias(a[k], b[k], f"{ruta}{k}.")
    elif isinstance(a, list) and isinstance(b, list) and len(a) == len(b):
        for i, (x, y) in enumerate(zip(a, b)):
            yield from diferencias(x, y, f"{ruta}{i}.")
    elif not (a == b and isinstance(a, bool) == isinstance(b, bool)):
        yield ruta.rstrip("."), a, b


def comparar(nuestro, oraculo):
    """demo.py --comparar <escenario.json> <oráculo.json>: campo a campo contra
    un escenario.json que exportó CorsixTH (el «inicio» de la batería de
    construir, $TH64_WORK/resultados/hito6/construir/pasillo/corsixth/inicio)."""
    difs = list(diferencias(json.load(open(nuestro)), json.load(open(oraculo))))
    for ruta, x, y in difs:
        print(f"   {ruta}: demo {json.dumps(x, ensure_ascii=False)}; CorsixTH {json.dumps(y, ensure_ascii=False)}")
    print(f"   {len(difs)} campos distintos")


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "--comparar":
        return comparar(sys.argv[2], sys.argv[3])
    if len(sys.argv) != 8:
        sys.exit(__doc__)
    nivel = json.load(open(sys.argv[1]))
    defs = json.load(open(sys.argv[2]))
    celdas = list(csv.DictReader(open(sys.argv[3]), delimiter="\t"))
    puntos = json.load(open(sys.argv[4]))
    dir_enf, n = sys.argv[5], int(sys.argv[6])
    town = nivel["towns"][clave(n)] if clave(n) in nivel.get("towns", {}) else nivel["town"]
    gbv = nivel["gbv"]
    casillas_parcela = sum(1 for c in celdas if c["parcela"] == "1")
    valor = casillas_parcela * gbv.get("LandCostPerTile", 25) + 20000
    lista = enfermedades(nivel, defs, dir_enf)
    del_mapa = {o["thob"]: oid for oid, o in defs["objetos"].items() if o.get("supports_creation_for_map")}
    objetos = []
    for c in sorted(celdas, key=lambda c: (int(c["y"]), int(c["x"]))):
        thob = int(c["thob"])
        if thob in del_mapa:
            objetos.append({"id": del_mapa[thob], "x": int(c["x"]), "y": int(c["y"]), "direccion": "north",
                            "orden": 0, "sala": 0})
    popn = nivel["popn"]
    ritmo = popn["#0"]["Change"]
    doc = {
        "version": 1,
        "nivel": n,
        "dificultad": "full",
        "fecha": {"anio": 1, "mes": 1, "dia": 1, "hora": 0},
        "mundo": {"hours_per_tick": 1, "tick_rate": 1, "spawn_rate": ritmo, "monthly_spawn_increase": ritmo,
                  "popn": popn, "puntos_aparicion": puntos, "enfermedades": lista},
        "hospital": {"saldo": town["StartCash"], "valor": valor,
                     "reputacion": min(max(town["StartRep"], 0), 1000), "poblacion": 0.25, "abierto": True,
                     "politicas": {"staff_allowed_to_move": True, "send_home": 0.1, "guess_cure": 0.9,
                                   "stop_procedure": 1, "goto_staffroom": gbv["Tired"] / 1000,
                                   "grant_wage_increase": False},
                     "saldo_seguros": [[0, 0, 0], [0, 0, 0], [0, 0, 0]],
                     "casebook": casebook(nivel, defs, lista)},
        "parcelas": [1],
        "objetos": objetos,
        "personal": [],
        "salas": [],
        "basura": [],
    }
    with open(sys.argv[7], "w") as f:
        json.dump(doc, f, ensure_ascii=False, sort_keys=True, indent=1)
    print(f"demo: nivel {n}, saldo {doc['hospital']['saldo']}, valor {valor} ({casillas_parcela} casillas × "
          f"{gbv.get('LandCostPerTile', 25)} + 20000), ritmo {ritmo}, {len(puntos)} puntos de aparición, "
          f"enfermedades {', '.join(lista)}; descubiertas: "
          f"{', '.join(k for k, v in sorted(doc['hospital']['casebook'].items()) if v['descubierta'])}; "
          f"{len(objetos)} objetos ({', '.join(o['id'] for o in objetos)})")


if __name__ == "__main__":
    main()
