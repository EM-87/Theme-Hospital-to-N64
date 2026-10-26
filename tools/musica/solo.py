#!/usr/bin/env python3
"""Separa un MIDI (SMF) en un fichero por canal: cada <prefijo>_cNN.mid
conserva todos los sucesos (tempo, programas, controladores) pero solo las
notas del canal NN. Sirve para medir cada instrumento por separado en la N64
y en fluidsynth.

Uso: python3 -I tools/musica/solo.py <entrada.mid> <prefijo_salida>
"""
import struct
import sys


def vlq(d, j):
    v = 0
    while True:
        b = d[j]
        j += 1
        v = v << 7 | b & 127
        if b < 128:
            return v, j


def a_vlq(v):
    b = [v & 127]
    v >>= 7
    while v:
        b.append(v & 127 | 128)
        v >>= 7
    return bytes(reversed(b))


def pistas(d):
    """Cada pista como lista de (tiempo absoluto, bytes del suceso con estado explícito)."""
    ntr = struct.unpack(">H", d[10:12])[0]
    i, res = 14, []
    for _ in range(ntr):
        assert d[i:i + 4] == b"MTrk"
        n = struct.unpack(">I", d[i + 4:i + 8])[0]
        j, fin, run, t, ev = i + 8, i + 8 + n, 0, 0, []
        while j < fin:
            dt, j = vlq(d, j)
            t += dt
            ini = j
            st = d[j]
            if st == 0xFF:
                ln, k = vlq(d, j + 2)
                j = k + ln
                ev.append((t, d[ini:j]))
                continue
            if st in (0xF0, 0xF7):
                ln, k = vlq(d, j + 1)
                j = k + ln
                ev.append((t, d[ini:j]))
                continue
            if st & 0x80:
                run = st
                j += 1
            n_datos = 1 if run >> 4 in (0xC, 0xD) else 2
            ev.append((t, bytes([run]) + d[j:j + n_datos]))
            j += n_datos
        res.append(ev)
        i = fin
    return res


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    d = open(sys.argv[1], "rb").read()
    trs = pistas(d)
    canales = sorted({e[0] & 15 for tr in trs for _, e in tr if e[0] >> 4 == 9})
    for c in canales:
        salida = bytearray(d[:14])
        for tr in trs:
            cuerpo, ant = bytearray(), 0
            for t, e in tr:
                if e[0] >> 4 in (8, 9) and e[0] & 15 != c:
                    continue
                cuerpo += a_vlq(t - ant) + e
                ant = t
            salida += b"MTrk" + struct.pack(">I", len(cuerpo)) + cuerpo
        open(f"{sys.argv[2]}_c{c:02d}.mid", "wb").write(salida)
        print(f"{sys.argv[2]}_c{c:02d}.mid")


if __name__ == "__main__":
    main()
