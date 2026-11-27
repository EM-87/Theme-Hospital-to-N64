#!/usr/bin/env bash
# La demo jugable (docs/06_plan.md, «La demo jugable», piezas 2 y 6): repite
# en el PC una partida jugada en la ROM y compara la huella y el saldo.
#
# El contrato con la ROM (las líneas que escribe por debugf y que
# tools/run_ares.sh recoge en isviewer.log; cada una puede llevar delante lo
# que añada el registro, se buscan por su texto):
#   al empezar la partida:
#     TH64 partida nueva: escenario <nombre>, semilla <n>
#       <nombre>: el escenario que cargó, motor/assets/escenarios/<nombre>.trg
#       (rom:/escenarios/<nombre>.trg); <n>: la semilla de th_sim_init, en decimal
#   por cada orden que el núcleo aplica (el gancho th_sim_events.order, una
#   línea por orden aceptada o rechazada), los campos separados por tabuladores:
#     TH64 orden<TAB><tick><TAB><orden><TAB><x><TAB><y><TAB><dir><TAB><qty><TAB><w><TAB><h><TAB><def_id><TAB><motivo>
#       <tick>: th_sim.hours al aplicarla (desde 0), en decimal;
#       <orden>: el nombre de la orden (comprar, colocar, cerrar, recoger,
#         vender, deshacer, sala, puerta, ventana, quitar_ventana, fin_ventanas,
#         comprar_sala, fin_sala, atras, contratar, devolver; se acepta también
#         el número TH_ORD_*);
#       <x> <y> <dir> <qty> <w> <h>: los campos de th_order tal cual, en
#         decimal (dir: 0 norte, 1 este, 2 sur, 3 oeste; en contratar y
#         devolver, la clase TH_HUM_*: 1 médico, 2 enfermera, 3 bedel, 4
#         recepcionista; qty, el índice del candidato);
#       <def_id>: el id de objetos.<id> o salas.<id> de th_order.def; en
#         contratar y devolver, el resumen del perfil en 8 cifras
#         hexadecimales; o «-»;
#       <motivo>: th_build_reason_name(reason), o «-» si se aceptó;
#       los campos que la orden no usa, a 0 (def_id, «-»); no cuentan:
#       repetir.py los pone así antes de comparar, como th64sim_leer_orden, y
#       avisa si alguno no lo estaba (la línea tal cual, en rom_ordenes_crudo.log);
#   al salir de la partida:
#     TH64 partida: <horas> horas, huella <hash>, saldo <saldo>
#       <horas>: th_sim.hours; <hash>: th_sim_hash en hexadecimal; <saldo>: el
#       del hospital. Se escribe entre dos ticks (th_sim_tick terminado) y con
#       la cola de órdenes vacía: th_sim_hash cuenta las órdenes que esperan en
#       la cola y el PC solo repite las aplicadas.
#   al empezar cada mes de la partida, entre dos ticks y con la cola vacía:
#     TH64 partida: control <horas> horas, huella <hash>, saldo <saldo>, <us> us
#       lo mismo que la del final, más lo que tardó th_sim_hash en la N64. Si
#       falta la del final (la consola se apagó o se reinició sin salir con
#       Z), vale la última de control y solo las órdenes de antes de ella.
# Si el registro trae varias partidas, vale la última.
#
# tools/hito6/repetir.py pasa las órdenes a la lista de th64sim partida
# (tick, orden y argumentos) y th64sim partida la repite con el mismo
# escenario, la misma semilla y las mismas horas. Se comparan las líneas de
# las órdenes (las de la ROM contra ordenes_rom.log de th64sim, que las
# escribe igual), la huella y el saldo.
#
# Uso: tools/hito6/repetir.sh <isviewer.log> [horas] [dir_salida]
#   horas: por defecto, las de la línea del final o, sin ella, las de la
#   última de control; si la ROM no escribió ninguna (se cortó la partida en
#   el primer mes), hay que darlas, y entonces solo se comparan las órdenes. dir_salida: por defecto $TH64_WORK/resultados/hito6/repetir.
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
[ $# -ge 1 ] || { sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 2; }
LOG=$1
HORAS=${2:-}
OUT=${3:-$TH64_WORK/resultados/hito6/repetir}
SIM=$REPO_DIR/motor/build/pc/th64sim
A=$REPO_DIR/motor/assets
mkdir -p "$OUT"
read -r ESCENARIO SEMILLA ROM_HORAS ROM_HUELLA ROM_SALDO < <(python3 -I "$REPO_DIR/tools/hito6/repetir.py" "$LOG" "$OUT")
if [ -z "$HORAS" ]; then
  if [ "$ROM_HORAS" = "-" ]; then
    echo "repetir: la ROM no escribió «TH64 partida: …»; di cuántas horas repetir" >&2
    exit 2
  fi
  HORAS=$ROM_HORAS
fi
TRG=$A/escenarios/$ESCENARIO.trg
[ -f "$TRG" ] || { echo "repetir: no está $TRG" >&2; exit 1; }
echo "repetir: escenario $ESCENARIO, semilla $SEMILLA, $(($(wc -l < "$OUT/ordenes.tsv") - 1)) órdenes, $HORAS horas"
"$SIM" partida "$A" "$TRG" "$OUT/ordenes.tsv" "$SEMILLA" "$HORAS" "$OUT" > "$OUT/partida.txt"
sed 's/^/   /' "$OUT/partida.txt"
fallos=0
if cmp -s "$OUT/rom_ordenes.log" "$OUT/ordenes_rom.log"; then
  echo "   órdenes: las mismas, con el mismo resultado"
else
  echo "   órdenes distintas (ROM < > PC):"
  diff "$OUT/rom_ordenes.log" "$OUT/ordenes_rom.log" | head -20 | sed 's/^/     /' || true
  fallos=$((fallos + 1))
fi
PC_HUELLA=$(sed -n 's/^partida: .*; huella \([0-9a-f]*\); saldo \(-\{0,1\}[0-9]*\);.*/\1/p' "$OUT/partida.txt")
PC_SALDO=$(sed -n 's/^partida: .*; huella \([0-9a-f]*\); saldo \(-\{0,1\}[0-9]*\);.*/\2/p' "$OUT/partida.txt")
if [ "$ROM_HUELLA" = "-" ]; then
  echo "   sin la línea final de la ROM: huella $PC_HUELLA y saldo $PC_SALDO en el PC, sin comparar"
elif [ "$HORAS" != "$ROM_HORAS" ]; then
  echo "   la ROM jugó $ROM_HORAS horas y el PC $HORAS: la huella no se compara"
else
  if [ "$PC_HUELLA" = "$ROM_HUELLA" ] && [ "$PC_SALDO" = "$ROM_SALDO" ]; then
    echo "   huella $PC_HUELLA y saldo $PC_SALDO: iguales que en la ROM"
  else
    echo "   DISTINTA: ROM huella $ROM_HUELLA, saldo $ROM_SALDO; PC huella $PC_HUELLA, saldo $PC_SALDO"
    fallos=$((fallos + 1))
  fi
fi
[ $fallos -eq 0 ] && echo "repetir: bien" || { echo "repetir: $fallos fallos"; exit 1; }
