#!/usr/bin/env bash
# Hito 6.3, paso 2: la batería de construir. CorsixTH aplica una lista de
# órdenes como su interfaz (bench/scenarios/construir.lua) y escribe, orden a
# orden, el resultado, el dinero y las casillas que cambian; después el núcleo
# repite la misma lista desde el mismo punto de partida (th64sim construir) y
# la compara orden a orden.
#   1. CorsixTH: <salida>/<lista>/corsixth (inicio/, ordenes.tsv, resultados.tsv,
#      dinero.tsv, cambios.tsv, objetos.tsv y celdas.tsv);
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
[ ${#listas[@]} -gt 0 ] || listas=("$REPO_DIR"/tools/hito6/ordenes/*.tsv)
mkdir -p "$OUT"
fallos=0
for lista in "${listas[@]}"; do
  nombre=$(basename "$lista" .tsv)
  D=$OUT/$nombre
  rm -rf "$D"
  mkdir -p "$D/partidas"
  echo "== $nombre"
  if ! CORSIXTH_SAVES="$D/partidas" "$REPO_DIR/tools/bench_run.sh" --timeout 600 \
      "$REPO_DIR/bench/scenarios/construir.lua" "$D/corsixth" --th64-ordenes="$(realpath "$lista")" \
      > "$D/corsixth.log" 2>&1; then
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
