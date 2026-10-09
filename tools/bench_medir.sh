#!/usr/bin/env bash
# Campaña de medición de la fase 1: cada partida de bench/saves con Lua 5.4 y
# 5.5, varias repeticiones con Tracy (tiempos y heap de Lua) y una pasada con
# heaptrack (heap de C++ por subsistema). Ejecutar sin otras cargas en la máquina.
#
# Uso: tools/bench_medir.sh [dir_resultados] [repeticiones=3]
#   Variables: PARTIDAS="inicio mediano lleno", LUAS="5.4 5.5",
#              WARMUP=100, HOURS=500 (horas de juego), HEAP_HOURS=200
# Después: python3 bench/analizar.py <dir_resultados> bench/resultados/fase1.json
set -euo pipefail

REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$REPO_DIR/tools/env.sh"

res=${1:-$TH64_WORK/resultados/$(date +%Y%m%d-%H%M)}
reps=${2:-3}
partidas=${PARTIDAS:-inicio mediano lleno}
luas=${LUAS:-5.4 5.5}
warmup=${WARMUP:-100} hours=${HOURS:-500} heap_hours=${HEAP_HOURS:-200}
mkdir -p "$res"
echo "Resultados en $res"

for partida in $partidas; do
  for lua in $luas; do
    for rep in $(seq 1 "$reps"); do
      out=$res/$partida/lua$lua/tracy$rep
      "$REPO_DIR/tools/bench_run.sh" --tracy --lua "$lua" --timeout 900 \
        "$REPO_DIR/bench/scenarios/medir.lua" "$out" \
        --load="$partida.sav" --th64-save="$partida" \
        --th64-warmup="$warmup" --th64-hours="$hours" | tail -2
      tracy-csvexport -u -f th64 "$out/traza.tracy" > "$out/zonas.csv"
    done
    out=$res/$partida/lua$lua/heaptrack
    "$REPO_DIR/tools/bench_run.sh" --heaptrack --lua "$lua" --timeout 1800 \
      "$REPO_DIR/bench/scenarios/medir.lua" "$out" \
      --load="$partida.sav" --th64-save="$partida" \
      --th64-warmup=50 --th64-hours="$heap_hours" | tail -2
    python3 -I "$REPO_DIR/bench/heaptrack_subsistemas.py" "$out/heaptrack.gz" "$out/subsistemas.json" > /dev/null
  done
done
echo "Hecho: $res"
