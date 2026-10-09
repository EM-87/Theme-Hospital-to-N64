#!/usr/bin/env python3
"""Tamaño en cartucho con la compresión de assets de libdragon (fase 2).

Uso: comprimir.py <dir_trabajo> <inventario.json> <salida.json>

Pasa por mkasset (-c 1, 2 y 3, ventana de 256 KiB, la mejor para asset_load)
dos cosas:
  - los blobs de <dir_trabajo>/blobs/ que deja graficos.py (sprites ya
    convertidos a CI8 o CI4/CI8 y pantallas a 320x240);
  - los ficheros en bruto (<dir_trabajo>/raw/) de los subgrupos del
    inventario que irían al cartucho tal cual: mapas, configuración de la
    campaña, textos, animaciones, paletas y las hojas en formato original.
Cada fichero se comprime por separado, como se cargaría en la consola.
Los resultados se guardan en <dir_trabajo>/mkasset_cache.json por SHA-1 del
contenido, nivel y ventana: al repetir solo se recomprime lo que ha cambiado.
"""
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed

MKASSET = os.path.join(os.environ.get("N64_INST", "/opt/libdragon"), "bin/mkasset")
LEVELS = (1, 2, 3)

# Subgrupos del inventario cuyos ficheros se miden tal cual.
RAW_SUBGROUPS = [
    "Mapas de campaña (LEVEL.L1-L12)",
    "Configuración de campaña (.SAM)",
    "Idiomas (LANG-x.DAT)",
    "Léxicos (nombres)",
    "Animaciones",
    "Sprites y bloques",
    "Hojas de interfaz y cursores",
    "Fuentes",
    "Fuentes y caras",
    "Pantallas completas (bitmap + paleta)",
]


CACHE = {}


def digest(path):
    return hashlib.sha1(open(path, "rb").read()).hexdigest()


def mk(path, level, key):
    """Tamaño de <path> tras mkasset -c <level> -w 256 (cada llamada en su directorio)."""
    with tempfile.TemporaryDirectory() as out:
        subprocess.run([MKASSET, "-c", str(level), "-w", "256", "-o", out, path], check=True,
                       stdout=subprocess.DEVNULL)
        return key, os.path.getsize(os.path.join(out, os.path.basename(path)))


def main():
    work, inv, out = sys.argv[1], sys.argv[2], sys.argv[3]
    cache = os.path.join(work, "mkasset_cache.json")
    if os.path.exists(cache):
        CACHE.update(json.load(open(cache)))
    blobs = os.path.join(work, "blobs")
    blob_paths = [os.path.join(blobs, f) for f in sorted(os.listdir(blobs))]
    raw = [f for f in json.load(open(inv))["ficheros"] if f["subgrupo"] in RAW_SUBGROUPS]
    paths = blob_paths + [os.path.join(work, "raw", f["ruta"]) for f in raw]
    keys = {p: digest(p) for p in paths}

    # mkasset -c 3 (Shrinkler) usa un solo núcleo: se reparten los ficheros pendientes.
    todo = {f"{keys[p]}:c{c}:w256": (p, c) for p in paths for c in LEVELS}
    todo = {k: v for k, v in todo.items() if k not in CACHE}
    print(f"{len(todo)} compresiones pendientes ({len(CACHE)} en caché)", flush=True)
    with ThreadPoolExecutor(os.cpu_count() or 1) as ex:
        futs = [ex.submit(mk, p, c, k) for k, (p, c) in todo.items()]
        for i, fut in enumerate(as_completed(futs), 1):
            k, size = fut.result()
            CACHE[k] = size
            if i % 20 == 0 or i == len(futs):
                json.dump(CACHE, open(cache, "w"))
                print(f"  {i}/{len(futs)}", flush=True)

    def size(p, c):
        return CACHE[f"{keys[p]}:c{c}:w256"]

    res = {"blobs": {}, "bruto": {}}
    for p in blob_paths:
        f = os.path.basename(p)
        r = {"bytes": os.path.getsize(p), **{f"c{c}": size(p, c) for c in LEVELS}}
        res["blobs"][f] = r
        print(f"{f:32s} {r['bytes']:9d} " + " ".join(f"c{c}={r[f'c{c}']:9d}" for c in LEVELS))
    for f in raw:
        key = f"{f['grupo']} / {f['subgrupo']}"
        acc = res["bruto"].setdefault(key, {"ficheros": 0, "disco": 0, "bruto": 0,
                                            **{f"c{c}": 0 for c in LEVELS}})
        p = os.path.join(work, "raw", f["ruta"])
        acc["ficheros"] += 1
        acc["disco"] += f["disco"]
        acc["bruto"] += f["bruto"]
        for c in LEVELS:
            acc[f"c{c}"] += size(p, c)
    for k, a in res["bruto"].items():
        print(f"{k:70s} disco {a['disco']:9d} bruto {a['bruto']:9d} "
              + " ".join(f"c{c}={a[f'c{c}']:9d}" for c in LEVELS))
    json.dump(res, open(out, "w"), indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
