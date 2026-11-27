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
#      la interfaz (hito 6.1) y las tres vistas giradas del hospital (hito
#      6.2) igual que la referencia, y estas con la misma lista de dibujo que
#      th64sim lista --giro;
#   3b. la partida de prueba de la demo jugable (comprobar_partida en la ROM de
#      pruebas: demo.trg, semilla 1, las 13 órdenes de ordenes/demo.tsv y
#      3.000 horas): tools/hito6/repetir.sh la repite en el PC con las órdenes
#      que grabó la ROM y tiene que dar «repetir: bien» (las mismas órdenes con
#      el mismo resultado, la misma huella y el mismo saldo); y las ventanas
#      de la partida en la ROM (comprobar_ventanas: los 15 estados de
#      tools/hito6/ventanas.txt dibujados por el RDP a 0 píxeles de la
#      referencia; comprobar_ventanas_partida: tools/hito6/ventanas_partida.txt,
#      la misma partida jugada con el puntero hasta las 3.000 horas, que
#      repetir.sh repite y que da el mismo registro que th64sim ventanas);
#   4. ROM normal: compila y arranca en el menú.
# En las dos ROM, y en las de make HORAS=0 MODO=4 y make MEDIR=1 (solo se
# compilan), comprueba también que el bucle del dibujo no choca con las
# funciones de rdpq que llama en la caché de instrucciones
# (tools/cache_instrucciones.py), con el margen que queda (avisa si baja de
# 256 B). De la ROM de pruebas, avisa también si la corrutina de la
# simulación usa más de la mitad de su pila.
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
# La pila de la corrutina de la simulación, pintada (simulacion.c): lo más
# que se usó en todas las pruebas, contra la mitad de su tamaño.
read -r pila_usada pila_total < <(sed -n 's/.*TH64 pruebas: pila de la simulacion \(-*[0-9]*\) B de \([0-9]*\).*/\1 \2/p' "$log") || true
echo "   pila de la simulación: ${pila_usada:-?} B de ${pila_total:-?}"
if [ -n "${pila_usada:-}" ] && [ "$pila_usada" -gt $((pila_total / 2)) ]; then
  echo "   AVISO: la simulación usa más de la mitad de su pila (PILA en motor/platform/n64/simulacion.c)"
fi
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
  if [ -n "$huella_h4" ]; then
    # Hito 6.2: las vistas giradas del hospital, dibujadas como la referencia y
    # con la misma lista de dibujo que el PC (th64sim lista --giro).
    grep -q "TH64 hito6.2: comparacion de las vistas giradas con la referencia: 0 pixeles distintos" "$log" || { echo "   FALLO: las vistas giradas no se dibujan como la referencia"; grep "TH64 hito6.2" "$log"; exit 1; }
    D=$M/assets/datam
    while read -r r x y h; do
      h_pc=$("$M/build/pc/th64sim" lista "$M/assets" "$M/assets/escenarios/hito4.trg" "$D/mblk-0.tsh" "$D/mspr-0.tsh" \
        "$D/animaciones.tan" 16 "$x" "$y" 320 240 3000 1 --giro="$r" --camara-girada | sed -n 's/.*huella \([0-9a-f]*\).*/\1/p')
      [ "$h" = "$h_pc" ] || { echo "   FALLO: vista $r: lista de dibujo N64 $h frente a PC $h_pc"; exit 1; }
    done < <(sed -n 's/.*TH64 hito6.2: vista \([0-9]\), camara (\(-*[0-9]*\),\(-*[0-9]*\)): .*huella \([0-9a-f]*\).*/\1 \2 \3 \4/p' "$log")
    grep "TH64 hito6.2" "$log" | sed 's/^/   /'
    echo "   listas de dibujo de las vistas giradas iguales en el PC y en la N64"
  fi
else
  echo "   (sin assets: no se comprueba el dibujo; ejecuta tools/conv/conv.py)"
fi

paso "3b. La partida de prueba de la demo, repetida en el PC"
if grep -q "TH64 partida nueva: escenario demo" "$log"; then
  # La partida de demo.tsv (comprobar_partida) es la primera del registro: todo
  # lo de antes de las ventanas (TH64 demo5), porque repetir.sh toma la última.
  sed '/TH64 demo5:/,$d' "$log" > "$OUT/partida_demo.log"
  grep "TH64 partida" "$OUT/partida_demo.log" | sed 's/^/   /'
  "$REPO_DIR/tools/hito6/repetir.sh" "$OUT/partida_demo.log" "" "$OUT/repetir" > "$OUT/repetir.txt" 2>&1 || true
  sed 's/^/   /' "$OUT/repetir.txt"
  grep -q "^repetir: bien" "$OUT/repetir.txt" || { echo "   FALLO: la partida de la ROM no se repite igual en el PC"; exit 1; }
elif [ -f "$M/assets/escenarios/demo.trg" ]; then
  echo "   FALLO: la ROM de pruebas no jugó la partida de prueba (TH64 partida nueva)"; exit 1
else
  echo "   (sin motor/assets/escenarios/demo.trg: no se prueba; tools/hito6/demo.sh)"
fi
if [ -f "$M/assets/escenarios/demo.trg" ]; then
  # Las ventanas en la ROM (comprobar_ventanas y comprobar_ventanas_partida):
  # las tablas de main.c copian tools/hito6/ventanas.txt y ventanas_partida.txt;
  # los 15 estados y la pantalla del final de la partida, a 0 píxeles distintos
  # de la referencia; la partida jugada desde las ventanas (la última del
  # registro) se repite en el PC con repetir.sh, y sus líneas del registro son
  # las mismas que escribe th64sim ventanas con el mismo guion.
  python3 -I - "$M/platform/n64/main.c" "$REPO_DIR/tools/hito6/ventanas.txt" "$REPO_DIR/tools/hito6/ventanas_partida.txt" <<'PY' \
    || { echo "   FALLO: las tablas de las ventanas de main.c no copian los guiones"; exit 1; }
import re, sys
c = open(sys.argv[1]).read()
for tabla, ruta in (("estados_ventanas", sys.argv[2]), ("partida_ventanas", sys.argv[3])):
    m = re.search(r"static const char \*const " + tabla + r"\[\] = \{(.*?)\n\};", c, re.S)
    rom = re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)) if m else []
    txt = [l.rstrip("\n") for l in open(ruta) if l.strip() and not l.startswith("#")]
    if rom != txt:
        sys.exit(f"{tabla} no es {ruta}")
print(f"   tablas de main.c iguales a los guiones")
PY
  grep "TH64 demo5" "$log" | grep -v ": empieza" | sed 's/^/   /'
  # Los 15 estados en el PC: la huella de la lista entera y el registro de la
  # partida de cada uno, iguales en la ROM (tools/hito6/ventanas_estados.py).
  mkdir -p "$OUT/ventanas_estados"
  "$M/build/pc/th64sim" ventanas baja "$M/assets" "$M/assets/escenarios/demo.trg" \
    "$REPO_DIR/tools/hito6/ventanas.txt" "$OUT/ventanas_estados" > "$OUT/ventanas_estados/salida.txt" \
    || { echo "   FALLO: th64sim ventanas baja sobre ventanas.txt"; exit 1; }
  python3 -I "$REPO_DIR/tools/hito6/ventanas_estados.py" "$log" "$OUT/ventanas_estados" \
    "$REPO_DIR/tools/hito6/ventanas.txt" \
    || { echo "   FALLO: los estados de las ventanas de la ROM no son los de th64sim ventanas"; exit 1; }
  grep -q "TH64 demo5: comparacion de las ventanas con la referencia: 0 pixeles distintos en total" "$log" \
    || { echo "   FALLO: las ventanas no se dibujan como la referencia en la ROM"; exit 1; }
  grep -q "TH64 demo5: la partida desde las ventanas contra la referencia: 0 pixeles distintos" "$log" \
    || { echo "   FALLO: la partida desde las ventanas no se dibuja como la referencia"; exit 1; }
  sed -n '/TH64 demo5: comparacion de las ventanas/,$p' "$log" | grep -q "TH64 partida nueva: escenario demo" \
    || { echo "   FALLO: la ROM de pruebas no jugó la partida desde las ventanas"; exit 1; }
  "$REPO_DIR/tools/hito6/repetir.sh" "$log" "" "$OUT/repetir_ventanas" > "$OUT/repetir_ventanas.txt" 2>&1 || true
  sed 's/^/   /' "$OUT/repetir_ventanas.txt"
  grep -q "^repetir: bien" "$OUT/repetir_ventanas.txt" \
    || { echo "   FALLO: la partida desde las ventanas de la ROM no se repite igual en el PC"; exit 1; }
  mkdir -p "$OUT/ventanas_partida"
  "$M/build/pc/th64sim" ventanas baja "$M/assets" "$M/assets/escenarios/demo.trg" \
    "$REPO_DIR/tools/hito6/ventanas_partida.txt" "$OUT/ventanas_partida" > "$OUT/ventanas_partida/salida.txt"
  sed -n '/TH64 demo5: comparacion de las ventanas/,$p' "$log" | tr -d '\r' \
    | sed -n 's/^.*\(TH64 partida nueva: \)/\1/p; s/^.*\(TH64 orden\t\)/\1/p; s/^.*\(TH64 partida: [0-9]* horas, huella\)/\1/p' \
    | sed -n '/TH64 partida nueva/,/TH64 partida: [0-9]* horas/p' > "$OUT/ventanas_partida/rom_las_dos.log"
  # Dos partidas desde las ventanas (comprobar_ventanas_partida): la del guion
  # y la de los clics por el mando con la hora troceada (fotogramas a medio
  # tick y órdenes retenidas); las dos con el registro de th64sim ventanas.
  n=$(grep -c "TH64 partida nueva" "$OUT/ventanas_partida/rom_las_dos.log")
  [ "$n" = 2 ] || { echo "   FALLO: $n partidas desde las ventanas en el registro de la ROM, en vez de 2"; exit 1; }
  for k in 1 2; do
    awk -v k="$k" '/TH64 partida nueva/{p++} p==k' "$OUT/ventanas_partida/rom_las_dos.log" \
      > "$OUT/ventanas_partida/rom$k.log"
    cmp -s "$OUT/ventanas_partida/rom$k.log" "$OUT/ventanas_partida/partida.log" \
      || { echo "   FALLO: el registro de la partida $k desde las ventanas no es el de th64sim ventanas"; \
           diff "$OUT/ventanas_partida/rom$k.log" "$OUT/ventanas_partida/partida.log" | head; exit 1; }
  done
  grep -q "TH64 demo5: la partida desde las ventanas con mando y la hora troceada: .*: iguales" "$log" \
    || { echo "   FALLO: la partida desde las ventanas con mando y la hora troceada no da lo mismo"; exit 1; }
  echo "   registro de las dos partidas desde las ventanas (el guion; el mando con la hora troceada) igual en la"
  echo "   ROM y en th64sim ventanas: $(tail -1 "$OUT/ventanas_partida/partida.log")"
fi

paso "4. ROM normal"
make -s -C "$M" > "$OUT/make_rom.log"
python3 -I "$REPO_DIR/tools/cache_instrucciones.py" "$N64_INST/bin/mips64-elf-nm" "$M/build/n64/th64.elf" --relleno \
  | sed 's/^/   /' \
  || { echo "   FALLO: el dibujo choca con rdpq en la caché de instrucciones (motor/platform/n64/relleno_cache.c)"; exit 1; }
# La ROM normal arranca en el menú de la ROM de prueba (hito 6.2).
hasta="TH64 menu: ROM de prueba lista"
ARES_UNTIL="$hasta" "$REPO_DIR/tools/run_ares.sh" "$M/th64.z64" "$OUT/ares_rom" 60 > /dev/null
grep "TH64 motor\|TH64 hito3\|TH64 menu" "$OUT/ares_rom/isviewer.log" | sed 's/^/   /'
grep -q "$hasta" "$OUT/ares_rom/isviewer.log" || { echo "   FALLO: la ROM normal no arranca"; exit 1; }
# Las otras dos ROM, solo compiladas: la de la partida nueva sin menú (para
# capturas) y la de medida (tools/hito5/medir.sh). El relleno de cada una va en
# relleno_cache.c.
for rom in "HORAS=0 MODO=4:n64_horas/th64_horas" "MEDIR=1:n64_medir/th64_medir"; do
  vars=${rom%%:*}
  # shellcheck disable=SC2086
  make -s -C "$M" $vars > "$OUT/make_${rom##*/}.log"
  echo "   make $vars:"
  python3 -I "$REPO_DIR/tools/cache_instrucciones.py" "$N64_INST/bin/mips64-elf-nm" "$M/build/${rom#*:}.elf" --relleno \
    | sed 's/^/     /' \
    || { echo "   FALLO: make $vars: el dibujo choca con rdpq en la caché de instrucciones (relleno_cache.c)"; exit 1; }
done
echo "== todo correcto"
