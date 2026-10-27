#!/usr/bin/env bash
# Hito 5: la ROM de medida (make -C motor MEDIR=1) en ares, con y sin
# Expansion Pak, y th64sim carga en el PC con el mismo escenario; después,
# analizar.py junta las dos y resume.
#
# Uso: tools/hito5/medir.sh [dir_salida]  (por defecto $TH64_WORK/resultados/hito5/medida)
# Antes: el escenario del hito 5 en motor/assets/escenarios/hito5.trg
# (tools/oraculo_hito4/campana.sh y motor.sh, ver tools/hito5/README.md).
set -euo pipefail
REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$REPO_DIR/tools/env.sh"
export TH64_WORK
O=$(mkdir -p "${1:-$TH64_WORK/resultados/hito5/medida}" && cd "${1:-$TH64_WORK/resultados/hito5/medida}" && pwd)
make -C "$REPO_DIR/motor" MEDIR=1 -j8 > /dev/null
python3 -I "$REPO_DIR/tools/cache_instrucciones.py" "$N64_INST/bin/mips64-elf-nm" "$REPO_DIR/motor/build/n64_medir/th64_medir.elf" --relleno || echo "AVISO: choques en la caché de instrucciones (motor/platform/n64/relleno_cache.c)"
"$REPO_DIR/motor/build/pc/th64sim" carga "$REPO_DIR/motor/assets" "$REPO_DIR/motor/assets/escenarios/hito5.trg" 1 9000 \
  > "$O/carga_pc.tsv" 2> "$O/carga_pc.txt"
for ram in 8 4; do
  opc=""
  [ "$ram" = 4 ] && opc=--no-expansion
  ARES_SETTINGS="General/HomebrewMode=true" ARES_UNTIL="TH64 hito5: fin\\|ASSERTION FAILED" \
    "$REPO_DIR/tools/run_ares.sh" "$REPO_DIR/motor/th64_medir.z64" "$O/ares_${ram}mib" 1800 $opc > /dev/null || true
done
python3 -I "$REPO_DIR/tools/hito5/analizar.py" "$O"
