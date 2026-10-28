# Motor de Theme Hospital 64

Motor propio en C sobre libdragon (fase 5). La lógica de CorsixTH v0.70.1 sirve de especificación; ver [`docs/04_decision.md`](../docs/04_decision.md) y [`docs/04_construccion.md`](../docs/04_construccion.md).

| Ruta | Contenido |
|---|---|
| `core/` | Núcleo de la simulación en C portable (gnu17): sin libdragon, solo enteros y coma fija Q16.16, pools de tamaño fijo. Compila igual en el PC y en la N64 |
| `vista/` | La vista del mapa en C portable: proyección isométrica, lista de sprites en el orden de dibujo de CorsixTH y un rasterizador por software que sirve de referencia. Las cuatro vistas (`girar.c`): el mapa, las paredes con su cara, las sombras, los objetos y las personas, girados. La usan la N64 y `th64sim` |
| `ui/` | La interfaz en C portable (fase 6): ventanas, paneles y botones como los de `window.lua`, el texto con las fuentes de mapa de bits (`bitmap_font`) y el formato de las cadenas de CorsixTH, el panel inferior (`UIBottomPanel`) y el puntero que mueve el stick. Se dibuja en la misma lista que el mapa |
| `platform/n64/` | Capa de la consola: vídeo, RDP, mandos, sonido, ficheros. La simulación va en una corrutina repartida entre fotogramas (`simulacion.c`), el dibujo con el RDP está en `mapa_n64.c` y la interfaz de la partida, con los mandos leídos en cada barrido, en `interfaz.c` |
| `platform/pc/` | `th64sim`, el núcleo en el PC sin pantalla |
| `tests/` | Pruebas del núcleo (`th_test.h`), las mismas en el PC y en la ROM |
| `filesystem/` | Assets de la ROM. Los genera el conversor a partir de los datos del juego de cada usuario; no están en el repositorio |

```bash
source tools/env.sh
tools/motor_check.sh        # todo: PC, PC con sanitizers, pruebas en ares, ROM
make -C motor               # ROM: motor/th64.z64
make -C motor PRUEBAS=1     # ROM con las pruebas del núcleo: motor/th64_pruebas.z64
make -C motor HORAS=4900    # ROM con los hospitales 4.900 horas adelantados: motor/th64_horas.z64
make -C motor HORAS=300 MODO=2 GIRO=1  # la misma, que abre el hospital de las vistas girado, sin menú (capturas)
make -C motor MEDIR=1       # ROM de medida del hito 5: motor/th64_medir.z64 (tools/hito5/medir.sh)
make -C motor PRUEBAS=1 ESCENARIO=hito5  # las pruebas con el escenario del hito 5 (por defecto, hito4)
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
- `th64sim lista <assets> <escenario.trg> <bloques.tsh> <sprites.tsh> <animaciones.tan> <unidad> <x> <y> <ancho> <alto> <horas> <veces> [semilla]`: la lista de dibujo del hospital tras unas horas, hecha `veces` veces: lo que tarda y su huella. Con la semilla 5 (por defecto, 1), las horas y la cámara de la ROM de medida del hito 5, es la misma huella que escribe la ROM. Sirve para perfilar la vista con callgrind y comprobar que un cambio da la misma lista;
- `th64sim hito4 <assets> <escenario.trg> <semilla> <horas> <dir>`: simula el hospital del escenario y escribe las trazas de `bench/scenarios/traza_hito4.lua` (`pacientes.tsv`, `dinero.tsv`, `dias.tsv`, `acciones.tsv`) y la huella. Con `TH64_SONDA_GP=<dir>`, además lo que escribe `bench/scenarios/sonda_gp.lua`; con `TH64_DEPURAR=<id>`, la cola de acciones de ese paciente tras cada hora;
- `th64sim vista ...`: dibuja una vista del mapa en PPM con el rasterizador de referencia;
- `th64sim escena ...`: lo mismo, con el humanoide de una caminata en un tick dado;
- `th64sim escena4 ...`: lo mismo con el hospital de un escenario (objetos y personas), tras unas horas de simulación;
- `th64sim panel <alta|baja> <nivel.tmp> <n> <dir_hojas> <textos.trg> <capturas.txt> <dir_salida>`: el mapa y el panel inferior en cada estado de `bench/scenarios/captura_panel.lua` (dinero, fecha, reputación, pausa, puntero, botón pulsado y descripción). Con `alta`, a 640 × 480 con los gráficos de CorsixTH, para compararlo con sus capturas; con `baja`, a 320 × 240 con los de la N64 ([`tools/hito6/`](../tools/hito6/README.md));
- `th64sim tabla_giro <salida.tsv>`: la pieza girada de cada bloque del mapa en cada vista (para el oráculo de las vistas).

`vista`, `escena4` y `lista` aceptan `--giro=r`: la cámara girada r × 90° con el mismo punto del mundo en el centro. Con `--camara-girada`, la cámara que se da ya es la de la vista girada.

Las de dibujar las usan [`tools/oraculo_mapa/`](../tools/oraculo_mapa/README.md) y [`tools/oraculo_hito4/`](../tools/oraculo_hito4/README.md).

La ROM normal arranca en un menú (hito 6.2) que abre:
- el hospital del hito 4 (`assets/escenarios/hito4.trg`, lo copia `tools/oraculo_hito4/motor.sh`);
- el del hito 5 (`hito5.trg`, el mismo con 18 llegadas al mes);
- el hospital de las vistas (`vistas.trg`, el nivel 1 con sus 8 salas, lo hace `tools/hito6/vistas.sh`);
- el visor de niveles.

Los que no estén en la ROM salen marcados. La cruceta o el stick eligen y A abre.

En un hospital, con el panel inferior de Theme Hospital y un puntero (hito 6.1):
- el stick mueve el puntero, con aceleración; Z mantenido lo hace ir más deprisa. El ratón de N64 también lo mueve;
- A y B son los botones izquierdo y derecho del ratón. Los botones del panel se hunden, pero todavía no abren nada;
- los botones C y la cruceta arriba y abajo desplazan la vista, y también el puntero al tocar un borde de la pantalla;
- la cruceta izquierda y derecha gira la vista 90° (hito 6.2), con el sol quieto en el mundo (hito 6.2b);
- con el puntero un momento sobre un botón del panel, sale su descripción;
- Start pausa (el panel dice «Pause»); en pausa, Z vuelve al menú;
- L alterna entre velocidad Normal (un tick, una hora de juego, cada 54 ms) y 8 veces más deprisa. La simulación avanza en el tiempo que le sobra a cada fotograma: si no le da tiempo, el juego va más despacio, pero sin saltar fotogramas;
- R muestra los datos de depuración, con la casilla que señala el puntero, resaltada en azul.

En el visor de niveles, un humanoide pasea por el mapa:
- el stick o la cruceta arriba y abajo mueven la cámara; la cruceta izquierda y derecha la gira;
- A sigue al primer humanoide o deja de seguirlo (mover la cámara también lo deja);
- B añade 10 humanoides que pasean por casillas al azar;
- Z pasa al nivel siguiente;
- R oculta los datos;
- L mide el tiempo del RDP en cada fotograma (solo en la consola: ares no lo emula);
- Start vuelve al menú.

Reglas del núcleo:
- solo tipos de `stdint`, sin `long` ni `int` sin tamaño en lo que se guarda;
- sin `float` ni `double` en el estado de la simulación;
- recorridos en orden de índice;
- sin `malloc` después de iniciar.
