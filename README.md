# Theme Hospital → Nintendo 64

Estudio de viabilidad y prototipo de Theme Hospital (Bullfrog, 1997) en Nintendo 64 con [libdragon](https://github.com/DragonMinded/libdragon), tomando [CorsixTH](https://github.com/CorsixTH/CorsixTH) como referencia.

Este repositorio **no contiene datos del juego**. Cada usuario aporta los suyos, que se leen desde `TH_DATA_DIR` (ver `docs/00_entorno.md`).

- Plan y fases: [`docs/PLAN.md`](docs/PLAN.md)
- Fase 0, entorno: [`docs/00_entorno.md`](docs/00_entorno.md)
- Fase 1, mediciones de CorsixTH: [`docs/01_mediciones.md`](docs/01_mediciones.md)
- Fase 2, inventario de datos y presupuesto de cartucho: [`docs/02_datos.md`](docs/02_datos.md)

```bash
tools/setup.sh          # instala todo (Ubuntu 24.04); necesita TH_DATA_URL o TH_DATA_ZIP para los datos
source tools/env.sh
make -C n64/hello && tools/run_ares.sh n64/hello/th64hello.z64 out/hello
tools/bench_medir.sh "$TH64_WORK/resultados/fase1" 3   # campaña de medición de la fase 1
tools/datos/fase2.sh                                    # inventario y tamaños de los datos (fase 2)
```

Estructura:

| Ruta | Contenido |
|---|---|
| `docs/` | plan y documento de hallazgos de cada fase |
| `tools/` | instalación del entorno, ejecución sin pantalla (CorsixTH, ares, mediciones) y análisis de los datos (`tools/datos/`) |
| `bench/` | arnés de medición dentro de CorsixTH, partidas de referencia y resultados resumidos |
| `n64/` | ROMs: hola mundo y benchmarks de CPU |
