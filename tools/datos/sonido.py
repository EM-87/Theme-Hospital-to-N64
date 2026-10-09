#!/usr/bin/env python3
"""Extrae y censa los sonidos de SOUND/DATA/SOUND-x.DAT (fase 2).

Uso: sonido.py <dir_trabajo> <salida.json>
  (lee <dir_trabajo>/raw/SOUND/DATA/SOUND-*.DAT y escribe los WAV en
   <dir_trabajo>/wav/<idioma>/)

Formato (sound_archive::load_from_th_file de CorsixTH): los 4 últimos bytes
dan la posición de una cabecera de 234 B; en ella, +50 es la posición de la
tabla y +58 su longitud; la tabla tiene entradas de 32 B (nombre de 18 B,
posición u32 en +18, longitud u32 en +26) y cada entrada es un WAV completo.
La entrada 0 es el propio índice y se descarta, como hace CorsixTH.

Para cada sonido se guarda nombre, formato (frecuencia, canales, bits) y
duración, y se marca si su contenido es idéntico en todos los idiomas (efecto
compartido) o cambia entre idiomas (voz del locutor y algún efecto con voz).
"""
import hashlib
import json
import os
import re
import struct
import sys


def read_archive(path):
    d = open(path, "rb").read()
    hp = struct.unpack_from("<I", d, len(d) - 4)[0]
    tpos, tlen = struct.unpack_from("<I", d, hp + 50)[0], struct.unpack_from("<I", d, hp + 58)[0]
    out = []
    for i in range(1, tlen // 32):
        e = tpos + i * 32
        name = d[e:e + 18].split(b"\0")[0].decode("latin-1")
        pos, ln = struct.unpack_from("<I", d, e + 18)[0], struct.unpack_from("<I", d, e + 26)[0]
        out.append((name, d[pos:pos + ln]))
    return out


def wav_chunks(b):
    fmt, pcm = None, b""
    i = 12
    while i + 8 <= len(b):
        cid, cl = b[i:i + 4], struct.unpack_from("<I", b, i + 4)[0]
        if cid == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", b, i + 8)
        elif cid == b"data":
            pcm = b[i + 8:i + 8 + cl]
        i += 8 + cl + (cl & 1)
    return fmt, pcm


def wav_info(b):
    fmt, pcm = wav_chunks(b)
    if not fmt:
        return None
    tag, ch, rate, _, _, bits = fmt
    return {"formato": tag, "canales": ch, "hz": rate, "bits": bits, "datos": len(pcm),
            "segundos": len(pcm) / (rate * ch * bits / 8) if rate and ch and bits else 0}


def main():
    work, out = sys.argv[1], sys.argv[2]
    src = os.path.join(work, "raw", "SOUND", "DATA")
    langs = sorted(f for f in os.listdir(src) if re.match(r"SOUND-\d\.DAT$", f))
    per_lang, hashes = {}, {}
    for f in langs:
        lang = f[6]
        dst = os.path.join(work, "wav", lang)
        os.makedirs(dst, exist_ok=True)
        snd = read_archive(os.path.join(src, f))
        per_lang[lang] = {}
        for name, b in snd:
            info = wav_info(b) or {}
            info["bytes"] = len(b)
            per_lang[lang][name.upper()] = info
            # Se compara solo el formato y las muestras: la cabecera RIFF y los
            # bloques extra cambian entre idiomas aunque el sonido sea el mismo.
            fmt, pcm = wav_chunks(b)
            hashes.setdefault(name.upper(), {})[lang] = hashlib.sha1(repr(fmt).encode() + pcm).hexdigest()
            open(os.path.join(dst, name.upper()), "wb").write(b)
    base = per_lang["0"]
    sounds = []
    for name, info in sorted(base.items()):
        hs = hashes[name]
        shared = len(hs) == len(langs) and len(set(hs.values())) == 1
        sounds.append({"nombre": name, "compartido": shared,
                       "idiomas": sorted(hs), **info})
    summary = {}
    for lang, snd in per_lang.items():
        summary[lang] = {"sonidos": len(snd), "bytes": sum(s["bytes"] for s in snd.values()),
                         "segundos": sum(s.get("segundos", 0) for s in snd.values())}
    sh = [s for s in sounds if s["compartido"]]
    nsh = [s for s in sounds if not s["compartido"]]
    split = {"compartidos": {"sonidos": len(sh), "bytes": sum(s["bytes"] for s in sh),
                             "segundos": sum(s.get("segundos", 0) for s in sh)},
             "por_idioma_ingles": {"sonidos": len(nsh), "bytes": sum(s["bytes"] for s in nsh),
                                   "segundos": sum(s.get("segundos", 0) for s in nsh)}}
    fmts = {}
    for s in sounds:
        k = f"{s.get('hz')} Hz {s.get('bits')} bit {s.get('canales')} can."
        fmts[k] = fmts.get(k, 0) + 1
    for lang, s in summary.items():
        print(f"idioma {lang}: {s['sonidos']} sonidos {s['bytes'] / 1e6:.2f} MB {s['segundos'] / 60:.1f} min")
    print("inglés, compartidos:", split["compartidos"], "\ninglés, propios del idioma:", split["por_idioma_ingles"])
    print("formatos:", fmts)
    json.dump({"idiomas": summary, "reparto": split, "formatos": fmts, "sonidos": sounds},
              open(out, "w"), indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
