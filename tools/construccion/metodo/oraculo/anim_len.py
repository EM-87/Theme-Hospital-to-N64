#!/usr/bin/env python3
"""Duración de cada animación calculada desde los ficheros originales
(VSTART-1.ANI y VFRA-1.ANI ya descomprimidos), como lo haría el conversor.
Compara con la salida de sonda_anim.lua.
Uso: python3 -I anim_len.py <START.bin> <FRA.bin> <anim_len.txt de CorsixTH>"""
import struct
import sys

start = open(sys.argv[1], "rb").read()
fra = open(sys.argv[2], "rb").read()
n_anim, n_frame = len(start) // 4, len(fra) // 10
first = [struct.unpack_from("<H", start, 4 * i)[0] for i in range(n_anim)]
nxt = [struct.unpack_from("<H", fra, 10 * i + 8)[0] for i in range(n_frame)]
mine = []
for i in range(n_anim):
    f = first[i] if first[i] <= n_frame else 0
    seen, length = set(), 0
    while f not in seen:
        seen.add(f)
        length += 1
        f = nxt[f] if nxt[f] < n_frame else 0
    mine.append(length)
ref = {int(a): int(b) for a, b in (l.split() for l in open(sys.argv[3]))}
diff = [(i, mine[i], ref[i]) for i in range(n_anim) if ref.get(i) != mine[i]]
print(f"animaciones {n_anim}, fotogramas {n_frame}; iguales {n_anim - len(diff)}, distintas {len(diff)}")
for d in diff[:10]:
    print(f"  anim {d[0]}: ficheros {d[1]}, CorsixTH {d[2]}")
