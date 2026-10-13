#!/usr/bin/env python3
"""Compara volcados del oráculo (volcado.jsonl).

Uso: python3 -I comparar.py <ref.jsonl> <otro.jsonl> [...]
Para cada fichero frente al de referencia: primera hora con diferencia en cada
campo (o "igual") y diferencias al final.
"""
import json
import sys


def load(p):
    return [json.loads(l) for l in open(p)]


def main():
    ref = load(sys.argv[1])
    for p in sys.argv[2:]:
        o = load(p)
        n = min(len(ref), len(o))
        first = {}
        for i in range(n):
            for k in ref[i]:
                if k not in first and ref[i][k] != o[i].get(k):
                    first[k] = ref[i]["hora"]
        print(f"== {p} ({n} horas comparadas)")
        if not first:
            print("   idéntico en todas las horas y campos")
            continue
        print("   primera divergencia por campo (hora):", dict(sorted(first.items(), key=lambda x: x[1])))
        a, b = ref[n - 1], o[n - 1]
        keys = ["balance", "reputation", "num_visitors", "num_cured", "not_cured", "num_deaths",
                "patients", "staff", "queue_total", "value"]
        print("   al final:", {k: (a.get(k), b.get(k)) for k in keys})


if __name__ == "__main__":
    main()
