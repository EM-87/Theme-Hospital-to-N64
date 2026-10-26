#!/usr/bin/env bash
# Graba lo que suena en la N64: la ROM n64/pruebas/musica toca las 8 piezas
# con el banco recortado y la configuración de sonido del motor, y vuelca cada
# bloque que mezcla la consola por EMUX (modo Homebrew de ares). Después
# rehace un WAV por pieza a 22.047 Hz y otro a 48 kHz para comparar.py.
#
# Uso: tools/musica/grabar_ares.sh [dir_salida]  (por defecto $TH64_WORK/resultados/musica/ares)
# Antes: make -C motor (los MID64 y el SF64) y make -C n64/pruebas/musica.
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
export TH64_WORK
O=$(mkdir -p "${1:-$TH64_WORK/resultados/musica/ares}" && cd "${1:-$TH64_WORK/resultados/musica/ares}" && pwd)
ARES_SETTINGS="General/HomebrewMode=true" ARES_UNTIL="TH64 musica: fin" \
  "$REPO_DIR/tools/run_ares.sh" "$REPO_DIR/n64/pruebas/musica/th64musica.z64" "$O/log" 3600 > /dev/null
grep -q "TH64 musica: fin" "$O/log/isviewer.log" || { echo "la ROM no llegó al final"; grep -a TH64 "$O/log/isviewer.log"; exit 1; }
python3 -I "$REPO_DIR/tools/musica/decodificar.py" "$O/log/isviewer.log" "$O"
for f in "$O"/*_n64.wav; do
  ffmpeg -v error -y -i "$f" -af aresample=48000:resampler=soxr "${f%.wav}48.wav"
done
rm "$O/log/isviewer.log" # unos 150 MiB
