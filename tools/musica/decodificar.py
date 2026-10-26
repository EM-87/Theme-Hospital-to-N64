#!/usr/bin/env python3
"""Rehace los WAV de la ROM n64/pruebas/musica a partir de su log de ares:
cada línea «P <base64>» es un bloque de audio de la N64 (PCM de 16 bits,
estéreo, big-endian) tal como lo mezcla la consola, y las líneas «TH64
musica: <pieza> empieza/acaba» separan las piezas. Escribe <pieza>_n64.wav a
la frecuencia de la N64 (22.047 Hz) y comprueba que no falte ningún bloque.

Uso: python3 -I tools/musica/decodificar.py <isviewer.log> <dir_salida>
"""
import base64
import math
import re
import sys
import wave

import numpy as np


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    sr = bloque = None
    pieza, dur, partes = None, 0, []
    errores = 0
    with open(sys.argv[1], "rb") as f:
        for linea in f:
            if linea.startswith(b"P "):
                if pieza is None:
                    continue
                datos = base64.b64decode(linea[2:].strip())
                if len(datos) != bloque * 4:
                    errores += 1
                partes.append(datos)
                continue
            texto = linea.decode("utf-8", "replace")
            if m := re.search(r"TH64 musica: \d+ presets; (\d+) Hz, bloques de (\d+) muestras", texto):
                sr, bloque = int(m[1]), int(m[2])
            elif m := re.search(r"TH64 musica: (\w+) empieza \((\d+) ms\)", texto):
                pieza, dur, partes = m[1], int(m[2]), []
            elif m := re.search(r"TH64 musica: (\w+) acaba", texto):
                x = np.frombuffer(b"".join(partes), dtype=">i2").reshape(-1, 2)
                previstos = math.ceil((dur + 2000) * sr // 1000 / bloque)
                with wave.open(f"{sys.argv[2]}/{pieza}_n64.wav", "wb") as w:
                    w.setnchannels(2)
                    w.setsampwidth(2)
                    w.setframerate(sr)
                    w.writeframes(x.astype("<i2").tobytes())
                rms = 20 * np.log10(np.sqrt(np.mean((x.astype(np.float64) / 32768) ** 2)) + 1e-12)
                pico = np.abs(x.astype(np.int32)).max()
                print(f"{pieza}: {len(partes)} bloques de {previstos}; {len(x) / sr:.1f} s; {rms:.1f} dB; "
                      f"pico {pico}" + (" (SATURA)" if pico >= 32767 else ""))
                if len(partes) != previstos:
                    errores += 1
                pieza = None
    if errores:
        sys.exit(f"{errores} bloques o piezas incompletos")


if __name__ == "__main__":
    main()
