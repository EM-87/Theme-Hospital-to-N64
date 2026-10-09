#!/usr/bin/env python3
"""Vídeos de Theme Hospital recodificados para la N64 (fase 2).

Uso: video.py <TH_DATA_DIR> <salida.json>

Lee la cabecera de cada .SMK/.SM2/.SM4 (Smacker) con ffprobe y recodifica
los que usa CorsixTH (movie_player.lua: INTRO.SM4, INTRO/ATTRACT.SMK, LOSE1-6,
AREA01-14V, WINGAME y WINLEVEL) con videoconv64 de libdragon a 320 px de
ancho, calidad 70 (la del ejemplo videoplayer), en MPEG-1 y en H.264.
Los Smacker van a 12-17 fps y MPEG-1 solo admite frecuencias estándar, así que
se fuerza 25 fps (los fotogramas repetidos apenas ocupan). El audio sale como
WAV64 mono a 32 kHz (VADPCM), lo que hace videoconv64 por defecto.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

VIDEOCONV = os.path.join(os.environ.get("N64_INST", "/opt/libdragon"), "bin/videoconv64")
USED = re.compile(r"^(INTRO/INTRO\.SM4|INTRO/ATTRACT\.SMK|ANIMS/(LOSE\d|AREA\d\dV|WINGAME|WINLEVEL)\.SMK)$")
CODECS = ("mpeg1", "h264")


def probe(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries",
                          "format=duration:stream=codec_type,width,height,r_frame_rate,sample_rate,channels",
                          "-of", "json", path], check=True, capture_output=True, text=True).stdout
    d = json.loads(out)
    v = next(s for s in d["streams"] if s["codec_type"] == "video")
    a = next((s for s in d["streams"] if s["codec_type"] == "audio"), None)
    num, den = map(int, v["r_frame_rate"].split("/"))
    return {"segundos": float(d["format"]["duration"]), "ancho": v["width"], "alto": v["height"],
            "fps": round(num / den, 2), "audio": f"{a['sample_rate']} Hz {a['channels']} can." if a else None}


def main():
    src, out = sys.argv[1], sys.argv[2]
    videos = []
    with tempfile.TemporaryDirectory() as tmp:
        for d in ("INTRO", "ANIMS"):
            for f in sorted(os.listdir(os.path.join(src, d))):
                p = os.path.join(src, d, f)
                rel = f"{d}/{f}".upper()
                v = {"video": rel, "bytes": os.path.getsize(p), **probe(p),
                     "usado_por_corsixth": bool(USED.match(rel))}
                if v["usado_por_corsixth"]:
                    for c in CODECS:
                        o = os.path.join(tmp, c)
                        os.makedirs(o, exist_ok=True)
                        subprocess.run([VIDEOCONV, "-o", o, "--codec", c, "--quality", "70", "-r", "25",
                                        "--quick", "--no-progress", p], check=True,
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                        base = os.path.splitext(f)[0]
                        outs = [x for x in os.listdir(o) if os.path.splitext(x)[0] == base]
                        v[c] = sum(os.path.getsize(os.path.join(o, x)) for x in outs if not x.endswith(".wav64"))
                        v["audio_wav64"] = sum(os.path.getsize(os.path.join(o, x)) for x in outs
                                               if x.endswith(".wav64"))
                        for x in outs:
                            os.remove(os.path.join(o, x))
                videos.append(v)
                print(v, flush=True)
    used = [v for v in videos if v["usado_por_corsixth"]]
    summary = {"videos": len(videos), "bytes": sum(v["bytes"] for v in videos),
               "segundos": round(sum(v["segundos"] for v in videos), 1),
               "usados": len(used), "usados_bytes": sum(v["bytes"] for v in used),
               "usados_segundos": round(sum(v["segundos"] for v in used), 1),
               "audio_wav64": sum(v["audio_wav64"] for v in used),
               **{c: sum(v[c] for v in used) for c in CODECS}}
    print(summary)
    json.dump({"resumen": summary, "videos": videos}, open(out, "w"), indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
