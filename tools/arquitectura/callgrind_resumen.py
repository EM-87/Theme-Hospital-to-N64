#!/usr/bin/env python3
"""Resumen de un callgrind.out de la simulación de CorsixTH (fase 3).

Uso: callgrind_resumen.py <callgrind.out> <salida.json> [horas]

Lee el fichero de callgrind (solo el evento Ir, instrucciones ejecutadas) y
reparte el coste propio de cada función:
  - por objeto (biblioteca o ejecutable),
  - para el ejecutable de CorsixTH, por fichero fuente,
  - y lista las funciones más caras.
Los costes de las líneas «calls=» son inclusivos de la función llamada y no
se suman al coste propio de quien llama.
"""
import json
import re
import sys
from collections import defaultdict


def main():
    path, out = sys.argv[1], sys.argv[2]
    hours = float(sys.argv[3]) if len(sys.argv) > 3 else None
    names = {"ob": {}, "fl": {}, "fn": {}}
    cur = {"ob": "?", "fl": "?", "fn": "?"}
    by_fn = defaultdict(int)
    skip_next = False
    ref = re.compile(r"^(ob|fl|fi|fe|fn|cob|cfi|cfl|cfn)=(?:\((\d+)\))?\s*(.*)$")
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.rstrip("\n")
        m = ref.match(line)
        if m:
            kind, num, name = m.groups()
            base = {"fi": "fl", "fe": "fl", "cfi": "fl", "cfl": "fl", "cob": "ob", "cfn": "fn"}.get(kind, kind)
            table = names[base]
            if num and name:
                table[num] = name
            resolved = table.get(num, name) if num else name
            if kind in ("ob", "fl", "fi", "fe", "fn"):
                cur["fl" if kind in ("fi", "fe") else kind] = resolved
            continue
        if line.startswith("calls="):
            skip_next = True
            continue
        if line and (line[0].isdigit() or line[0] in "+-*"):
            if skip_next:
                skip_next = False
                continue
            parts = line.split()
            if len(parts) >= 2:
                by_fn[(cur["ob"], cur["fl"], cur["fn"])] += int(parts[1])
    total = sum(by_fn.values())
    by_ob, by_file = defaultdict(int), defaultdict(int)
    for (ob, fl, fn), c in by_fn.items():
        obn = ob.split("/")[-1]
        by_ob[obn] += c
        if obn == "corsix-th":
            by_file[fl.split("/Src/")[-1].split("/")[-1]] += c
    top = sorted(by_fn.items(), key=lambda kv: -kv[1])[:60]
    res = {
        "instrucciones": total,
        "horas": hours,
        "instrucciones_por_hora": total / hours if hours else None,
        "por_objeto": dict(sorted(by_ob.items(), key=lambda kv: -kv[1])),
        "corsixth_por_fichero": dict(sorted(by_file.items(), key=lambda kv: -kv[1])),
        "funciones": [{"objeto": ob.split("/")[-1], "fichero": fl.split("/")[-1], "funcion": fn, "ir": c}
                      for (ob, fl, fn), c in top],
    }
    print(f"{total:,} instrucciones" + (f", {total / hours:,.0f} por hora de juego" if hours else ""))
    for k, v in list(res["por_objeto"].items())[:12]:
        print(f"  {100 * v / total:5.1f}% {k}")
    print("CorsixTH por fichero:")
    for k, v in list(res["corsixth_por_fichero"].items())[:12]:
        print(f"  {100 * v / total:5.1f}% {k}")
    print("funciones:")
    for f in res["funciones"][:25]:
        print(f"  {100 * f['ir'] / total:5.1f}% {f['objeto']}: {f['funcion'][:90]}")
    json.dump(res, open(out, "w"), indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
