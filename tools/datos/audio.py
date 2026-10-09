#!/usr/bin/env python3
"""Tamaño del audio convertido a WAV64 de libdragon (fase 2).

Uso: audio.py <dir_trabajo> <sonido.json> <salida.json>
  (lee los WAV que extrae sonido.py en <dir_trabajo>/wav/<idioma>/)

Convierte con audioconv64 dos conjuntos por separado, porque en la N64 se
tratarían distinto:
  - efectos: los sonidos idénticos en todos los idiomas (se guardan una vez);
  - locutor: los propios de cada idioma (uno por idioma incluido).
Variantes: VADPCM (--wav-compress 1, la que decodifica el RSP sin coste de
CPU apreciable), VADPCM remuestreado a 11025 Hz y Opus (--wav-compress 3).
Idiomas de SOUND-x.DAT según languages/*.lua de CorsixTH: 0 inglés, 1 francés,
2 alemán, 3 italiano, 4 español, 5 sueco.
"""
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile

AUDIOCONV = os.path.join(os.environ.get("N64_INST", "/opt/libdragon"), "bin/audioconv64")
LANGS = {"0": "inglés", "1": "francés", "2": "alemán", "3": "italiano", "4": "español", "5": "sueco"}
VARIANTS = {
    "vadpcm": ["--wav-compress", "1"],
    "vadpcm_11025": ["--wav-compress", "1", "--wav-resample", "11025"],
    "opus": ["--wav-compress", "3"],
}


def has_samples(path):
    # NULL.WAV son 2 muestras de silencio y hacen abortar al compresor VADPCM de
    # audioconv64 (todas las muestras iguales); no hace falta en la N64.
    b = open(path, "rb").read()
    i = b.find(b"data")
    pcm = b[i + 8:i + 8 + struct.unpack_from("<I", b, i + 4)[0]] if i >= 0 else b""
    return len(set(pcm)) > 1


def convert(src_dir, names, flags, tmp):
    inp, out = os.path.join(tmp, "in"), os.path.join(tmp, "out")
    for d in (inp, out):
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
    for n in names:
        os.symlink(os.path.join(src_dir, n), os.path.join(inp, n))
    subprocess.run([AUDIOCONV, *flags, "-o", out, inp], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return sum(os.path.getsize(os.path.join(out, f)) for f in os.listdir(out))


def main():
    work, sjson, out = sys.argv[1], sys.argv[2], sys.argv[3]
    snd = json.load(open(sjson))["sonidos"]
    shared = {s["nombre"] for s in snd if s["compartido"]}
    res = {}
    with tempfile.TemporaryDirectory() as tmp:
        wav0 = os.path.join(work, "wav", "0")
        names = sorted(n for n in shared if has_samples(os.path.join(wav0, n)))
        r = {"sonidos": len(names), "wav": sum(os.path.getsize(os.path.join(wav0, n)) for n in names)}
        for v, flags in VARIANTS.items():
            r[v] = convert(wav0, names, flags, tmp)
        res["efectos"] = r
        print("efectos", r, flush=True)
        for lang, lname in LANGS.items():
            d = os.path.join(work, "wav", lang)
            names = sorted(n for n in os.listdir(d)
                           if n not in shared and has_samples(os.path.join(d, n)))
            r = {"sonidos": len(names), "wav": sum(os.path.getsize(os.path.join(d, n)) for n in names)}
            for v, flags in VARIANTS.items():
                r[v] = convert(d, names, flags, tmp)
            res[f"locutor_{lname}"] = r
            print(f"locutor {lname}", r, flush=True)
    json.dump(res, open(out, "w"), indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
