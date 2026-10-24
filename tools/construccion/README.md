# Scripts de la fase 4 (cómo se construye el motor)

Scripts del PC que sostienen las cifras de [`docs/04_construccion.md`](../../docs/04_construccion.md). Las ROMs de medida están en [`n64/pruebas/`](../../n64/pruebas/README.md).

Antes de nada: `source tools/env.sh`.
- Los scripts de Python se ejecutan con `python3 -I`.
- Los de Lua, con `lua5.4` de serie.
- Los que dicen «guion para `bench_run.sh`» se ejecutan dentro de CorsixTH con el arnés de la fase 1. Por ejemplo: `tools/bench_run.sh tools/construccion/metodo/oraculo/volcado.lua <dir> --load=lleno.sav --th64-hours=2000 --th64-frames=0`.
- Ninguno escribe nada en el repositorio. Los que leen los datos del juego los toman de `$TH_DATA_DIR`.

| Script | Qué hace | Resultado en el documento |
|---|---|---|
| `sprites/tmem_stats.py <TH_DATA_DIR> <salida.json>` | Tamaños de los 4.601 sprites de `DATAM` y cuántos no caben en la TMEM en CI8 o CI4 | 29 no caben en CI8; mediana 11 × 13 |
| `sprites/ele_flags.py <TH_DATA_DIR>` | Flags de los elementos de animación de `DATAM` (espejo, transparencias) y elementos por fotograma | 30,6 % con espejo; mediana de 14 elementos |
| `sprites/overhead.py <TH_DATA_DIR>` | RAM de los sprites como `.sprite` sueltos frente a un formato propio por hoja | 3,96 frente a 1,34 MiB |
| `sprites/gen_png.py`, `sprites/read_sprite.py` | PNG indexados sintéticos y lector de `.sprite`, para comprobar que `mksprite` conserva índices y paleta | Idénticos |
| `fuentes/th2bmfont.py <TAB> <DAT> <MPALETTE.DAT> <salida> [LANG.DAT]` | Fuente de TH a BMFont de texto para `mkfont`, y cobertura de los caracteres de un `LANG-x.DAT` | Sin glifos que falten en `LANG-4`; faltan Á, Í, Ó y Ú |
| `fidelidad/sam_keys.py <LEVELS> <Lua> <salida.json>` | Campos de los `.SAM` de la campaña que lee y que no lee el Lua de CorsixTH | 83 de 197 sin leer |
| `fidelidad/base_vs_sam.py <base_config.lua> <LEVELS>` | Claves de `base_config.lua` que no están en ningún `.SAM` (parámetros nuevos de CorsixTH) | 13 |
| `fidelidad/lang_dump.py <LANG-x.DAT> sección,cadena…` | Lee cadenas concretas de un `LANG-x.DAT`, para comprobar qué mecánicas tenía el original | — |
| `metodo/datos_codigo.py <Lua> <salida.json>` | Clasifica las líneas de los directorios de lógica en datos, andamiaje y funciones (con los rangos de `luac -p -l`) | 34,3 % datos |
| `metodo/random_usos.py <Lua> <salida.json>` | Llamadas a `math.random` en la lógica | 332 |
| `metodo/extraer/extraer.lua <Lua> <dir_salida>` | Carga las 126 definiciones con stubs y las vuelca a JSON | 126 de 126 |
| `metodo/extraer/initpatient.lua <Lua> <N>` | Convierte el aspecto de cada enfermedad (`initPatient`) en una tabla | 33 de 34 tabulables |
| `metodo/extraer/base_config_dump.lua <base_config.lua>` | Cuenta las entradas de `base_config.lua` | 1.138 |
| `metodo/oraculo/volcado.lua` | Guion para `bench_run.sh`: vuelca el estado por hora de juego | Reproducibilidad bit a bit sin dibujar |
| `metodo/oraculo/eventos.lua` | Guion para `bench_run.sh`: traza de llegadas, diagnósticos, curas, muertes y salidas | No altera la simulación |
| `metodo/oraculo/sonda_*.lua` | Guiones para `bench_run.sh`: duración de animaciones, orden de `pairs`, llamadas a `math.random`, estado de las consultas | — |
| `metodo/oraculo/anim_len.py <VSTART> <VFRA> <anim_len.txt>` | Duración de las 5.186 animaciones calculada desde los ficheros originales, frente a la de CorsixTH | 5.183 iguales; 3 ajustes conocidos |
| `metodo/oraculo/campana.sh [dir]` | Campaña completa: reproducibilidad, Lua 5.5, `math.random` por fase y 10 semillas | — |
| `metodo/oraculo/comparar.py`, `estadistica2.py` | Comparación exacta de volcados y dispersión entre semillas | ±1,9 curados con N = 10 |
| `metodo/oraculo/crear_hito4.lua` | Primer intento de partida mínima para el hito 4. No servía tal cual: se probaba durante 400 horas de juego, y los pacientes tardan más de 1.000 en llegar andando a la recepción. Lo sustituye `bench/scenarios/crear_hito4.lua` ([`docs/05_prototipo.md`](../../docs/05_prototipo.md#escenario-del-hito-4)) | — |
| `demo/niveles.lua <Lua> <Levels> <LEVELS> <mapas>` | Casillas, habitaciones, enfermedades y condiciones de victoria de cada nivel (los mapas, descomprimidos de RNC antes) | Tabla de niveles |
| `demo/anims.lua`, `demo/sprites.py <DATAM> <Lua> <anims.tsv>` | Animaciones y sprites que necesita el nivel 1 | 2.046 sprites, 0,47 MiB |
| `demo/banco.py <sf2> <mid…>` | Instrumentos del SoundFont que usa la música del juego | 28 instrumentos |
| `demo/loc.py <Lua>` | Líneas de Lua por parte: nivel 1, niveles 2–12, ventanas | ~10 % exclusivo del juego completo |
