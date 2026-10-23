# Fase 5 — Prototipos en N64

Fecha de inicio: 2026-10-23. Estado: **en curso**. Plan: [`PLAN.md`](PLAN.md#fase-5--prototipos-en-n64-solo-tras-aprobar-la-fase-4), con los cambios de [`04_decision.md`](04_decision.md#cambios-al-plan-de-la-fase-5). Cada hito se cierra con un commit, una captura de ares y sus cifras aquí.

## Entorno del motor

**Hecho.** El código está en [`motor/`](../motor/README.md), y [`tools/motor_check.sh`](../tools/motor_check.sh) lo comprueba todo de una vez:

| Comprobación | Resultado |
|---|---|
| Núcleo en el PC con gcc (`-Wall -Wextra -Werror`) | 60.045 comprobaciones, 0 fallos, 0,05 s |
| Lo mismo con ASan y UBSan | 0 fallos |
| ROM de pruebas en ares (modo Homebrew, se cierra sola) | 60.045 comprobaciones, 0 fallos |
| Huella de la simulación de referencia (semilla 1, 10.000 pasos) | `d38fbaef` en el PC y en la N64 |
| ROM normal | Arranca: 8.192 KiB de RDRAM con Expansion Pak, heap usado 588 de 7.865 KiB, DFS correcto |

- **Tamaño.** La ROM ocupa 208 KiB. El código ocupa 245.404 B (`text`), casi todo libdragon: vídeo, RDP, texto y mandos.
- **Tiempo de compilación.** La ROM tarda 0,36 s desde cero y 0,32 s tras tocar un fichero.

![Pantalla de estado del motor en ares](img/05_entorno.png)

*Pantalla de estado de la ROM en ares. El «1 VPS» de la barra de ares se debe al vídeo por software del contenedor, no a la ROM.*

Punto de partida: el núcleo de ejemplo de la fase 4 (`n64/pruebas/nucleo`), con:
- el pool de entidades con asas;
- la coma fija Q16.16;
- el generador xorshift32;
- el marco de pruebas.

## Hito 0 — Conversor (en curso)

El hito 0 se hace por partes. Primero los gráficos, porque es lo único que necesita el hito 1. Las animaciones, las tablas de reglas, los niveles, los textos, las fuentes y el banco de sonidos van después, antes de los hitos 2–4.

**Gráficos: hecho.** [`tools/conv/`](../tools/conv/README.md):
- **RNC.** Un descompresor en Python, portado del de CorsixTH. Da exactamente lo mismo que `rnc_decode` en los 241 ficheros RNC de los datos: 16,3 MB descomprimidos en 11 s. El conversor no necesita compilar CorsixTH.
- **Paletas.** `MPALETTE.DAT` pasa a RGBA5551 con la entrada 255 transparente. Lleva también las dos paletas fantasma de CorsixTH:
  - la gris, que es su mismo cálculo de `makeGreyscaleGhost`;
  - la roja, la tabla 6 de `GHOST1.DAT`.

  Ocupan 1.544 B en total.
- **Hojas.** Las 8 hojas de `DATAM` pasan a TSH1: CI8 con filas de 8 B, la misma numeración que el original y una comprobación de ida y vuelta exacta.

  | Hoja | Sprites | Sin comprimir | En la ROM (aPLib) |
  |---|---:|---:|---:|
  | `MSPR-0` (personajes y objetos) | 4.397 | 1.212.544 B | 392.866 B |
  | `MBLK-0` (suelos y paredes) | 226 | 192.360 B | — |
  | Las otras 6 (panel, reloj, punteros, menús…) | 546 | 309.336 B | — |

  Los píxeles de `MSPR-0` y `MBLK-0` (1.177.352 y 190.536 B) coinciden con lo medido en la fase 4.

**Animaciones: hecho.** Las tablas de `DATAM` (5.186 animaciones, 11.643 fotogramas, 108.607 entradas de lista y 24.983 elementos) pasan al formato TAN1: 481.038 B, dentro de los 0,48 MiB del presupuesto. Cada animación lleva su duración en fotogramas, calculada como `getAnimLength` de CorsixTH. Comprobaciones:
- **El encadenamiento de `DATAM` es idéntico al de `DATA`**, que es el que usa CorsixTH. Las animaciones de baja resolución duran lo mismo.
- **Las duraciones coinciden con las de CorsixTH en 5.183 de 5.186.** Las 3 distintas son los ajustes que hace su Lua: 2348 y 2938 duran 1 fotograma, y 4086 dura 8. Irán con las tablas de reglas.
- **Pendiente para el hito 3:** el desplazamiento de los elementos. CorsixTH solo usa las animaciones de alta resolución y les resta (141, 186); el de `DATAM` se ajustará al dibujarlas sobre el mapa.

**Niveles: hecho** (para el hito 2). Los 12 mapas de `LEVELS` pasan al formato TMP1 (`tools/conv/niveles.py`):
- se leen como en `load_from_th_file` de CorsixTH, con su tabla de bloques;
- cada casilla ocupa 10 B: las capas, las banderas del fichero, el objeto, su dirección y la parcela;
- las paredes van dos veces: las actuales y las originales, que incluyen las paredes divisorias entre parcelas (de 0 a 37 por nivel).

Cada nivel ocupa 163.872 B; en la ROM, con aPLib, entre 2.216 y 5.743 B (42.644 B los 12). El conversor no interpreta nada más: lo demás que hace CorsixTH al cargar es lógica del juego y va en el núcleo (hito 2).

## Hito 1 — La ROM carga los assets y dibuja un sprite con su paleta

**Hecho.**
- **Carga.** La ROM carga `paleta.tpl` y `datam/mspr-0.tsh` del sistema de ficheros de la ROM.
- **Dibujo.** Dibuja con el RDP en CI8, cargando la textura y un rectángulo con APIs estables, como propone [`04_construccion.md`](04_construccion.md#sprites-y-dibujo). Los sprites que no caben en la TMEM pasan por `rdpq_tex_blit`.
- **Visor.** La pantalla muestra un sprite a escala 3 con las tres paletas y reflejado, y una tira con los siguientes. Con la cruceta se cambia de sprite, para verlo en la consola cuando llegue el flashcart.

**Comprobación automática.** Al arrancar, la ROM dibuja el sprite de prueba sobre un fondo conocido y espera al RDP. Después compara cada píxel del framebuffer con el color que le toca según la hoja y la paleta: **598 de 598 píxeles iguales** (sprite 1 de `MSPR-0`, 23 × 26). `tools/motor_check.sh` lo comprueba en cada ejecución.

| Dato | Valor |
|---|---|
| ROM | 688.128 B (`MSPR-0` comprimida con aPLib: 392.866 B) |
| Código (`text`) | 234.904 B |
| Heap usado con `MSPR-0` entera cargada | 1.774 KiB de 7.833 (el presupuesto de la fase 4 cuenta 1,30 MiB para todos los sprites residentes) |

![Hito 1 en ares: un sprite con las paletas normal, fantasma gris y fantasma roja, reflejado, y una tira de sprites](img/05_hito1.png)

*Hito 1 en ares. La paleta roja es la que usa el juego para paredes, puertas y ventanas en una posición no válida; aplicada a una persona no tiene sentido, pero muestra que la tabla se aplica bien.*

## Hito 2 — Mapa isométrico con scroll a 30 fps

**Hecho.** La ROM carga un nivel y lo dibuja con el RDP. La cámara se mueve con el stick a 30 fps, sin saltar fotogramas en ares. El mapa es exacto: el núcleo coincide con CorsixTH casilla a casilla y, con los mismos gráficos que CorsixTH, la vista coincide píxel a píxel. Solo faltan las entidades (puertas y personal), que llegan en el hito 3.

![A la izquierda, CorsixTH a 640 × 480; a la derecha, la ROM en ares a 320 × 240, con la misma cámara](img/05_hito2.png)

*Nivel 1 con la cámara inicial: CorsixTH a la izquierda (gráficos de `DATA`) y la ROM en ares a la derecha (gráficos de `DATAM`, ampliada al doble). Falta la puerta de entrada, que es un objeto. Las cifras de CPU de la pantalla son las del visor, que gasta más que el recorrido medido abajo (ver «Rendimiento»).*

### Qué hay

- **Núcleo: [`th/map.h`](../motor/core/include/th/map.h).** El mapa ocupa 128 × 128 casillas de 16 B: 256 KiB, como preveía [`04_decision.md`](04_decision.md#presupuesto-de-ram). Al cargar un nivel hace lo mismo que CorsixTH (`level_map` en `th_map.cpp` y `Map` en `map.lua`):
  - las parcelas que no son del jugador se ven como césped a rayas con adornos;
  - entre parcelas de distinto dueño se ponen paredes divisorias;
  - calcula el tránsito en las cuatro direcciones (los bordes del mapa y las paredes lo cortan) y las sombras (incluido un descuido de CorsixTH, que mira la bandera «alto» de la casilla equivocada);
  - aplica las correcciones de `Map:_fixTiles` (solo el suelo de interior es hospital; dos casillas del nivel 6).

  Comprar una parcela (`th_map_set_parcel_owner`) ya funciona como en CorsixTH, salvo una cosa: cerrar con pared las salas que queden abiertas. Eso llegará con las salas.
- **Vista: [`motor/vista/`](../motor/vista/vista.h).** Es C portable. Genera la lista de sprites que se ven, en el orden de `level_map::draw`:
  1. todos los suelos, cada uno con su sombra;
  2. por cada línea de casillas, las paredes norte de derecha a izquierda y después las oeste de izquierda a derecha.

  La proyección lleva una «unidad»: 32 con los gráficos de `DATA` (los de CorsixTH) y 16 con los de `DATAM` (la N64). Así, el mismo código dibuja lo que CorsixTH y lo que la consola. El rasterizador de referencia (`raster.c`) pinta la lista sprite a sprite.
- **N64: [`mapa_n64.c`](../motor/platform/n64/mapa_n64.c).** El RDP pinta la lista:
  - los suelos, agrupados por bloque: una carga de textura por bloque distinto y un rectángulo por casilla;
  - las sombras, al 25 % con el mezclador;
  - las paredes, en el orden de la lista.

  Dos suelos que caben en su rombo no se pisan, así que da igual en qué orden se pinten. De los 74 bloques de suelo de `MBLK-0`, 7 se salen del rombo; esos cortan la agrupación para conservar el orden de CorsixTH (se vio en la comparación: el bloque 60 tiene un píxel fuera y pisaba al vecino).
- **Visor.**
  - El stick o la cruceta mueven la cámara, limitada al rombo del mapa.
  - Z pasa al nivel siguiente (los 12).
  - R oculta los datos.
  - L mide el RDP en cada fotograma, solo en la consola: ares no emula sus contadores.

### Comprobaciones

| Qué | Contra qué | Resultado |
|---|---|---|
| Las 16.384 casillas de los 12 niveles: suelo, paredes, transitable, hospital, 5 de edificable, 4 de tránsito, 2 de objeto alto, parcela y objeto | Volcado de CorsixTH justo después de `loadLevel` ([`volcar_mapa.lua`](../bench/scenarios/volcar_mapa.lua) y `th64sim mapa`) | **0 diferencias** lejos de los objetos. Hay 103 casillas distintas, todas a 3 casillas o menos de un objeto del mapa: son las banderas que ponen los objetos al colocarse (edificable y objeto alto). Llegan con los objetos |
| Lo que se ve del mapa a 640 × 480 con los gráficos de `DATA`, en 15 posiciones de cámara de los niveles 1, 5 y 12 (sombras sobre paredes, paredes divisorias, parcelas sin dueño y los bordes del mapa) | Capturas de CorsixTH sin ventanas ni cursor ([`tools/oraculo_mapa/`](../tools/oraculo_mapa/README.md)) | De 4.608.000 píxeles: **4.426.872 idénticos**, 142.815 de sombra con 1 o 2 de 255 de diferencia (redondeo de la mezcla) y 38.313 distintos. Todos los distintos están en las entidades que aún no se dibujan: puertas, médicos y las sombras que proyectan las puertas. **7 de las 15 capturas no tienen ningún píxel distinto** |
| Lo que dibuja el RDP en la N64, en 5 vistas de 320 × 240: en el nivel 1, la cámara inicial, una sombra sobre pared, la esquina del mapa y el exterior; en el 12, parcelas sin dueño con paredes divisorias | El rasterizador de referencia, en la misma ROM | **0 píxeles distintos de 384.000.** De los 5.216 píxeles de sombra, 3.443 difieren en 1 de 31: el mezclador del RDP redondea distinto |
| Pruebas del núcleo | — | 60.125 comprobaciones (80 nuevas del mapa: formato, tránsito, sombras, parcelas y correcciones), 0 fallos en el PC, con ASan y UBSan, y en la N64 |

Los 38.313 distintos de la segunda fila son 37.216 que difieren en más de 2 y 1.097 que difieren en 1 o 2 sin ser de sombra; estos últimos están todos a menos de 3 píxeles de una entidad. `tools/motor_check.sh` repite en cada ejecución la tercera y la cuarta fila.

### Rendimiento

Recorrido automático en ares (ROM de pruebas). Son 416 fotogramas a 30 fps, en el nivel 1: la cámara da la vuelta a un rectángulo de 768 × 512 píxeles alrededor de la cámara inicial, a 8 píxeles por fotograma.

| Dato | Media | Máximo |
|---|---:|---:|
| CPU del mapa por fotograma | **5,24 ms** (15,7 % de 33,3) | 6,73 ms |
| — generar la lista | 1,65 ms | — |
| — suelos y sombras (comandos del RDP) | 2,19 ms | — |
| — paredes | 1,39 ms | — |
| Tiempo entre fotogramas | 33,25 ms | 33,45 ms |
| Fotogramas que se saltan (más de 50 ms) | **0** | — |
| Sprites | 379 | 428 |
| Cargas de textura | 48 | 79 |
| Relleno del RDP (píxeles cubiertos, transparentes incluidos) | 196.380 | 236.064 |

- **Con texto encima.** Una segunda vuelta dibuja además las 5 líneas de texto del visor. El mapa cuesta lo mismo (5,29 ms): el texto no le enfría las cachés. El texto suma 1,69 ms (6,98 ms el fotograma entero, 8,28 de máximo).
- **La vista más cargada** es la cámara inicial: 400 sprites y 73 cargas. Con la cámara fija en ella (medido aparte, con el recorrido parado y con el visor), gasta 6,2 ms en la ROM de pruebas y 7,0 en la normal (lista 1,6; suelos 2,7; paredes 2,6). La diferencia es la disposición del código en la caché de instrucciones: se vio lo mismo en la fase 4 ([`04_construccion.md`](04_construccion.md#sprites-y-dibujo)).
- **Lo que se ganó al medir.** La lista se generaba en tres pasadas por todo el mapa visible, que recorren las casillas en diagonal y fallan en caché. Ahora se genera en una sola pasada por línea, llenando la lista por los dos extremos: de 2,69 a 1,65 ms. El resultado es idéntico (las 15 capturas dan lo mismo).
- **El RDP no se puede medir en ares**, que no emula sus contadores de ciclos. Una estimación: 236.064 píxeles en el peor caso, a un píxel por ciclo en el modo de un ciclo y a 62,5 MHz, son 3,8 ms, más la preparación de cada rectángulo; lejos de los 33,3 ms. En la consola, el botón L da la cifra de verdad. Queda para cuando llegue el flashcart.
- **Para el hito 3.** El mapa deja unos 28 ms por fotograma para la simulación, los humanoides, los objetos y la interfaz. El riesgo de CPU de la fase 4 sigue en los humanoides: cada uno tiene unos 14 elementos de animación, cada uno con su textura.

### Memoria y tamaño

| Dato | Valor |
|---|---|
| Mapa (`th_map`, estático) | 256 KiB |
| Lista de dibujo (4.096 elementos de 10 B, estática) | 40 KiB |
| `MBLK-0` en RAM | 192.360 B (28.098 B en la ROM) |
| Heap usado con el nivel cargado | 778 KiB de 7.518, con los 3 framebuffers (450 KiB) |
| ROM | 737.280 B, con los 12 niveles |
| Código (`text`) | 244.216 B |

### Pendiente

- **Objetos del mapa.** Las puertas de entrada y los demás objetos que trae el mapa del nivel: dibujarlos y poner sus banderas (edificable, objeto alto, que cambia sombras). Es lo único que separa el núcleo de CorsixTH en la comparación de casillas. Hito 3 o 4.
- **Banderas de dibujo de las paredes.** CorsixTH guarda en el byte alto de cada capa banderas de dibujo (espejo, transparencia) que pone el Lua al construir salas. Los mapas originales no las traen; harán falta con las salas (hito 4).
- **Límite de la cámara.** El motor mantiene el centro de la pantalla dentro del rombo del mapa. CorsixTH usa un rombo algo distinto (`limitPointToDiamond`), así que en los bordes la cámara puede quedar unos píxeles más allá o más acá. No afecta al dibujo.

