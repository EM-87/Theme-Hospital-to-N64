#!/usr/bin/env bash
# El motor contra CorsixTH en el escenario del hito 4. Para cada semilla de una
# campaña ya hecha (campana.sh):
#   1. exporta la partida guardada (bench/scenarios/exportar_escenario.lua) y
#      la pasa a TRG1 (escenario.py), si no está ya;
#   2. comprueba que el motor deja las 16.384 casillas como CorsixTH;
#   3. corre la simulación del motor (th64sim hito4) con R semillas propias.
# Al final, resumen.py de cada lado y comparar.py. El escenario de la primera
# semilla se copia a motor/assets/escenarios/hito4.trg: es el que llevan las
# ROM (make) y el que usa tools/motor_check.sh.
#
# Uso: tools/oraculo_hito4/motor.sh [semillas] [horas] [repeticiones] [dir_campaña]
#   semillas: por defecto "1 2 3 4 5 6 7 8 9 10"
#   horas: por defecto 9000, las de la campaña
#   repeticiones: corridas del motor por escenario, por defecto 10
#   dir_campaña: por defecto $TH64_WORK/resultados/hito4/campana
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
semillas=${1:-"1 2 3 4 5 6 7 8 9 10"}
horas=${2:-9000}
reps=${3:-10}
O=${4:-$TH64_WORK/resultados/hito4/campana}
SIM=$REPO_DIR/motor/build/pc/th64sim
A=$REPO_DIR/motor/assets
M=$O/motor
mkdir -p "$M"
for s in $semillas; do
  E=$O/escenario_$s
  if [ ! -f "$E/escenario.json" ]; then
    CORSIXTH_SAVES=$O/saves "$REPO_DIR/tools/bench_run.sh" "$REPO_DIR/bench/scenarios/exportar_escenario.lua" "$E" \
      --load="hito4_$s.sav" > "$E.log" 2>&1
  fi
  nivel=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["nivel"])' "$E/escenario.json")
  tmp=$A/niveles/nivel$(printf %02d "$nivel").tmp
  "$SIM" mapa "$tmp" "$nivel" --volcar "$E/motor.tsv"
  python3 -I "$REPO_DIR/tools/oraculo_hito4/escenario.py" "$E" "$E/motor.tsv" "$E/escenario.trg" > /dev/null
  "$SIM" escenario "$tmp" "$E/escenario.trg" "$E/celdas.tsv" > "$E/comprobacion.txt" \
    || { cat "$E/comprobacion.txt"; echo "semilla $s: las casillas no coinciden"; exit 1; }
  for r in $(seq 1 "$reps"); do
    d=$M/semilla_${s}_$r
    mkdir -p "$d"
    "$SIM" hito4 "$A" "$E/escenario.trg" $((s * 1000 + r)) "$horas" "$d" > "$d/salida.txt"
  done
  echo "semilla $s: $(tail -1 "$E/comprobacion.txt"); $(cat "$M/semilla_${s}_1/salida.txt")"
done
primera=${semillas%% *}
mkdir -p "$A/escenarios"
cp "$O/escenario_$primera/escenario.trg" "$A/escenarios/hito4.trg"
python3 -I "$REPO_DIR/tools/oraculo_hito4/resumen.py" "$O"/semilla_[0-9]* --json "$O/corsixth.json" > "$O/corsixth.txt"
python3 -I "$REPO_DIR/tools/oraculo_hito4/resumen.py" "$M"/semilla_* --json "$M/motor.json" > "$M/motor.txt"
python3 -I "$REPO_DIR/tools/oraculo_hito4/comparar.py" "$O/corsixth.json" "$M/motor.json"
