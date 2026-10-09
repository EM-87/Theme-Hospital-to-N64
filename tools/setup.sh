#!/usr/bin/env bash
# Prepara el entorno de trabajo en Ubuntu 24.04 x86_64 (fase 0).
#
# Uso:   tools/setup.sh [paso ...]
# Pasos: apt data corsixth lua55 libdragon ares   (sin argumentos: todos, en ese orden)
#
# Variables (todas con valor por defecto, ver tools/env.sh):
#   TH64_WORK     directorio de trabajo fuera del repo (código, compilaciones, datos)
#   N64_INST      prefijo de la toolchain y libdragon (/opt/libdragon)
#   TRACY_PREFIX  prefijo de Tracy (/opt/tracy)
#   TH_DATA_URL   URL del zip con los datos de Theme Hospital (solo paso "data").
#                 No se guarda en el repo: va como variable o secreto del entorno.
#   TH_DATA_ZIP   alternativa a TH_DATA_URL: ruta local al zip
#   TH_DATA_MD5   opcional: MD5 esperado del zip
#
# Versiones fijadas: ver docs/00_entorno.md.
set -euo pipefail

REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$REPO_DIR/tools/env.sh"

CORSIXTH_REF=v0.70.1
TRACY_REF=v0.13.1                       # la misma que fija el baseline de vcpkg de CorsixTH v0.70.1
LIBDRAGON_BRANCH=preview
LIBDRAGON_COMMIT=39d0d6096130836a65710da6730290701d060272
TOOLCHAIN_URL=https://github.com/DragonMinded/libdragon/releases/download/toolchain-continuous-prerelease/gcc-toolchain-mips64-x86_64.deb
TOOLCHAIN_SHA256=fac8e6572493a66468b7d45df41b042f90b1a630ec96aa218bd5fb1f320a0a4d   # GCC 16.2.0, 2026-10-09
ARES_REF=v148
LUA55_VERSION=5.5.0
LUA55_SHA512=3253d2cdc929da6438095a30d66ef16a1abdbb0ada8fee238705b3b38492f14be9553640fdca6b25661e01155ba5582032e0a2ef064e4c283e85efc0a128cabe
LPEG_VERSION=1.1.0
LPEG_SHA512=01b2a4ceb2d110e143603bc63c84a59736ea735dd0ed9866286ba115d41be48d09c9ff21c8e2327974d2296944f6508d50a5c3a18f26ac1d81b8b2fc41f61222
LFS_TAG=v1_9_0

JOBS=${JOBS:-$(nproc)}
LOGS=$TH64_WORK/logs
mkdir -p "$TH64_WORK/src" "$TH64_WORK/download" "$LOGS"

log() { printf '\n== %s\n' "$*"; }

# Ejecuta un comando guardando la salida en $LOGS/<nombre>.log y mide el tiempo.
run_logged() {
  local name=$1; shift
  local start=$SECONDS
  if ! "$@" > "$LOGS/$name.log" 2>&1; then
    echo "ERROR en $name, ver $LOGS/$name.log" >&2
    tail -20 "$LOGS/$name.log" >&2
    return 1
  fi
  echo "   $name: $((SECONDS - start)) s"
}

clone_at() {   # clone_at <url> <dir> <tag o commit>: clon superficial de un único commit
  local url=$1 dir=$2 ref=$3
  if [ ! -d "$dir/.git" ]; then
    git init -q "$dir"
    git -C "$dir" remote add origin "$url"
  fi
  git -C "$dir" fetch -q --depth 1 origin "$ref"
  git -C "$dir" -c advice.detachedHead=false checkout -q FETCH_HEAD
}

step_apt() {
  log "Paquetes del sistema"
  export DEBIAN_FRONTEND=noninteractive
  apt-get update -qq
  run_logged apt apt-get install -y -qq --no-install-recommends \
    build-essential cmake ninja-build git curl ca-certificates zip unzip tar pkg-config python3 ccache \
    `# CorsixTH v0.70.1 con bibliotecas del sistema` \
    liblua5.4-dev lua5.4 lua-filesystem lua-lpeg libsdl2-dev libsdl2-mixer-dev libfreetype-dev libpng-dev zlib1g-dev \
    `# ares (interfaz GTK3, OpenGL, Vulkan por software para paraLLEl-RDP)` \
    libgtk-3-dev libao-dev libopenal-dev libpulse-dev libasound2-dev libudev-dev \
    libx11-dev libxext-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev libxkbcommon-dev \
    libgl1-mesa-dev libegl1-mesa-dev libdbus-1-dev \
    mesa-vulkan-drivers libvulkan1 libgl1-mesa-dri \
    `# Medición, datos y ejecución sin pantalla` \
    heaptrack valgrind p7zip-full innoextract xvfb x11-utils xdotool imagemagick
}

step_data() {
  log "Datos de Theme Hospital"
  local zip=${TH_DATA_ZIP:-$TH64_WORK/download/th-data.zip}
  if [ -z "${TH_DATA_ZIP:-}" ]; then
    if [ -z "${TH_DATA_URL:-}" ]; then
      echo "Falta TH_DATA_URL (o TH_DATA_ZIP). Los datos no están en el repo." >&2
      return 1
    fi
    [ -f "$zip" ] || curl -sS -L --fail -o "$zip" "$TH_DATA_URL"
  fi
  if [ -n "${TH_DATA_MD5:-}" ]; then
    echo "$TH_DATA_MD5  $zip" | md5sum -c --quiet
  fi

  local stage=$TH64_WORK/download/th-data-unzip
  rm -rf "$stage" && mkdir -p "$stage"
  unzip -q "$zip" -d "$stage"

  # Si el zip trae la imagen del CD, se extrae la ISO completa.
  local iso
  iso=$(find "$stage" -iname '*.iso' -print -quit)
  local root=$stage
  if [ -n "$iso" ]; then
    root=$TH64_WORK/download/th-data-iso
    rm -rf "$root" && mkdir -p "$root"
    7z x -y -o"$root" "$iso" > "$LOGS/data-iso.log"
  fi

  # La carpeta de juego es la que contiene DATA/VBLK-0.TAB y LEVELS/LEVEL.L1.
  local vblk game
  vblk=$(find "$root" -ipath '*/data/vblk-0.tab' -print -quit)
  [ -n "$vblk" ] || { echo "No encuentro DATA/VBLK-0.TAB en los datos" >&2; return 1; }
  game=$(dirname "$(dirname "$vblk")")
  rm -rf "$TH_DATA_DIR" && mkdir -p "$(dirname "$TH_DATA_DIR")"
  cp -r "$game" "$TH_DATA_DIR"
  rm -rf "$stage" "$TH64_WORK/download/th-data-iso"
  echo "   TH_DATA_DIR=$TH_DATA_DIR ($(du -sh "$TH_DATA_DIR" | cut -f1))"
}

# build_corsixth <variante> <WITH_TRACY ON|OFF> [args de cmake]: build/<variante>
build_corsixth() {
  local name=$1 tracy=$2; shift 2
  run_logged "corsixth-$name-configure" cmake -S "$CORSIXTH_DIR" -B "$CORSIXTH_DIR/build/$name" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUSE_SOURCE_DATADIRS=ON -DBUILD_TOOLS=ON -DWITH_TRACY="$tracy" \
    -DWITH_MOVIES=OFF -DWITH_UPDATE_CHECK=OFF -DWITH_MIDI_DEVICE=OFF -DENABLE_UNIT_TESTS=OFF \
    -DCMAKE_PREFIX_PATH="$TRACY_PREFIX" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$@"
  run_logged "corsixth-$name-build" cmake --build "$CORSIXTH_DIR/build/$name" -j"$JOBS"
}

step_corsixth() {
  log "Tracy $TRACY_REF"
  local tracy=$TH64_WORK/src/tracy
  clone_at https://github.com/wolfpld/tracy "$tracy" "$TRACY_REF"
  run_logged tracy-client bash -c "cmake -S '$tracy' -B '$tracy/build-client' -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX='$TRACY_PREFIX' \
    && cmake --build '$tracy/build-client' && cmake --install '$tracy/build-client'"
  run_logged tracy-capture bash -c "cmake -S '$tracy/capture' -B '$tracy/build-capture' -G Ninja -DCMAKE_BUILD_TYPE=Release -DNO_ISA_EXTENSIONS=ON \
    && cmake --build '$tracy/build-capture' -j$JOBS"
  install -D "$tracy/build-capture/tracy-capture" "$TRACY_PREFIX/bin/tracy-capture"
  run_logged tracy-csvexport bash -c "cmake -S '$tracy/csvexport' -B '$tracy/build-csvexport' -G Ninja -DCMAKE_BUILD_TYPE=Release -DNO_ISA_EXTENSIONS=ON \
    && cmake --build '$tracy/build-csvexport' -j$JOBS"
  install -D "$tracy/build-csvexport/tracy-csvexport" "$TRACY_PREFIX/bin/tracy-csvexport"

  log "CorsixTH $CORSIXTH_REF"
  clone_at https://github.com/CorsixTH/CorsixTH "$CORSIXTH_DIR" "$CORSIXTH_REF"
  # Equivale al preset linux-tracy sin vcpkg: aquí la política de red bloquea
  # las descargas github.com/<repo>/archive/... que usa vcpkg. El build sin
  # Tracy es para heaptrack, para que las colas de Tracy no cuenten.
  build_corsixth tracy ON
  build_corsixth notracy OFF
}

# Segundo build de CorsixTH contra Lua 5.5.0, la versión que instala el baseline
# de vcpkg de v0.70.1 (las compilaciones oficiales). Mismos tarballs y SHA-512
# que los ports de vcpkg; luafilesystem por git (vcpkg usa el mismo tag).
step_lua55() {
  log "Lua $LUA55_VERSION, luafilesystem $LFS_TAG, LPeg $LPEG_VERSION"
  local src=$TH64_WORK/src/lua55 dl=$TH64_WORK/download
  mkdir -p "$src" "$LUA55_PREFIX/include" "$LUA55_PREFIX/lib/lua/5.5"
  fetch_checked "https://www.lua.org/ftp/lua-$LUA55_VERSION.tar.gz" "$dl/lua-$LUA55_VERSION.tar.gz" "$LUA55_SHA512"
  fetch_checked "https://www.inf.puc-rio.br/~roberto/lpeg/lpeg-$LPEG_VERSION.tar.gz" "$dl/lpeg-$LPEG_VERSION.tar.gz" "$LPEG_SHA512"
  tar -xzf "$dl/lua-$LUA55_VERSION.tar.gz" -C "$src"
  tar -xzf "$dl/lpeg-$LPEG_VERSION.tar.gz" -C "$src"
  clone_at https://github.com/lunarmodules/luafilesystem "$src/luafilesystem" "$LFS_TAG"

  # Lua como biblioteca compartida, para que lfs.so y lpeg.so resuelvan sus símbolos.
  local lua=$src/lua-$LUA55_VERSION/src
  run_logged lua55-build make -C "$lua" -j"$JOBS" linux MYCFLAGS=-fPIC
  gcc -shared -Wl,-soname,liblua5.5.so -o "$LUA55_PREFIX/lib/liblua5.5.so" \
    $(ls "$lua"/*.o | grep -v -E '/(lua|luac)\.o$') -lm
  cp "$lua"/{lua.h,luaconf.h,lualib.h,lauxlib.h,lua.hpp} "$LUA55_PREFIX/include/"
  cp "$lua/lua" "$LUA55_PREFIX/lua5.5"
  gcc -O2 -shared -fPIC -I"$LUA55_PREFIX/include" -o "$LUA55_PREFIX/lib/lua/5.5/lfs.so" \
    "$src/luafilesystem/src/lfs.c"
  gcc -O2 -shared -fPIC -I"$LUA55_PREFIX/include" -o "$LUA55_PREFIX/lib/lua/5.5/lpeg.so" \
    "$src/lpeg-$LPEG_VERSION"/lp*.c

  log "CorsixTH $CORSIXTH_REF con Lua $LUA55_VERSION"
  local lua55=(-DLUA_INCLUDE_DIR="$LUA55_PREFIX/include" -DLUA_LIBRARY="$LUA55_PREFIX/lib/liblua5.5.so"
    -DCMAKE_PROJECT_INCLUDE="$REPO_DIR/tools/cmake/lua55/project_include.cmake"
    -DCMAKE_BUILD_RPATH="$LUA55_PREFIX/lib")
  build_corsixth tracy-lua55 ON "${lua55[@]}"
  build_corsixth notracy-lua55 OFF "${lua55[@]}"
}

fetch_checked() {   # fetch_checked <url> <destino> <sha512>
  [ -f "$2" ] || curl -sS -L --fail -o "$2" "$1"
  echo "$3  $2" | sha512sum -c --quiet
}

step_libdragon() {
  log "Toolchain de libdragon"
  local deb=$TH64_WORK/download/gcc-toolchain-mips64-x86_64.deb
  [ -f "$deb" ] || curl -sS -L --fail -o "$deb" "$TOOLCHAIN_URL"
  if ! echo "$TOOLCHAIN_SHA256  $deb" | sha256sum -c --quiet; then
    # La release "continuous-prerelease" se regenera: avisar, no abortar.
    echo "AVISO: la toolchain descargada no es la documentada; anota su versión en docs/00_entorno.md" >&2
  fi
  run_logged toolchain dpkg -i "$deb"

  log "libdragon $LIBDRAGON_BRANCH @ ${LIBDRAGON_COMMIT:0:10}"
  local ld=$TH64_WORK/src/libdragon
  clone_at https://github.com/DragonMinded/libdragon "$ld" "$LIBDRAGON_COMMIT"
  run_logged libdragon-build bash -c "cd '$ld' && JOBS=$JOBS ./build.sh"
}

step_ares() {
  log "ares $ARES_REF"
  local ares=$TH64_WORK/src/ares
  clone_at https://github.com/ares-emulator/ares "$ares" "$ARES_REF"
  run_logged ares-configure cmake -S "$ares" -B "$ares/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DARES_CORES=n64 -DENABLE_IPO=OFF
  run_logged ares-build cmake --build "$ares/build" -j"$JOBS"
}

steps=("$@")
[ ${#steps[@]} -gt 0 ] || steps=(apt data corsixth lua55 libdragon ares)
for s in "${steps[@]}"; do
  case $s in
    apt|data|corsixth|lua55|libdragon|ares) "step_$s" ;;
    *) echo "Paso desconocido: $s" >&2; exit 2 ;;
  esac
done
