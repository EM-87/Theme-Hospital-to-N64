#!/usr/bin/env bash
# Huellas de la lista de dibujo (th64sim lista) en una rejilla de cámaras y
# horas de los escenarios del hito 4 y del hito 5: 658 listas, 344 con
# entidades. Para comprobar que un cambio en la vista da las mismas listas:
# se corre antes y después y se comparan las dos salidas con diff.
#
# Uso: tools/hito5/huellas_lista.sh > huellas.txt   (unos 70 s)
# Antes: el núcleo compilado en motor/build/pc y los escenarios en motor/assets/escenarios.
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
A=$REPO_DIR/motor/assets
sim=$REPO_DIR/motor/build/pc/th64sim
lista() { # escenario horas x y
  echo "$1 $2 $3 $4 $("$sim" lista "$A" "$A/escenarios/$1.trg" "$A/datam/mblk-0.tsh" "$A/datam/mspr-0.tsh" \
    "$A/datam/animaciones.tan" 16 "$3" "$4" 320 240 "$2" 1 | sed 's/; [0-9.]* us;//')"
}
# Todo el mapa, a trozos.
for esc in hito4 hito5; do
  for h in 0 500 3501 9000; do
    for x in -1500 -1000 -464 0 500 1000 1500; do
      for y in 100 500 1048 1400 1800; do
        lista $esc $h $x $y
      done
    done
  done
done
# Alrededor del hospital, con desplazamientos que no son múltiplos de una casilla.
for esc_h in "hito4 1500" "hito4 3000" "hito5 1500" "hito5 3501" "hito5 6000" "hito5 9000"; do
  set -- $esc_h
  for x in -700 -637 -580 -523 -464 -401 -350 -287 -230; do
    for y in 900 957 1001 1048 1103 1150 1207; do
      lista "$1" "$2" $x $y
    done
  done
done
