#!/usr/bin/env bash
# Sintetiza las 8 piezas con fluidsynth y TimGM6mb, a 48 kHz, de tres
# formas, para compararlas con la N64 (grabar_ares.sh). Se usa el banco
# entero: con fluidsynth suena igual que el recortado (tools/conv/README.md),
# y el del motor lleva además el ajuste de atenuación para la N64.
#   <pieza>_pc.wav      como en el PC: fluidsynth con su reverberación y coro
#   <pieza>_seco.wav    sin reverberación ni coro (-R 0 -C 0)
#   <pieza>_sinlfo.wav  seco y con el banco sin filtro ni LFO (banco_n64.py):
#                       lo más parecido a lo que hace el sintetizador SF64
#   <pieza>_sinlfo16.wav  lo mismo con 16 voces, como en la N64
#
# Uso: tools/musica/fluidsynth.sh [dir_salida]  (por defecto $TH64_WORK/resultados/musica/fluidsynth)
# Antes: make -C motor (motor/assets/musica, sale de los datos del juego).
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
O=$(mkdir -p "${1:-$TH64_WORK/resultados/musica/fluidsynth}" && cd "${1:-$TH64_WORK/resultados/musica/fluidsynth}" && pwd)
M=$REPO_DIR/motor/assets/musica
B=/usr/share/sounds/sf2/TimGM6mb.sf2
python3 -I "$REPO_DIR/tools/musica/banco_n64.py" "$B" "$O/banco_n64.sf2"
for p in atlantis candy doctor fortune mend nightsh sixseven steady; do
  fluidsynth -q -n -i -r 48000 -F "$O/${p}_pc.wav" "$B" "$M/$p.mid" > /dev/null 2>&1
  fluidsynth -q -n -i -r 48000 -R 0 -C 0 -F "$O/${p}_seco.wav" "$B" "$M/$p.mid" > /dev/null 2>&1
  fluidsynth -q -n -i -r 48000 -R 0 -C 0 -F "$O/${p}_sinlfo.wav" "$O/banco_n64.sf2" "$M/$p.mid" > /dev/null 2>&1
  fluidsynth -q -n -i -r 48000 -R 0 -C 0 -o synth.polyphony=16 -F "$O/${p}_sinlfo16.wav" "$O/banco_n64.sf2" \
    "$M/$p.mid" > /dev/null 2>&1
done
ls "$O"/*.wav | wc -l
