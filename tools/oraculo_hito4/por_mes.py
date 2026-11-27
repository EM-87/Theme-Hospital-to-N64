"""Llegadas de pacientes por mes de juego, CorsixTH frente al motor.

Es la vista que delata una diferencia en las llegadas que las cifras totales
tapan: la campaña del hito 4 regenerada daba un 5 % menos de consultas en el
motor, y por meses se veía que todo el déficit estaba en el primero (6,22
frente a 5,16 llegadas): le faltaba la llegada forzada del primer paciente
de PlayerHospital:onEndDay.

Uso: python3 -I tools/oraculo_hito4/por_mes.py <dir_campaña> [<dir_motor>] [--hora-inicial H]
  <dir_campaña>: con semilla_N/pacientes.tsv (lo que escribe campana.sh);
  <dir_motor>: con semilla_N_R/pacientes.tsv (lo que escribe motor.sh); por
  defecto, <dir_campaña>/motor.
La hora de llegada cuenta desde la carga; la partida empieza en la hora 2 del
1 de enero (dias.tsv) y un día tiene 50 horas. --hora-inicial cambia esa
hora: la campaña de la demo (tools/hito6/campana_demo.sh), una partida nueva,
empieza en la 0.
"""
import csv
import glob
import os
import statistics
import sys
from collections import Counter

MESES = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
HORA_INICIAL = 2
HORAS_DIA = 50


def mes_de(hora, hora_inicial=HORA_INICIAL):
    dia = (hora + hora_inicial) // HORAS_DIA
    m = 0
    while m < 11 and dia >= MESES[m]:
        dia -= MESES[m]
        m += 1
    return m + 1


def llegadas_por_mes(directorio, hora_inicial=HORA_INICIAL):
    c = Counter()
    with open(os.path.join(directorio, "pacientes.tsv"), newline="") as f:
        for fila in csv.DictReader(f, delimiter="\t"):
            h = int(fila["llegada"])
            if h >= 0:
                c[mes_de(h, hora_inicial)] += 1
    return c


def resumen(nombre, dirs, meses, hora_inicial=HORA_INICIAL):
    por_mes = {m: [] for m in range(1, meses + 1)}
    total = []
    for d in dirs:
        c = llegadas_por_mes(d, hora_inicial)
        for m in por_mes:
            por_mes[m].append(c[m])
        total.append(sum(c.values()))
    print(f"{nombre}: {len(dirs)} corridas; llegadas por corrida {statistics.mean(total):.2f}")
    for m, v in por_mes.items():
        sd = statistics.stdev(v) if len(v) > 1 else 0.0
        print(f"  mes {m}: {statistics.mean(v):5.2f} ± {sd:.2f}")
    return por_mes


def main():
    args = sys.argv[1:]
    hora_inicial = HORA_INICIAL
    if "--hora-inicial" in args:
        k = args.index("--hora-inicial")
        hora_inicial = int(args[k + 1])
        del args[k:k + 2]
    if not args:
        print(__doc__)
        return 2
    campana = args[0]
    motor = args[1] if len(args) > 1 else os.path.join(campana, "motor")
    corsixth = sorted(d for d in glob.glob(os.path.join(campana, "semilla_*")) if os.path.isfile(os.path.join(d, "pacientes.tsv")))
    del_motor = sorted(d for d in glob.glob(os.path.join(motor, "semilla_*_*")) if os.path.isfile(os.path.join(d, "pacientes.tsv")))
    if not corsixth or not del_motor:
        print(f"faltan pacientes.tsv: CorsixTH {len(corsixth)} corridas, motor {len(del_motor)}")
        return 1
    meses = 7
    a = resumen("CorsixTH", corsixth, meses, hora_inicial)
    b = resumen("motor", del_motor, meses, hora_inicial)
    print("diferencia motor − CorsixTH por mes:",
          ", ".join(f"{m}: {statistics.mean(b[m]) - statistics.mean(a[m]):+.2f}" for m in range(1, meses + 1)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
