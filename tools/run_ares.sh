#!/usr/bin/env bash
# Ejecuta una ROM en ares sin pantalla (Xvfb), guarda la salida de ISViewer
# (debugf) y una captura de la ventana.
#
# Uso: tools/run_ares.sh <rom.z64> <dir_salida> [segundos=12] [--no-expansion]
#   ARES_UNTIL="texto": terminar cuando la ROM escriba ese texto (máx. segundos)
#   ARES_SETTINGS="Clave/Sub=valor ...": ajustes extra de ares, por ejemplo
#     General/HomebrewMode=true (debugf por EMUX sin límite de tamaño de ROM,
#     profile.h con fallos de caché y salida de ares desde la ROM) o
#     "DebugServer/Enabled=true DebugServer/UseIPv4=true Boot/AwaitGDBClient=true"
#     para conectar gdb-multiarch al puerto DebugServer/Port (9123 por defecto)
#   ARES_HOOK="orden": se ejecuta mientras ares corre (p. ej. una sesión de gdb
#     por lotes); su salida va a <dir_salida>/hook.log
#   <dir_salida>/isviewer.log   salida de la ROM por ISViewer
#   <dir_salida>/ares.log       resto de la salida de ares
#   <dir_salida>/captura.png    ventana de ares unos segundos antes de cerrarla
set -euo pipefail

REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$REPO_DIR/tools/env.sh"

rom=$(realpath "$1"); out=$2; secs=${3:-12}
expansion=true
[ "${4:-}" = "--no-expansion" ] && expansion=false
mkdir -p "$out"; out=$(realpath "$out")

display=:$((90 + RANDOM % 9))
Xvfb "$display" -screen 0 1280x960x24 > /dev/null 2>&1 &
xvfb_pid=$!
trap 'kill $xvfb_pid 2>/dev/null || true' EXIT
for _ in $(seq 50); do xdpyinfo -display "$display" > /dev/null 2>&1 && break; sleep 0.1; done
export DISPLAY=$display

# Configuración propia en el directorio de salida: no depende de ~/.local.
# paraLLEl-RDP corre sobre Vulkan por software (lavapipe) y el vídeo sobre GLX (llvmpipe).
extra=()
for s in ${ARES_SETTINGS:-}; do extra+=(--setting "$s"); done
"$ARES_BIN" --settings-file "$out/settings.bml" \
  --setting Video/Driver="OpenGL 3.2" \
  --setting Audio/Driver=None \
  --setting Input/Driver=None \
  --setting Nintendo64/ExpansionPak=$expansion \
  "${extra[@]}" \
  --system "Nintendo 64" --no-file-prompt "$rom" \
  > "$out/isviewer.log" 2> "$out/ares.log" &
ares_pid=$!

# Con ARES_UNTIL="texto", se espera a que la ROM lo escriba por ISViewer
# (como máximo los segundos indicados) en lugar de un tiempo fijo.
# Si la ROM cierra ares (EMUX en modo Homebrew), se deja de esperar.
if [ -n "${ARES_HOOK:-}" ]; then
  bash -c "$ARES_HOOK" > "$out/hook.log" 2>&1 || echo "hook rc=$?" >> "$out/hook.log"
elif [ -n "${ARES_UNTIL:-}" ]; then
  for _ in $(seq "$secs"); do
    grep -q "$ARES_UNTIL" "$out/isviewer.log" 2>/dev/null && break
    kill -0 "$ares_pid" 2>/dev/null || break
    sleep 1
  done
else
  sleep "$((secs > 2 ? secs - 2 : 1))"
fi
win=$(xdotool search --pid "$ares_pid" 2>/dev/null | tail -1 || true)
if [ -n "$win" ]; then
  import -window "$win" "$out/captura.png"
else
  import -window root "$out/captura.png"
fi
sleep 2
kill "$ares_pid" 2>/dev/null || true
wait "$ares_pid" 2>/dev/null || true
echo "ROM: $rom"
echo "Expansion Pak: $expansion"
echo "--- ISViewer ---"
cat "$out/isviewer.log"
