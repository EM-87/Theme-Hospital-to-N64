#!/usr/bin/env bash
# Ejecuta CorsixTH sin pantalla (Xvfb, audio desactivado) con los datos de
# $TH_DATA_DIR, y captura una traza de Tracy o un perfil de heaptrack.
#
# Uso: tools/run_corsixth.sh [--heaptrack] <dir_salida> [segundos=20] [args de corsix-th...]
#   Por defecto:  <dir_salida>/corsixth.tracy (tracy-capture)
#   --heaptrack:  <dir_salida>/heaptrack.corsixth.gz (sin Tracy conectado)
#   Siempre:      <dir_salida>/corsixth.log, captura.png, config.txt
# Variables: CORSIXTH_LUA=5.4|5.5 (build contra Lua del sistema o Lua 5.5.0),
#            CORSIXTH_SAVES (directorio de partidas)
# Ejemplo: tools/run_corsixth.sh out/menu 20
#          tools/run_corsixth.sh out/partida 60 --load=mediano.sav
set -euo pipefail

REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$REPO_DIR/tools/env.sh"

mode=tracy
if [ "${1:-}" = "--heaptrack" ]; then mode=heaptrack; shift; fi
out=$1; secs=${2:-20}; shift $(( $# >= 2 ? 2 : 1 ))
mkdir -p "$out"; out=$(realpath "$out")
savedir=${CORSIXTH_SAVES:-$TH64_WORK/run/corsixth/Saves}
mkdir -p "$savedir"

# player_name evita un fallo de CorsixTH v0.70.1 cuando USER no está definido
# (app.lua: fixConfig llama a :match sobre nil).
cat > "$out/config.txt" <<EOF
theme_hospital_install = [[$TH_DATA_DIR]]
savegames = [[$savedir]]
fullscreen = false
width = 640
height = 480
audio = false
play_intro = false
check_for_updates = false
player_name = [[TH64]]
EOF

display=:$((80 + RANDOM % 9))
Xvfb "$display" -screen 0 1024x768x24 > /dev/null 2>&1 &
xvfb_pid=$!
trap 'kill $xvfb_pid 2>/dev/null || true' EXIT
for _ in $(seq 50); do xdpyinfo -display "$display" > /dev/null 2>&1 && break; sleep 0.1; done
export DISPLAY=$display SDL_AUDIODRIVER=dummy

case ${CORSIXTH_LUA:-5.4} in
  5.4) build=tracy ;;
  5.5) build=tracy-lua55; export LUA_CPATH_5_5="$LUA55_PREFIX/lib/lua/5.5/?.so;;" ;;
  *) echo "CORSIXTH_LUA debe ser 5.4 o 5.5" >&2; exit 2 ;;
esac
cmd=("$CORSIXTH_DIR/build/$build/CorsixTH/corsix-th" --config-file="$out/config.txt" "$@")
if [ $mode = heaptrack ]; then
  cmd=(heaptrack -o "$out/heaptrack.corsixth" "${cmd[@]}")
else
  tracy-capture -o "$out/corsixth.tracy" -f -s "$((secs > 4 ? secs - 4 : 1))" > "$out/tracy-capture.log" 2>&1 &
  tracy_pid=$!
fi

# CorsixTH busca sus scripts Lua relativos al directorio de trabajo.
(cd "$CORSIXTH_DIR/CorsixTH" && exec timeout -s INT "$secs" "${cmd[@]}") > "$out/corsixth.log" 2>&1 &
cth_pid=$!
sleep "$((secs > 3 ? secs - 3 : 1))"
import -window root "$out/captura.png"
wait "$cth_pid" || true
[ $mode = tracy ] && { wait "$tracy_pid" || true; tail -4 "$out/tracy-capture.log"; }
[ $mode = heaptrack ] && heaptrack_print -f "$out/heaptrack.corsixth.gz" 2>/dev/null | tail -6
if grep -q "An error has occurred" "$out/corsixth.log"; then
  echo "AVISO: CorsixTH registró errores, ver $out/corsixth.log" >&2
fi
