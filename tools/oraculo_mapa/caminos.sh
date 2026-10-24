#!/usr/bin/env bash
# Compara con CorsixTH el A* y la acción de caminar del núcleo:
# - caminos: en cada nivel, CorsixTH (bench/scenarios/caminos.lua) busca
#   1.000 caminos entre pares de casillas transitables; th64sim caminos los
#   repite con el A* del núcleo y los compara casilla a casilla;
# - caminatas: para cada caminata de la lista, CorsixTH
#   (bench/scenarios/caminata.lua) hace caminar a un humanoide y vuelca su
#   estado tras cada tick; th64sim caminata lo repite con el núcleo y lo
#   compara tick a tick.
#
# Uso: tools/oraculo_mapa/caminos.sh [niveles] [lista] [dir_salida]
#   niveles: los de los caminos, por defecto "1 2 3 4 5 6 7 8 9 10 11 12"
#   lista: fichero con «nivel x1,y1;x2,y2 ticks» por línea (por defecto, las
#          16 caminatas de docs/05_prototipo.md)
#   dir_salida: por defecto $TH64_WORK/resultados/hito3
# Antes: tools/conv/conv.py y el núcleo compilado en motor/build/pc.
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
niveles=${1:-"1 2 3 4 5 6 7 8 9 10 11 12"}
O=${3:-$TH64_WORK/resultados/hito3}
mkdir -p "$O/caminos" "$O/caminatas"
sim=$REPO_DIR/motor/build/pc/th64sim
tmp() { echo "$REPO_DIR/motor/assets/niveles/nivel$(printf %02d "$1").tmp"; }
fallos=0
for n in $niveles; do
  "$REPO_DIR/tools/bench_run.sh" "$REPO_DIR/bench/scenarios/caminos.lua" "$O/caminos" --th64-nivel="$n" \
    > "$O/caminos/corsixth_$n.log" 2>&1
  echo "== nivel $n"
  "$sim" caminos "$(tmp "$n")" "$n" "$O/caminos/caminos_nivel$n.tsv" | sed 's/^/   /' || fallos=$((fallos + 1))
done
lista=${2:-}
if [ -z "$lista" ]; then
  # La primera, la del humanoide de la ROM; las demás, 5 caminos al azar de
  # entre 15 y 90 pasos de los niveles 1, 5 y 12, con 30 ticks de margen.
  lista=$O/caminatas/caminatas.txt
  cat > "$lista" <<'FIN'
1 62,75;70,65 200
1 77,92;26,123 686
1 4,115;1,63 470
1 86,70;54,70 286
1 4,51;5,123 614
1 122,66;124,8 510
5 61,80;20,67 574
5 47,74;76,89 382
5 101,88;106,50 374
5 61,109;25,57 734
5 24,33;23,65 294
12 125,25;121,13 158
12 65,82;59,69 182
12 70,104;75,123 302
12 127,12;121,26 190
12 5,10;1,71 550
FIN
fi
i=0
while read -r n ruta ticks; do
  i=$((i + 1))
  "$REPO_DIR/tools/bench_run.sh" "$REPO_DIR/bench/scenarios/caminata.lua" "$O/caminatas" --th64-nivel="$n" \
    --th64-ruta="$ruta" --th64-ticks="$ticks" --th64-nombre="caminata_$i.tsv" > "$O/caminatas/corsixth_$i.log" 2>&1
  "$sim" caminata "$(tmp "$n")" "$REPO_DIR/motor/assets/datam/animaciones.tan" "$O/caminatas/caminata_$i.tsv" \
    | sed 's/^/   /' || fallos=$((fallos + 1))
done < "$lista"
echo "fallos: $fallos"
[ "$fallos" -eq 0 ]
