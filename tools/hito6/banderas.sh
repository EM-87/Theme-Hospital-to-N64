#!/usr/bin/env bash
# Hito 6.3, pasos 1 y 3: las banderas y las paredes del mapa calculadas por el
# núcleo contra CorsixTH, en cuatro partes:
#   1. los 12 niveles recién cargados (th64sim mapa) contra los volcados del
#      hito 2 (bench/scenarios/volcar_mapa.lua): capas, 13 banderas, parcela y
#      objeto;
#   2. el nivel 1 recién cargado contra todas las columnas de getCellFlags
#      (bench/scenarios/celdas_nivel.lua, que se ejecuta si no está);
#   3. los hospitales exportados de CorsixTH (exportar_escenario.lua) de las
#      campañas de los hitos 4 y 5, el de las vistas y el estado final de cada
#      lista de la batería de construir (construir.sh; la de las salas trae
#      ventanas, paredes compartidas y la puerta doble por los cuatro lados,
#      que los hospitales de las campañas no tienen): th64sim banderas rehace
#      las banderas desde las salas y los objetos (las capas, copiadas del
#      volcado) y compara las 16.384 casillas con todas las columnas;
#   4. los mismos hospitales con las salas levantadas por el núcleo (paso 3,
#      th64sim salas): desde el nivel recién cargado, el núcleo pone el suelo,
#      las paredes, las ventanas y las puertas de cada sala como
#      UIEditRoom:finishRoom (th_build_rebuild_room) y coloca los objetos en el
#      orden de world.entities, y se comparan las 16.384 casillas con todas
#      las columnas, también suelo, norte y oeste. El punto de partida es el
#      escenario «vacío»: el mismo escenario.json sin salas, sin listas, sin
#      personal, sin basura y solo con los objetos que se crean desde el mapa
#      (las puertas de la entrada, supports_creation_for_map), pasado a TRG1
#      contra el volcado del propio motor para que no lleve casillas; las
#      parcelas compradas se compran al cargar (th_sim_init sin «celdas»).
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
for E in "$R"/hito4/campana/escenario_* "$R"/hito4/escenario_1 "$R"/hito5/campana/escenario_* "$R"/hito6/vistas/escenario \
    "$R"/hito6/construir/*/corsixth; do
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

echo "== 4. Hospitales de CorsixTH con las salas levantadas por el núcleo (paredes, ventanas y puertas)"
# Los objetos que se crean desde el mapa (supports_creation_for_map): lo único
# que conserva el escenario vacío, porque ya los pone th_map_load.
del_mapa=$("$SIM" reglas "$A/reglas/definiciones.trg" objetos 2>/dev/null | python3 -I -c '
import json, sys
print(" ".join(sorted(k for k, v in json.load(sys.stdin).items() if v.get("supports_creation_for_map"))))')
# Lo de esta parte va en $OUT/salas, aparte de los .txt de la parte 3 (que suma sus cifras de $OUT/escenarios/*.txt).
mkdir -p "$OUT/salas"
total4=0
iguales4=0
for E in "$R"/hito4/campana/escenario_* "$R"/hito4/escenario_1 "$R"/hito5/campana/escenario_* "$R"/hito6/vistas/escenario \
    "$R"/hito6/construir/*/corsixth; do
  [ -f "$E/celdas.tsv" ] && [ -f "$E/escenario.json" ] || continue
  nombre=$(echo "${E#"$R"/}" | tr '/' '_')
  nivel=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["nivel"])' "$E/escenario.json")
  motor_tsv=$OUT/motor_nivel$nivel.tsv
  [ -f "$motor_tsv" ] || "$SIM" mapa "$A/niveles/nivel$(printf %02d "$nivel").tmp" "$nivel" --volcar "$motor_tsv"
  V=$OUT/salas/${nombre}_vacio
  mkdir -p "$V"
  # shellcheck disable=SC2086
  python3 -I - "$E/escenario.json" "$V/escenario.json" $del_mapa <<'EOF'
import json, sys
doc = json.load(open(sys.argv[1]))
del_mapa = set(sys.argv[3:])
doc["salas"] = []
doc["personal"] = []
doc["basura"] = []  # th_sim_init no carga basura (-28); no cambia capas ni banderas
doc.pop("listas", None)
doc["objetos"] = [o for o in doc["objetos"] if o["id"] in del_mapa]
json.dump(doc, open(sys.argv[2], "w"), ensure_ascii=False, sort_keys=True)
EOF
  # Contra el volcado del propio motor: ninguna casilla difiere y el TRG1 no lleva «celdas».
  ln -sf "$motor_tsv" "$V/celdas.tsv"
  python3 -I "$REPO_DIR/tools/oraculo_hito4/escenario.py" "$V" "$motor_tsv" "$OUT/salas/${nombre}_vacio.trg" > /dev/null
  total4=$((total4 + 1))
  if "$SIM" salas "$A" "$OUT/salas/${nombre}_vacio.trg" "$OUT/escenarios/$nombre.trg" "$E/celdas.tsv" \
      > "$OUT/salas/$nombre.txt"; then
    iguales4=$((iguales4 + 1))
  else
    fallos=$((fallos + 1))
    echo "   $nombre:"
    sed 's/^/     /' "$OUT/salas/$nombre.txt"
  fi
done
# Las cifras de la línea «salas: ...» de cada hospital, sumadas.
suma4() { cat "$OUT"/salas/*.txt | sed -n "s/$1/\1/p" | awk '{s += $1} END {print s + 0}'; }
salas4=$(suma4 '.*; \([0-9]*\) salas rehechas.*')
puertas4=$(suma4 '.*(\([0-9]*\) puertas, .*')
ventanas4=$(suma4 '.*, \([0-9]*\) ventanas).*')
objetos4=$(suma4 '.*, \([0-9]*\) colocados (.*')
sinpuerta4=$(suma4 '.*) y \([0-9]*\) sin puerta;.*')
echo "   $iguales4 de $total4 hospitales con las 16.384 casillas iguales, capas incluidas ($salas4 salas rehechas, $puertas4 puertas, $ventanas4 ventanas, $objetos4 objetos colocados; $sinpuerta4 salas sin puerta)"
[ "$fallos" -eq 0 ] || { echo "FALLOS: $fallos"; exit 1; }
echo "todo igual"
