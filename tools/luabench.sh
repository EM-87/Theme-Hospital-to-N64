#!/usr/bin/env bash
# Factor de CPU de la fase 1: ejecuta los microbenchmarks de
# n64/luabench/filesystem/run.lua (y la búsqueda en C de
# n64/luabench_common/bfs.h) en el host y en la N64 emulada por ares, con
# Lua 5.4.6 y 5.5.0 compilados desde los mismos fuentes y con -O2 en los dos lados.
#
# Uso: tools/luabench.sh <dir_salida> [repeticiones_host=5]
# Salida: <dir_salida>/luabench.json con tiempos y cocientes N64/host.
set -euo pipefail

REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$REPO_DIR/tools/env.sh"
out=$(mkdir -p "$1" && realpath "$1"); reps=${2:-5}
src=$TH64_WORK/src/luabench; dl=$TH64_WORK/download
mkdir -p "$src"

declare -A SHA=(
  [5.4.6]=7d5ea1b9cb6aa0b59ca3dde1c6adcb57ef83a1ba8e5432c0ecd06bf439b3ad88
  [5.5.0]=57ccc32bbbd005cab75bcc52444052535af691789dba2b9016d5c50640d68b3d
)

# Búsqueda en C en el host (mismo código que en la ROM).
cat > "$src/bfs_host.c" <<'C'
#include <stdio.h>
#include <time.h>
#include "bfs.h"
int main(void) {
  struct timespec a, b;
  clock_gettime(CLOCK_MONOTONIC, &a);
  long chk = th64_bfs_bench();
  clock_gettime(CLOCK_MONOTONIC, &b);
  printf("TH64C bfs us=%lld chk=%ld\n", (long long)((b.tv_sec - a.tv_sec) * 1000000LL + (b.tv_nsec - a.tv_nsec) / 1000), chk);
  return 0;
}
C
gcc -O2 -I"$REPO_DIR/n64/luabench_common" -o "$src/bfs_host" "$src/bfs_host.c"
for i in $(seq "$reps"); do "$src/bfs_host"; done > "$out/host_c.txt"

for v in 5.4.6 5.5.0; do
  tgz=$dl/lua-$v.tar.gz
  [ -f "$tgz" ] || curl -sS -L --fail -o "$tgz" "https://www.lua.org/ftp/lua-$v.tar.gz"
  echo "${SHA[$v]}  $tgz" | sha256sum -c --quiet
  rm -rf "$src/lua-$v" && tar -xzf "$tgz" -C "$src"
  # Host: intérprete estándar con -O2 (el Makefile de Lua ya usa -O2).
  make -s -C "$src/lua-$v/src" -j"$(nproc)" linux > /dev/null
  for i in $(seq "$reps"); do
    "$src/lua-$v/src/lua" -e 'th64_us = function() return math.floor(os.clock() * 1e6) end th64_log = print' \
      "$REPO_DIR/n64/luabench/filesystem/run.lua"
  done > "$out/host_lua$v.txt"
  # N64: ROM con libdragon (-O2 de n64.mk) y ejecución en ares hasta "TH64 fin".
  make -s -C "$REPO_DIR/n64/luabench" LUA_DIR="$src/lua-$v/src" ROMNAME="luabench$v" > /dev/null
  ARES_UNTIL="TH64 fin" "$REPO_DIR/tools/run_ares.sh" "$REPO_DIR/n64/luabench/luabench$v.z64" \
    "$out/ares_lua$v" 600 > /dev/null
  cp "$out/ares_lua$v/isviewer.log" "$out/n64_lua$v.txt"
done

python3 -I - "$out" <<'PY'
import json, math, re, statistics, sys
out = sys.argv[1]
def parse(path):
    res = {}
    for line in open(path):
        m = re.match(r"TH64(LUA|C) (\w+) us=(\d+) chk=(\S+)", line.strip())
        if m:
            res.setdefault(m.group(2), []).append((int(m.group(3)), m.group(4)))
    return res
result = {"benchmarks": {}}
host_c, n64_c = parse(f"{out}/host_c.txt"), parse(f"{out}/n64_lua5.4.6.txt")
for v in ("5.4.6", "5.5.0"):
    host, n64 = parse(f"{out}/host_lua{v}.txt"), parse(f"{out}/n64_lua{v}.txt")
    if v == "5.4.6":
        host["bfs_c"], n64["bfs_c"] = host_c["bfs"], n64_c["bfs"]
    rows, ratios = {}, []
    for name, hv in host.items():
        if name not in n64:
            continue
        h_us = statistics.median(x[0] for x in hv)
        n_us = n64[name][0][0]
        same = {x[1] for x in hv} == {n64[name][0][1]}
        r = n_us / h_us
        rows[name] = {"host_us_mediana": h_us, "n64_us": n_us, "cociente": r, "misma_suma_control": same}
        if name != "bfs_c":
            ratios.append(r)
    result["benchmarks"][f"Lua {v}"] = {
        "pruebas": rows,
        "cociente_media_geometrica_lua": math.exp(statistics.fmean(math.log(r) for r in ratios)),
        "cociente_min_lua": min(ratios), "cociente_max_lua": max(ratios),
    }
cal = {}
for v in ("5.4.6", "5.5.0"):
    for line in open(f"{out}/n64_lua{v}.txt"):
        m = re.match(r"TH64CAL (\w+) (.*)", line.strip())
        if m:
            cal.setdefault(v, {})[m.group(1)] = dict(kv.split("=") for kv in m.group(2).split())
result["calibrado_n64"] = cal
json.dump(result, open(f"{out}/luabench.json", "w"), indent=1, ensure_ascii=False)
for v, c in cal.items():
    print("calibrado", v, c)
for v, b in result["benchmarks"].items():
    print(v, "media geométrica N64/host = %.1f (%.1f-%.1f)" % (b["cociente_media_geometrica_lua"], b["cociente_min_lua"], b["cociente_max_lua"]))
    for n, r in b["pruebas"].items():
        print("   %-9s host %8.0f us  N64 %10d us  x%6.1f  %s" % (n, r["host_us_mediana"], r["n64_us"], r["cociente"], "ok" if r["misma_suma_control"] else "SUMA DISTINTA"))
PY
