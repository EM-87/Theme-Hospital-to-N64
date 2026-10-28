#!/usr/bin/env bash
# Hito 6.1: el panel inferior del motor contra CorsixTH.
#   1. los gráficos de alta resolución (tools/oraculo_mapa/alta.py), si no están;
#   2. capturas de CorsixTH en 11 estados (bench/scenarios/captura_panel.lua);
#   3. lo mismo con el motor a 640 × 480 (th64sim panel alta) y la comparación
#      (comparar_panel.py);
#   4. los mismos estados a 320 × 240 con los gráficos de la N64
#      (th64sim panel baja), para verlos.
#
# Uso: tools/hito6/panel.sh [dir]   (por defecto $TH64_WORK/resultados/hito6/panel; unos 30 s)
# Antes: el núcleo compilado en motor/build/pc y los assets del motor (tools/conv/conv.py).
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
O=${1:-$TH64_WORK/resultados/hito6/panel}
ALTA=$TH64_WORK/resultados/alta
SIM=$REPO_DIR/motor/build/pc/th64sim
A=$REPO_DIR/motor/assets
[ -f "$ALTA/panel02v.tsh" ] || python3 -I "$REPO_DIR/tools/oraculo_mapa/alta.py" "$ALTA" > /dev/null
rm -rf "$O"
mkdir -p "$O" "$O/baja"
"$REPO_DIR/tools/bench_run.sh" "$REPO_DIR/bench/scenarios/captura_panel.lua" "$O" > "$O/corsixth.txt" 2>&1
"$SIM" panel alta "$A/niveles/nivel01.tmp" 1 "$ALTA" "$A/textos/english.trg" "$O/capturas.txt" "$O" > /dev/null
"$SIM" panel baja "$A/niveles/nivel01.tmp" 1 "$A" "$A/textos/english.trg" "$O/capturas.txt" "$O/baja" > /dev/null
python3 -I "$REPO_DIR/tools/hito6/comparar_panel.py" "$O" --dif
