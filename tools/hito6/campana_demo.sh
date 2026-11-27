#!/usr/bin/env bash
# La campaña de la demo jugable (docs/06_plan.md, pieza 7): CorsixTH juega la
# partida de una lista de órdenes con tick (por defecto ordenes/demo.tsv: una
# partida nueva del nivel 1, abierta, con el mostrador, la recepcionista, la
# consulta y el médico) con varias semillas, y el motor la misma partida
# (th64sim partida sobre motor/assets/escenarios/demo.trg); se compara con
# estadística como el oráculo del hito 4 (tools/oraculo_hito4).
# Para cada semilla s:
#   1. th64sim mercado <assets> 1 s k: el mercado de candidatos del núcleo de
#      cada mes k de la partida (el del mes 0 es el de la partida nueva; el
#      núcleo lo regenera al acabar cada mes con la semilla y el mes);
#   2. CorsixTH: bench/scenarios/crear_demo.lua con la semilla s, ese mercado
#      y la lista, durante las horas pedidas → <dir>/semilla_s (la traza:
#      pacientes.tsv, dinero.tsv, dias.tsv, acciones.tsv, final.tsv y
#      ordenes_resultado.tsv). Una instancia a la vez, con nice;
#   3. el motor: th64sim partida con la semilla s (el mismo mercado: es la
#      partida emparejada) y R − 1 semillas propias más, s × 1000 + r
#      (r = 2..R, como motor.sh; con otro mercado, otro médico: la
#      estadística compara las dos poblaciones) → <dir>/motor/semilla_s_r.
# Al final, resumen.py --demo de los dos lados, comparar.py (t de Welch) y
# por_mes.py (las llegadas por mes, con la hora inicial 0), y se comprueba que
# CorsixTH aceptó las órdenes que aceptó el motor con la semilla s.
#
# Uso: tools/hito6/campana_demo.sh [semillas] [horas] [dir] [repeticiones] [lista.tsv]
#   semillas: por defecto "1 2 3 4 5 6 7 8 9 10"
#   horas: por defecto 9050 (181 días, 6 meses)
#   dir: por defecto $TH64_WORK/resultados/hito6/campana_demo
#   repeticiones: corridas del motor por semilla de CorsixTH, por defecto 10
#   lista.tsv: por defecto tools/hito6/ordenes/demo.tsv
# Variables: REHACER=1 vuelve a correr CorsixTH aunque la semilla ya esté.
# Si no, una semilla se salta solo si su corrida es la misma que se pediría
# ahora: dir/semilla_s/sello.txt (la semilla, las horas y el sha256 de la
# lista, de los mercados de la semilla y de los guiones de CorsixTH que la
# juegan) igual al de ahora, y la última línea «demo:» de th64.log con esas
# horas y esa semilla. Así la campaña se alarga pasando más semillas sin
# mezclar corridas hechas con otras horas, otra lista u otros guiones.
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
semillas=${1:-"1 2 3 4 5 6 7 8 9 10"}
horas=${2:-9050}
O=${3:-$TH64_WORK/resultados/hito6/campana_demo}
reps=${4:-10}
lista=$(realpath "${5:-$REPO_DIR/tools/hito6/ordenes/demo.tsv}")
SIM=$REPO_DIR/motor/build/pc/th64sim
A=$REPO_DIR/motor/assets
ESC=$A/escenarios/demo.trg
[ -f "$ESC" ] || { echo "falta $ESC (tools/hito6/demo.sh)" >&2; exit 2; }
mkdir -p "$O/saves" "$O/mercados" "$O/motor"
O=$(realpath "$O")
# Los meses de la partida (desde 0): uno por cada 28 días, de sobra.
meses=$(( horas / (28 * 50) + 1 ))
fallos=0
# El sello de una corrida de CorsixTH (lo que la decide, salvo CorsixTH).
sello() {
  echo "semilla $1"
  echo "horas $horas"
  (cd "$REPO_DIR" && sha256sum bench/scenarios/crear_demo.lua bench/lib/traza.lua bench/lib/ordenes.lua \
    bench/lib/builder.lua bench/lib/json.lua bench/lib/th64.lua)
  sha256sum "$lista" | sed "s|  .*|  lista|"
  (cd "$O/mercados" && sha256sum mercado_"$1"_*.json)
}
for s in $semillas; do
  for k in $(seq 0 "$meses"); do
    "$SIM" mercado "$A" 1 "$s" "$k" > "$O/mercados/mercado_${s}_$k.json"
  done
  C=$O/semilla_$s
  hecha=0
  if [ "${REHACER:-0}" != 1 ] && [ -f "$C/pacientes.tsv" ] && [ -f "$C/sello.txt" ] \
      && cmp -s "$C/sello.txt" <(sello "$s") \
      && grep '^demo:' "$C/th64.log" | tail -1 | grep -q "; $horas horas; .*; semilla $s\$"; then
    hecha=1
  fi
  if [ "$hecha" = 0 ]; then
    rm -rf "$C"
    t0=$(date +%s)
    if ! CORSIXTH_SAVES=$O/saves nice -n 10 "$REPO_DIR/tools/bench_run.sh" --timeout 1200 \
        "$REPO_DIR/bench/scenarios/crear_demo.lua" "$C" --th64-ordenes="$lista" --th64-semilla="$s" \
        --th64-horas="$horas" --th64-mercado="$O/mercados/mercado_${s}_%d.json" > "$O/crear_$s.log" 2>&1; then
      echo "semilla $s: CorsixTH falló ($O/crear_$s.log)"
      # Fuera: resumen.py y por_mes.py recorren semilla_* y no la saltan.
      rm -rf "$C"
      fallos=$((fallos + 1))
      continue
    fi
    sello "$s" > "$C/sello.txt"
    echo "semilla $s: CorsixTH en $(( $(date +%s) - t0 )) s: $(grep '^traza:' "$C/th64.log")"
  fi
  for r in $(seq 1 "$reps"); do
    d=$O/motor/semilla_${s}_$r
    mkdir -p "$d"
    sm=$s
    [ "$r" -gt 1 ] && sm=$((s * 1000 + r))
    "$SIM" partida "$A" "$ESC" "$lista" "$sm" "$horas" "$d" > "$d/salida.txt"
  done
  # Las órdenes: CorsixTH tiene que aceptar y rechazar lo mismo que el motor
  # con la misma semilla (el mismo mercado).
  if ! diff -q <(cut -f1,3,4 "$C/ordenes_resultado.tsv") <(cut -f1,3,4 "$O/motor/semilla_${s}_1/ordenes_resultado.tsv") \
      > /dev/null; then
    echo "semilla $s: las órdenes no dan lo mismo en CorsixTH y en el motor"
    diff <(cut -f1-4 "$C/ordenes_resultado.tsv") <(cut -f1-4 "$O/motor/semilla_${s}_1/ordenes_resultado.tsv") || true
    fallos=$((fallos + 1))
  fi
  # Lo cobrado al contratar sale del mercado del núcleo en los dos lados
  # (Hospital:addStaff cobra profile.wage, hospital.lua:1403; el motor,
  # TH_MONEY_HIRE_STAFF): con la misma semilla es el mismo, movimiento a
  # movimiento (hora e importe). Se comprueba emparejado, no con la t.
  if ! diff -q <(awk -F'\t' '$3 ~ /Hire Staff/ {print $1, $2}' "$C/dinero.tsv") \
      <(awk -F'\t' '$3 ~ /^hire_staff:/ {print $1, $2}' "$O/motor/semilla_${s}_1/dinero.tsv") > /dev/null; then
    echo "semilla $s: lo cobrado al contratar no es lo mismo en CorsixTH y en el motor"
    fallos=$((fallos + 1))
  fi
  echo "semilla $s: motor $(tail -1 "$O/motor/semilla_${s}_1/salida.txt")"
done
python3 -I "$REPO_DIR/tools/oraculo_hito4/resumen.py" --demo "$O"/semilla_[0-9]* --json "$O/corsixth.json" \
  > "$O/corsixth.txt"
python3 -I "$REPO_DIR/tools/oraculo_hito4/resumen.py" --demo "$O"/motor/semilla_* --json "$O/motor/motor.json" \
  > "$O/motor/motor.txt"
python3 -I "$REPO_DIR/tools/oraculo_hito4/comparar.py" "$O/corsixth.json" "$O/motor/motor.json" \
  --json "$O/comparacion.json" | tee "$O/comparacion.txt"
python3 -I "$REPO_DIR/tools/oraculo_hito4/por_mes.py" "$O" "$O/motor" --hora-inicial 0 | tee "$O/por_mes.txt"
[ "$fallos" -eq 0 ] || { echo "FALLOS: $fallos"; exit 1; }
