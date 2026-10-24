#!/usr/bin/env bash
# Compara con CorsixTH, a 640 × 480, un humanoide caminando sobre el mapa:
# para cada escena, CorsixTH hace caminar a un humanoide
# (bench/scenarios/caminata.lua), se para en un tick y hace una captura
# centrada en él; th64sim escena repite la caminata con el núcleo hasta ese
# tick y dibuja lo mismo con los gráficos de DATA; comparar.py cuenta los
# píxeles distintos, aparte los del humanoide.
#
# Uso: tools/oraculo_mapa/escenas.sh [lista] [dir_salida]
#   lista: fichero con «nivel x1,y1;x2,y2 tick» por línea (por defecto, las 8
#          escenas de docs/05_prototipo.md)
#   dir_salida: por defecto $TH64_WORK/resultados/hito3/escenas
# Antes: tools/conv/conv.py y el núcleo compilado en motor/build/pc.
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
O=${2:-$TH64_WORK/resultados/hito3/escenas}
mkdir -p "$O"
A=$TH64_WORK/resultados/alta
[ -f "$A/vspr-0.tsh" ] || python3 -I "$REPO_DIR/tools/oraculo_mapa/alta.py" "$A"
lista=${1:-}
if [ -z "$lista" ]; then
  lista=$O/escenas.txt
  cat > "$lista" <<'FIN'
1 62,75;70,65 12
1 62,75;70,65 37
1 62,75;70,65 85
1 86,70;54,70 60
1 86,70;54,70 133
5 47,74;76,89 90
5 61,80;20,67 200
12 65,82;59,69 50
FIN
fi
i=0
while read -r n ruta tick; do
  i=$((i + 1))
  png=$O/escena_$i.png
  "$REPO_DIR/tools/bench_run.sh" "$REPO_DIR/bench/scenarios/caminata.lua" "$O/corsixth_$i" --th64-nivel="$n" \
    --th64-ruta="$ruta" --th64-tick-png="$tick" --th64-png="$png" --th64-nombre="caminata_$i.tsv" > /dev/null 2>&1
  read -r k sx sy < "$png.txt"
  echo "== escena $i: nivel $n, $ruta, tick $k, desplazamiento ($sx,$sy)"
  "$REPO_DIR/motor/build/pc/th64sim" escena "$REPO_DIR/motor/assets/niveles/nivel$(printf %02d "$n").tmp" \
    "$A/vblk-0.tsh" "$A/vspr-0.tsh" "$A/animaciones.tan" "$A/mpalette.dat" 32 "$sx" "$sy" 640 480 \
    "$O/corsixth_$i/caminata_$i.tsv" "$tick" "$O/escena_$i.ppm" "$O/escena_${i}_mezcla.pgm" \
    "$O/escena_${i}_entidades.pgm" | sed 's/^/   /'
  python3 -I "$REPO_DIR/tools/oraculo_mapa/comparar.py" "$png" "$O/escena_$i.ppm" "$O/escena_${i}_dif.png" \
    --mezcla "$O/escena_${i}_mezcla.pgm" --entidades "$O/escena_${i}_entidades.pgm" | sed 's/^/   /' || true
done < "$lista"
