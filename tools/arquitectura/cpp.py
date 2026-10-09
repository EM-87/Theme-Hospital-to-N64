#!/usr/bin/env python3
"""Inventario del núcleo C++ de CorsixTH por subsistema (fase 3).

Uso: cpp.py <CORSIXTH_DIR> <salida.json>

Para cada fichero de CorsixTH/Src, CorsixTH/SrcUnshared y libs/ cuenta:
  - líneas de código (sin vacías ni comentarios);
  - referencias a SDL (identificadores SDL_*, Mix_*, cabeceras SDL),
    a la API de Lua (lua_*, luaL_*), a FFmpeg (av*/sws_*/swr_*) y a
    FluidSynth;
  - #include de otros ficheros del proyecto.
Y lo agrupa por subsistema según la tabla SUBSYSTEMS (asignada a mano leyendo
cada fichero).
"""
import json
import os
import re
import sys

SUBSYSTEMS = [
    ("Arranque y bucle principal", r"(SrcUnshared/main|Src/main|bootstrap|sdl_core|sdl_wm|lua_sdl|config\.h|libs/whereami)"),
    ("Enlace C++ ↔ Lua (registro de clases)", r"(Src/th_lua\.|th_lua_internal|Src/lua\.hpp|th_lua_ui|Src/th\.(h|cpp))"),
    ("Mapa y superposiciones", r"(th_map|th_lua_map)"),
    ("Pathfinding", r"th_pathfind"),
    ("Gráficos: formatos, animaciones y fuentes", r"(th_gfx\.|th_gfx_common|th_gfx_font|th_lua_anims|th_lua_gfx)"),
    ("Gráficos: render con SDL", r"th_gfx_sdl"),
    ("Sonido (efectos)", r"(th_sound|th_lua_sound|sdl_audio)"),
    ("Música (MIDI)", r"(midi_player|th_lua_midi|xmi2mid)"),
    ("Vídeo (FMV)", r"(th_movie|th_lua_movie)"),
    ("Textos y codificaciones", r"(th_strings|th_lua_strings|cp437|cp936|cpmik)"),
    ("Guardado de partidas", r"(persist_lua|run_length_encoder)"),
    ("Ficheros (ISO, RNC, lfs)", r"(iso_fs|th_lua_iso|th_lua_lfs_ext|lua_rnc|libs/rnc)"),
    ("Números aleatorios", r"random\.c"),
]
COMPILED = [(n, re.compile(p)) for n, p in SUBSYSTEMS]

PATTERNS = {
    "sdl": re.compile(r"\b(SDL_\w+|Mix_\w+)"),
    "lua_api": re.compile(r"\b(lua_\w+|luaL_\w+)"),
    "ffmpeg": re.compile(r"\b(av_\w+|avcodec_\w+|avformat_\w+|sws_\w+|swr_\w+|AV[A-Z]\w+)"),
    "fluidsynth": re.compile(r"\bfluid_\w+"),
}
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def main():
    root, out = sys.argv[1], sys.argv[2]
    files = []
    for d in ("CorsixTH/Src", "CorsixTH/SrcUnshared", "libs"):
        for dp, _, names in os.walk(os.path.join(root, d)):
            for n in sorted(names):
                if n.endswith((".c", ".cpp", ".h", ".hpp")) or n == "config.h.in":
                    files.append(os.path.relpath(os.path.join(dp, n), root))
    res = []
    for rel in sorted(files):
        raw = open(os.path.join(root, rel), encoding="utf-8", errors="replace").read()
        code = strip_comments(raw)
        lines = [ln for ln in code.splitlines() if ln.strip()]
        sub = next((n for n, rx in COMPILED if rx.search(rel)), "Otros")
        r = {"fichero": rel, "subsistema": sub, "lineas": len(raw.splitlines()), "codigo": len(lines),
             "includes": sorted(set(os.path.basename(i) for i in INCLUDE.findall(raw)))}
        for k, rx in PATTERNS.items():
            r[k] = len(rx.findall(code))
        r["incluye_sdl"] = any(i.startswith("SDL") for i in INCLUDE.findall(raw)) or "<SDL" in raw
        res.append(r)
    groups = {}
    for r in res:
        g = groups.setdefault(r["subsistema"], {"ficheros": 0, "codigo": 0, "sdl": 0, "lua_api": 0,
                                                "ffmpeg": 0, "fluidsynth": 0, "ficheros_con_sdl": 0})
        g["ficheros"] += 1
        for k in ("codigo", "sdl", "lua_api", "ffmpeg", "fluidsynth"):
            g[k] += r[k]
        g["ficheros_con_sdl"] += 1 if (r["sdl"] or r["incluye_sdl"]) else 0
    for n, g in sorted(groups.items(), key=lambda kv: -kv[1]["codigo"]):
        print(f"{g['codigo']:6d} líneas {g['ficheros']:3d} fich. SDL {g['sdl']:4d} Lua {g['lua_api']:4d} "
              f"FFmpeg {g['ffmpeg']:3d} fluid {g['fluidsynth']:3d}  {n}")
    print(sum(g["codigo"] for g in groups.values()), "líneas de código en total")
    json.dump({"subsistemas": groups, "ficheros": res}, open(out, "w"), indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
