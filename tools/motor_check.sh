#!/usr/bin/env bash
# Comprobaciones del motor (fase 5), en este orden:
#   1. núcleo en el PC con gcc: -Wall -Wextra -Werror y pruebas (ctest);
#   2. lo mismo con ASan y UBSan;
#   3. ROM de pruebas en ares (modo Homebrew): las mismas pruebas sin fallos
#      y la misma huella de la simulación de referencia que en el PC; si hay
#      assets (tools/conv/conv.py), el RDP dibuja un sprite sin ningún píxel
#      distinto de lo que dicen la hoja y la paleta (hito 1), dibuja el mapa
#      igual que el rasterizador de referencia y el recorrido de la cámara va
#      a 30 fps sin saltos (hito 2);
#   4. ROM normal: compila y arranca.
# Uso: tools/motor_check.sh [dir_salida]   (por defecto $TH64_WORK/resultados/motor_check)
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$REPO_DIR/tools/env.sh"
OUT=${1:-$TH64_WORK/resultados/motor_check}
mkdir -p "$OUT"
M=$REPO_DIR/motor

paso() { echo "== $*"; }

paso "1. PC (gcc)"
cmake -S "$M" -B "$M/build/pc" -G Ninja > "$OUT/cmake_pc.log"
cmake --build "$M/build/pc" > "$OUT/build_pc.log"
ctest --test-dir "$M/build/pc" --output-on-failure > "$OUT/ctest_pc.log" || { cat "$OUT/ctest_pc.log"; exit 1; }
huella_pc=$("$M/build/pc/th64sim" huella 1 10000)
echo "   pruebas ok; huella de referencia $huella_pc"

paso "2. PC con ASan y UBSan"
cmake -S "$M" -B "$M/build/pc-asan" -G Ninja -DTH_SANITIZE=ON > "$OUT/cmake_asan.log"
cmake --build "$M/build/pc-asan" > "$OUT/build_asan.log"
ctest --test-dir "$M/build/pc-asan" --output-on-failure > "$OUT/ctest_asan.log" || { cat "$OUT/ctest_asan.log"; exit 1; }
echo "   pruebas ok"

paso "3. ROM de pruebas en ares"
make -s -C "$M" PRUEBAS=1 > "$OUT/make_pruebas.log"
ARES_SETTINGS="General/HomebrewMode=true" ARES_UNTIL="TH64 pruebas: fin" \
  "$REPO_DIR/tools/run_ares.sh" "$M/th64_pruebas.z64" "$OUT/ares_pruebas" 900 > /dev/null
log="$OUT/ares_pruebas/isviewer.log"
grep -q "TH64 pruebas: fin, 0 fallos" "$log" || { echo "   FALLO: las pruebas no terminaron sin fallos en la N64"; grep "TH64" "$log"; exit 1; }
huella_n64=$(sed -n 's/.*huella golden_sim(1, 10000) = \([0-9a-f]*\).*/\1/p' "$log")
[ "$huella_n64" = "$huella_pc" ] || { echo "   FALLO: huella N64 $huella_n64 frente a PC $huella_pc"; exit 1; }
grep "comprobaciones" "$log" | sed 's/^/   /'
echo "   huella igual en el PC y en la N64: $huella_n64"
if grep -q "assets ok" "$log"; then
  grep -q "TH64 hito1: .* 0 distintos" "$log" || { echo "   FALLO: el sprite dibujado no coincide"; grep "TH64 hito1" "$log"; exit 1; }
  grep "TH64 hito1" "$log" | sed 's/^/   /'
  grep -q "TH64 hito2: comparacion con la referencia: 0 pixeles distintos" "$log" || { echo "   FALLO: el mapa no se dibuja como la referencia"; grep "TH64 hito2" "$log"; exit 1; }
  grep -q "TH64 hito2: recorrido .* 0 saltos" "$log" || { echo "   FALLO: el recorrido no va a 30 fps"; grep "TH64 hito2" "$log"; exit 1; }
  grep "TH64 hito2: \(comparacion\|recorrido\|por fotograma\)" "$log" | sed 's/^/   /'
else
  echo "   (sin assets: no se comprueba el dibujo; ejecuta tools/conv/conv.py)"
fi

paso "4. ROM normal"
make -s -C "$M" > "$OUT/make_rom.log"
# Con assets, la ROM avisa de que ha cargado el nivel; sin ellos, solo del heap.
hasta="TH64 motor: heap"
grep -q "assets ok" "$log" && hasta="TH64 hito2: nivel"
ARES_UNTIL="$hasta" "$REPO_DIR/tools/run_ares.sh" "$M/th64.z64" "$OUT/ares_rom" 60 > /dev/null
grep "TH64 motor\|TH64 hito2" "$OUT/ares_rom/isviewer.log" | sed 's/^/   /'
grep -q "$hasta" "$OUT/ares_rom/isviewer.log" || { echo "   FALLO: la ROM normal no arranca"; exit 1; }
echo "== todo correcto"
