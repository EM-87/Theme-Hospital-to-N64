#!/usr/bin/env python3
"""Música de Theme Hospital en la N64: MIDI + SoundFont frente a audio (fase 2).

Uso: musica.py <dir_trabajo> <salida.json> [soundfont.sf2]
  (lee <dir_trabajo>/raw/SOUND/MIDI/*.XMI; deja los MIDI en <dir_trabajo>/mid/)

1. Convierte cada XMI a MIDI estándar con xmi2mid.cpp, que enlaza el mismo
   conversor que usa CorsixTH (CorsixTH/Src/xmi2mid.cpp). Se compila en
   <dir_trabajo>/bin/ si no existe.
2. Lee cada MIDI: duración (con su mapa de tempo), programas General MIDI
   usados, notas de percusión y polifonía máxima (notas sonando a la vez).
3. Mide en cartucho las dos vías de libdragon:
   a) MID64 + SF64 (audioconv64; el SoundFont por defecto es TimGM6mb, GPL);
   b) audio renderizado con fluidsynth y convertido a WAV64 VADPCM y Opus.
"""
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
N64 = os.environ.get("N64_INST", "/opt/libdragon")
AUDIOCONV = os.path.join(N64, "bin/audioconv64")
CORSIXTH = os.environ.get("CORSIXTH_DIR", os.path.expanduser("~/th64-work/src/CorsixTH"))
SF2 = "/usr/share/sounds/sf2/TimGM6mb.sf2"


def build_xmi2mid(work):
    exe = os.path.join(work, "bin", "xmi2mid")
    if not os.path.exists(exe):
        os.makedirs(os.path.dirname(exe), exist_ok=True)
        cfg = os.path.join(CORSIXTH, "build/notracy/CorsixTH/Src")
        subprocess.run(["g++", "-std=c++17", "-O2", "-I", os.path.join(CORSIXTH, "CorsixTH/Src"),
                        "-I", cfg, "-o", exe, os.path.join(HERE, "xmi2mid.cpp"),
                        os.path.join(CORSIXTH, "CorsixTH/Src/xmi2mid.cpp")], check=True)
    return exe


def varlen(d, i):
    v = 0
    while True:
        b = d[i]
        i += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80:
            return v, i


def parse_midi(path):
    d = open(path, "rb").read()
    _, ntrk, div = struct.unpack(">HHH", d[8:14])
    i, events = 14, []
    for _ in range(ntrk):
        ln = struct.unpack(">I", d[i + 4:i + 8])[0]
        j, end, t, status = i + 8, i + 8 + ln, 0, 0
        while j < end:
            dt, j = varlen(d, j)
            t += dt
            b = d[j]
            if b & 0x80:
                status = b
                j += 1
            if status == 0xFF:
                typ = d[j]
                ln2, j = varlen(d, j + 1)
                if typ == 0x51:
                    events.append((t, "tempo", int.from_bytes(d[j:j + 3], "big")))
                j += ln2
            elif status in (0xF0, 0xF7):
                ln2, j = varlen(d, j)
                j += ln2
            else:
                hi, ch = status & 0xF0, status & 0x0F
                n = 1 if hi in (0xC0, 0xD0) else 2
                a = d[j:j + n]
                j += n
                if hi == 0xC0:
                    events.append((t, "prog", (ch, a[0])))
                elif hi == 0x90 and a[1] > 0:
                    events.append((t, "on", (ch, a[0])))
                elif hi == 0x80 or (hi == 0x90 and a[1] == 0):
                    events.append((t, "off", (ch, a[0])))
        i = end
    events.sort(key=lambda e: (e[0], e[1] != "off"))
    tempo, last_t, secs = 500000, 0, 0.0
    progs, drums, sounding, poly = set(), set(), set(), 0
    for t, kind, v in events:
        secs += (t - last_t) * tempo / div / 1e6
        last_t = t
        if kind == "tempo":
            tempo = v
        elif kind == "prog" and v[0] != 9:
            progs.add(v[1])
        elif kind == "on":
            if v[0] == 9:
                drums.add(v[1])
            sounding.add(v)
            poly = max(poly, len(sounding))
        elif kind == "off":
            sounding.discard(v)
    return {"segundos": round(secs, 1), "programas": sorted(progs), "percusion": sorted(drums),
            "polifonia_max": poly}


def files(d):
    # audioconv64 no recoge los .MID al recorrer un directorio: se pasan uno a uno.
    return [os.path.join(d, f) for f in sorted(os.listdir(d))]


def dir_size(d):
    return sum(os.path.getsize(os.path.join(d, f)) for f in os.listdir(d))


def main():
    work, out = sys.argv[1], sys.argv[2]
    sf2 = sys.argv[3] if len(sys.argv) > 3 else SF2
    exe = build_xmi2mid(work)
    src = os.path.join(work, "raw", "SOUND", "MIDI")
    mid = os.path.join(work, "mid")
    os.makedirs(mid, exist_ok=True)
    songs = {}
    for f in sorted(os.listdir(src)):
        if not f.endswith(".XMI"):
            continue
        dst = os.path.join(mid, f[:-4] + ".MID")
        subprocess.run([exe, os.path.join(src, f), dst], check=True)
        songs[f[:-4]] = {"xmi": os.path.getsize(os.path.join(src, f)), "mid": os.path.getsize(dst),
                         **parse_midi(dst)}
    res = {"canciones": songs}
    progs = sorted(set().union(*(s["programas"] for s in songs.values())))
    drums = sorted(set().union(*(s["percusion"] for s in songs.values())))
    res["programas_usados"] = progs
    res["notas_percusion_usadas"] = drums
    res["segundos_total"] = round(sum(s["segundos"] for s in songs.values()), 1)
    with tempfile.TemporaryDirectory() as tmp:
        o = os.path.join(tmp, "mid64")
        os.makedirs(o)  # audioconv64 no crea el directorio de salida
        subprocess.run([AUDIOCONV, "-o", o, *files(mid)], check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
        res["mid64"] = dir_size(o)
        o = os.path.join(tmp, "sf64")
        os.makedirs(o)
        subprocess.run([AUDIOCONV, "-o", o, sf2], check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
        res["soundfont"] = {"sf2": os.path.basename(sf2), "bytes": os.path.getsize(sf2), "sf64": dir_size(o)}
        if shutil.which("fluidsynth"):
            wav = os.path.join(tmp, "wav")
            os.makedirs(wav)
            for name in songs:
                subprocess.run(["fluidsynth", "-ni", "-q", "-r", "32000", "-F",
                                os.path.join(wav, name + ".wav"), sf2, os.path.join(mid, name + ".MID")],
                               check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            r = {"wav_32k_estereo": dir_size(wav)}
            for v, flags in {"vadpcm_estereo_32k": ["--wav-compress", "1"],
                             "vadpcm_mono_22k": ["--wav-compress", "1", "--wav-mono", "--wav-resample", "22050"],
                             "opus_estereo_32k": ["--wav-compress", "3"]}.items():
                o = os.path.join(tmp, v)
                os.makedirs(o)
                subprocess.run([AUDIOCONV, *flags, "-o", o, *files(wav)], check=True, stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL)
                r[v] = dir_size(o)
            res["renderizado"] = r
    for n, s in songs.items():
        print(n, s)
    print({k: v for k, v in res.items() if k != "canciones"})
    json.dump(res, open(out, "w"), indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
