#!/usr/bin/env bash
# Compara, pieza a pieza, la grabación de la N64 (grabar_ares.sh) con las
# síntesis de fluidsynth (fluidsynth.sh), y escribe además un A/B de 25 s
# por pieza para escuchar (primero la N64, luego fluidsynth como en el PC).
#
# Uso: tools/musica/comparar_todo.sh [dir_resultados]  (por defecto $TH64_WORK/resultados/musica)
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
R=${1:-$TH64_WORK/resultados/musica}
mkdir -p "$R/ab"
for p in atlantis candy doctor fortune mend nightsh sixseven steady; do
  echo "== $p"
  for v in pc seco sinlfo sinlfo16; do
    python3 -I "$REPO_DIR/tools/musica/comparar.py" "$R/fluidsynth/${p}_$v.wav" "$R/ares/${p}_n6448.wav" \
      --nombres "$v,n64" | sed "s/^comparación:/n64 frente a $v:/"
  done
  python3 -I "$REPO_DIR/tools/musica/comparar.py" "$R/fluidsynth/${p}_pc.wav" "$R/ares/${p}_n6448.wav" \
    --ab "$R/ab/$p.wav" --desde 10 > /dev/null
  ffmpeg -v error -y -i "$R/ab/$p.wav" -c:a libmp3lame -q:a 2 "$R/ab/$p.mp3" && rm "$R/ab/$p.wav"
done
