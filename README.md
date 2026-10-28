# Theme Hospital → Nintendo 64

Estudio de viabilidad y prototipo de Theme Hospital (Bullfrog, 1997) en Nintendo 64 con [libdragon](https://github.com/DragonMinded/libdragon), tomando [CorsixTH](https://github.com/CorsixTH/CorsixTH) como referencia.

Este repositorio **no contiene datos del juego**. Cada usuario aporta los suyos, que se leen desde `TH_DATA_DIR` (ver `docs/00_entorno.md`).

- Plan y fases: [`docs/PLAN.md`](docs/PLAN.md)
- Fase 0, entorno: [`docs/00_entorno.md`](docs/00_entorno.md)
- Fase 1, mediciones de CorsixTH: [`docs/01_mediciones.md`](docs/01_mediciones.md)
- Fase 2, inventario de datos y presupuesto de cartucho: [`docs/02_datos.md`](docs/02_datos.md)
- Fase 3, arquitectura de CorsixTH y reutilización: [`docs/03_arquitectura.md`](docs/03_arquitectura.md)
- Fase 4, decisión (camino, RAM, recortes, controles, riesgos): [`docs/04_decision.md`](docs/04_decision.md), y cómo se construye el motor (herramientas, APIs, método, fidelidad, modo demo): [`docs/04_construccion.md`](docs/04_construccion.md)
- Fase 5, prototipos en N64: [`docs/05_prototipo.md`](docs/05_prototipo.md). El hospital mínimo (recepción, consulta, cobro y salida) funciona en la ROM con la misma simulación que en el PC, comparado con CorsixTH, y con 50 pacientes a 30 fps
- Fase 6, juego base (el nivel 1 jugable), en curso: plan en [`docs/06_plan.md`](docs/06_plan.md) y cifras de cada hito en [`docs/06_juego_base.md`](docs/06_juego_base.md). Hasta ahora, el panel inferior y el puntero (hito 6.1), y las cuatro vistas y el menú de la ROM de prueba (hito 6.2)

```bash
tools/setup.sh          # instala todo (Ubuntu 24.04); necesita TH_DATA_URL o TH_DATA_ZIP para los datos
source tools/env.sh
make -C n64/hello && tools/run_ares.sh n64/hello/th64hello.z64 out/hello
tools/bench_medir.sh "$TH64_WORK/resultados/fase1" 3   # campaña de medición de la fase 1
tools/datos/fase2.sh                                    # inventario y tamaños de los datos (fase 2)
make -C n64/rambase && ARES_UNTIL="TH64 ram: fin" tools/run_ares.sh n64/rambase/th64rambase.z64 out/rambase 30   # RAM libre (fase 4)
python3 -I tools/conv/conv.py && tools/motor_check.sh   # assets del motor y todas sus comprobaciones (fase 5)
make -C motor && tools/run_ares.sh motor/th64.z64 out/motor 30   # el motor en ares
```

Estructura:

| Ruta | Contenido |
|---|---|
| `docs/` | plan y documento de hallazgos de cada fase |
| `tools/` | instalación del entorno, ejecución sin pantalla (CorsixTH, ares, mediciones), análisis de los datos (`tools/datos/`), del código (`tools/arquitectura/`), de cómo construir el motor (`tools/construccion/`), el conversor de datos del motor (`tools/conv/`), los oráculos del mapa (`tools/oraculo_mapa/`) y los inventarios del arte (`tools/arte/`) |
| `bench/` | arnés de medición dentro de CorsixTH, partidas de referencia y resultados resumidos |
| `n64/` | ROMs: hola mundo, benchmarks de CPU (`luabench`), medida de RAM libre (`rambase`) y ROMs de medida de la fase 4 (`pruebas/`) |
| `motor/` | el motor de la fase 5: núcleo portable, vista del mapa, capa de la N64 y pruebas ([`motor/README.md`](motor/README.md)) |
