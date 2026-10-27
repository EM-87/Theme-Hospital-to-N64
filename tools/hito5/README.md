# Medida del hito 5 (fase 5)

RAM libre, coste por tick y fotogramas con 50 pacientes en la N64, y lo mismo en CorsixTH para compararlo con la estimación de la fase 1. Los resultados están en [`docs/05_prototipo.md`](../../docs/05_prototipo.md#hito-5--medición-en-la-n64).

El escenario es el del hito 4 con 18 llegadas al mes en lugar de 6 (`--th64-ritmo=72`): en la hora 3.501 hay 50 pacientes en el mapa, y 69 en la 9.000.

```bash
source tools/env.sh
# 1. El escenario: 10 semillas en CorsixTH y el motor contra ellas; copia la primera a motor/assets/escenarios/hito5.trg.
#    (La semilla 4 no se puede exportar: en el guardado queda una sala sin puerta. Se usa la 11.)
tools/oraculo_hito4/campana.sh "1 2 3 5 6 7 8 9 10 11" 9000 $TH64_WORK/resultados/hito5/campana --th64-ritmo=72
tools/oraculo_hito4/motor.sh "1 2 3 5 6 7 8 9 10 11" 9000 10 $TH64_WORK/resultados/hito5/campana hito5
# 2. La N64 (ares, con y sin Expansion Pak) y el PC: unos 2 min.
tools/hito5/medir.sh
# 3. CorsixTH en el mismo escenario, horas 3.500 a 4.500, con Tracy (como la fase 1): unos 3 min por repetición.
CORSIXTH_SAVES=$TH64_WORK/resultados/hito5/campana/saves tools/bench_run.sh --tracy --lua 5.4 \
  bench/scenarios/medir_hito5.lua $TH64_WORK/resultados/hito5/corsixth/tracy1 --load=hito4_1.sav \
  --th64-save=hito5 --th64-warmup=3500 --th64-hours=1000
# 4. El tamaño del código y cuánta lógica de CorsixTH cubre el núcleo.
python3 -I tools/hito5/codigo.py motor/build/n64/th64.map
python3 -I tools/hito5/cobertura.py motor/core $TH64_WORK/src/CorsixTH/CorsixTH/Lua
```

| Fichero | Qué hace |
|---|---|
| `medir.sh [dir]` | Compila la ROM de medida (`make -C motor MEDIR=1`), comprueba la caché de instrucciones, corre `th64sim carga` en el PC y la ROM en ares con 8 y 4 MiB, y resume con `analizar.py` |
| `analizar.py <dir>` | La memoria tras cada paso de la carga, la huella (N64 frente a PC), el coste por tick con su regresión (personas, casillas de temperatura recalculadas, nodos y búsquedas de camino), por tramos de personas, y los fotogramas a cada velocidad. Escribe `resumen.json` |
| `codigo.py <rom.map>` | Código, datos de solo lectura, datos y bss del motor (por fichero), libdragon (por módulo) y las bibliotecas de C, del mapa del enlazador |
| `cobertura.py <motor/core> <CorsixTH/Lua>` | Las funciones de CorsixTH que el núcleo nombra en sus comentarios y las acciones de humanoide, en líneas de Lua: una cota de cuánta lógica hay ya en el motor |
| [`bench/scenarios/medir_hito5.lua`](../../bench/scenarios/medir_hito5.lua) | `medir.lua` de la fase 1 con el jugador de la traza del hito 4 (abre los faxes y los contesta igual), para que la partida sea la de la campaña |

La ROM de medida (`motor/platform/n64/main.c`, `TH_MEDIR`):
1. simula tan deprisa como puede hasta que hay 50 pacientes en el mapa, con una línea por tick (`H5 t`);
2. con la música sonando, dibuja 900 fotogramas a velocidad Normal, 300 a «Max speed» y 300 a «And then some more», con la cámara donde se ven más personas. Escribe una línea por fotograma (`H5 f`);
3. sigue hasta la hora 9.000 y escribe la huella, que tiene que ser la de `th64sim carga`.
