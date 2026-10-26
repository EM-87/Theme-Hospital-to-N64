#!/usr/bin/env python3
"""Compara la música sintetizada de dos formas (WAV de 16 bits, la misma
frecuencia de muestreo): nivel, brillo (centroide espectral), reparto de la
energía por bandas, distancia de timbre (diferencia media del espectro a
largo plazo por tercios de octava, con el nivel igualado) y parecido de la
dinámica (correlación de las envolventes de 50 ms). Antes alinea la segunda
con la primera por correlación de envolventes, buscando hasta ±2 s, y mide
la deriva (cómo cambia el desfase entre los 30 primeros segundos y los 30
últimos; se mide en pasos de 10 ms).

Uso: python3 -I tools/musica/comparar.py <a.wav> <b.wav> [--nombres a,b] [--ab salida.wav [--desde s] [--segundos s]]
Escribe una línea por fichero y una de comparación. Con --ab escribe además
un WAV para escuchar: el fragmento de b (por defecto, los primeros 25 s), un
segundo de silencio y el mismo fragmento de a, alineados y con el mismo nivel.
"""
import sys
import wave

import numpy as np

BANDAS = [(0, 250), (250, 1000), (1000, 4000), (4000, 11025), (11025, 24000)]


def leer(ruta):
    with wave.open(ruta, "rb") as w:
        sr, ch, n = w.getframerate(), w.getnchannels(), w.getnframes()
        x = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float64) / 32768
    return x.reshape(-1, ch).mean(axis=1), sr


def envolvente(x, sr, hop_s=0.01, ventana_s=0.05):
    hop, win = int(sr * hop_s), int(sr * ventana_s)
    n = (len(x) - win) // hop
    c = np.concatenate([[0], np.cumsum(x * x)])
    i = np.arange(n) * hop
    return np.sqrt((c[i + win] - c[i]) / win)


def alinear(a, b, sr, max_s=2.0):
    """Desfase (en muestras) que hay que quitar a b para que case con a."""
    ea, eb = envolvente(a, sr), envolvente(b, sr)
    n = min(len(ea), len(eb))
    ea, eb = ea[:n] - ea[:n].mean(), eb[:n] - eb[:n].mean()
    m = int(max_s / 0.01)
    mejores = [(np.dot(ea[max(0, -k):n - max(0, k)], eb[max(0, k):n - max(0, -k)]), k) for k in range(-m, m + 1)]
    return max(mejores)[1] * int(sr * 0.01)


def espectro(x, sr, n=4096):
    win = np.hanning(n)
    tramas = [x[i:i + n] * win for i in range(0, len(x) - n, n // 2)]
    s = np.abs(np.fft.rfft(np.array(tramas), axis=1)) ** 2
    rms = np.sqrt(np.mean(np.array(tramas) ** 2, axis=1))
    activas = rms > 10 ** (-50 / 20)
    return s[activas], np.fft.rfftfreq(n, 1 / sr)


def medidas(x, sr):
    s, f = espectro(x, sr)
    total = s.sum(axis=1)
    centroide = float(np.sum((s * f).sum(axis=1)) / total.sum())
    larga = s.mean(axis=0)
    bandas = [larga[(f >= lo) & (f < hi)].sum() / larga.sum() for lo, hi in BANDAS]
    nivel = 20 * np.log10(np.sqrt(np.mean(x * x)) + 1e-12)
    return {"nivel": nivel, "centroide": centroide, "bandas": bandas, "larga": larga, "f": f}


def tercios(larga, f):
    centros = 1000 * 2 ** (np.arange(-10, 11) / 3)  # 100 Hz – 10 kHz
    res = []
    for c in centros:
        m = (f >= c / 2 ** (1 / 6)) & (f < c * 2 ** (1 / 6))
        res.append(larga[m].sum())
    return np.array(res)


def main():
    args = sys.argv[1:]
    opciones = {}
    for op in ("--nombres", "--ab", "--desde", "--segundos"):
        if op in args:
            k = args.index(op)
            opciones[op] = args[k + 1]
            del args[k:k + 2]
    nombres = opciones["--nombres"].split(",") if "--nombres" in opciones else None
    if len(args) != 2:
        sys.exit(__doc__)
    (a, sra), (b, srb) = leer(args[0]), leer(args[1])
    if sra != srb:
        sys.exit("frecuencias distintas")
    d = alinear(a, b, sra)
    b = b[d:] if d >= 0 else np.concatenate([np.zeros(-d), b])
    n = min(len(a), len(b))
    a, b = a[:n], b[:n]
    # Deriva: cuánto cambia el desfase entre los primeros 30 s y los últimos.
    v = 30 * sra
    deriva = (alinear(a[-v:], b[-v:], sra, 0.5) - alinear(a[:v], b[:v], sra, 0.5)) / sra * 1000 if n > 2 * v else 0
    ma, mb = medidas(a, sra), medidas(b, sra)
    nombres = nombres or args
    for nom, m in zip(nombres, (ma, mb)):
        print(f"{nom}: nivel {m['nivel']:.1f} dB; centroide {m['centroide']:.0f} Hz; bandas "
              + " ".join(f"{100 * v:.1f}" for v in m["bandas"]))
    ta, tb = tercios(ma["larga"], ma["f"]), tercios(mb["larga"], mb["f"])
    ta, tb = ta / ta.sum(), tb / tb.sum()
    dist = float(np.mean(np.abs(10 * np.log10(ta + 1e-15) - 10 * np.log10(tb + 1e-15))))
    ea, eb = envolvente(a, sra, 0.05, 0.05), envolvente(b, sra, 0.05, 0.05)
    corr = float(np.corrcoef(ea, eb)[0, 1])
    print(f"comparación: desfase {d / sra * 1000:.0f} ms, deriva {deriva:+.0f} ms; nivel {mb['nivel'] - ma['nivel']:+.1f} dB; centroide "
          f"{mb['centroide'] - ma['centroide']:+.0f} Hz; distancia de timbre {dist:.1f} dB; "
          f"correlación de envolventes {corr:.3f}")
    if "--ab" in opciones:
        i0 = int(float(opciones.get("--desde", 0)) * sra)
        i1 = i0 + int(float(opciones.get("--segundos", 25)) * sra)
        fa, fb = a[i0:i1], b[i0:i1]
        # El mismo nivel (RMS) en los dos, a -20 dB.
        fa = fa * 10 ** (-20 / 20) / (np.sqrt(np.mean(fa * fa)) + 1e-12)
        fb = fb * 10 ** (-20 / 20) / (np.sqrt(np.mean(fb * fb)) + 1e-12)
        y = np.concatenate([fb, np.zeros(sra), fa])
        with wave.open(opciones["--ab"], "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(sra)
            w.writeframes((np.clip(y, -1, 1) * 32767).astype("<i2").tobytes())


if __name__ == "__main__":
    main()
