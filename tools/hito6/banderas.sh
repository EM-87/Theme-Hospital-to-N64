#!/usr/bin/env bash
# Hito 6.3, paso 1: las banderas del mapa calculadas por el núcleo contra
# CorsixTH, en tres partes:
#   1. los 12 niveles recién cargados (th64sim mapa) contra los volcados del
#      hito 2 (bench/scenarios/volcar_mapa.lua): capas, 13 banderas, parcela y
#      objeto;
#   2. el nivel 1 recién cargado contra todas las columnas de getCellFlags
#      (bench/scenarios/celdas_nivel.lua, que se ejecuta si no está);
#   3. los hospitales exportados de CorsixTH (exportar_escenario.lua) de las
#      campañas de los hitos 4 y 5 y el de las vistas: th64sim banderas rehace
#      las banderas desde las salas y los objetos y compara las 16.384 casillas
#      con todas las columnas.
# Los escenarios se pasan a TRG1 aquí (tools/oraculo_hito4/escenario.py), sin
# tocar los directorios de las campañas.
# Uso: tools/hito6/banderas.sh [dir_salida]   (por defecto $TH64_WORK/resultados/hito6/banderas)
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
OUT=${1:-$TH64_WORK/resultados/hito6/banderas}
R=$TH64_WORK/resultados
SIM=$REPO_DIR/motor/build/pc/th64sim
A=$REPO_DIR/motor/assets
mkdir -p "$OUT/escenarios"
fallos=0

echo "== 1. Los 12 niveles recién cargados (volcados del hito 2)"
for n in $(seq 1 12); do
  tsv=$R/hito2/oraculo/mapa_nivel$n.tsv
  if [ ! -f "$tsv" ]; then
    echo "   nivel $n: falta $tsv"
    fallos=$((fallos + 1))
    continue
  fi
  if ! "$SIM" mapa "$A/niveles/nivel$(printf %02d "$n").tmp" "$n" "$tsv" > "$OUT/nivel$n.txt"; then
    fallos=$((fallos + 1))
  fi
  echo "   nivel $n: $(grep -o 'huella [0-9a-f]*' "$OUT/nivel$n.txt"); $(grep 'oráculo' "$OUT/nivel$n.txt")"
done

echo "== 2. El nivel 1 recién cargado, todas las columnas"
if [ ! -f "$OUT/nivel1_celdas/celdas.tsv" ]; then
  "$REPO_DIR/tools/bench_run.sh" "$REPO_DIR/bench/scenarios/celdas_nivel.lua" "$OUT/nivel1_celdas" --th64-nivel=1 \
    > "$OUT/nivel1_celdas.log" 2>&1 || true
fi
if [ -f "$OUT/nivel1_celdas/celdas.tsv" ]; then
  if ! "$SIM" mapa "$A/niveles/nivel01.tmp" 1 --comparar "$OUT/nivel1_celdas/celdas.tsv" > "$OUT/nivel1_celdas.txt"; then
    fallos=$((fallos + 1))
  fi
  sed 's/^/   /' "$OUT/nivel1_celdas.txt"
else
  echo "   no se pudo volcar el nivel 1 con CorsixTH ($OUT/nivel1_celdas.log)"
  fallos=$((fallos + 1))
fi

echo "== 3. Hospitales de CorsixTH rehechos desde sus salas y objetos"
"$SIM" mapa "$A/niveles/nivel01.tmp" 1 --volcar "$OUT/motor_nivel1.tsv"
total=0
iguales=0
for E in "$R"/hito4/campana/escenario_* "$R"/hito4/escenario_1 "$R"/hito5/campana/escenario_* "$R"/hito6/vistas/escenario; do
  [ -f "$E/celdas.tsv" ] && [ -f "$E/escenario.json" ] || continue
  nombre=$(echo "${E#"$R"/}" | tr '/' '_')
  nivel=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["nivel"])' "$E/escenario.json")
  motor_tsv=$OUT/motor_nivel$nivel.tsv
  [ -f "$motor_tsv" ] || "$SIM" mapa "$A/niveles/nivel$(printf %02d "$nivel").tmp" "$nivel" --volcar "$motor_tsv"
  python3 -I "$REPO_DIR/tools/oraculo_hito4/escenario.py" "$E" "$motor_tsv" "$OUT/escenarios/$nombre.trg" > /dev/null
  total=$((total + 1))
  if "$SIM" banderas "$A" "$OUT/escenarios/$nombre.trg" "$E/celdas.tsv" > "$OUT/escenarios/$nombre.txt"; then
    iguales=$((iguales + 1))
  else
    fallos=$((fallos + 1))
    echo "   $nombre:"
    sed 's/^/     /' "$OUT/escenarios/$nombre.txt"
  fi
done
objetos=$(cat "$OUT"/escenarios/*.txt | sed -n 's/.*; \([0-9]*\) objetos: .*/\1/p' | awk '{s += $1} END {print s + 0}')
borde=$(cat "$OUT"/escenarios/*.txt | sed -n 's/.*(\([0-9]*\) de borde).*/\1/p' | awk '{s += $1} END {print s + 0}')
echo "   $iguales de $total hospitales con las 16.384 casillas iguales ($objetos objetos, $borde de borde)"
[ "$fallos" -eq 0 ] || { echo "FALLOS: $fallos"; exit 1; }
echo "todo igual"
