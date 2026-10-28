#!/usr/bin/env bash
# Comprobaciones del motor (fase 5), en este orden:
#   1. núcleo en el PC con gcc: -Wall -Wextra -Werror y pruebas (ctest);
#   2. lo mismo con ASan y UBSan;
#   3. ROM de pruebas en ares (modo Homebrew): las mismas pruebas sin fallos
#      y la misma huella de la simulación de referencia que en el PC; si hay
#      assets (tools/conv/conv.py), el RDP dibuja un sprite sin ningún píxel
#      distinto de lo que dicen la hoja y la paleta (hito 1), dibuja el mapa
#      igual que el rasterizador de referencia y el recorrido de la cámara va
#      a 30 fps sin saltos (hito 2); lo mismo con humanoides caminando, y la
#      cámara que sigue a uno va a 30 fps sin saltos (hito 3); las reglas y los
#      textos se leen en la N64 con la misma huella que en el PC, se cargan las
#      fuentes y suenan la música y un efecto (hito 0); con el escenario del
#      hito 4 (assets/escenarios/hito4.trg), 3.000 horas de hospital dan la
#      misma huella que en el PC, el RDP dibuja el hospital (objetos y
#      personas) igual que la referencia y el hospital en marcha va a 30 fps;
#   4. ROM normal: compila y arranca.
# En las dos ROM comprueba también que el bucle del dibujo no choca con las
# funciones de rdpq que llama en la caché de instrucciones
# (tools/cache_instrucciones.py).
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
huella_h4=""
if [ -f "$M/assets/escenarios/hito4.trg" ]; then
  mkdir -p "$OUT/hito4"
  linea=$("$M/build/pc/th64sim" hito4 "$M/assets" "$M/assets/escenarios/hito4.trg" 1 3000 "$OUT/hito4")
  huella_h4=$(echo "$linea" | sed -n 's/.*huella \([0-9a-f]*\).*/\1/p')
  echo "   $linea"
fi
if [ -d "$M/assets/reglas" ]; then
  python3 -I "$REPO_DIR/tools/conv/reglas.py" comprobar "$M/build/pc/th64sim" "$M/assets" | sed 's/^/   /' \
    || { echo "   FALLO: el núcleo no lee las reglas como el conversor"; exit 1; }
fi

paso "2. PC con ASan y UBSan"
cmake -S "$M" -B "$M/build/pc-asan" -G Ninja -DTH_SANITIZE=ON > "$OUT/cmake_asan.log"
cmake --build "$M/build/pc-asan" > "$OUT/build_asan.log"
ctest --test-dir "$M/build/pc-asan" --output-on-failure > "$OUT/ctest_asan.log" || { cat "$OUT/ctest_asan.log"; exit 1; }
echo "   pruebas ok"

paso "3. ROM de pruebas en ares"
make -s -C "$M" PRUEBAS=1 > "$OUT/make_pruebas.log"
python3 -I "$REPO_DIR/tools/cache_instrucciones.py" "$N64_INST/bin/mips64-elf-nm" \
  "$M/build/n64_pruebas/th64_pruebas.elf" --relleno | sed 's/^/   /' \
  || { echo "   FALLO: el dibujo choca con rdpq en la caché de instrucciones (motor/platform/n64/relleno_cache.c)"; exit 1; }
ARES_SETTINGS="General/HomebrewMode=true" ARES_UNTIL="TH64 pruebas: fin" \
  "$REPO_DIR/tools/run_ares.sh" "$M/th64_pruebas.z64" "$OUT/ares_pruebas" 900 > /dev/null
log="$OUT/ares_pruebas/isviewer.log"
grep -q "TH64 pruebas: fin, 0 fallos" "$log" || { echo "   FALLO: las pruebas no terminaron sin fallos en la N64"; grep "TH64" "$log"; exit 1; }
huella_n64=$(sed -n 's/.*huella golden_sim(1, 10000) = \([0-9a-f]*\).*/\1/p' "$log")
[ "$huella_n64" = "$huella_pc" ] || { echo "   FALLO: huella N64 $huella_n64 frente a PC $huella_pc"; exit 1; }
grep "comprobaciones" "$log" | sed 's/^/   /'
echo "   huella igual en el PC y en la N64: $huella_n64"
if grep -q "assets ok" "$log"; then
  for f in definiciones full01 english spanish; do
    grep -q "TH64 reglas: .*/$f.trg: .*huella" "$log" || continue
    h_n64=$(sed -n "s/.*TH64 reglas: .*$f.trg: .*huella \([0-9a-f]*\).*/\1/p" "$log")
    ruta="$M/assets/reglas/$f.trg"
    [ -f "$ruta" ] || ruta="$M/assets/textos/$f.trg"
    h_pc=$("$M/build/pc/th64sim" reglas "$ruta" 2>&1 > /dev/null | sed -n 's/^huella //p')
    [ "$h_n64" = "$h_pc" ] || { echo "   FALLO: reglas $f: huella N64 $h_n64 frente a PC $h_pc"; exit 1; }
    grep "TH64 reglas: .*/$f.trg" "$log" | sed 's/^/   /'
  done
  if ls "$M"/assets/fuentes/*.fnt > /dev/null 2>&1; then
    grep -q "TH64 fuentes: 5 de 5 cargadas" "$log" || { echo "   FALLO: no se cargan las fuentes"; grep "TH64 fuentes" "$log"; exit 1; }
    grep "TH64 fuentes" "$log" | sed 's/^/   /'
  fi
  if [ -f "$M/assets/musica/banco.sf2" ]; then
    grep -q "TH64 sonido: .*sonando" "$log" || { echo "   FALLO: no suena el sonido"; grep "TH64 sonido" "$log"; exit 1; }
    grep "TH64 sonido" "$log" | sed 's/^/   /'
  fi
  grep -q "TH64 hito1: .* 0 distintos" "$log" || { echo "   FALLO: el sprite dibujado no coincide"; grep "TH64 hito1" "$log"; exit 1; }
  grep "TH64 hito1" "$log" | sed 's/^/   /'
  grep -q "TH64 hito2: comparacion con la referencia: 0 pixeles distintos" "$log" || { echo "   FALLO: el mapa no se dibuja como la referencia"; grep "TH64 hito2" "$log"; exit 1; }
  grep -q "TH64 hito2: recorrido .* 0 saltos" "$log" || { echo "   FALLO: el recorrido no va a 30 fps"; grep "TH64 hito2" "$log"; exit 1; }
  grep "TH64 hito2: \(comparacion\|recorrido\|por fotograma\)" "$log" | sed 's/^/   /'
  grep -q "TH64 hito3: comparacion con la referencia: 0 pixeles distintos" "$log" || { echo "   FALLO: los humanoides no se dibujan como la referencia"; grep "TH64 hito3" "$log"; exit 1; }
  grep -q "TH64 hito3: recorrido siguiendo a un humanoide .* 0 saltos" "$log" || { echo "   FALLO: el recorrido con un humanoide no va a 30 fps"; grep "TH64 hito3" "$log"; exit 1; }
  grep "TH64 hito3: \(comparacion\|recorrido\|por fotograma\|siguiendo\|con 50\)" "$log" | sed 's/^/   /'
  if [ -n "$huella_h4" ]; then
    h=$(sed -n 's/.*TH64 hito4: 3000 horas: .*huella \([0-9a-f]*\).*/\1/p' "$log")
    [ "$h" = "$huella_h4" ] || { echo "   FALLO: hospital: huella N64 $h frente a PC $huella_h4"; grep "TH64 hito4" "$log"; exit 1; }
    grep -q "TH64 hito4: comparacion con la referencia: 0 pixeles distintos" "$log" || { echo "   FALLO: el hospital no se dibuja como la referencia"; grep "TH64 hito4" "$log"; exit 1; }
    grep -q "TH64 hito4: recorrido .* 0 saltos" "$log" || { echo "   FALLO: el hospital en marcha no va a 30 fps"; grep "TH64 hito4" "$log"; exit 1; }
    grep "TH64 hito4" "$log" | sed 's/^/   /'
  fi
  grep -q "TH64 hito6.1: comparacion de la interfaz con la referencia: 0 pixeles distintos" "$log" || { echo "   FALLO: la interfaz no se dibuja como la referencia"; grep "TH64 hito6.1" "$log"; exit 1; }
  grep "TH64 hito6.1" "$log" | sed 's/^/   /'
else
  echo "   (sin assets: no se comprueba el dibujo; ejecuta tools/conv/conv.py)"
fi

paso "4. ROM normal"
make -s -C "$M" > "$OUT/make_rom.log"
python3 -I "$REPO_DIR/tools/cache_instrucciones.py" "$N64_INST/bin/mips64-elf-nm" "$M/build/n64/th64.elf" --relleno \
  | sed 's/^/   /' \
  || { echo "   FALLO: el dibujo choca con rdpq en la caché de instrucciones (motor/platform/n64/relleno_cache.c)"; exit 1; }
# Con assets, la ROM avisa de que ha cargado el nivel; sin ellos, solo del heap.
hasta="TH64 motor: heap"
grep -q "assets ok" "$log" && hasta="TH64 hito3: nivel"
[ -n "$huella_h4" ] && grep -q "assets ok" "$log" && hasta="TH64 hito4: hospital del escenario cargado"
ARES_UNTIL="$hasta" "$REPO_DIR/tools/run_ares.sh" "$M/th64.z64" "$OUT/ares_rom" 60 > /dev/null
grep "TH64 motor\|TH64 hito3\|TH64 hito4" "$OUT/ares_rom/isviewer.log" | sed 's/^/   /'
grep -q "$hasta" "$OUT/ares_rom/isviewer.log" || { echo "   FALLO: la ROM normal no arranca"; exit 1; }
echo "== todo correcto"
