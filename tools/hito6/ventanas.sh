#!/usr/bin/env bash
# La demo jugable, pieza 5: las ventanas de la partida (motor/ui/ventanas.h)
# dibujadas con el rasterizador de referencia sobre el escenario de la demo,
# en los estados de tools/hito6/ventanas.txt (th64sim ventanas baja): la
# pantalla entera de 320 × 240 en PPM y, si hay Python, en PNG ampliado ×2.
# Son arte derivado de Theme Hospital: no van al repositorio.
#
# Uso: tools/hito6/ventanas.sh [dir]   (por defecto $TH64_WORK/resultados/hito6/ventanas; unos segundos)
# Antes: el núcleo compilado en motor/build/pc y los assets del motor (tools/conv/conv.py, tools/hito6/demo.sh).
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
O=${1:-$TH64_WORK/resultados/hito6/ventanas}
SIM=$REPO_DIR/motor/build/pc/th64sim
A=$REPO_DIR/motor/assets
mkdir -p "$O"
"$SIM" ventanas baja "$A" "$A/escenarios/demo.trg" "$REPO_DIR/tools/hito6/ventanas.txt" "$O" | tee "$O/ventanas.txt"
python3 -I - "$O" <<'EOF'
import glob, struct, sys, zlib
for ruta in sorted(glob.glob(sys.argv[1] + '/*.ppm')):
    d = open(ruta, 'rb').read()
    cab = d.split(b'\n', 3)
    w, h = map(int, cab[1].split())
    rgb, k = cab[3], 2
    filas = []
    for y in range(h):
        fila = b''.join(rgb[(y * w + x) * 3:(y * w + x) * 3 + 3] * k for x in range(w))
        filas += [b'\0' + fila] * k
    def trozo(t, x):
        return struct.pack('>I', len(x)) + t + x + struct.pack('>I', zlib.crc32(t + x) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + trozo(b'IHDR', struct.pack('>IIBBBBB', w * k, h * k, 8, 2, 0, 0, 0))
    png += trozo(b'IDAT', zlib.compress(b''.join(filas), 6)) + trozo(b'IEND', b'')
    open(ruta[:-4] + '.png', 'wb').write(png)
EOF
