#!/usr/bin/env python3
"""Genera compile_commands.json a partir de `make -nB` (sin bear ni compiledb).

Uso: python3 -I gen_ccdb.py <dir del proyecto> <salida.json> [args de make...]
Toma cada línea de `make -nB ...` que invoca un compilador con -c y anota el
fichero fuente (el argumento que termina en .c/.cpp/.S tras -c ... -o x $<).
"""
import json
import os
import shlex
import subprocess
import sys

SRC_EXT = (".c", ".cc", ".cpp", ".S", ".s")


def main():
    root, outp, margs = os.path.abspath(sys.argv[1]), sys.argv[2], sys.argv[3:]
    txt = subprocess.run(["make", "-nB", *margs], cwd=root, capture_output=True, text=True,
                         check=True).stdout
    entries = []
    for line in txt.splitlines():
        line = line.strip()
        if not line or " -c " not in f" {line} ":
            continue
        try:
            argv = shlex.split(line)
        except ValueError:
            continue
        if not argv or not os.path.basename(argv[0]).endswith(("gcc", "g++", "cc", "clang", "clang++")):
            continue
        srcs = [a for a in argv[1:] if a.endswith(SRC_EXT) and not a.startswith("-")]
        if not srcs:
            continue
        entries.append({"directory": root, "arguments": argv, "file": os.path.join(root, srcs[-1])})
    json.dump(entries, open(outp, "w"), indent=1)
    print(f"{len(entries)} entradas en {outp}")


if __name__ == "__main__":
    main()
