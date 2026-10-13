"""Distribución de tamaños de los sprites de DATAM y encaje en TMEM (CI8/CI4).
Uso: python3 -I tmem_stats.py <TH_DATA_DIR> <salida.json>"""
import json, os, struct, sys, statistics
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "datos"))
import graficos  # decode(), del repo (solo lectura)

data_dir, out = sys.argv[1], sys.argv[2]
TMEM_CI8 = 2048   # rdpq_tex.c:190: CI4/CI8 usan la mitad baja (la alta es la TLUT)
TMEM_CI4 = 2048

def pitch(w, bpp):  # rdpq_tex.c:137: ROUND_UP(bytes por fila, 8)
    b = (w * bpp + 7) // 8
    return (b + 7) // 8 * 8

def pct(v, p):
    v = sorted(v); k = (len(v) - 1) * p
    lo = int(k); hi = min(lo + 1, len(v) - 1)
    return v[lo] + (v[hi] - v[lo]) * (k - lo)

res = {}
allsp = []
for name in ("MSPR-0", "MBLK-0"):
    tab = open(os.path.join(data_dir, "DATAM", name + ".TAB"), "rb").read()
    dat = open(os.path.join(data_dir, "DATAM", name + ".DAT"), "rb").read()
    sp = []
    for k in range(0, len(tab) - 5, 6):
        off, w, h = struct.unpack_from("<IBB", tab, k)
        if not w or not h:
            continue
        px = graficos.decode(dat[off:], w, h, False)
        ncol = len(set(px) - {0xFF})
        # recorte al rectángulo opaco (bbox)
        xs = [i % w for i, p in enumerate(px) if p != 0xFF]
        ys = [i // w for i, p in enumerate(px) if p != 0xFF]
        if xs:
            bw, bh = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
        else:
            bw, bh = 0, 0
        sp.append(dict(i=k // 6, w=w, h=h, ncol=ncol, bw=bw, bh=bh))
    allsp += sp
    res[name] = sp

def summary(sp):
    W = [s["w"] for s in sp]; H = [s["h"] for s in sp]; A = [s["w"] * s["h"] for s in sp]
    ci8 = [pitch(s["w"], 8) * s["h"] for s in sp]
    over8 = [s for s, b in zip(sp, ci8) if b > TMEM_CI8]
    ci4fit = [s for s in sp if s["ncol"] <= 15]
    over4 = [s for s in ci4fit if pitch(s["w"], 4) * s["h"] > TMEM_CI4]
    # tiras que hacen falta: rdpq_tex.c tex_loader_calc_max_height = 2048 / pitch
    strips = [-(-s["h"] // (TMEM_CI8 // pitch(s["w"], 8))) for s in sp]
    # LOAD_BLOCK posible solo si stride == ancho en bytes y múltiplo de 8 (rdpq_tex.c:144)
    w8 = sum(1 for s in sp if s["w"] % 8 == 0)
    # bbox recortado
    ci8b = [pitch(s["bw"], 8) * s["bh"] for s in sp if s["bw"]]
    over8b = sum(1 for b in ci8b if b > TMEM_CI8)
    return dict(
        sprites=len(sp),
        ancho=dict(mediana=statistics.median(W), p90=pct(W, .9), max=max(W)),
        alto=dict(mediana=statistics.median(H), p90=pct(H, .9), max=max(H)),
        area=dict(mediana=statistics.median(A), p90=pct(A, .9), max=max(A)),
        max_wxh=max(((s["w"], s["h"]) for s in sp), key=lambda t: t[0] * t[1]),
        ci8_bytes_tmem=dict(mediana=statistics.median(ci8), p90=pct(ci8, .9), max=max(ci8)),
        ci8_no_caben=len(over8),
        ci8_no_caben_pct=round(100 * len(over8) / len(sp), 1),
        ci8_pixeles_en_no_caben=sum(s["w"] * s["h"] for s in over8),
        pixeles_total=sum(A),
        tiras_total=sum(strips), tiras_max=max(strips),
        tiras_hist={n: strips.count(n) for n in sorted(set(strips))},
        ci4_posibles=len(ci4fit), ci4_no_caben=len(over4),
        ancho_multiplo_8=w8,
        bbox_ci8_no_caben=over8b,
        ejemplos_no_caben=[(s["i"], s["w"], s["h"]) for s in sorted(over8, key=lambda s: -s["w"] * s["h"])[:8]],
    )

outj = {k: summary(v) for k, v in res.items()}
outj["total"] = summary(allsp)
json.dump(outj, open(out, "w"), indent=1)
for k, v in outj.items():
    print(k, json.dumps(v))
