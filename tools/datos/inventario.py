#!/usr/bin/env python3
"""Inventario de los datos de Theme Hospital (fase 2).

Uso: inventario.py <TH_DATA_DIR> <dir_trabajo> <salida.json>

Recorre los datos, clasifica cada fichero en un grupo, detecta la compresión
RNC (cabecera "RNC\\x01") y descomprime con rnc_decode (herramienta de
CorsixTH) en <dir_trabajo>/raw/, para que las mediciones siguientes trabajen
sobre los datos en bruto. Los datos nunca entran en el repositorio.
"""
import json
import os
import re
import subprocess
import sys

RNC = os.environ.get("RNC_DECODE") or os.path.join(
    os.environ.get("CORSIXTH_DIR", os.path.expanduser("~/th64-work/src/CorsixTH")),
    "build/notracy/tools/rnc/rnc_decode")

# (grupo, subgrupo, expresión sobre la ruta relativa en mayúsculas). Gana la primera.
RULES = [
    ("Vídeo", "Intro y demo", r"^INTRO/"),
    ("Vídeo", "Escenas (ANIMS)", r"^ANIMS/"),
    ("Sonido", "Efectos y voces (SOUND-x.DAT, uno por idioma)", r"^SOUND/DATA/SOUND-\d\.DAT$"),
    ("Sonido", "Otros datos de sonido", r"^SOUND/DATA/"),
    ("Música", "Partituras XMI", r"^SOUND/MIDI/.*\.XMI$"),
    ("Ejecutables y controladores DOS", "Utilidades de música", r"^SOUND/MIDI/"),
    ("Ejecutables y controladores DOS", "Controladores de sonido", r"^SOUND/"),
    ("Mapas y niveles", "Mapas de campaña (LEVEL.L1-L12)", r"^LEVELS/LEVEL\.L([1-9]|1[0-2])$"),
    ("Mapas y niveles", "Configuración de campaña (.SAM)", r"^LEVELS/(EASY|FULL|HARD)\d+\.SAM$"),
    ("Mapas y niveles", "Multijugador y extra (fuera de alcance)", r"^LEVELS/"),
    ("Textos", "Idiomas (LANG-x.DAT)", r"^DATA/LANG-\d\.DAT$"),
    ("Textos", "Léxicos (nombres)", r"\.LEX$"),
    ("Gráficos juego, alta resolución (DATA)", "Animaciones", r"^DATA/V(STA|START|FRA|LIST|ELE)-\d\.ANI$"),
    ("Gráficos juego, alta resolución (DATA)", "Sprites y bloques", r"^DATA/"),
    ("Gráficos juego, baja resolución (DATAM)", "Animaciones", r"^DATAM/M(START|FRA|LIST|ELE)-\d\.ANI$"),
    ("Gráficos juego, baja resolución (DATAM)", "Sprites y bloques", r"^DATAM/"),
    ("Interfaz (QDATA)", "Fuentes", r"^QDATA/FONT"),
    ("Interfaz (QDATA)", "Pantallas completas (bitmap + paleta)", r"^QDATA/\w+01[VM]\.(DAT|PAL)$|^QDATA/(TOWN|FACE01V)\.DAT$"),
    ("Interfaz (QDATA)", "Paletas alternativas (GHOST)", r"^QDATA/GHOST"),
    ("Interfaz (QDATA)", "Hojas de interfaz y cursores", r"^QDATA/"),
    ("Interfaz baja resolución (QDATAM)", "Fuentes y caras", r"^QDATAM/"),
    ("Ejecutables y controladores DOS", "Ejecutables", r"\.(EXE|DLL|CFG|INI)$"),
    ("Otros", "Tabla de récords (SAVE/HISCORE.DAT)", r"^SAVE/"),
]
COMPILED = [(g, s, re.compile(p)) for g, s, p in RULES]


def classify(rel):
    up = rel.upper()
    for g, s, rx in COMPILED:
        if rx.search(up):
            return g, s
    return "Otros", "Otros"


def main():
    src, work, out = sys.argv[1], sys.argv[2], sys.argv[3]
    raw_root = os.path.join(work, "raw")
    files = []
    for root, _, names in os.walk(src):
        for n in sorted(names):
            p = os.path.join(root, n)
            rel = os.path.relpath(p, src).replace(os.sep, "/")
            g, s = classify(rel)
            disk = os.path.getsize(p)
            with open(p, "rb") as f:
                rnc = f.read(4) == b"RNC\x01"
            dst = os.path.join(raw_root, rel.upper())
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            if rnc:
                subprocess.run([RNC, p, dst], check=True, stdout=subprocess.DEVNULL)
            elif not os.path.exists(dst) or os.path.getsize(dst) != disk:
                with open(p, "rb") as fi, open(dst, "wb") as fo:
                    fo.write(fi.read())
            files.append({"ruta": rel.upper(), "grupo": g, "subgrupo": s, "disco": disk,
                          "bruto": os.path.getsize(dst), "rnc": rnc})
    groups = {}
    for f in files:
        k = groups.setdefault(f["grupo"], {"ficheros": 0, "disco": 0, "bruto": 0, "rnc": 0, "subgrupos": {}})
        sg = k["subgrupos"].setdefault(f["subgrupo"], {"ficheros": 0, "disco": 0, "bruto": 0})
        for d in (k, sg):
            d["ficheros"] += 1
            d["disco"] += f["disco"]
            d["bruto"] += f["bruto"]
        k["rnc"] += f["rnc"]
    json.dump({"origen": src, "grupos": groups, "ficheros": files}, open(out, "w"),
              indent=1, ensure_ascii=False)
    tot_d = sum(f["disco"] for f in files)
    tot_b = sum(f["bruto"] for f in files)
    for g, k in sorted(groups.items(), key=lambda kv: -kv[1]["disco"]):
        print(f"{k['disco'] / 1e6:8.2f} MB disco {k['bruto'] / 1e6:8.2f} MB bruto {k['ficheros']:4d} ficheros "
              f"({k['rnc']} RNC)  {g}")
        for s, sg in sorted(k["subgrupos"].items(), key=lambda kv: -kv[1]["disco"]):
            print(f"      {sg['disco'] / 1e6:8.2f} {sg['bruto'] / 1e6:8.2f} {sg['ficheros']:4d}  {s}")
    print(f"{tot_d / 1e6:8.2f} MB disco {tot_b / 1e6:8.2f} MB bruto {len(files)} ficheros  TOTAL")


if __name__ == "__main__":
    main()
