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
cmake -S motor -B motor/build/pc -G Ninja && cmake --build motor/build/pc && ctest --test-dir motor/build/pc
```

`th64sim` (en `motor/build/pc/`):
- `th64sim huella <semilla> <ticks>`: huella de la simulación de referencia, la misma que en la ROM;
- `th64sim mapa <nivel.tmp> <n> [mapa_nivelN.tsv]`: carga un nivel y lo compara casilla a casilla con el volcado de CorsixTH (`bench/scenarios/volcar_mapa.lua`);
- `th64sim caminos <nivel.tmp> <n> <caminos_nivelN.tsv>`: repite con el A* del núcleo los caminos que pidió CorsixTH (`bench/scenarios/caminos.lua`) y los compara casilla a casilla;
- `th64sim caminata <nivel.tmp> <animaciones.tan> <caminata.tsv>`: repite con el núcleo la caminata de un humanoide que volcó CorsixTH (`bench/scenarios/caminata.lua`) y la compara tick a tick (casilla, desplazamiento, animación, fotograma y banderas);
- `th64sim reglas <reglas.trg> [ruta]`: vuelca un fichero de reglas, o una parte («objetos.bench.orientations.north»), en el JSON canónico del conversor, y su huella;
- `th64sim vista ...`: dibuja una vista del mapa en PPM con el rasterizador de referencia;
- `th64sim escena ...`: lo mismo, con el humanoide de una caminata en un tick dado.

Las dos últimas las usa [`tools/oraculo_mapa/`](../tools/oraculo_mapa/README.md).

La ROM normal abre el visor del mapa con un humanoide que pasea por él:
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
