#!/usr/bin/env bash
# Compara el código del port de CorsixTH a Wii (tueidj, v1.02) con el
# CorsixTH oficial del que parte. Uso:
#   tools/arquitectura/wii_diff.sh <CorsixTH-wii-src.zip> <dir_trabajo>
# Deja en <dir_trabajo>: src/ (ZIP extraído), norm/ (árbol de CorsixTH del port
# con finales de línea LF), upstream/ (historial oficial), diffs/d/*.diff (un
# diff por fichero cambiado, ignorando espacios) y lua-stock/ (Lua 5.1.3).
# El código del ZIP solo se lee: no se compila ni se ejecuta nada de él.
set -euo pipefail
zip=$(realpath "$1"); work=$(realpath -m "$2")
mkdir -p "$work"
rm -rf "$work/src" "$work/norm" "$work/diffs"
mkdir -p "$work/src" "$work/norm" "$work/diffs/base" "$work/diffs/d"
(cd "$work/src" && unzip -q "$zip")
W=$work/src/src/corsix-th
[ -d "$work/upstream" ] || git clone -q --filter=blob:none --no-checkout \
  https://github.com/CorsixTH/CorsixTH.git "$work/upstream"

python3 -I - "$W" "$work" <<'PY'
import os, subprocess, sys
W, work = sys.argv[1], sys.argv[2]
files = sorted(os.path.relpath(os.path.join(d, n), W) for d, _, ns in os.walk(W)
               for n in ns if "__MACOSX" not in d)
hashes = {}
for f in files:
    b = open(os.path.join(W, f), "rb").read()
    if b"\0" not in b[:8000]:
        b = b.replace(b"\r\n", b"\n")
    dst = os.path.join(work, "norm", f)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst, "wb").write(b)
    hashes[f] = subprocess.run(["git", "hash-object", dst], capture_output=True, text=True).stdout.strip()
up = os.path.join(work, "upstream")
commits = subprocess.run(["git", "-C", up, "log", "--format=%h", "--after=2012-11-01", "--before=2013-02-01"],
                         capture_output=True, text=True).stdout.split()
best = None
for c in commits:
    tree = dict(l.split("\t")[::-1] for l in subprocess.run(
        ["git", "-C", up, "ls-tree", "-r", c], capture_output=True, text=True).stdout.splitlines())
    tree = {p: h.split()[2] for p, h in tree.items()}
    same = sum(1 for f, h in hashes.items() if tree.get(f) == h)
    if not best or same > best[1]:
        best = (c, same, tree)
c, same, tree = best
mod = [f for f, h in hashes.items() if f in tree and tree[f] != h]
new = [f for f in hashes if f not in tree]
print(f"base: {c}, {same} de {len(files)} ficheros idénticos; {len(mod)} modificados; nuevos: {new}")
for f in mod:
    base = os.path.join(work, "diffs", "base", f)
    os.makedirs(os.path.dirname(base), exist_ok=True)
    open(base, "wb").write(subprocess.run(["git", "-C", up, "show", f"{c}:{f}"], capture_output=True).stdout)
    d = subprocess.run(["diff", "-u", "-w", "--strip-trailing-cr", base, os.path.join(work, "norm", f)],
                       capture_output=True, text=True).stdout
    if d:
        open(os.path.join(work, "diffs", "d", f.replace("/", "_") + ".diff"), "w").write(d)
if "CorsixTH/Src/th_main.cpp" in new and "CorsixTH/Src/main.cpp" in tree:
    base = os.path.join(work, "diffs", "base", "CorsixTH/Src/main.cpp")
    os.makedirs(os.path.dirname(base), exist_ok=True)
    open(base, "wb").write(subprocess.run(["git", "-C", up, "show", f"{c}:CorsixTH/Src/main.cpp"], capture_output=True).stdout)
    d = subprocess.run(["diff", "-u", "-w", "--strip-trailing-cr", base,
                        os.path.join(work, "norm", "CorsixTH/Src/th_main.cpp")], capture_output=True, text=True).stdout
    open(os.path.join(work, "diffs", "d", "CorsixTH_Src_main.cpp-to-th_main.cpp.diff"), "w").write(d)
PY

if [ ! -d "$work/lua-stock/lua-5.1.3" ]; then
  mkdir -p "$work/lua-stock"
  curl -sS -o "$work/lua-stock/lua-5.1.3.tar.gz" https://www.lua.org/ftp/lua-5.1.3.tar.gz
  tar -C "$work/lua-stock" -xzf "$work/lua-stock/lua-5.1.3.tar.gz"
fi
diff -r -u -w --strip-trailing-cr "$work/lua-stock/lua-5.1.3/src" "$work/src/src/lua/lua-5.1.3/src" \
  > "$work/diffs/d/lua-5.1.3_src.diff" || true
cd "$work/diffs/d"
add=0; del=0
for f in $(ls *.diff | grep -v '^lua-5'); do
  add=$((add + $(grep '^+' "$f" | grep -vc '^+++' || true)))
  del=$((del + $(grep '^-' "$f" | grep -vc '^---' || true)))
done
echo "$(ls *.diff | grep -vc '^lua-5') ficheros de CorsixTH con cambios: +$add -$del líneas (sin espacios)"
