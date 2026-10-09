# Variables del entorno de trabajo. Uso: source tools/env.sh
# Las rutas coinciden con las que usa tools/setup.sh.
export TH64_WORK=${TH64_WORK:-$HOME/th64-work}
export N64_INST=${N64_INST:-/opt/libdragon}
export TRACY_PREFIX=${TRACY_PREFIX:-/opt/tracy}
export TH_DATA_DIR=${TH_DATA_DIR:-$TH64_WORK/data/HOSP}
export CORSIXTH_DIR=${CORSIXTH_DIR:-$TH64_WORK/src/CorsixTH}
export ARES_BIN=${ARES_BIN:-$TH64_WORK/src/ares/build/desktop-ui/ares}

case ":$PATH:" in
  *":$N64_INST/bin:"*) ;;
  *) export PATH="$N64_INST/bin:$TRACY_PREFIX/bin:$PATH" ;;
esac
