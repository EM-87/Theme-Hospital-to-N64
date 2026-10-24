#!/usr/bin/env bash
# Oráculo estadístico del hito 4: para cada semilla, crea el escenario en
# CorsixTH (bench/scenarios/crear_hito4.lua) y lo traza
# (bench/scenarios/traza_hito4.lua); al final, resumen.py junta las cifras.
#
# Uso: tools/oraculo_hito4/campana.sh [semillas] [horas] [dir_salida] [opciones de crear_hito4...]
#   semillas: por defecto "1 2 3 4 5 6 7 8 9 10"
#   horas: por defecto 9000 (6 meses de juego)
#   dir_salida: por defecto $TH64_WORK/resultados/hito4/campana
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
semillas=${1:-"1 2 3 4 5 6 7 8 9 10"}
horas=${2:-9000}
O=${3:-$TH64_WORK/resultados/hito4/campana}
shift 3 2>/dev/null || shift $#
mkdir -p "$O/saves"
export CORSIXTH_SAVES=$O/saves
for s in $semillas; do
  "$REPO_DIR/tools/bench_run.sh" "$REPO_DIR/bench/scenarios/crear_hito4.lua" "$O/crear_$s" --th64-seed="$s" \
    --th64-nombre="hito4_$s" "$@" > "$O/crear_$s.log" 2>&1
  "$REPO_DIR/tools/bench_run.sh" "$REPO_DIR/bench/scenarios/traza_hito4.lua" "$O/semilla_$s" --load="hito4_$s.sav" \
    --th64-horas="$horas" > "$O/traza_$s.log" 2>&1
  echo "semilla $s: $(grep '^traza:' "$O/semilla_$s/th64.log")"
done
python3 -I "$REPO_DIR/tools/oraculo_hito4/resumen.py" "$O"/semilla_*
