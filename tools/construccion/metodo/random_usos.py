#!/usr/bin/env python3
"""Cuenta los usos de math.random (y t_random, n_random, p_random) en el Lua de
CorsixTH, por grupo de tools/arquitectura/lua_modulos.py (misma asignación de
ficheros a grupos), ignorando comentarios. También cuenta `pairs(` sobre
tablas, porque el orden de pairs con claves tabla depende de direcciones de
memoria.

Uso: python3 -I random_usos.py <dir Lua> <salida.json>
"""
import json
import os
import re
import sys

GROUPS = [
    ("Interfaz", r"^(ui|game_ui|window|sprite_viewer)\.lua$|^dialogs/"),
    ("Textos de idioma", r"^languages/"),
    ("Plataforma y servicios", r"^(app|audio|graphics|filesystem|movie_player|persistance|config_finder|base_config|api_version|debug_script|strings|string_extensions)\.lua$"),
    ("Utilidades del lenguaje", r"^(class|utility|strict|date)\.lua$"),
    ("Mapa y entidades en el mapa", r"^(map|entity_map)\.lua$|^walls/"),
    ("Entidades: humanoides", r"^entities/(humanoid\.lua|humanoids/)"),
    ("Acciones de humanoides", r"^humanoid_action\.lua$|^humanoid_actions/"),
    ("Entidades: objetos y máquinas", r"^entity\.lua$|^entities/(object|machine)\.lua$|^objects/"),
    ("Habitaciones", r"^room\.lua$|^rooms/"),
    ("Enfermedades y diagnóstico", r"^diseases/|^diagnosis/"),
    ("Mundo, hospital y economía", r"^(world|hospital|calls_dispatcher|queue|research_department|staff_profile|endconditions)\.lua$|^hospitals/"),
    ("Eventos", r"^(epidemic|earthquake|cheats|announcer)\.lua$"),
]
LOGIC = {"Mapa y entidades en el mapa", "Entidades: humanoides", "Acciones de humanoides",
         "Entidades: objetos y máquinas", "Habitaciones", "Enfermedades y diagnóstico",
         "Mundo, hospital y economía", "Eventos"}
PAT = {
    "math.random": re.compile(r"\bmath\.random\s*\("),
    "math.t_random": re.compile(r"\bmath\.t_random\s*\("),
    "math.n_random": re.compile(r"\bmath\.n_random\s*\("),
    "math.p_random": re.compile(r"\bmath\.p_random\s*\("),
    "pairs(": re.compile(r"(?<![i_])\bpairs\s*\("),
}


def strip_comments(src):
    src = re.sub(r"--\[(=*)\[.*?\]\1\]", lambda m: "\n" * m.group(0).count("\n"), src, flags=re.S)
    return re.sub(r"--[^\n]*", "", src)


def main():
    base, outp = sys.argv[1], sys.argv[2]
    res = {g: {k: 0 for k in PAT} | {"ficheros_con_random": 0} for g, _ in GROUPS}
    per_file = {}
    for root, _, files in os.walk(base):
        for f in files:
            if not f.endswith(".lua"):
                continue
            p = os.path.join(root, f)
            rel = os.path.relpath(p, base)
            grp = next((g for g, r in GROUPS if re.search(r, rel)), None)
            if grp is None:
                continue
            src = strip_comments(open(p, encoding="utf-8", errors="replace").read())
            counts = {k: len(r.findall(src)) for k, r in PAT.items()}
            for k, v in counts.items():
                res[grp][k] += v
            if counts["math.random"] + counts["math.t_random"] + counts["math.n_random"] + counts["math.p_random"]:
                res[grp]["ficheros_con_random"] += 1
                per_file[rel] = counts["math.random"]
    logic = {k: sum(res[g][k] for g in LOGIC) for k in list(PAT) + ["ficheros_con_random"]}
    out = {"por_grupo": res, "logica": logic,
           "top_ficheros": sorted(per_file.items(), key=lambda x: -x[1])[:15]}
    json.dump(out, open(outp, "w"), indent=1, ensure_ascii=False)
    for g, _ in GROUPS:
        r = res[g]
        print(f"{g:32} random={r['math.random']:4} t={r['math.t_random']} n={r['math.n_random']} p={r['math.p_random']} pairs={r['pairs(']:4}{'  (lógica)' if g in LOGIC else ''}")
    print("LÓGICA:", logic)
    print("top:", out["top_ficheros"][:10])


if __name__ == "__main__":
    main()
