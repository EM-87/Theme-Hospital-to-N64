#!/usr/bin/env bash
# Tamaño en memoria de las estructuras del mapa, el pathfinding y las
# animaciones del núcleo C++ de CorsixTH, en x86-64 (tamanos.cpp, compilado y
# ejecutado) y en N64 (tamanos_n64.cpp, compilado con mips64-elf-g++ y la ABI
# o64 de libdragon, leído con nm). Uso: tools/arquitectura/tamanos.sh <salida.json>
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
source "$here/../env.sh"
out=$1
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
src=$CORSIXTH_DIR/CorsixTH/Src
cfg=$CORSIXTH_DIR/build/notracy/CorsixTH/Src
lua_n64=$TH64_WORK/src/luabench/lua-5.4.6/src

g++ -std=c++17 -I"$src" -I"$cfg" $(pkg-config --cflags lua5.4 sdl2) -c -o "$tmp/x86.o" "$here/tamanos.cpp"
g++ -o "$tmp/x86" "$tmp/x86.o" $(pkg-config --libs lua5.4)
# luaconf.h no encuentra LLONG_MAX con las cabeceras de newlib en C++; no
# afecta a estas estructuras.
"$N64_INST/bin/mips64-elf-g++" -std=gnu++17 -march=vr4300 -mtune=vr4300 -mabi=o64 \
  -I"$N64_INST/mips64-elf/include" -include ktls.h \
  -D'LLONG_MAX=__LONG_LONG_MAX__' -D'LLONG_MIN=(-__LONG_LONG_MAX__-1LL)' \
  -I"$src" -I"$cfg" -I"$lua_n64" -c -o "$tmp/n64.o" "$here/tamanos_n64.cpp"
"$tmp/x86" > "$tmp/x86.json"
"$N64_INST/bin/mips64-elf-nm" -S "$tmp/n64.o" > "$tmp/n64.nm"
python3 -I - "$tmp/x86.json" "$tmp/n64.nm" "$out" <<'PY'
import json, sys
x86 = json.load(open(sys.argv[1]))
n64 = {}
for line in open(sys.argv[2]):
    f = line.split()
    if len(f) == 4 and f[3].startswith("sz_"):
        n64[f[3][3:]] = int(f[1], 16)
n64["tiles_128x128_map_tile_x2"] = 2 * 128 * 128 * n64["map_tile"]
n64["path_nodes_128x128"] = 128 * 128 * n64["path_node"]
d = {"x86_64": x86, "n64": n64}
json.dump(d, open(sys.argv[3], "w"), indent=1)
print(json.dumps(d))
PY
