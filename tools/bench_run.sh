#!/usr/bin/env bash
# Ejecuta un guion del arnés (bench/scenarios/*.lua) dentro de CorsixTH, sin
# pantalla, y espera a que el guion termine.
#
# Uso: tools/bench_run.sh [opciones] <guion.lua> <dir_salida> [args de corsix-th...]
#   --lua 5.4|5.5     versión de Lua (por defecto 5.4)
#   --tracy           captura una traza de Tracy en <dir_salida>/traza.tracy
#   --heaptrack       perfil de memoria en <dir_salida>/heaptrack.gz (build sin Tracy)
#   --timeout N       segundos máximos (por defecto 1800)
#   --lua-hook        mantiene el hook de Tracy en cada llamada Lua (perfil por función)
# Variables: CORSIXTH_SAVES (directorio de partidas; por defecto bench/saves)
set -euo pipefail

REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$REPO_DIR/tools/env.sh"

lua=5.4 mode=plain timeout=1800 extra=()
while [ $# -gt 0 ]; do
  case $1 in
    --lua) lua=$2; shift 2 ;;
    --tracy) mode=tracy; shift ;;
    --heaptrack) mode=heaptrack; shift ;;
    --timeout) timeout=$2; shift 2 ;;
    --lua-hook) extra+=(--th64-lua-hook=1); shift ;;
    *) break ;;
  esac
done
script=$(realpath "$1"); out=$2; shift 2
mkdir -p "$out"; out=$(realpath "$out")
saves=${CORSIXTH_SAVES:-$REPO_DIR/bench/saves}
mkdir -p "$saves"

# Solo las mediciones de tiempo usan el build con Tracy. heaptrack y las
# ejecuciones normales usan el build sin Tracy: así no se cuelan sus colas en
# la memoria ni hay dos clientes compitiendo por el puerto.
variant=notracy
[ $mode = tracy ] && variant=tracy
case $lua in
  5.4) build=$variant ;;
  5.5) build=$variant-lua55; export LUA_CPATH_5_5="$LUA55_PREFIX/lib/lua/5.5/?.so;;" ;;
  *) echo "--lua debe ser 5.4 o 5.5" >&2; exit 2 ;;
esac
exe=$CORSIXTH_DIR/build/$build/CorsixTH/corsix-th
[ -x "$exe" ] || { echo "No existe $exe (tools/setup.sh corsixth lua55)" >&2; exit 2; }

cat > "$out/config.txt" <<EOF
theme_hospital_install = [[$TH_DATA_DIR]]
savegames = [[$saves]]
fullscreen = false
width = 640
height = 480
audio = false
play_intro = false
check_for_updates = false
player_name = [[TH64]]
adviser_disabled = true
autosave_frequency = 0
EOF

display=:$((70 + RANDOM % 9))
Xvfb "$display" -screen 0 1024x768x24 > /dev/null 2>&1 &
xvfb_pid=$!
trap 'kill $xvfb_pid 2>/dev/null || true' EXIT
for _ in $(seq 50); do xdpyinfo -display "$display" > /dev/null 2>&1 && break; sleep 0.1; done
export DISPLAY=$display SDL_AUDIODRIVER=dummy

cmd=("$exe" --interpreter="$REPO_DIR/bench/th64bench.lua"
  --th64-corsixth="$CORSIXTH_DIR/CorsixTH/CorsixTH.lua"
  --th64-script="$script" --th64-out="$out" "${extra[@]}"
  --config-file="$out/config.txt" "$@")
if [ $mode = heaptrack ]; then
  cmd=(heaptrack -o "$out/heaptrack" "${cmd[@]}")
fi
if [ $mode = tracy ]; then

  # Puerto propio: con varias ejecuciones a la vez, tracy-capture podría
  # conectarse a otro proceso.
  export TRACY_PORT=$((20000 + RANDOM % 20000))
  tracy-capture -o "$out/traza.tracy" -f -p "$TRACY_PORT" > "$out/tracy-capture.log" 2>&1 &
  tracy_pid=$!
fi

start=$SECONDS
set +e
(cd "$CORSIXTH_DIR/CorsixTH" && exec timeout -s INT "$timeout" "${cmd[@]}") > "$out/corsixth.log" 2>&1 &
cth_pid=$!
if [ $mode = tracy ]; then
  # Con tracy-capture conectado, el destructor de Tracy no termina al salir
  # (espera a su hilo de red). Cuando el guion escribe "fin" se le da 3 s para
  # vaciar la traza y se termina el proceso; tracy-capture guarda al cortarse.
  while kill -0 "$cth_pid" 2>/dev/null; do
    if grep -q "^fin (código" "$out/th64.log" 2>/dev/null; then
      sleep 3
      pkill -INT -P "$cth_pid" 2>/dev/null
      break
    fi
    sleep 1
  done
fi
wait "$cth_pid"
rc=$?
set -e
[ $mode = tracy ] && { wait "$tracy_pid" || true; grep -q "^fin (código 0)" "$out/th64.log" && rc=0; }
echo "corsix-th terminó con código $rc en $((SECONDS - start)) s (Lua $lua, $build)"
grep -E "^\[th64\]" "$out/corsixth.log" | tail -5 || true
exit $rc
