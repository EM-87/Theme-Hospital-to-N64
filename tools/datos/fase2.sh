#!/usr/bin/env bash
# Fase 2: inventario de los datos de Theme Hospital y su tamaño en cartucho.
# Uso: tools/datos/fase2.sh [dir_trabajo] [dir_resultados]
#   dir_trabajo:    datos descomprimidos, WAV, MIDI y blobs (fuera del repo;
#                   por defecto $TH64_WORK/fase2)
#   dir_resultados: JSON resumidos (por defecto bench/resultados/fase2)
# Necesita el entorno de tools/setup.sh (libdragon, CorsixTH compilado para
# rnc_decode, ffmpeg, fluidsynth y TimGM6mb). Tarda unos 30 min; casi todo es
# mkasset -c 3 (Shrinkler).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
source "$repo/tools/env.sh"
work=${1:-$TH64_WORK/fase2}
res=${2:-$repo/bench/resultados/fase2}
mkdir -p "$work" "$res"
py() { python3 -I "$@"; }

py "$here/inventario.py" "$TH_DATA_DIR" "$work" "$res/inventario.json"
py "$here/graficos.py" "$work" "$res/graficos.json"
py "$here/comprimir.py" "$work" "$res/inventario.json" "$res/compresion.json"
py "$here/sonido.py" "$work" "$res/sonido.json"
py "$here/audio.py" "$work" "$res/sonido.json" "$res/audio.json"
py "$here/musica.py" "$work" "$res/musica.json"
py "$here/video.py" "$TH_DATA_DIR" "$res/video.json"
py "$here/codigo.py" "$CORSIXTH_DIR/CorsixTH/Lua" "$res/codigo.json"
py "$here/muestra_ci4.py" "$work" "$repo/docs/img/02_sprites_ci4.png" 16
