# Motor de Theme Hospital 64

Motor propio en C sobre libdragon (fase 5). La lógica de CorsixTH v0.70.1 sirve de especificación; ver [`docs/04_decision.md`](../docs/04_decision.md) y [`docs/04_construccion.md`](../docs/04_construccion.md).

| Ruta | Contenido |
|---|---|
| `core/` | Núcleo de la simulación en C portable (gnu17): sin libdragon, solo enteros y coma fija Q16.16, pools de tamaño fijo. Compila igual en el PC y en la N64 |
| `vista/` | La vista del mapa en C portable: proyección isométrica, lista de sprites en el orden de dibujo de CorsixTH y un rasterizador por software que sirve de referencia. La usan la N64 y `th64sim` |
| `platform/n64/` | Capa de la consola: vídeo, RDP, mandos, sonido, ficheros |
| `platform/pc/` | `th64sim`, el núcleo en el PC sin pantalla |
| `tests/` | Pruebas del núcleo (`th_test.h`), las mismas en el PC y en la ROM |
| `filesystem/` | Assets de la ROM. Los genera el conversor a partir de los datos del juego de cada usuario; no están en el repositorio |

```bash
source tools/env.sh
tools/motor_check.sh        # todo: PC, PC con sanitizers, pruebas en ares, ROM
make -C motor               # ROM: motor/th64.z64
make -C motor PRUEBAS=1     # ROM con las pruebas del núcleo: motor/th64_pruebas.z64
make -C motor HORAS=4900    # ROM normal con el hospital 4.900 horas adelantado: motor/th64_horas.z64
make -C motor MEDIR=1       # ROM de medida del hito 5: motor/th64_medir.z64 (tools/hito5/medir.sh)
make -C motor ESCENARIO=hito5  # con make o make HORAS=N: abrir el escenario del hito 5 (por defecto, hito4)
cmake -S motor -B motor/build/pc -G Ninja && cmake --build motor/build/pc && ctest --test-dir motor/build/pc
```

`th64sim` (en `motor/build/pc/`):
- `th64sim huella <semilla> <ticks>`: huella de la simulación de referencia, la misma que en la ROM;
- `th64sim mapa <nivel.tmp> <n> [mapa_nivelN.tsv]`: carga un nivel y lo compara casilla a casilla con el volcado de CorsixTH (`bench/scenarios/volcar_mapa.lua`); con `--volcar celdas.tsv`, vuelca el mapa del núcleo en el formato de `bench/scenarios/exportar_escenario.lua`;
- `th64sim escenario <nivel.tmp> <escenario.trg> <celdas.tsv>`: carga un nivel con un escenario exportado de CorsixTH (`tools/oraculo_hito4/escenario.py`) y compara las 16.384 casillas con las de CorsixTH;
- `th64sim caminos <nivel.tmp> <n> <caminos_nivelN.tsv>`: repite con el A* del núcleo los caminos que pidió CorsixTH (`bench/scenarios/caminos.lua`) y los compara casilla a casilla;
- `th64sim caminata <nivel.tmp> <animaciones.tan> <caminata.tsv>`: repite con el núcleo la caminata de un humanoide que volcó CorsixTH (`bench/scenarios/caminata.lua`) y la compara tick a tick (casilla, desplazamiento, animación, fotograma y banderas);
- `th64sim reglas <reglas.trg> [ruta]`: vuelca un fichero de reglas, o una parte («objetos.bench.orientations.north»), en el JSON canónico del conversor, y su huella;
- `th64sim carga <assets> <escenario.trg> <semilla> <horas>`: la carga de cada tick (personas, pacientes, cuántas caminan, casillas de temperatura recalculadas, búsquedas de camino y nodos) y lo que tarda en el PC, como la ROM de medida del hito 5 (`tools/hito5`);
- `th64sim hito4 <assets> <escenario.trg> <semilla> <horas> <dir>`: simula el hospital del escenario y escribe las trazas de `bench/scenarios/traza_hito4.lua` (`pacientes.tsv`, `dinero.tsv`, `dias.tsv`, `acciones.tsv`) y la huella. Con `TH64_SONDA_GP=<dir>`, además lo que escribe `bench/scenarios/sonda_gp.lua`; con `TH64_DEPURAR=<id>`, la cola de acciones de ese paciente tras cada hora;
- `th64sim vista ...`: dibuja una vista del mapa en PPM con el rasterizador de referencia;
- `th64sim escena ...`: lo mismo, con el humanoide de una caminata en un tick dado;
- `th64sim escena4 ...`: lo mismo con el hospital de un escenario (objetos y personas), tras unas horas de simulación.

Las tres últimas las usan [`tools/oraculo_mapa/`](../tools/oraculo_mapa/README.md) y [`tools/oraculo_hito4/`](../tools/oraculo_hito4/README.md).

Con el escenario del hito 4 en `assets/escenarios/hito4.trg` (lo copia `tools/oraculo_hito4/motor.sh`), la ROM normal abre el hospital en marcha, con el dinero y la fecha abajo:
- el stick o la cruceta mueven la cámara;
- A sigue al último paciente que ha llegado o deja de seguirlo;
- Z alterna entre velocidad Normal (un tick, una hora de juego, cada 54 ms) y 8 veces más deprisa;
- R oculta los datos; L mide el tiempo del RDP.

Sin el escenario, la ROM normal abre el visor del mapa con un humanoide que pasea por él:
- el stick o la cruceta mueven la cámara;
- A sigue al primer humanoide o deja de seguirlo (mover la cámara también lo deja);
- B añade 10 humanoides que pasean por casillas al azar;
- Z pasa al nivel siguiente;
- R oculta los datos;
- L mide el tiempo del RDP en cada fotograma (solo en la consola: ares no lo emula).

Reglas del núcleo:
- solo tipos de `stdint`, sin `long` ni `int` sin tamaño en lo que se guarda;
- sin `float` ni `double` en el estado de la simulación;
- recorridos en orden de índice;
- sin `malloc` después de iniciar.
