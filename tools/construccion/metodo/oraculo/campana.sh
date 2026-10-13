#!/usr/bin/env bash
# Campaña del oráculo (fase 4, método): reproducibilidad de CorsixTH, llamadas
# a math.random y dispersión entre semillas. Ejecuta en serie.
# Uso: tools/construccion/metodo/oraculo/campana.sh [dir_salida]
#   (por defecto $TH64_WORK/resultados/fase4/oraculo; partidas de bench/saves)
set -u
W=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$W/../../../.." && pwd)
source "$R/tools/env.sh"
OUT=${1:-$TH64_WORK/resultados/fase4/oraculo}
mkdir -p "$OUT/runs2"
run() { # nombre guion args...
  local n=$1 s=$2; shift 2
  [ -f "$OUT/runs2/$n/th64.log" ] && grep -q "^fin (código 0)" "$OUT/runs2/$n/th64.log" && return
  "$R/tools/bench_run.sh" --timeout 900 "$@" > "$OUT/runs2/$n.out" 2>&1 || echo "FALLO $n" >> "$OUT/runs2/fallos.txt"
}
cd "$R"
# 1) Reproducibilidad: misma partida, sin dibujar
for i in 1 2 3; do run lleno_r$i x $W/volcado.lua $OUT/runs2/lleno_r$i --load=lleno.sav --th64-hours=2000 --th64-batch=30 --th64-frames=0; done
for i in 1 2; do run mediano_r$i x $W/volcado.lua $OUT/runs2/mediano_r$i --load=mediano.sav --th64-hours=2000 --th64-batch=30 --th64-frames=0; done
# 2) Con dibujo (frames consumen math.random)
for i in 1 2; do run lleno_f$i x $W/volcado.lua $OUT/runs2/lleno_f$i --load=lleno.sav --th64-hours=500 --th64-batch=30 --th64-frames=1; done
# 3) Otro intérprete (Lua 5.5)
run lleno_l55 x --lua 5.5 $W/volcado.lua $OUT/runs2/lleno_l55 --load=lleno.sav --th64-hours=2000 --th64-batch=30 --th64-frames=0
# 4) Orden de pairs dentro de CorsixTH
for i in 1 2 3; do run hash$i x $W/sonda_hash.lua $OUT/runs2/hash$i --load=lleno.sav; done
# 5) Llamadas a math.random por fase
run rng_f0 x $W/sonda_rng.lua $OUT/runs2/rng_f0 --load=lleno.sav --th64-hours=200 --th64-batch=30 --th64-frames=0
run rng_f1 x $W/sonda_rng.lua $OUT/runs2/rng_f1 --load=lleno.sav --th64-hours=200 --th64-batch=30 --th64-frames=1
# 6) Dispersión entre semillas: 10 semillas, 1.500 h (un mes)
for s in 1 2 3 4 5 6 7 8 9 10; do run lleno_s$s x $W/volcado.lua $OUT/runs2/lleno_s$s --load=lleno.sav --th64-hours=1500 --th64-batch=30 --th64-frames=0 --th64-seed=$s; done
# semilla repetida: ¿misma semilla, mismo resultado?
run lleno_s1b x $W/volcado.lua $OUT/runs2/lleno_s1b --load=lleno.sav --th64-hours=1500 --th64-batch=30 --th64-frames=0 --th64-seed=1
echo CAMPAÑA_TERMINADA > "$OUT/runs2/fin.txt"
