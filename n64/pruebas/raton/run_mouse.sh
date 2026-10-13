#!/usr/bin/env bash
# Lanza ares con la ROM, abre el menú "Nintendo 64 > Controller Port 1 > Mouse" con xdotool
# y guarda capturas de cada paso. Mata Xvfb y ares por PID al terminar.
set -u
source "$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)/tools/env.sh"
rom=$(realpath "$1"); out=$(realpath -m "$2"); mkdir -p "$out"
display=:$((70 + RANDOM % 9))
Xvfb "$display" -screen 0 1280x960x24 > /dev/null 2>&1 &
xvfb_pid=$!
ares_pid=
cleanup() { [ -n "$ares_pid" ] && kill "$ares_pid" 2>/dev/null; kill "$xvfb_pid" 2>/dev/null; }
trap cleanup EXIT
for _ in $(seq 50); do xdpyinfo -display "$display" > /dev/null 2>&1 && break; sleep 0.1; done
export DISPLAY=$display
"$ARES_BIN" --settings-file "$out/settings.bml" --setting Video/Driver="OpenGL 3.2" --setting Audio/Driver=None \
  --setting Input/Driver=None --setting Nintendo64/ExpansionPak=true --system "Nintendo 64" --no-file-prompt "$rom" \
  > "$out/isviewer.log" 2> "$out/ares.log" &
ares_pid=$!
sleep 6
win=$(xdotool search --pid "$ares_pid" 2>/dev/null | tail -1)
echo "ventana: $win"
xdotool getwindowgeometry "$win"
import -window root "$out/p0.png"
shift 2
i=1
for act in "$@"; do
  eval "xdotool $act"
  sleep 1
  import -window root "$out/p$i.png"; i=$((i+1))
done
sleep 4
import -window root "$out/final.png"
grep "TH64 joy" "$out/isviewer.log" | tail -6
