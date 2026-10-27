#!/usr/bin/env python3
"""Lo que ocupa cada parte de una ROM, del mapa del enlazador (.map): código
(.text), datos de solo lectura, datos y bss, separado en el motor (núcleo,
vista y capa de la N64), libdragon (por módulo) y las bibliotecas de C.

Uso: python3 -I tools/hito5/codigo.py <rom.map> [--json salida.json]
"""
import collections
import json
import re
import sys

SECCIONES = {".text": "codigo", ".rodata": "solo_lectura", ".data": "datos", ".sdata": "datos", ".bss": "bss",
             ".sbss": "bss"}


def origen(ruta):
    if "/build/" in ruta or ruta.startswith("build/"):
        m = re.search(r"build/[^/]+/(core|vista|platform|tests)/(?:[^/]+/)*([^/]+)\.o$", ruta)
        return "motor", f"{m[1]}/{m[2]}" if m else "otro"
    m = re.search(r"lib(dragon|c|m|gcc|g|nosys)\.a\(([^)]+)\)", ruta)
    if m:
        lib = "lib" + m[1]
        return lib, m[2]
    if "crt" in ruta:
        return "crt", ruta.rsplit("/", 1)[-1]
    return "otro", ruta.rsplit("/", 1)[-1]


def main():
    args = sys.argv[1:]
    salida_json = None
    if "--json" in args:
        k = args.index("--json")
        salida_json = args[k + 1]
        del args[k:k + 2]
    if len(args) != 1:
        sys.exit(__doc__)
    texto = open(args[0], encoding="utf-8", errors="replace").read()
    texto = texto[texto.index("Linker script and memory map"):]
    total = collections.defaultdict(int)         # (parte, sección) -> bytes
    detalle = collections.defaultdict(int)       # (parte, sub, sección) -> bytes
    seccion = None
    pendiente = None
    for linea in texto.splitlines():
        m = re.match(r"^(\.\w+)\s", linea)
        if m:
            seccion = SECCIONES.get(m[1])
            continue
        if seccion is None:
            continue
        # Sección de entrada: « .text.x  0xdir  0xtam  ruta», o el nombre solo en una línea y el resto en la siguiente.
        m = re.match(r"^ (\.\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$", linea)
        if not m and pendiente:
            m2 = re.match(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$", linea)
            if m2:
                m = (None, pendiente, m2[1], m2[2], m2[3])
        pendiente = None
        if not m:
            m3 = re.match(r"^ (\.\S+)$", linea)
            if m3:
                pendiente = m3[1]
            continue
        dir_, tam, ruta = int(m[2], 16), int(m[3], 16), m[4].strip()
        if dir_ == 0 or tam == 0:
            continue
        parte, sub = origen(ruta)
        total[(parte, seccion)] += tam
        detalle[(parte, sub, seccion)] += tam
    partes = sorted({p for p, _ in total}, key=lambda p: -total[(p, "codigo")])
    cols = ["codigo", "solo_lectura", "datos", "bss"]
    print(f"{'parte':<12}" + "".join(f"{c:>14}" for c in cols))
    for p in partes:
        print(f"{p:<12}" + "".join(f"{total[(p, c)]:>14,}".replace(",", ".") for c in cols))
    print(f"{'total':<12}" + "".join(f"{sum(v for (q, c2), v in total.items() if c2 == c):>14,}".replace(",", ".")
                                     for c in cols))
    for p in ("motor", "libdragon"):
        subs = sorted({s for q, s, _ in detalle if q == p}, key=lambda s: -detalle[(p, s, "codigo")])
        print(f"-- {p}, código por {'fichero' if p == 'motor' else 'módulo'}:")
        for s in subs[:25]:
            if detalle[(p, s, "codigo")]:
                print(f"   {s:<30}{detalle[(p, s, 'codigo')]:>10,}".replace(",", "."))
    if salida_json:
        json.dump({"total": {f"{p}/{c}": v for (p, c), v in total.items()},
                   "detalle": {f"{p}/{s}/{c}": v for (p, s, c), v in detalle.items()}}, open(salida_json, "w"),
                  indent=1)


if __name__ == "__main__":
    main()
