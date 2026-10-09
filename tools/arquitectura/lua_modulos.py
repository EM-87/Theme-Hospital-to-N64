#!/usr/bin/env python3
"""Mapa de módulos del Lua de CorsixTH (fase 3).

Uso: lua_modulos.py <CorsixTH/Lua> <salida.json> [perfil.json]

1. Asigna cada fichero a un grupo (GROUPS, por ruta) y cuenta sus líneas de
   código (sin vacías ni comentarios).
2. Saca los símbolos globales que define cada fichero: clases
   (class "Nombre" (Base)), funciones globales (function nombre() en la
   columna 0) y asignaciones globales (Nombre = ... en la columna 0).
3. Dependencias: cuántas veces nombra cada fichero (fuera de comentarios y
   cadenas, y sin contar accesos a campos como x.nombre) un símbolo definido
   en otro fichero; se suman por grupo. Los ficheros de idioma no cuentan
   como definidores (son tablas de textos), y _S/_A (las tablas de textos que
   monta strings.lua) se atribuyen a strings.lua.
4. Acoplamiento con la interfaz y con los servicios de la plataforma: líneas
   de cada fichero que tocan la UI (ventanas, UI*, .ui), el sonido (.audio,
   playSound), los gráficos (.gfx, animaciones, setAnimation, setLayer) o el
   ratón/cursor. Un fichero de lógica con cero líneas así es «lógica pura».
5. Si se da perfil.json (bench/scenarios/perfil.lua), suma por grupo las
   muestras propias (las inclusivas no se pueden sumar por grupo).
"""
import json
import os
import re
import sys

GROUPS = [
    ("Interfaz", r"^(ui|game_ui|window|sprite_viewer)\.lua$|^dialogs/"),
    ("Textos de idioma (languages/, datos)", r"^languages/"),
    ("Plataforma y servicios", r"^(app|audio|graphics|filesystem|movie_player|persistance|config_finder|base_config|api_version|debug_script|strings|string_extensions)\.lua$"),
    ("Utilidades del lenguaje", r"^(class|utility|strict|date)\.lua$"),
    ("Mapa y entidades en el mapa", r"^(map|entity_map)\.lua$|^walls/"),
    ("Entidades: humanoides", r"^entities/(humanoid\.lua|humanoids/)"),
    ("Acciones de humanoides", r"^humanoid_action\.lua$|^humanoid_actions/"),
    ("Entidades: objetos y máquinas", r"^entity\.lua$|^entities/(object|machine)\.lua$|^objects/"),
    ("Habitaciones", r"^room\.lua$|^rooms/"),
    ("Enfermedades y diagnóstico", r"^diseases/|^diagnosis/"),
    ("Mundo, hospital y economía", r"^(world|hospital|calls_dispatcher|queue|research_department|staff_profile|endconditions)\.lua$|^hospitals/"),
    ("Eventos (epidemias, terremotos, trucos, locutor)", r"^(epidemic|earthquake|cheats|announcer)\.lua$"),
]
COMPILED = [(g, re.compile(p)) for g, p in GROUPS]
LOGIC_GROUPS = {"Mapa y entidades en el mapa", "Entidades: humanoides", "Acciones de humanoides",
                "Entidades: objetos y máquinas", "Habitaciones", "Enfermedades y diagnóstico",
                "Mundo, hospital y economía", "Eventos (epidemias, terremotos, trucos, locutor)"}

COUPLING = {
    "ui": re.compile(r"\bUI[A-Z]\w*|\.ui\b|\bui\s*[:.]|:getWindow\b|:addWindow\b|\bWindow\b|adviser|\bGameUI\b"),
    "audio": re.compile(r"\.audio\b|playSound|:playAnnouncement|\bsound_fx\b|announcer|\bAnnouncer\b"),
    "graficos": re.compile(r"\.gfx\b|setAnimation\b|setLayer\b|:setTilePositionSpeed\b|setDrawingLayer|"
                           r"\bth:\w*[Aa]nim|animation_info|\bDrawFlags\b|setMood\b|loadMainCursor|cursor"),
}


def strip(src):
    src = re.sub(r"--\[(=*)\[.*?\]\1\]", lambda m: "\n" * m.group(0).count("\n"), src, flags=re.S)
    src = re.sub(r"\[(=*)\[.*?\]\1\]", lambda m: '""' + "\n" * m.group(0).count("\n"), src, flags=re.S)
    out = []
    for line in src.split("\n"):
        line = re.sub(r'"(\\.|[^"\\])*"|\'(\\.|[^\'\\])*\'', '""', line)
        line = re.sub(r"--.*", "", line)
        out.append(line)
    return "\n".join(out)


def group_of(rel):
    return next((g for g, rx in COMPILED if rx.search(rel)), "Otros")


def main():
    root, out = sys.argv[1], sys.argv[2]
    prof = json.load(open(sys.argv[3])) if len(sys.argv) > 3 else None
    files = {}
    for dp, _, names in os.walk(root):
        for n in sorted(names):
            if n.endswith(".lua"):
                rel = os.path.relpath(os.path.join(dp, n), root).replace(os.sep, "/")
                raw = open(os.path.join(dp, n), encoding="utf-8", errors="replace").read()
                code = strip(raw)
                files[rel] = {"grupo": group_of(rel), "code": code,
                              "codigo": sum(1 for ln in code.split("\n") if ln.strip())}
    # Símbolos globales
    defs, bases = {"_S": "strings.lua", "_A": "strings.lua"}, {}
    # Las cadenas ya están vacías en "code": las clases se buscan en el original.
    for rel, f in files.items():
        if rel.startswith("languages/"):
            continue
        raw = open(os.path.join(root, rel), encoding="utf-8", errors="replace").read()
        for m in re.finditer(r'^class\s*"(\w+)"\s*(?:\(\s*(\w+)\s*\))?', raw, re.M):
            defs.setdefault(m.group(1), rel)
            if m.group(2):
                bases[m.group(1)] = m.group(2)
        for m in re.finditer(r"^function\s+([A-Za-z_]\w*)\s*\(", f["code"], re.M):
            defs.setdefault(m.group(1), rel)
        for m in re.finditer(r"^([A-Za-z_]\w*)\s*=[^=]", f["code"], re.M):
            if len(m.group(1)) > 1:  # «_ = ...» es una variable descartable, no un símbolo
                defs.setdefault(m.group(1), rel)
    ident = re.compile(r"(?<![.:\w])[A-Za-z_]\w*\b")
    edges = {}
    for rel, f in files.items():
        for name in ident.findall(f["code"]):
            src = defs.get(name)
            if src and src != rel:
                edges[(rel, src)] = edges.get((rel, src), 0) + 1
    # Acoplamiento
    for rel, f in files.items():
        lines = f["code"].split("\n")
        for k, rx in COUPLING.items():
            f[k] = sum(1 for ln in lines if rx.search(ln))
    # Agregados por grupo
    groups = {}
    for rel, f in files.items():
        g = groups.setdefault(f["grupo"], {"ficheros": 0, "codigo": 0, "lineas_ui": 0, "lineas_audio": 0,
                                           "lineas_graficos": 0, "ficheros_sin_ui": 0,
                                           "logica": f["grupo"] in LOGIC_GROUPS})
        g["ficheros"] += 1
        g["codigo"] += f["codigo"]
        g["lineas_ui"] += f["ui"]
        g["lineas_audio"] += f["audio"]
        g["lineas_graficos"] += f["graficos"]
        g["ficheros_sin_ui"] += 1 if f["ui"] == 0 else 0
    gedges = {}
    for (a, b), w in edges.items():
        ga, gb = files[a]["grupo"], files[b]["grupo"]
        if ga != gb:
            gedges[(ga, gb)] = gedges.get((ga, gb), 0) + w
    if prof:
        T = prof["muestras"]
        for x in prof["ficheros_propio"]:
            rel = x["clave"]
            if rel in files:
                g = groups[files[rel]["grupo"]]
                g["perfil_propio"] = g.get("perfil_propio", 0) + x["muestras"] / T
    res = {
        "grupos": groups,
        "dependencias_entre_grupos": [{"de": a, "a": b, "referencias": w}
                                      for (a, b), w in sorted(gedges.items(), key=lambda kv: -kv[1])],
        "herencia": bases,
        "ficheros": {rel: {k: v for k, v in f.items() if k != "code"} for rel, f in files.items()},
    }
    for g, a in sorted(groups.items(), key=lambda kv: -kv[1]["codigo"]):
        print(f"{a['codigo']:6d} líneas {a['ficheros']:3d} fich. UI {a['lineas_ui']:4d} audio {a['lineas_audio']:3d} "
              f"gfx {a['lineas_graficos']:4d} sin UI {a['ficheros_sin_ui']:3d}  "
              f"perfil {100 * a.get('perfil_propio', 0):5.1f}% propio  {g}")
    print("dependencias entre grupos (top 30):")
    for e in res["dependencias_entre_grupos"][:30]:
        print(f"  {e['referencias']:5d} {e['de']} -> {e['a']}")
    json.dump(res, open(out, "w"), indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
