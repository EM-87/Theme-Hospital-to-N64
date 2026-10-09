# Theme Hospital → Nintendo 64

Estudio de viabilidad y prototipo de Theme Hospital (Bullfrog, 1997) en Nintendo 64 con [libdragon](https://github.com/DragonMinded/libdragon), tomando [CorsixTH](https://github.com/CorsixTH/CorsixTH) como referencia.

Este repositorio **no contiene datos del juego**. Cada usuario aporta los suyos, que se leen desde `TH_DATA_DIR` (ver `docs/00_entorno.md`).

- Plan y fases: [`docs/PLAN.md`](docs/PLAN.md)
- Fase 0, entorno: [`docs/00_entorno.md`](docs/00_entorno.md)

```bash
tools/setup.sh          # instala todo (Ubuntu 24.04); necesita TH_DATA_URL o TH_DATA_ZIP para los datos
source tools/env.sh
make -C n64/hello && tools/run_ares.sh n64/hello/th64hello.z64 out/hello
```
