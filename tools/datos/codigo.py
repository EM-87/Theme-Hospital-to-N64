#!/usr/bin/env python3
"""Tamaño en cartucho del código Lua de CorsixTH (fase 2).

Uso: codigo.py <dir_Lua_de_CorsixTH> <salida.json>

Compila cada .lua con luac5.4 -s (sin información de depuración, como iría
en la ROM) y comprime el bytecode con mkasset -c 1 y -c 3 (ventana de 256
KiB), fichero a fichero. Separa los ficheros de idioma (languages/*.lua),
porque en la N64 solo irían los idiomas incluidos.
"""
import json
import os
import subprocess
import sys
import tempfile

N64 = os.environ.get("N64_INST", "/opt/libdragon")
MKASSET = os.path.join(N64, "bin/mkasset")
LUAC = os.environ.get("LUAC", "luac5.4")


def main():
    src, out = sys.argv[1], sys.argv[2]
    groups = {}
    with tempfile.TemporaryDirectory() as tmp:
        for root, _, names in os.walk(src):
            for n in sorted(names):
                if not n.endswith(".lua"):
                    continue
                p = os.path.join(root, n)
                rel = os.path.relpath(p, src)
                if rel.startswith("languages" + os.sep):
                    g = "idioma " + n[:-4]
                else:
                    g = "motor"
                bc = os.path.join(tmp, "x.luac")
                subprocess.run([LUAC, "-s", "-o", bc, p], check=True)
                acc = groups.setdefault(g, {"ficheros": 0, "fuente": 0, "bytecode": 0, "c1": 0, "c3": 0})
                acc["ficheros"] += 1
                acc["fuente"] += os.path.getsize(p)
                acc["bytecode"] += os.path.getsize(bc)
                for c in (1, 3):
                    o = os.path.join(tmp, f"c{c}")
                    os.makedirs(o, exist_ok=True)
                    subprocess.run([MKASSET, "-c", str(c), "-w", "256", "-o", o, bc], check=True,
                                   stdout=subprocess.DEVNULL)
                    acc[f"c{c}"] += os.path.getsize(os.path.join(o, "x.luac"))
                    os.remove(os.path.join(o, "x.luac"))
    for g, a in sorted(groups.items(), key=lambda kv: -kv[1]["fuente"]):
        print(f"{g:28s} {a['ficheros']:4d} fuente {a['fuente']:9d} bytecode {a['bytecode']:9d} "
              f"c1 {a['c1']:9d} c3 {a['c3']:9d}")
    json.dump(groups, open(out, "w"), indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
