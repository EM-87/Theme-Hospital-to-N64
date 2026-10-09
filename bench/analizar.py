#!/usr/bin/env python3
"""Resume la campaña de medición de la fase 1 (tools/bench_medir.sh).

Uso: analizar.py <dir_resultados> [salida.json] [--factor-cpu F]

Para cada partida y versión de Lua junta las repeticiones con Tracy
(tiempos por hora de juego, por tick vacío y por frame; heap de Lua;
entidades) y la pasada de heaptrack (heap de C++ por subsistema).

Ritmo del juego a velocidad Normal: el temporizador de SDL va a 18 ms
(55,6 eventos/s) y cada 3 eventos se simula una hora de juego, así que por
segundo real hay 18,5 horas ("th64_hora") y 37 ticks sin hora ("th64_tick").
Con --factor-cpu se estima el coste en la N64 multiplicando los tiempos.
"""
import csv
import json
import os
import statistics
import sys

EVENTS_PER_S = 1000 / 18
HOURS_PER_S = EVENTS_PER_S / 3
TICKS_PER_S = EVENTS_PER_S - HOURS_PER_S


def pct(values, p):
    if not values:
        return None
    s = sorted(values)
    i = max(0, min(len(s) - 1, int(round(p / 100 * len(s) + 0.5)) - 1))
    return s[i]


def zone_stats(values_ms):
    if not values_ms:
        return None
    return {
        "n": len(values_ms),
        "media_ms": statistics.fmean(values_ms),
        "p50_ms": pct(values_ms, 50),
        "p95_ms": pct(values_ms, 95),
        "p99_ms": pct(values_ms, 99),
        "max_ms": max(values_ms),
    }


def load_zones(path):
    zones = {}
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            zones.setdefault(row["name"], []).append(int(row["exec_time_ns"]) / 1e6)
    return zones


def summarize_rep(rep_dir):
    m = json.load(open(os.path.join(rep_dir, "medida.json")))
    z = load_zones(os.path.join(rep_dir, "zonas.csv"))
    hora = zone_stats(z.get("th64_hora", []))
    tick = zone_stats(z.get("th64_tick", []))
    frame = zone_stats(z.get("th64_frame", []))
    sim_ms_s = HOURS_PER_S * hora["media_ms"] + TICKS_PER_S * (tick["media_ms"] if tick else 0)
    return {
        "hora": hora, "tick": tick, "frame": frame,
        "sim_ms_por_s_normal": sim_ms_s,
        "lua_heap_kb": m["lua_heap_kb"],
        "lua_vivo_kb": [m["start"]["lua_live_kb"], m["finish"]["lua_live_kb"]],
        "entidades_inicio": {k: v for k, v in m["start"]["entities"].items() if k != "by_class"},
        "entidades_fin": {k: v for k, v in m["finish"]["entities"].items() if k != "by_class"},
        "clases_fin": m["finish"]["entities"].get("by_class", {}),
        "pacientes_medios": m.get("mean_patients"),
        "horas": m["hours"],
    }


def combine(reps):
    def agg(getter):
        vals = [getter(r) for r in reps if getter(r) is not None]
        return {"media": statistics.fmean(vals), "min": min(vals), "max": max(vals)} if vals else None
    return {
        "repeticiones": len(reps),
        "hora_media_ms": agg(lambda r: r["hora"]["media_ms"]),
        "hora_p95_ms": agg(lambda r: r["hora"]["p95_ms"]),
        "hora_p99_ms": agg(lambda r: r["hora"]["p99_ms"]),
        "hora_max_ms": agg(lambda r: r["hora"]["max_ms"]),
        "tick_media_ms": agg(lambda r: r["tick"]["media_ms"] if r["tick"] else None),
        "frame_media_ms": agg(lambda r: r["frame"]["media_ms"] if r["frame"] else None),
        "sim_ms_por_s_normal": agg(lambda r: r["sim_ms_por_s_normal"]),
        "lua_heap_max_kb": agg(lambda r: r["lua_heap_kb"]["max"]),
        "lua_heap_media_kb": agg(lambda r: r["lua_heap_kb"]["mean"]),
        "lua_heap_p95_kb": agg(lambda r: r["lua_heap_kb"]["p95"]),
        "lua_vivo_kb": agg(lambda r: max(r["lua_vivo_kb"])),
        "pacientes_medios": agg(lambda r: r["pacientes_medios"]),
        "entidades_fin": reps[-1]["entidades_fin"],
        "clases_fin": reps[-1]["clases_fin"],
    }


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    factor = None
    if "--factor-cpu" in sys.argv:
        factor = float(sys.argv[sys.argv.index("--factor-cpu") + 1])
        args = [a for a in args if a != str(sys.argv[sys.argv.index("--factor-cpu") + 1])]
    res_dir = args[0]
    out = {"ritmo": {"horas_por_s": HOURS_PER_S, "ticks_vacios_por_s": TICKS_PER_S},
           "factor_cpu": factor, "partidas": {}}
    for partida in sorted(os.listdir(res_dir)):
        pdir = os.path.join(res_dir, partida)
        if not os.path.isdir(pdir):
            continue
        for luadir in sorted(os.listdir(pdir)):
            ldir = os.path.join(pdir, luadir)
            reps = []
            for d in sorted(os.listdir(ldir)):
                if d.startswith("tracy") and os.path.exists(os.path.join(ldir, d, "zonas.csv")):
                    reps.append(summarize_rep(os.path.join(ldir, d)))
            entry = combine(reps) if reps else {}
            entry["por_repeticion"] = reps
            sub = os.path.join(ldir, "heaptrack", "subsistemas.json")
            if os.path.exists(sub):
                h = json.load(open(sub))
                entry["heap_cpp"] = {"pico_total_bytes": h["pico_total_bytes"],
                                     "categorias_bytes": h["categorias_bytes"],
                                     "heaptrack_pico": h.get("heaptrack_pico")}
            if factor and entry.get("sim_ms_por_s_normal"):
                entry["n64_estimado"] = {
                    "hora_media_ms": entry["hora_media_ms"]["media"] * factor,
                    "hora_p95_ms": entry["hora_p95_ms"]["media"] * factor,
                    "cpu_simulacion_normal": entry["sim_ms_por_s_normal"]["media"] * factor / 1000,
                }
            out["partidas"].setdefault(partida, {})[luadir.replace("lua", "Lua ")] = entry

    for partida, luas in out["partidas"].items():
        for lua, e in luas.items():
            if "hora_media_ms" not in e:
                continue
            line = (f"{partida:8s} {lua:8s} hora {e['hora_media_ms']['media']:.3f} ms "
                    f"(p95 {e['hora_p95_ms']['media']:.3f}, máx {e['hora_max_ms']['max']:.2f}) | "
                    f"sim {e['sim_ms_por_s_normal']['media']:.1f} ms/s | "
                    f"Lua vivo {e['lua_vivo_kb']['media'] / 1024:.2f} MB, máx {e['lua_heap_max_kb']['max'] / 1024:.2f} MB | "
                    f"pacientes {e['pacientes_medios']['media'] if e['pacientes_medios'] else 0:.0f}")
            if "heap_cpp" in e:
                line += f" | heap C++ pico {e['heap_cpp']['pico_total_bytes'] / 1e6:.1f} MB"
            print(line)
    if len(args) > 1:
        with open(args[1], "w", encoding="utf-8") as f:
            json.dump(out, f, indent=1, ensure_ascii=False)
            f.write("\n")


if __name__ == "__main__":
    main()
