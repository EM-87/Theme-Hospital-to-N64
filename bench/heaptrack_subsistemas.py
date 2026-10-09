#!/usr/bin/env python3
"""Reparte el pico de heap de un perfil de heaptrack por subsistema.

Uso: heaptrack_subsistemas.py <heaptrack.gz> [salida.json]

Usa `heaptrack_print --flamegraph-cost-type peak`, que da los bytes vivos de
cada pila de llamadas en el momento de máximo consumo. Cada pila se asigna a
una categoría según el marco con nombre más cercano a la hoja que coincida con
alguna regla (las bibliotecas sin símbolos aparecen como direcciones 0x...).
"""
import json
import re
import subprocess
import sys
import tempfile

# (categoría, expresión regular sobre el nombre del marco). El orden importa
# solo cuando un mismo marco coincide con varias reglas.
RULES = [
    ("Gráficos: texturas (SDL/driver)", r"render_target::create_(palettized_)?texture|SDL_CreateTexture"),
    ("Vídeo: presentación y contexto (driver)", r"render_target::(end_frame|start_frame|render_target|set_scale|update)|SDL_RenderPresent|SDL_CreateRenderer|SDL_CreateWindow|dri\w+|glX\w+|XOpenDisplay"),
    ("Gráficos: sprites, animaciones y fuentes", r"sprite_sheet::|animation_manager::|convertLegacySprite|raw_bitmap::|bitmap_font::|freetype_font::|FT_\w+|line_sequence::|chunk_renderer|palette::|full_colour_renderer|wx_storing"),
    ("Mapa y pathfinding", r"level_map::|map_tile|pathfinder::|th_map|path_node|abstract_pathfinder"),
    ("Sonido y música", r"sound_archive::|sound_player::|Mix_\w+|xmi_to_midi|music_info"),
    ("Carga de partida (persistencia)", r"lua_persist_\w+::|lua_persist_basic_\w+::"),
    ("Lua (intérprete)", r"^(lua[A-Z]?|luaL|luaM|luaH|luaC|luaS|luaD|luaV|luaF|luaE|luaZ|luaY|luaK|luaX|luaU|luaT|luaO|luaG|luaB)_\w+$|^l_alloc$"),
    ("Cadenas y textos del juego", r"th_string_list|l_load_strings|utf8"),
    ("Ficheros y descompresión", r"rnc_\w+|rnc_inpack|lfs_|l_load_|read_file|load_file"),
]
COMPILED = [(c, re.compile(p)) for c, p in RULES]


def classify(frames):
    for f in reversed(frames):  # de la hoja hacia la raíz
        if f.startswith("0x"):
            continue
        for cat, rx in COMPILED:
            if rx.search(f):
                return cat
    if any(("(th_" in f or "(main.cpp)" in f or "(sdl_core.cpp)" in f) for f in frames):
        return "Otros (CorsixTH)"
    return "Otros (sistema: SDL, libc, cargador...)"


def main():
    src = sys.argv[1]
    with tempfile.NamedTemporaryFile(suffix=".txt") as tmp:
        subprocess.run(["heaptrack_print", "-f", src, "--flamegraph-cost-type", "peak",
                        "-F", tmp.name], check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
        lines = open(tmp.name, encoding="utf-8", errors="replace").read().splitlines()
    summary = subprocess.run(["heaptrack_print", "-f", src, "-p", "0", "-a", "0", "-T", "0"],
                             capture_output=True, text=True).stdout
    total, cats, top = 0, {}, {}
    for line in lines:
        stack, cost = line.rsplit(" ", 1)
        cost = int(cost)
        if cost <= 0:
            continue
        frames = stack.split(";")
        cat = classify(frames)
        total += cost
        cats[cat] = cats.get(cat, 0) + cost
        named = [f for f in frames if not f.startswith("0x")]
        top.setdefault(cat, []).append((cost, " > ".join(n.split(" (")[0] for n in named[-4:])))
    out = {
        "fichero": src,
        "pico_total_bytes": total,
        "categorias_bytes": dict(sorted(cats.items(), key=lambda kv: -kv[1])),
        "principales_pilas": {c: [{"bytes": b, "pila": p} for b, p in sorted(v, reverse=True)[:3]]
                              for c, v in top.items()},
    }
    m = re.search(r"peak heap memory consumption: (.+)", summary)
    out["heaptrack_pico"] = m.group(1) if m else None
    m = re.search(r"peak RSS \(including heaptrack overhead\): (.+)", summary)
    out["heaptrack_rss"] = m.group(1) if m else None
    text = json.dumps(out, indent=1, ensure_ascii=False)
    if len(sys.argv) > 2:
        open(sys.argv[2], "w", encoding="utf-8").write(text + "\n")
    for cat, b in out["categorias_bytes"].items():
        print(f"{b / 1e6:9.2f} MB  {cat}")
    print(f"{total / 1e6:9.2f} MB  TOTAL (pico)")


if __name__ == "__main__":
    main()
