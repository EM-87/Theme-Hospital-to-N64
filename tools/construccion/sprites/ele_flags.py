"""Cuenta flags de los elementos de animación de DATAM (MELE/MLIST/MFRA).
Uso: python3 -I ele_flags.py <TH_DATA_DIR>"""
import os, struct, sys, collections
d = os.path.join(sys.argv[1], "DATAM")
ele = open(os.path.join(d, "MELE-1.ANI"), "rb").read()
lst = open(os.path.join(d, "MLIST-1.ANI"), "rb").read()
fra = open(os.path.join(d, "MFRA-1.ANI"), "rb").read()
tab = open(os.path.join(d, "MSPR-0.TAB"), "rb").read()
nspr = len(tab) // 6
ne = len(ele) // 6
flags = collections.Counter(); layers = collections.Counter()
spr_flip = set(); spr_noflip = set()
for i in range(ne):
    tp, x, y, lf, lid = struct.unpack_from("<HBBBB", ele, i * 6)
    f = lf & 0xF; flags[f] += 1; layers[lf >> 4] += 1
    s = tp // 6
    (spr_flip if f & 1 else spr_noflip).add(s)
print("elementos", ne)
print("flags (bit0 flipH, bit1 flipV, bit2 a50, bit3 a75):", dict(sorted(flags.items())))
print("elementos con flipH:", sum(v for k, v in flags.items() if k & 1),
      "flipV:", sum(v for k, v in flags.items() if k & 2),
      "alpha50:", sum(v for k, v in flags.items() if k & 4),
      "alpha75:", sum(v for k, v in flags.items() if k & 8))
print("sprites usados volteados:", len(spr_flip), "sin voltear:", len(spr_noflip),
      "en ambos:", len(spr_flip & spr_noflip), "solo volteados:", len(spr_flip - spr_noflip), "de", nspr)
# referencias por fotograma
nf = len(fra) // 10
L = [struct.unpack_from("<H", lst, 2 * k)[0] for k in range(len(lst) // 2)]
per_frame = []
fflags = collections.Counter()
for f in range(nf):
    li, w, h, snd, fl, nxt = struct.unpack_from("<IBBBBH", fra, f * 10)
    fflags[fl] += 1
    n = 0; k = li
    while k < len(L) and L[k] < ne:
        n += 1; k += 1
    per_frame.append(n)
per_frame.sort()
print("fotogramas", nf, "elementos/fotograma mediana", per_frame[len(per_frame)//2],
      "p90", per_frame[int(.9*(len(per_frame)-1))], "max", per_frame[-1])
print("flags de fotograma:", dict(sorted(fflags.items())))
print("capas:", dict(sorted(layers.items())))
