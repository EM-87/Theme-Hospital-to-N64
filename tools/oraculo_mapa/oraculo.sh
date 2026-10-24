#!/usr/bin/env bash
# Compara la vista del mapa del motor con CorsixTH a 640 × 480: captura el
# mapa de un nivel en CorsixTH (bench/scenarios/captura_nivel.lua), lo dibuja
# con th64sim vista desde las mismas posiciones de cámara y con los mismos
# gráficos (DATA/VBLK-0) y compara píxel a píxel (comparar.py).
#
# Uso: tools/oraculo_mapa/oraculo.sh <nivel> [casillas] [dir_salida]
#   casillas: "x,y;x,y;..." centros de las capturas; «i» = cámara inicial
#   dir_salida: por defecto $TH64_WORK/resultados/hito2/vista
# Antes: tools/conv/conv.py niveles y el núcleo compilado en motor/build/pc.
# Los gráficos de alta resolución van a $TH64_WORK/resultados/alta (alta.py).
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
nivel=$1; casillas=${2:-i}
out=${3:-$TH64_WORK/resultados/hito2/vista}/nivel$nivel
mkdir -p "$out"
sim=$REPO_DIR/motor/build/pc/th64sim
tmp=$REPO_DIR/motor/assets/niveles/nivel$(printf %02d "$nivel").tmp
alta=$TH64_WORK/resultados/alta
[ -f "$alta/vblk-0.tsh" ] || python3 -I "$REPO_DIR/tools/oraculo_mapa/alta.py" "$alta"
"$REPO_DIR/tools/bench_run.sh" "$REPO_DIR/bench/scenarios/captura_nivel.lua" "$out/corsixth" \
  --th64-nivel="$nivel" --th64-casillas="$casillas" > "$out/corsixth.log" 2>&1
fallos=0
while IFS=$'\t' read -r png sx sy w h; do
  b=$(basename "$png" .png)
  echo "== $b: desplazamiento ($sx, $sy)"
  "$sim" vista "$tmp" "$nivel" "$alta/vblk-0.tsh" "$alta/mpalette.dat" 32 "$sx" "$sy" "$w" "$h" \
    "$out/$b.ppm" "$out/${b}_mezcla.pgm" | sed 's/^/   /'
  python3 -I "$REPO_DIR/tools/oraculo_mapa/comparar.py" "$png" "$out/$b.ppm" "$out/${b}_dif.png" \
    --mezcla "$out/${b}_mezcla.pgm" | sed 's/^/   /' || fallos=$((fallos + 1))
done < "$out/corsixth/capturas.txt"
echo "capturas con píxeles distintos: $fallos"
