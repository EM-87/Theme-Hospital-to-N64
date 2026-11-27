#!/usr/bin/env bash
# Hito 6.3, pasos 2 y 3: la batería de construir. CorsixTH aplica una lista de
# órdenes como su interfaz (bench/scenarios/construir.lua) y escribe, orden a
# orden, el resultado, el dinero y las casillas que cambian; después el núcleo
# repite la misma lista desde el mismo punto de partida (th64sim construir) y
# la compara orden a orden.
#   0. el mercado de candidatos del núcleo para el nivel recién cargado
#      (th64sim mercado, semilla 1 y mes 0), que CorsixTH mete en
#      world.available_staff (--th64-mercado): «contratar <clase> <índice>»
#      es la misma persona en los dos lados;
#   1. CorsixTH: <salida>/<lista>/corsixth (inicio/, ordenes.tsv, resultados.tsv,
#      dinero.tsv, cambios.tsv, objetos.tsv, salas.tsv, personal.tsv y
#      celdas.tsv);
#   2. el punto de partida, inicio/escenario.json, pasado a TRG1 con
#      tools/oraculo_hito4/escenario.py;
#   3. th64sim construir, que para en la primera orden distinta.
# Uso: tools/hito6/construir.sh [lista.tsv ...]   (por defecto, todas las de
#      tools/hito6/ordenes/). Salida en $TH64_WORK/resultados/hito6/construir.
# Variables: SALIDA (otro directorio de salida), SEGUIR=1 (no parar en la
#            primera orden distinta).
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
OUT=${SALIDA:-$TH64_WORK/resultados/hito6/construir}
SIM=$REPO_DIR/motor/build/pc/th64sim
A=$REPO_DIR/motor/assets
listas=("$@")
if [ ${#listas[@]} -eq 0 ]; then
  # Las de la batería; no las de th64sim partida (demo.tsv), que llevan el
  # tick en la primera columna (la cabecera «tick»).
  for l in "$REPO_DIR"/tools/hito6/ordenes/*.tsv; do
    grep -q $'^tick\t' "$l" || listas+=("$l")
  done
fi
mkdir -p "$OUT"
fallos=0
for lista in "${listas[@]}"; do
  nombre=$(basename "$lista" .tsv)
  D=$OUT/$nombre
  rm -rf "$D"
  mkdir -p "$D/partidas"
  echo "== $nombre"
  # El nivel 1 (el de construir.lua por defecto), con la semilla de th64sim
  # construir (1) y el mes de una partida nueva (0).
  "$SIM" mercado "$A" 1 1 0 > "$D/mercado.json"
  if ! CORSIXTH_SAVES="$D/partidas" "$REPO_DIR/tools/bench_run.sh" --timeout 600 \
      "$REPO_DIR/bench/scenarios/construir.lua" "$D/corsixth" --th64-ordenes="$(realpath "$lista")" \
      --th64-mercado="$(realpath "$D")/mercado.json" > "$D/corsixth.log" 2>&1; then
    echo "   CorsixTH falló ($D/corsixth.log)"
    fallos=$((fallos + 1))
    continue
  fi
  nivel=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["nivel"])' "$D/corsixth/inicio/escenario.json")
  "$SIM" mapa "$A/niveles/nivel$(printf %02d "$nivel").tmp" "$nivel" --volcar "$D/motor_nivel.tsv"
  python3 -I "$REPO_DIR/tools/oraculo_hito4/escenario.py" "$D/corsixth/inicio" "$D/motor_nivel.tsv" "$D/inicio.trg" \
    > /dev/null
  extra=()
  [ "${SEGUIR:-0}" = 1 ] && extra=(--seguir)
  if "$SIM" construir "$A" "$D/inicio.trg" "$D/corsixth" "${extra[@]}" > "$D/motor.txt"; then
    tail -n 2 "$D/motor.txt" | sed 's/^/   /'
  else
    fallos=$((fallos + 1))
    grep -n "DISTINTA" "$D/motor.txt" | head -n 3 | sed 's/^/   /'
    tail -n 1 "$D/motor.txt" | sed 's/^/   /'
  fi
done
[ "$fallos" -eq 0 ] || { echo "FALLOS: $fallos"; exit 1; }
echo "todo igual"
