#!/usr/bin/env bash
# La demo jugable (docs/06_plan.md, «La demo jugable», pieza 2): el escenario
# de una partida nueva del nivel 1, sin CorsixTH en marcha, para el modo
# «Partida nueva» de la ROM y para th64sim partida.
#   1. las reglas del nivel y las definiciones en JSON (th64sim reglas), el
#      mapa del nivel recién cargado por el motor (th64sim mapa --volcar) y sus
#      puntos de aparición (th64sim aparicion, World:calculateSpawnTiles);
#   2. tools/hito6/demo.py escribe escenario.json (lo que deja World:new y
#      Hospital:new, con el hospital abierto: la decisión 1 del plan);
#   3. tools/oraculo_hito4/escenario.py lo pasa a TRG1 contra el volcado del
#      propio motor (0 casillas) → motor/assets/escenarios/demo.trg;
#   4. comprobaciones: th64sim escenario da 0 casillas cambiadas, y th64sim
#      hito4 corre el escenario 9.000 horas (sin personal no viene nadie: el
#      saldo no se mueve); si está el «inicio» que exportó CorsixTH para la
#      batería de construir ($TH64_WORK/resultados/hito6/construir/pasillo/
#      corsixth/inicio/escenario.json, el nivel 1 recién cargado), lo compara
#      campo a campo (las diferencias esperadas están en tools/hito6/README.md).
# Uso: tools/hito6/demo.sh [dir_salida]   (por defecto $TH64_WORK/resultados/hito6/demo)
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
OUT=${1:-$TH64_WORK/resultados/hito6/demo}
SIM=$REPO_DIR/motor/build/pc/th64sim
A=$REPO_DIR/motor/assets
NIVEL=1
mkdir -p "$OUT/escenario" "$OUT/hito4"

echo "== 1. Reglas, mapa y puntos de aparición del nivel $NIVEL"
"$SIM" reglas "$A/reglas/full$(printf %02d $NIVEL).trg" > "$OUT/nivel.json" 2> /dev/null
"$SIM" reglas "$A/reglas/definiciones.trg" > "$OUT/definiciones.json" 2> /dev/null
"$SIM" mapa "$A/niveles/nivel$(printf %02d $NIVEL).tmp" $NIVEL --volcar "$OUT/motor_nivel$NIVEL.tsv"
"$SIM" aparicion "$A/niveles/nivel$(printf %02d $NIVEL).tmp" $NIVEL > "$OUT/aparicion.json"

echo "== 2. escenario.json"
python3 -I "$REPO_DIR/tools/hito6/demo.py" "$OUT/nivel.json" "$OUT/definiciones.json" "$OUT/motor_nivel$NIVEL.tsv" \
  "$OUT/aparicion.json" "$CORSIXTH_DIR/CorsixTH/Lua/diseases" $NIVEL "$OUT/escenario/escenario.json"

echo "== 3. TRG1"
ln -sf "$OUT/motor_nivel$NIVEL.tsv" "$OUT/escenario/celdas.tsv"
python3 -I "$REPO_DIR/tools/oraculo_hito4/escenario.py" "$OUT/escenario" "$OUT/motor_nivel$NIVEL.tsv" "$OUT/demo.trg"
mkdir -p "$A/escenarios"
cp "$OUT/demo.trg" "$A/escenarios/demo.trg"
echo "   $A/escenarios/demo.trg"

echo "== 4. Comprobaciones"
fallos=0
"$SIM" escenario "$A/niveles/nivel$(printf %02d $NIVEL).tmp" "$A/escenarios/demo.trg" "$OUT/motor_nivel$NIVEL.tsv" \
  > "$OUT/escenario.txt" || fallos=$((fallos + 1))
sed 's/^/   /' "$OUT/escenario.txt"
grep -q ' 0 casillas cambiadas' "$OUT/escenario.txt" || { echo "   el TRG1 lleva casillas"; fallos=$((fallos + 1)); }
"$SIM" hito4 "$A" "$A/escenarios/demo.trg" 1 9000 "$OUT/hito4" > "$OUT/hito4.txt" || fallos=$((fallos + 1))
sed 's/^/   /' "$OUT/hito4.txt"
grep -q '^hito4: 0 pacientes en 9000 horas; saldo 40000 (+0)' "$OUT/hito4.txt" ||
  { echo "   sin personal tendría que quedar en 0 pacientes y 40000"; fallos=$((fallos + 1)); }
ORACULO=$TH64_WORK/resultados/hito6/construir/pasillo/corsixth/inicio/escenario.json
if [ -f "$ORACULO" ]; then
  echo "   contra el nivel 1 recién cargado en CorsixTH ($ORACULO):"
  python3 -I "$REPO_DIR/tools/hito6/demo.py" --comparar "$OUT/escenario/escenario.json" "$ORACULO"
fi
if [ $fallos -gt 0 ]; then
  echo "demo: $fallos fallos"
  exit 1
fi
echo "demo: bien"
