#!/usr/bin/env bash
# Hito 6.2: las cuatro vistas del motor contra el hospital girado en CorsixTH.
#   1. los gráficos de alta resolución (tools/oraculo_mapa/alta.py), si no están;
#   2. el hospital de prueba: el nivel 1 con sus 8 salas
#      (bench/scenarios/crear_vistas.lua), guardado como vistas.sav, y llevado
#      al motor (exportar_escenario.lua, escenario.py) como
#      motor/assets/escenarios/vistas.trg, el «hospital de las vistas» del
#      menú de la ROM; solo si no está ya;
#   3. la tabla de piezas giradas del motor (th64sim tabla_giro);
#   4. para cada zona y cada vista, la captura de CorsixTH con el hospital
#      girado (bench/scenarios/captura_girada.lua), lo mismo con el motor
#      (th64sim escena4 --giro, con la cámara de la captura) y la comparación
#      (comparar_giro.py). Desde el hito 6.2b, sin sombras en los dos: el sol
#      del motor no gira y CorsixTH solo sabe dibujarlas con la luz girada
#      (las del motor se comprueban en motor/tests/test_vista.c). El motor
#      escribe además la vista con sombras (motor_<zona>_<vista>_sombras.ppm),
#      para mirarla.
#
# Uso: tools/hito6/vistas.sh [zonas] [dir]
#   zonas: casillas del mundo en el centro, «x,y» separadas por espacios; por
#     defecto las 10 de docs/06_juego_base.md
#   dir: por defecto $TH64_WORK/resultados/hito6/vistas (unos 3 minutos)
# Antes: el núcleo compilado en motor/build/pc y los assets del motor (tools/conv/conv.py).
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
ZONAS=${1:-"62,66 80,62 64,58 84,61 55,53 75,79 65,92 8,60 122,4 60,123"}
V=${2:-$TH64_WORK/resultados/hito6/vistas}
ALTA=$TH64_WORK/resultados/alta
SIM=$REPO_DIR/motor/build/pc/th64sim
A=$REPO_DIR/motor/assets
TRG=$A/escenarios/vistas.trg
# Los gráficos de alta resolución, si no están o si son de antes del hito 6.2b
# (sin el bloque 226).
if [ ! -f "$ALTA/vblk-0.tsh" ] || [ "$(od -An -j4 -N2 -tu1 "$ALTA/vblk-0.tsh" | tr -s ' ')" != " 0 227" ]; then
  python3 -I "$REPO_DIR/tools/oraculo_mapa/alta.py" "$ALTA" > /dev/null
fi
mkdir -p "$V/saves" "$V/oraculo"

if [ ! -f "$V/saves/vistas.sav" ]; then
  echo "== el hospital de prueba en CorsixTH"
  CORSIXTH_SAVES=$V/saves "$REPO_DIR/tools/bench_run.sh" "$REPO_DIR/bench/scenarios/crear_vistas.lua" "$V/crear" \
    > "$V/crear.log" 2>&1
fi
if [ ! -f "$TRG" ]; then
  echo "== llevado al motor: $TRG"
  CORSIXTH_SAVES=$V/saves "$REPO_DIR/tools/bench_run.sh" "$REPO_DIR/bench/scenarios/exportar_escenario.lua" \
    "$V/escenario" --load=vistas.sav > "$V/exportar.log" 2>&1
  "$SIM" mapa "$A/niveles/nivel01.tmp" 1 --volcar "$V/motor.tsv"
  python3 -I "$REPO_DIR/tools/oraculo_hito4/escenario.py" "$V/escenario" "$V/motor.tsv" "$TRG"
fi
"$SIM" tabla_giro "$V/tabla_giro.tsv"

malas=0
for c in $ZONAS; do
  for g in 0 1 2 3; do
    n=${c/,/_}_$g
    O=$V/oraculo
    CORSIXTH_SAVES=$V/saves "$REPO_DIR/tools/bench_run.sh" "$REPO_DIR/bench/scenarios/captura_girada.lua" \
      "$O/corsixth_$n" --load=vistas.sav --th64-png="$O/captura_$n.png" --th64-giro=$g \
      --th64-tabla="$V/tabla_giro.tsv" --th64-centro="$c" > "$O/corsixth_$n.log" 2>&1
    read -r _ x y _ sustitutos < "$O/captura_$n.png.txt"
    "$SIM" escena4 "$REPO_DIR/motor/assets" "$TRG" "$ALTA/vblk-0.tsh" "$ALTA/vspr-0.tsh" "$ALTA/animaciones.tan" \
      "$ALTA/mpalette.dat" 32 "$x" "$y" 640 480 0 0 "$O/motor_$n.ppm" "$O/motor_${n}_mezcla.pgm" \
      "$O/motor_${n}_ent.pgm" --giro=$g --camara-girada --sin-sombras > /dev/null
    "$SIM" escena4 "$REPO_DIR/motor/assets" "$TRG" "$ALTA/vblk-0.tsh" "$ALTA/vspr-0.tsh" "$ALTA/animaciones.tan" \
      "$ALTA/mpalette.dat" 32 "$x" "$y" 640 480 0 0 "$O/motor_${n}_sombras.ppm" --giro=$g --camara-girada > /dev/null
    echo "zona $c, vista $g ($sustitutos objetos sustitutos):"
    python3 -I "$REPO_DIR/tools/hito6/comparar_giro.py" "$O/captura_$n.png" "$O/motor_$n.ppm" \
      "$O/motor_${n}_ent.pgm" "$O/motor_${n}_mezcla.pgm" "$O/dif_$n.png" | sed 's/^/   /' || malas=$((malas + 1))
  done
done
echo "== $malas capturas con diferencias fuera de lo esperado"
[ "$malas" = 0 ]
