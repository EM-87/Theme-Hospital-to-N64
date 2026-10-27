#!/usr/bin/env python3
"""Resume la medida del hito 5 (tools/hito5/medir.sh): la memoria con 8 y 4
MiB, la huella (N64 frente a PC), el coste de cada tick de la simulación con
su regresión por personas, casillas de temperatura y nodos de búsqueda, lo
que cuesta cada carga de textura y los fotogramas a cada velocidad.

Uso: python3 -I tools/hito5/analizar.py <dir>
  <dir>/carga_pc.tsv, carga_pc.txt      th64sim carga
  <dir>/ares_8mib/isviewer.log          la ROM de medida con Expansion Pak
  <dir>/ares_4mib/isviewer.log          y sin él
Escribe además <dir>/resumen.json.
"""
import json
import re
import sys

import numpy as np

TICKS_POR_MS = 46875.0  # get_ticks: la mitad del reloj de la CPU (93,75 MHz)
CICLOS_POR_TICK = 2      # ciclos de la CPU por tick de get_ticks
PRESUPUESTO_HORA = 5.07e6  # ciclos por hora de juego a velocidad Normal (docs/04_decision.md)


def leer_log(ruta):
    try:
        lineas = open(ruta, "rb").read().decode("utf-8", "replace").splitlines()
    except FileNotFoundError:
        return None
    r = {"memoria": [], "ticks": [], "fotogramas": [], "texto": [], "cargas": [], "aserto": None}
    for l in lineas:
        if l.startswith("TH64 memoria: "):
            m = re.match(r"TH64 memoria: (.*): heap usado (\d+) KiB de (\d+) KiB( \(FALLO\))?", l)
            if m:
                r["memoria"].append((m[1], int(m[2]), int(m[3]), bool(m[4])))
        elif l.startswith("H5 t "):
            r["ticks"].append([int(x) for x in l.split()[2:]])
        elif l.startswith("H5 c "):
            m = re.match(r"H5 c (\S+) (\d): (\d+) sprites; CPU (\d+) ciclos por sprite; hasta que acaban el RSP y el "
                         r"RDP, (\d+)", l)
            if m:
                r["cargas"].append((m[1], int(m[2]), int(m[3]), int(m[4]), int(m[5])))
        elif l.startswith("H5 f "):
            p = l.split()
            r["fotogramas"].append([p[2]] + [int(x) for x in p[3:]])
        elif l.startswith("TH64 "):
            r["texto"].append(l)
        if "ASSERTION" in l or "assert" in l.lower() and "failed" in l.lower() or "Out of memory" in l:
            r["aserto"] = r["aserto"] or l.strip()
    return r


def ms(t):
    return t / TICKS_POR_MS


def resumen_ticks(t, pc):
    a = np.array(t, dtype=np.float64)
    hora, personas, pacientes, andando, celdas, busq, nodos, ticks = a.T
    us = ticks / TICKS_POR_MS * 1000
    res = {"ticks": len(a), "media_ms": float(us.mean() / 1000), "max_ms": float(us.max() / 1000),
           "hora_max": int(hora[us.argmax()]), "personas_en_max": int(personas[us.argmax()]),
           "nodos_en_max": int(nodos[us.argmax()]), "celdas_en_max": int(celdas[us.argmax()])}
    # Regresión: us = a + b·personas + c·casillas + d·nodos + e·búsquedas.
    X = np.column_stack([np.ones_like(us), personas, celdas, nodos, busq])
    coef, *_ = np.linalg.lstsq(X, us, rcond=None)
    pred = X @ coef
    r2 = 1 - ((us - pred) ** 2).sum() / ((us - us.mean()) ** 2).sum()
    res["regresion_us"] = {"fijo": coef[0], "por_persona": coef[1], "por_casilla": coef[2], "por_nodo": coef[3],
                           "por_busqueda": coef[4], "r2": r2}
    # Ticks sin temperatura ni búsquedas: el coste de las personas, sin más.
    limpio = (celdas == 0) & (busq == 0)
    res["limpios"] = int(limpio.sum())
    if limpio.sum() > 10:
        Xl = np.column_stack([np.ones(limpio.sum()), personas[limpio]])
        cl, *_ = np.linalg.lstsq(Xl, us[limpio], rcond=None)
        res["limpios_us"] = {"fijo": cl[0], "por_persona": cl[1]}
    # Por tramos de personas.
    tramos = []
    for lo, hi in ((0, 10), (10, 20), (20, 30), (30, 40), (40, 50), (50, 60), (60, 80)):
        m = (personas >= lo) & (personas < hi)
        if m.sum():
            tramos.append({"personas": f"{lo}-{hi - 1}", "ticks": int(m.sum()), "media_ms": float(us[m].mean() / 1000),
                           "max_ms": float(us[m].max() / 1000), "sin_temp_ni_caminos_ms":
                           float(us[m & limpio].mean() / 1000) if (m & limpio).sum() else None,
                           "busquedas_por_tick": float(busq[m].mean()), "nodos_por_tick": float(nodos[m].mean())})
    res["tramos"] = tramos
    # Búsquedas: cuántas, cuántos nodos y los ticks con más nodos.
    res["busquedas"] = {"total": int(busq.sum()), "nodos": int(nodos.sum()), "ticks_con_busqueda": int((busq > 0).sum()),
                        "max_nodos_tick": int(nodos.max()), "ticks_mas_de_5000_nodos": int((nodos > 5000).sum())}
    # Con el PC: misma carga por hora y cociente de tiempos.
    if pc:
        por_hora = {r[0]: r for r in pc}
        iguales = sum(1 for r in a if int(r[0]) in por_hora and por_hora[int(r[0])][1:7] == [int(x) for x in r[1:7]])
        ns_pc = np.array([por_hora[int(h)][7] for h in hora if int(h) in por_hora], dtype=np.float64)
        res["pc"] = {"horas_iguales": iguales, "us_pc_media": float(ns_pc.mean() / 1000),
                     "cociente_n64_pc": float(us.mean() / (ns_pc.mean() / 1000))}
    return res


def resumen_fotogramas(f):
    out = {}
    for vel in dict.fromkeys(r[0] for r in f):
        a = np.array([r[2:] for r in f if r[0] == vel], dtype=np.float64)
        cpu, lista, suelos, paredes, ent, cargas, mundo, temp, audio, ticks, sprites, ents, ncargas, intervalo = a.T[:14]
        vista_sim = a.T[14] if a.shape[1] > 14 else np.zeros(len(a))
        atraso = a.T[15] if a.shape[1] > 15 else np.zeros(len(a))
        dibujo = lista + suelos + paredes
        iv = intervalo[1:]
        out[vel] = {
            "fotogramas": len(a), "cpu_ms": float(ms(cpu.mean())), "cpu_max_ms": float(ms(cpu.max())),
            "cpu_p99_ms": float(ms(np.percentile(cpu, 99))), "dibujo_ms": float(ms(dibujo.mean())),
            "lista_ms": float(ms(lista.mean())), "objetos_y_personas_a_entidades_ms": float(ms(vista_sim.mean())),
            "suelos_ms": float(ms(suelos.mean())),
            "paredes_y_entidades_ms": float(ms(paredes.mean())), "entidades_ms": float(ms(ent.mean())),
            "cargas_ms": float(ms(cargas.mean())), "mundo_ms": float(ms(mundo.mean())),
            "mundo_max_ms": float(ms(mundo.max())), "temperatura_ms": float(ms(temp.mean())),
            "audio_ms": float(ms(audio.mean())), "ticks_por_fotograma": float(ticks.mean()),
            "sprites": float(sprites.mean()), "sprites_entidades": float(ents.mean()), "cargas": float(ncargas.mean()),
            "us_por_elemento_entidad": float(ms(ent.sum()) * 1000 / max(1, ents.sum())),
            "us_por_carga": float(ms(cargas.sum()) * 1000 / max(1, ncargas.sum())),
            "intervalo_ms": float(ms(iv.mean())), "intervalo_max_ms": float(ms(iv.max())),
            "saltos": int((iv > TICKS_POR_MS * 50).sum()),
            "tarde": int((iv > TICKS_POR_MS * 34).sum()),
            "atraso_max": int(atraso.max()), "fotogramas_con_atraso": int((atraso > 1).sum()),
        }
    return out


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    d = sys.argv[1]
    pc = [[int(x) for x in l.split("\t")] for l in open(f"{d}/carga_pc.tsv").read().splitlines()[1:]]
    huella_pc = re.search(r"huella (\w+)", open(f"{d}/carga_pc.txt").read())
    salida = {}
    for ram in (8, 4):
        log = leer_log(f"{d}/ares_{ram}mib/isviewer.log")
        if log is None:
            continue
        r = {"memoria": log["memoria"], "aserto": log["aserto"]}
        print(f"== {ram} MiB")
        for nombre, usado, total, fallo in log["memoria"]:
            print(f"   memoria: {nombre}: {usado} KiB de {total} (libres {total - usado}){' FALLO' if fallo else ''}")
        if log["aserto"]:
            print(f"   se para: {log['aserto']}")
        huella = next((re.search(r"huella (\w+)", l)[1] for l in log["texto"] if "TH64 hito5:" in l and "huella" in l),
                      None)
        if huella:
            r["huella"] = huella
            print(f"   huella N64 {huella}; PC {huella_pc[1] if huella_pc else '?'}: "
                  f"{'IGUAL' if huella_pc and huella == huella_pc[1] else 'DISTINTA'}")
        for l in log["texto"]:
            if l.startswith("TH64 hito5:"):
                print("   " + l)
        if log["cargas"]:
            # Cargar y dibujar un sprite, con el RDP libre al empezar: la mejor de las dos repeticiones.
            r["cargas_ciclos"] = {}
            for modo in dict.fromkeys(c[0] for c in log["cargas"]):
                filas = [c for c in log["cargas"] if c[0] == modo]
                r["cargas_ciclos"][modo] = {"cpu": min(c[3] for c in filas), "con_rsp_y_rdp": min(c[4] for c in filas),
                                            "sprites": filas[0][2]}
            print("   cargar y dibujar un sprite (ciclos de CPU; hasta que acaban el RSP y el RDP): " +
                  "; ".join(f"{k} {v['cpu']} ({v['con_rsp_y_rdp']})" for k, v in r["cargas_ciclos"].items()))
        if log["ticks"]:
            r["ticks"] = resumen_ticks(log["ticks"], pc)
            t = r["ticks"]
            g = t["regresion_us"]
            print(f"   ticks: {t['ticks']}; {t['media_ms']:.3f} ms de media, {t['max_ms']:.2f} ms el más largo (hora "
                  f"{t['hora_max']}: {t['personas_en_max']} personas, {t['nodos_en_max']} nodos, "
                  f"{t['celdas_en_max']} casillas)")
            print(f"   regresión (us): fijo {g['fijo']:.0f}; persona {g['por_persona']:.2f}; casilla "
                  f"{g['por_casilla']:.3f}; nodo {g['por_nodo']:.3f}; búsqueda {g['por_busqueda']:.1f}; "
                  f"R² {g['r2']:.3f}")
            if "limpios_us" in t:
                print(f"   ticks sin temperatura ni búsquedas ({t['limpios']}): fijo {t['limpios_us']['fijo']:.0f} us, "
                      f"{t['limpios_us']['por_persona']:.2f} us por persona")
            for tr in t["tramos"]:
                print(f"     {tr['personas']:>6} personas: {tr['ticks']:5d} ticks, {tr['media_ms']:.3f} ms (máx. "
                      f"{tr['max_ms']:.2f}); sin temperatura ni caminos "
                      f"{tr['sin_temp_ni_caminos_ms'] if tr['sin_temp_ni_caminos_ms'] is None else round(tr['sin_temp_ni_caminos_ms'], 3)} "
                      f"ms; {tr['busquedas_por_tick']:.2f} búsquedas y {tr['nodos_por_tick']:.0f} nodos por tick")
            print(f"   búsquedas: {t['busquedas']}")
            if "pc" in t:
                print(f"   PC: {t['pc']}")
        if log["fotogramas"]:
            r["fotogramas"] = resumen_fotogramas(log["fotogramas"])
            for vel, v in r["fotogramas"].items():
                print(f"   fotogramas {vel}: " + "; ".join(f"{k} {round(x, 3)}" for k, x in v.items()))
        salida[f"{ram}mib"] = r
    json.dump(salida, open(f"{d}/resumen.json", "w"), indent=1, default=float)


if __name__ == "__main__":
    main()
