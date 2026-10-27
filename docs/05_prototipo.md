# Fase 5 — Prototipos en N64

Fecha de inicio: 2026-10-23. Estado: **hitos 0 a 5 hechos**, y después, en parte, los riesgos de CPU que dejó el hito 5. Plan: [`PLAN.md`](PLAN.md#fase-5--prototipos-en-n64-solo-tras-aprobar-la-fase-4), con los cambios de [`04_decision.md`](04_decision.md#cambios-al-plan-de-la-fase-5). Cada hito se cierra con un commit, una captura de ares y sus cifras aquí.

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

## Hito 0 — Conversor

**Hecho.** Solo queda una parte que usa la interfaz (al final de esta sección). Se hizo por partes: primero los gráficos, porque es lo único que necesitaba el hito 1, y después las animaciones, los niveles, las reglas, los textos, las fuentes y el sonido, antes de los hitos 2–4.

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
- **Las duraciones coinciden con las de CorsixTH en 5.183 de 5.186.** Las 3 distintas son los ajustes que hace su Lua: 2348 y 2938 duran 1 fotograma, y 4086 dura 8. Van en las reglas (ver abajo).
- **El desplazamiento de los elementos** (resuelto en el hito 3). CorsixTH solo usa las animaciones de alta resolución y les resta (141, 186). En `DATAM` todas las x crudas son impares y todas las y pares, así que (x − 141)/2 e (y − 186)/2 son exactas: el mismo origen, a media escala.

**Niveles: hecho** (para el hito 2). Los 12 mapas de `LEVELS` pasan al formato TMP1 (`tools/conv/niveles.py`):
- se leen como en `load_from_th_file` de CorsixTH, con su tabla de bloques;
- cada casilla ocupa 10 B: las capas, las banderas del fichero, el objeto, su dirección y la parcela;
- las paredes van dos veces: las actuales y las originales, que incluyen las paredes divisorias entre parcelas (de 0 a 37 por nivel).

Cada nivel ocupa 163.872 B; en la ROM, con aPLib, entre 2.216 y 5.743 B (42.644 B los 12). El conversor no interpreta nada más: lo demás que hace CorsixTH al cargar es lógica del juego y va en el núcleo (hito 2).

**Reglas: hecho** (para el hito 4). Son las definiciones de CorsixTH y la configuración de cada nivel, en el formato TRG1 ([`tools/conv/reglas.py`](../tools/conv/reglas.py)).

- **Extracción.** [`reglas.lua`](../tools/conv/reglas.lua) carga el código de CorsixTH con `lua5.4` de serie y unos stubs mínimos, como en la fase 4, y lo procesa como el juego al arrancar (`Object.processTypeDefinition`). Saca:
  - 60 objetos, 23 salas, 34 enfermedades y 9 diagnósticos;
  - 332 marcadores de animación (272 llamadas, desplegadas como `_unfoldAnims`);
  - las animaciones de caminar de cada clase y la configuración por defecto de CorsixTH;
  - `base_config.lua`;
  - la configuración de los 36 niveles de la campaña (3 dificultades × 12), con la propia `Map:_loadOriginalCampaignLevel` de CorsixTH: `base_config.lua`, encima `FULL00.SAM`, `FULL01.SAM` y, en 5 niveles, el `originalNN.level` de CorsixTH.
- **Las 3 duraciones fijadas en el Lua** se resuelven con las tablas de animación: 2348 y 2938 duran 1 fotograma, y 4086 dura 8, porque es `getAnimLength(2166)`.
- **El aspecto de los pacientes** (`initPatient`, que elige tipo y capas con `math.random`) pasa a una distribución exacta. Se recorren todas las ramas de sus llamadas a `math.random`, ejecutando la propia función de CorsixTH. Después se factoriza: el tipo y, dentro de cada tipo, bloques de capas independientes, comprobado con fracciones. Sale exacto en las 34 enfermedades. Solo los huesos rotos necesitan un bloque de 3 capas (las vendas, 71 combinaciones), y con la configuración por defecto de CorsixTH solo hay hombres con huesos rotos.
- **TRG1** guarda el árbol tal cual, como el de Lua: enteros, reales, cadenas, listas y mapas con las claves ordenadas, y las claves enteras distintas de las de texto. El núcleo ([`th/rules.h`](../motor/core/include/th/rules.h)):
  - valida el fichero entero al abrirlo;
  - busca por búsqueda binaria;
  - lee los reales en Q16.16 solo con enteros;
  - lee lo que necesita al cargar y suelta el fichero.

| Fichero | Sin comprimir | En la ROM | Validar en la N64 |
|---|---:|---:|---:|
| `reglas/definiciones.trg` | 336.868 B | 60.908 B | 50 ms |
| Un nivel (`reglas/full01.trg`…) | 26.936–30.740 B | unos 5.000 B | 4 ms |
| Los 37 | 1.379.440 B | 257.726 B | — |

Comprobaciones:
- **Ida y vuelta:** los 37 ficheros se leen igual que se escribieron, con igualdad estricta (1 no es 1,0).
- **El núcleo frente al conversor:** `th64sim reglas` vuelca cada fichero al mismo JSON canónico, y salen los 37 iguales. En la N64, la ROM de pruebas da la misma huella que el PC (`tools/motor_check.sh`).
- **Contra CorsixTH:** [`volcar_reglas.lua`](../bench/scenarios/volcar_reglas.lua) vuelca, desde dentro del juego, lo que tiene en memoria. [`comparar_reglas.py`](../tools/conv/comparar_reglas.py) lo compara con el extractor, sin contar las cadenas de idioma, que en el juego ya están traducidas:

  | Qué | Iguales |
  |---|---:|
  | Objetos | 60 de 60 |
  | Salas | 23 de 23 |
  | Enfermedades | 34 de 34 |
  | Diagnósticos | 9 de 9 |
  | Configuración de nivel | 36 de 36 |

  Para comprobar el comparador, se metieron 3 cambios a mano (un entero, un entero cambiado por real y una lista más corta), y detectó los 3.

**Textos: hecho.** [`textos.lua`](../tools/conv/textos.lua) ejecuta el `strings.lua` de CorsixTH con `lua5.4`, igual que en el juego: cada idioma hereda de otro y de las cadenas originales (`LANG-0.DAT` el inglés y `LANG-4.DAT` el español). Lo que en el juego es C++ se hace aquí en Lua:
- la lectura de `LANG-x.DAT` (`th_string_list`);
- el paso de CP437 a UTF-8, con la tabla del propio CorsixTH (`Src/cp437_table.h`), que no es CP437 puro: 0x92 pasa a «?».

| Idioma | Cadenas | TRG1 | Validar en la N64 |
|---|---:|---:|---:|
| Inglés | 5.524 | 263.592 B | 18 ms |
| Español | 5.543 | 289.036 B | 18 ms |

Comprobaciones:
- [`volcar_textos.lua`](../bench/scenarios/volcar_textos.lua) carga los dos idiomas dentro de CorsixTH, con su lector en C++. `comparar_reglas.py --textos` da las dos tablas idénticas, cadena a cadena.
- La N64 lee los dos ficheros con la misma huella que el PC.

Las tablas incluyen todo lo que añade CorsixTH: opciones de PC, trucos, errores. Son unas tres veces lo que preveía el presupuesto de RAM (0,10 MiB por idioma). Cuando llegue la interfaz se verá si se quitan las partes que la N64 no usa o si el texto se lee del cartucho.

**Fuentes: hecho.** Las 5 fuentes de baja resolución de `QDATAM` pasan a BMFont, un `.fnt` de texto con su atlas `.png` ([`fuentes.py`](../tools/conv/fuentes.py)), y el Makefile las convierte a `font64` con `mkfont`:
- **Codificación:** cada carácter va por su código Unicode, con la misma tabla que los textos.
- **`FONT100M`:** son las 26 cifras y signos con fondo degradado, en 15 colores; seguramente las del contador del panel. Ocupa 487 B.
- **Las otras 4:** son fuentes completas de un color (rojo, negro, azul y blanco), de 145 caracteres. Ocupan unos 1.540 B cada una.
- **Cobertura:** en inglés no falta ningún carácter. En español faltan Á, Í y Ó, que salen 4 veces en todos los textos.

La ROM de pruebas carga las 5.

**Sonido: hecho** ([`sonidos.py`](../tools/conv/sonidos.py)). Lo pasa a los formatos de libdragon el Makefile, con `audioconv64`.

- **Efectos y locutor.** Cada idioma tiene su `SOUND-n.DAT`, con el número de su `LANG-n.DAT`: `SOUND-0` el inglés y `SOUND-4` el español.
  - Cada sonido se guarda una sola vez, como WAV, aunque esté en los dos idiomas: 191 son comunes, 185 solo del inglés y 197 solo del español.
  - Van en VADPCM sin Huffman, como decidió la fase 4.
  - CorsixTH toca el sonido de un fotograma por su posición en el archivo del idioma cargado, y el orden cambia de un idioma a otro. Por eso cada idioma lleva una tabla de las 746 posiciones, de cada una a su fichero (`sonido/english.trg`).
  - De las 127 posiciones que usan las animaciones, dos suenan distinto según el idioma (la 80 y la 223), igual que en CorsixTH.
- **Música.** Las 8 piezas XMI pasan a MIDI con el `xmi2mid.cpp` de CorsixTH, compilado aquí con `g++`; no hace falta compilar CorsixTH entero.
- **Banco de instrumentos.** Se toca con TimGM6mb, como en la fase 4, recortado a lo que usan las piezas. Es un SF2 nuevo con solo esos presets, sus instrumentos y sus muestras: 28 de 136 presets, 43 instrumentos y 177 de 520 muestras.
  - Comprobación: fluidsynth sintetiza cada pieza con el banco entero y con el recortado, y en las 8 el audio sale idéntico byte a byte (120 MB de WAV a 22.050 Hz).
  - En la N64 suena casi igual que en fluidsynth, salvo por el filtro de los instrumentos. Al medirlo salió una diferencia mayor, que se corrige en el conversor: la N64 aplicaba la atenuación de los instrumentos entera, y fluidsynth la multiplica por 0,4. Ver [Música: la N64 frente a fluidsynth](#música-la-n64-frente-a-fluidsynth).

| Qué | En la ROM | Estimación de la fase 2 |
|---|---:|---:|
| Efectos comunes (wav64) | 1.656.394 B | 1,43 MiB |
| Propios del inglés | 5.628.660 B | 4,88 MiB |
| Propios del español | 4.099.038 B | 3,46 MiB |
| Música (8 piezas en mid64) | 78.482 B | 77 KiB |
| Banco recortado (sf64) | 713.908 B | 1,81 MiB entero |

Los efectos y la voz ocupan entre un 10 y un 13 % más que lo estimado en la fase 2: es el Huffman que se quita, como ya avisó la fase 4. El banco recortado deja libre más de lo que suma eso. La ROM normal ocupa ahora 13.697.024 B.

En la ROM de pruebas, en ares:
- carga el banco recortado, que ocupa **98 KiB de RAM** (la fase 4 estimaba unos 101);
- toca `FORTUNE` 3 s, y a los 3 s va por los 3.258 ms;
- busca a la vez un efecto por su posición en la tabla del idioma (la 10, `FALLOVER.WAV`) y lo hace sonar;
- **el audio se lleva el 4,4 % de la CPU**. Se mide con el método de la fase 4: trabajo fijo hecho con y sin audio, llamando a `mixer_try_play` entre medias.

**Lo que queda del hito 0:**
- **Las posiciones por fotograma de los marcadores.** Solo las usa la interfaz (la ventana del paciente, los iconos de humor) y se calcularán con ella.
- **El mapa de prueba** que preveía el plan ya no hace falta. Las pruebas del núcleo construyen mapas sintéticos, sin datos del juego, y las comparaciones usan los niveles de verdad y el escenario del hito 4.

**Una medida de paso.** La ROM de pruebas con el lector de reglas da 17,59 ms con 50 humanoides en pantalla, frente a 23,73 en la del hito 3, y 5,52 ms en el recorrido del hito 2, frente a 6,41. El dibujo no ha cambiado: es la disposición del código en la caché de instrucciones, que mueve estas cifras hasta un 26 %. El hito 5 tendrá que medir con varias compilaciones, o fijar la disposición del código.

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


## Hito 3 — Un humanoide que camina por el mapa

**Hecho.** En la ROM, un paciente camina por el nivel 1 de una casilla a otra, con el A* de CorsixTH y su animación de caminar. La cámara lo sigue a 30 fps sin saltar fotogramas en ares. Con B se añaden más, que pasean por casillas al azar; con 50 en pantalla sigue a 30 fps. Contra CorsixTH:
- **caminos:** 12.000 de 12.000 iguales, casilla a casilla;
- **caminatas:** 6.522 de 6.522 ticks iguales en 16 caminatas (casilla, desplazamiento, animación, fotograma y banderas);
- **pantalla:** el humanoide se ve igual, salvo diferencias de 5 o menos (de 255) en su interior. Vienen del filtrado lineal con que CorsixTH dibuja las animaciones.

![El visor en ares siguiendo a un paciente por el nivel 1](img/05_hito3.png)

*El visor en ares (320 × 240, ampliada al doble) siguiendo al paciente por el nivel 1. Las cifras de CPU de la pantalla son las del visor, con el texto encima.*

![A la izquierda, CorsixTH; a la derecha, th64sim escena: el paciente pasa por detrás de una pared con ventana](img/05_hito3_oraculo.png)

*Escena 4 a 640 × 480 (nivel 1, tick 60 de la caminata de (86,70) a (54,70)), recortada y ampliada al doble: CorsixTH a la izquierda y el motor a la derecha, con los mismos gráficos. El paciente va al oeste (la animación del norte en espejo) por detrás de la pared: la ventana y la pared de delante se vuelven a dibujar encima de él, como en CorsixTH.*

### Qué hay

- **Caminos: [`th/path.h`](../motor/core/include/th/path.h).** Es el A* de CorsixTH (`basic_pathfinder` en `th_pathfind.cpp`), con:
  - el mismo montículo binario y sus mismas comparaciones (`<=` al subir un nodo, `<` al sacarlo);
  - el mismo orden de vecinos (oeste, este, norte, sur);
  - la misma regla de tránsito: desde una casilla transitable solo se pasa a otra transitable.

  Cambia la memoria: nodos de 8 B (128 KiB para el mapa entero, como preveía [`04_decision.md`](04_decision.md#presupuesto-de-ram)) y un mapa de bits de nodos vistos en lugar de la lista de nodos sucios. Con el montículo, 162 KiB estáticos.
- **Animaciones: [`th/anim.h`](../motor/core/include/th/anim.h).** Lee las tablas TAN1 del conversor con las reglas de `animation_manager` (`th_gfx.cpp`): primer fotograma, encadenamiento, listas de elementos y el filtro por capas, incluido el caso especial de la capa 5.
- **Humanoides: [`th/humanoid.h`](../motor/core/include/th/humanoid.h).** La parte de `Entity` y `Humanoid` que hace falta para caminar:
  - las acciones de caminar (`walk.lua`) y de quedarse quieto (`idle.lua`), con su cola. `setNextAction` respeta las acciones que tienen que ocurrir, y una caminata interrumpida acaba antes el paso en curso;
  - el orden de `Entity:tick`: primero avanzan el fotograma y el desplazamiento, después el temporizador. Cada paso dura 8 ticks, a 4 × 2 píxeles por tick;
  - las animaciones de cada clase (`walk_anims` de `humanoid.lua`). Oeste y sur son las de norte y este en espejo;
  - si cambia el tránsito del camino por delante, se busca otro (`recalc_route`).

  El camino va dentro del humanoide, a 2 bits por paso: 256 B para 1.024 pasos (el más largo de los 12.000 caminos tiene 353). Si no cabe, al acabarlo se busca el resto. Cada humanoide ocupa 332 B.
- **Vista.** Las entidades entran en la lista de dibujo en el orden de `level_map::draw`:
  - en cada casilla van después de su pared oeste, por capa de dibujo y, dentro de una capa, la última en llegar primero;
  - de cada fotograma van los elementos de las capas que lleva el humanoide (ropa, pelo, piel), con su espejo y su transparencia;
  - después de cada entidad animada se vuelven a dibujar encima, como en CorsixTH, las entidades de los lados sur y este de las casillas vecinas y, si hace falta, la pared norte y las entidades de la casilla de delante. Sin esto, el humanoide se vería delante de las paredes por las que pasa por detrás.
- **N64: [`mapa_n64.c`](../motor/platform/n64/mapa_n64.c).** El RDP pinta los elementos en la misma pasada que las paredes:
  - el espejo, con un rectángulo de textura con las coordenadas al revés;
  - las transparencias (50 y 75 %), con el combinador y el mezclador.

  `MSPR-0` está entera en RAM, como en el hito 1.
- **Visor.** El mundo avanza un tick cada 54 ms de reloj, como CorsixTH a velocidad Normal, y como mucho 4 por fotograma. Cuando un humanoide llega, elige otra casilla al azar y vuelve a caminar. Hay dos botones nuevos:
  - A sigue al primer humanoide o deja de seguirlo;
  - B añade 10 humanoides.

### Comprobaciones

| Qué | Contra qué | Resultado |
|---|---|---|
| Caminos del A*: 1.000 pares de casillas transitables por nivel, en los 12 niveles | `World:getPath` de CorsixTH ([`caminos.lua`](../bench/scenarios/caminos.lua) y `th64sim caminos`) | **12.000 de 12.000 iguales** casilla a casilla, incluidos los 54 pares sin camino. El más largo tiene 353 pasos |
| Caminatas: 16 caminatas de 16 a 88 pasos en los niveles 1, 5 y 12, tick a tick (casilla, desplazamiento, animación, fotograma y banderas) | Un «Standard Male Patient» que camina en CorsixTH ([`caminata.lua`](../bench/scenarios/caminata.lua) y `th64sim caminata`) | **6.522 de 6.522 ticks iguales** |
| Lo que se ve a 640 × 480 con el humanoide, en 8 escenas de los niveles 1, 5 y 12: de frente, de espaldas, en espejo y junto a paredes. En la escena 4 se vuelven a dibujar paredes encima | Capturas de CorsixTH parado en ese tick ([`escenas.sh`](../tools/oraculo_mapa/escenas.sh) y `th64sim escena`) | De los 7.922 píxeles del humanoide: 6.128 idénticos, 1.767 a 2 o menos y 27 de 3 a 5. **Ninguno difiere en más de 5.** Fuera de él, ver abajo |
| Lo que dibuja el RDP en la N64 con el humanoide, en 5 vistas de 320 × 240 del nivel 1 | El rasterizador de referencia, en la misma ROM | **0 píxeles distintos de 384.000**, 1.328 de ellos del humanoide. En una vista se vuelve a dibujar una casilla |
| Pruebas del núcleo | — | 60.180 comprobaciones, 0 fallos en el PC, con ASan y UBSan, y en la N64. Hay 55 nuevas: el A*, las animaciones y la caminata, con un mapa y unas animaciones sintéticos, sin datos del juego |

Fuera del humanoide, en las 8 escenas:
- 687 píxeles a 2 o menos de su borde difieren en 7 o menos: el filtrado lineal de CorsixTH también tiñe el borde;
- otros 761 difieren en 7 o menos y están a 3 píxeles o menos de entidades que solo dibuja CorsixTH;
- los 23.660 que difieren en más de 7 son todos de entidades que el motor aún no tiene (puertas y un médico). Ninguno está a menos de 3 píxeles del humanoide.

Esta última cifra cambia un poco entre ejecuciones (23.485 en la anterior), porque la captura de CorsixTH de la escena 6 no sale igual cada vez. Lo que dibuja el motor sí sale igual, byte a byte.

`tools/motor_check.sh` repite en cada ejecución la cuarta y la quinta fila, y comprueba que el recorrido siguiendo al humanoide no salta fotogramas. Las tres primeras se repiten con [`tools/oraculo_mapa/`](../tools/oraculo_mapa/README.md), que necesita CorsixTH.

### Rendimiento

Dos recorridos automáticos en ares (ROM de pruebas), en el nivel 1, a 30 fps:
- **siguiendo a un humanoide:** 450 fotogramas, con la cámara pegada a él mientras camina entre casillas al azar;
- **con 50 humanoides en pantalla:** 300 fotogramas con la cámara fija. Los 50 pasean entre casillas al azar a 5 casillas o menos de la cámara.

| Dato | Siguiendo a 1 | Con 50 en pantalla |
|---|---:|---:|
| CPU del dibujo por fotograma | **7,79 ms** (máx. 9,11) | **23,73 ms** (máx. 26,27) |
| — generar la lista | 2,33 ms | 5,82 ms |
| — suelos y sombras | 2,60 ms | 2,64 ms |
| — paredes y entidades | 2,86 ms | 15,25 ms |
| Fotograma entero, con el mundo | 7,87 ms (máx. 10,70) | 24,02 ms (máx. 26,53) |
| Tiempo entre fotogramas | 33,30 ms (máx. 33,45) | 33,37 ms (máx. 33,45) |
| Fotogramas que se saltan | **0** | **0** |
| Sprites por fotograma (de entidades) | 412 (5) | 661 (258) |
| Cargas de textura | 63 (máx. 87) | 334 (máx. 363) |
| Relleno del RDP | 202.618 (máx. 222.464) | 260.358 (máx. 267.719) |
| Mundo: mover a todos, por tick | 0,022 ms | 0,222 ms (4,4 µs por humanoide) |
| Caminos buscados | 2: 1,09 ms de media, 1,94 el más largo | 152: 0,19 ms de media, 0,61 el más largo |

- **La simulación es barata.** A velocidad Normal, un tick del mundo es una hora de juego. Con 50 humanoides, caminar cuesta 4,4 µs por humanoide y tick, unos 410 ciclos. Buscar caminos añade unos 300 por humanoide y hora (152 búsquedas de 0,19 ms en 183 ticks). Juntos son el 10 % de los 7.000 ciclos por humanoide y hora que [`04_decision.md`](04_decision.md#presupuesto-de-cpu) deja a la simulación. Con un solo humanoide el tick cuesta 22 µs, probablemente porque entre tick y tick el dibujo saca su código de la caché.
- **Un camino largo cuesta 1–2 ms.** En el hospital lleno habrá que ver cuántas búsquedas caen en el mismo tick y, si hace falta, repartirlas. Lo mide el hito 5.
- **Dibujar entidades es caro.** Con 50 humanoides, cada elemento de entidad cuesta unos 49 µs (4.600 ciclos) en los comandos del RDP y unos 14 µs (1.300 ciclos) en la lista. Son cifras aproximadas: se descuenta lo que cuestan las paredes y la lista en el otro recorrido. Lo más probable es que sea la carga de la textura, porque cada elemento es un sprite distinto (334 cargas por fotograma frente a 63), pero no se ha medido por separado. Es el riesgo de CPU que preveía la fase 4, y queda para el hito 5.
- **Variación entre compilaciones.** Una ejecución anterior del mismo recorrido con 50, antes de añadir las pruebas nuevas a la ROM, dio 21,38 ms (paredes y entidades, 13,07). Como en el hito 2, la disposición del código en la caché de instrucciones mueve estas cifras en torno a un 10 %.
- **El recorrido del hito 2**, sin entidades, sube de 5,24 a 6,41 ms con los mismos sprites (lista 1,78; suelos 2,56; paredes 2,06). Cada sprite comprueba ahora la hoja, el espejo y la transparencia, y el código ha cambiado de sitio. No se ha separado cuánto pesa cada cosa.
- **El RDP sigue sin poder medirse en ares.** El peor relleno, 267.719 píxeles, son unos 4,3 ms a un píxel por ciclo, más las 363 cargas de textura.

### Memoria y tamaño

| Dato | Valor |
|---|---|
| Buscador de caminos (estático) | 162 KiB: 128 KiB de nodos, 32 KiB de montículo y 2 KiB de mapa de bits |
| Humanoide | 332 B. El visor tiene sitio para 64 (21 KiB) |
| Lista de dibujo (4.096 elementos de 12 B, estática) | 48 KiB |
| Tablas de animación en RAM | 481.038 B (224.888 B en la ROM) |
| Heap usado con el nivel cargado | 2.432 KiB de 7.311. Lo principal: `MSPR-0` (1.184 KiB), los 3 framebuffers (450 KiB), las animaciones (470 KiB) y `MBLK-0` (188 KiB) |
| ROM | 966.656 B |
| Código (`text`) | 299.052 B |

### Pendiente

- **Objetos del mapa.** Siguen pendientes del hito 2: las puertas y los demás objetos del nivel, sus banderas y su dibujo. Son lo que aún queda distinto en las capturas. Hito 4.
- **Puertas al caminar.** `walk.lua` hace pasar a los humanoides por las puertas (`navigateDoor`). Llega con los objetos.
- **Cerrar las salas al comprar una parcela.** Llega con las salas (hito 4).
- **El camino dentro del humanoide.** Ocupa 256 de sus 332 B, y el presupuesto de la fase 4 cuenta 256 B por humanoide en total. Con 384 humanoides serían 125 KiB en lugar de 96. Como no todos caminan a la vez, un almacén de caminos compartido lo arreglaría. Se decide con las cifras del hito 5.
- **El paso del personal.** El personal camina más deprisa o más despacio según su destreza y su cansancio: `walk.lua` tiene un paso el doble de rápido (`speed = "fast"`) y hay animaciones lentas (`slow_animation`). Llega con el personal.
- **Entidades que se dibujan antes que las paredes** (la lista temprana de `level_map::draw`). La usan las puertas, algunos objetos (camas, máquinas) y los humanoides mientras los usan. Llega con los objetos.
- **El coste de dibujar entidades** (ver «Rendimiento»). Hito 5.

## Escenario del hito 4

**Hecho.** El hito 4 se compara con CorsixTH por estadística, y para eso hace falta un escenario propio y validado ([`04_construccion.md`](04_construccion.md#cómo-se-comprueba)). Está en [`tools/oraculo_hito4/`](../tools/oraculo_hito4/README.md).

### Por qué «nunca entraba nadie en la consulta»

En la fase 4, la partida mínima del hito 4 parecía atascada: en 400 horas de juego nadie entraba en la consulta. No era un atasco. Los pacientes aparecen en el borde del mapa y caminan a una casilla cada 8 horas (un paso de `walk.lua`), así que tardan **1.439 horas de media en llegar a la recepción** (entre 908 y 1.984): de 18 a 40 días de juego. Con 400 horas no da tiempo a nada. Una sonda del hospital ([`sonda_hospital.lua`](../bench/scenarios/sonda_hospital.lua)) lo mostró: tras 5 meses, el hospital funcionaba.

Lo de la partida `mediano` (un paciente 300 horas en la consulta con certeza 0) no se ha vuelto a mirar. En este escenario, ninguna visita a la consulta pasa de 572 horas.

### El escenario

Nivel 1 con un mostrador de recepción, 6 bancos, una consulta de GP de 4 × 4, una recepcionista y un médico ([`crear_hito4.lua`](../bench/scenarios/crear_hito4.lua)). Se construye con las mismas ventanas y validaciones que la interfaz, y sin emergencias, VIP, terremotos ni epidemias. Tiene tres ajustes:
- **Llegan 6 pacientes al mes.** Una consulta atiende unos 6 al mes, porque cada visita dura unas 190 horas. Con 40 al mes, la cola de la consulta crecía sin parar: 88 esperando a los 4 meses.
- **Las enfermedades del nivel están descubiertas.** En el nivel 1 no lo están, y sin eso el fax de «no hay más salas de diagnóstico» no deja arriesgar un diagnóstico: nadie llegaba a diagnosticarse.
- **El jugador responde cada fax en cuanto llega**, con la ventana del juego (`UIFax:choice`): arriesga un diagnóstico si puede y, si no, manda al paciente a casa. Un jugador que no responde deja al paciente 60 días esperando.

El ciclo de cada paciente queda así:
1. llega andando desde el borde del mapa;
2. hace cola en la recepción, donde acepta el precio de la consulta (`agreesToPay("diag_gp")`);
3. espera sentado en un banco y llama a la puerta;
4. pasa la consulta: el médico alterna el archivador y la mesa, sube la certeza del diagnóstico (0,59 de media) y cobra 100 $;
5. no hay más salas de diagnóstico: fax, y el jugador arriesga el diagnóstico (199 de 234) o el médico ya lo tenía (35);
6. no hay sala de tratamiento: fax, y el jugador lo manda a casa;
7. vuelve andando al borde del mapa.

### Validación

[`traza_hito4.lua`](../bench/scenarios/traza_hito4.lua) sigue a cada paciente hora a hora y anota cada movimiento de dinero, sin gastar números aleatorios. Con la misma partida, dos ejecuciones dan las trazas idénticas byte a byte.

**10 semillas de 6 meses de juego (9.000 horas) cada una**, 2 min 19 s en total:

| Por semilla | Media | Desviación típica |
|---|---:|---:|
| Pacientes que llegan | 28,6 | 2,4 |
| Pasan por la recepción | 24,6 | 1,8 |
| Pasan por la consulta | 24,0 | 1,8 |
| Diagnosticados | 23,4 | 1,8 |
| Se van a casa | 23,4 | 1,8 |
| Cobrado por la consulta (directo o por seguro) | 2.340 $ | 178 |
| Sueldos | 1.330 $ | 170 |
| Saldo al final, frente al inicial | +620 $ | 308 |

Los pacientes que llegan y no pasan por la recepción siguen andando al acabar.

| Fase (los 286 pacientes de las 10 semillas) | Media | Mediana | p10–p90 | Máximo |
|---|---:|---:|---:|---:|
| Llegar andando a la cola de la recepción | 1.439 h | 1.468 | 948–1.924 | 1.984 |
| Cola de la recepción | 55 h | 59 | 26–100 | 150 |
| Esperar la consulta | 253 h | 143 | 37–616 | 1.305 |
| Consulta | 189 h | 161 | 112–322 | 572 |
| De la consulta a irse | 23 h | 24 | 8–24 | 73 |
| Volver andando al borde del mapa | 1.448 h | 1.411 | 995–1.963 | 2.013 |

Criterios de validez, que se cumplen en las 10 semillas:
- hay diagnósticos en todas (de 21 a 26);
- ninguna consulta se atasca: la visita más larga dura 572 horas;
- la espera no crece: como mucho, 5 pacientes esperan la consulta a la vez;
- nadie muere ni se va por el precio.

### Qué hacen los pacientes

Horas-paciente de las 10 semillas, por acción:

| Acción | Parte del tiempo |
|---|---:|
| Andar (llegar, 46,2 %; irse, 37,3 %; dentro del hospital, 5,9 %) | 89,4 % |
| Usar un objeto: bancos (4,7 %), silla de la consulta (3,8 %) | 8,6 % |
| Aparecer y desaparecer en el borde del mapa | 1,1 % |
| Esperar de pie en la cola de la recepción | 0,7 % |
| Vomitar (la diarrea) | 0,1 % |
| Llamar a la puerta de la consulta | 0,1 % |
| Buscar sala, orinar en el suelo | < 0,01 % |

Esto acota el hito 4.

Lo que el motor tiene que hacer:
- llegadas por mes como `updateSpawnDates`;
- aparecer en un punto del borde y andar;
- buscar la recepción y hacer cola;
- la recepcionista y su temporizador;
- buscar la consulta, la cola de la puerta, los bancos y llamar a la puerta;
- el ciclo del médico y la certeza del diagnóstico;
- el cobro, con el 25 % por seguro, que la aseguradora paga dos meses después;
- las dos decisiones del fax;
- volver al borde y desaparecer;
- los sueldos de fin de mes y las subidas de sueldo: hubo 24 en las 10 semillas, unas 2 por semilla.

Lo que queda fuera, porque pesa menos del 0,2 %: vomitar, orinar y la basura que dejan. La comparación del hito 4 enseñó que, aun así, vomitar y orinar retrasan al 7,5 % de los pacientes en su salida (ver [«De la consulta a irse»](#comparación-con-corsixth)).

## Hito 4 — Bucle mínimo

**Hecho.** En la ROM, el hospital del escenario funciona solo, con el contador de dinero y la fecha abajo. Los pacientes:
1. llegan andando desde el borde del mapa;
2. hacen cola en la recepción y esperan sentados en los bancos;
3. pasan la consulta, donde los diagnostican y les cobran;
4. se van.

La simulación es la de CorsixTH v0.70.1, escrita en C con enteros. Contra CorsixTH:
- **comparación estadística:** 15 cifras por semilla en 40 semillas de 9.000 horas (400 corridas del motor). En 14, la diferencia está dentro del azar: \|t\| < 2. La otra, el tiempo de la consulta a irse, es 2,4 horas más corto (−10 %). Sale de los pacientes que vomitan u orinan por el camino, algo que el motor aún no tiene;
- **el ciclo del médico:** en qué reparte el tiempo y cuánto dura cada uso de la mesa, igual dentro del azar;
- **el escenario:** 16.384 de 16.384 casillas iguales en las 40 semillas;
- **lo que se ve:** los objetos salen como en la captura de CorsixTH, salvo diferencias de 5 o menos en los bordes.

La N64 simula exactamente lo mismo que el PC: tras 3.000 horas, la misma huella del estado.

![El hospital del escenario en ares, 4.900 horas después de empezar](img/05_hito4.png)

*La ROM en ares (320 × 240, ampliada), en la hora 4.900: pacientes en el mostrador de recepción y, en la consulta, el médico en su mesa y un paciente. Abajo, el saldo (197.088 $) y la fecha (9 de abril del año 1) con FONT100M, una fuente de contador de `QDATAM` que solo tiene cifras y guion. Es el primer fotograma tras simular las 4.900 horas: por eso marca 0,1 fps.*

### Qué hay

- **Simulación: [`th/sim.h`](../motor/core/include/th/sim.h)** (`sim.c` y `sim_rooms.c`, 2.500 líneas). Es la parte de `World`, `Hospital`, `Room`, `Patient` y `Staff` que usa el escenario, en el mismo orden que `World:onTick`: fin del día y del mes, llegadas, mostrador, humanoides por orden de creación, temperatura y faxes.
  - **Unidades y azar.** Las fracciones van en millonésimas y el azar es un xorshift propio. No es el generador de Lua, así que la comparación con CorsixTH es estadística, con muchas semillas.
  - **Llegadas.** `updateSpawnDates`, con su Poisson y la población. El primer paciente llega por el punto más cercano a un mostrador atendido.
  - **Recepción.** La recepcionista va a su mostrador, y está el temporizador del mostrador. El paciente hace cola y acepta el precio (`agreesToPay`).
  - **Consulta de GP.**
    - El paciente espera en la cola de la puerta, en un banco o de pie, y llama a la puerta.
    - El médico alterna el archivador y la mesa (`doStaffUseCycle`).
    - La certeza del diagnóstico (`completeDiagnosticStep`).
    - `dealtWithPatient` y el cobro: el 25 % por seguro, que la aseguradora paga dos meses después.
  - **Llamadas de personal** (`CallsDispatcher`). El médico deambula por el pasillo hasta que la consulta lo llama, cuando un paciente va hacia ella. Ver «El médico llegaba a la consulta antes».
  - **Faxes y subidas de sueldo.** Los abre y contesta el jugador del arnés de CorsixTH:
    - arriesga el diagnóstico si puede y, si no, manda al paciente a casa;
    - sube el sueldo cuando se lo piden.

    Se modela la puerta de los mensajes del panel inferior (`UIBottomPanel`) y la pausa del arnés, porque deciden cuántas horas pasan hasta la respuesta.
  - **Personal.** Cansancio y felicidad (calor, sueldo frente al justo, sala) y peticiones de subida. Los sueldos se pagan a fin de mes. El paso rápido o lento depende de la destreza y del cansancio.
  - **Reputación.** La baja mandar a un paciente a casa y la mueven los precios.
  - **Temperatura del mapa** (`level_map::update_temperatures`). Da los mismos valores, casilla a casilla, y solo recalcula las casillas que pueden cambiar. Ver «Rendimiento».
- **Acciones de los humanoides: [`th/humanoid.h`](../motor/core/include/th/humanoid.h).** La cola de acciones general de `humanoid.lua`: `must_happen`, interrupciones, `setNextAction`, `queueAction` y temporizadores. Hay 12 acciones:
  - caminar, con puertas y llamada a la puerta;
  - aparecer y desaparecer;
  - quedarse quieto y deambular;
  - buscar la recepción y buscar sala;
  - hacer cola, con bancos;
  - usar un objeto, con uso prolongado y el bucle de la mesa del médico;
  - atender el mostrador;
  - `call_checkpoint` y `answer_call`.
- **Caminos: [`th/path.h`](../motor/core/include/th/path.h).** Además del A*:
  - la casilla libre para esperar (`find_idle_tile`);
  - el recorrido de objetos cercanos (`visit_objects`);
  - las distancias a todo el mapa en una búsqueda en anchura. Dan las mismas longitudes que el A*: el primer paciente busca así su punto de llegada con una búsqueda por mostrador, en lugar de un A* por punto.
- **Vista.** Los objetos del escenario, con su animación, su espejo y su capa:
  - la lista temprana de `level_map::draw`: las puertas de la pared norte y los objetos con `early_list` se dibujan antes que la pared;
  - el recorte por columnas de `thdf_crop` de los objetos de varias casillas (el mostrador, los bancos y la mesa).

  Los objetos no se ven mientras alguien los usa, como en CorsixTH: se ve al humanoide que los usa.
- **N64.** Con el escenario del hito 4 en el cartucho, la ROM abre el hospital en marcha:
  - un tick (una hora de juego) cada 54 ms de reloj, como CorsixTH a velocidad Normal;
  - Z multiplica la velocidad por 8, y A sigue al último paciente que ha llegado;
  - `make HORAS=N` hace una ROM con el hospital N horas adelantado, para las capturas.
- **PC.**
  - `th64sim hito4` corre el escenario y escribe las mismas trazas que `traza_hito4.lua`;
  - `th64sim escena4` dibuja una vista del hospital;
  - [`tools/oraculo_hito4/motor.sh`](../tools/oraculo_hito4/README.md) y `comparar.py` comparan el motor con una campaña de CorsixTH;
  - dos sondas de CorsixTH sirven para buscar diferencias: [`sonda_gp.lua`](../bench/scenarios/sonda_gp.lua), el ciclo del médico, y [`sonda_paciente.lua`](../bench/scenarios/sonda_paciente.lua), la cola de acciones de un paciente.

### Comprobaciones

| Qué | Contra qué | Resultado |
|---|---|---|
| Casillas del escenario de las 40 semillas, cargado en el motor | Lo que exporta CorsixTH de cada partida ([`exportar_escenario.lua`](../bench/scenarios/exportar_escenario.lua) y `th64sim escenario`) | **16.384 de 16.384 iguales** en las 40 |
| El bucle: 15 cifras por semilla en 40 semillas de 9.000 horas, con 10 corridas del motor por semilla | La campaña de CorsixTH ([`traza_hito4.lua`](../bench/scenarios/traza_hito4.lua), `motor.sh` y `comparar.py`) | **14 de 15 con \|t\| < 2.** La otra, ver «Comparación con CorsixTH» |
| El ciclo del médico: qué hace cada hora y cuándo se sienta y se despacha cada paciente, en 10 semillas | [`sonda_gp.lua`](../bench/scenarios/sonda_gp.lua), con la misma partida que la campaña: el mismo saldo final y la misma huella del generador | Igual dentro del azar (tabla abajo) |
| Los objetos a 640 × 480, sin personas, en la escena del escenario 1 | Captura de CorsixTH parado ([`captura_escenario.lua`](../bench/scenarios/captura_escenario.lua) y `th64sim escena4`) | De los 17.513 píxeles de objetos: 12.689 idénticos, 4.694 a 2 o menos y 130 de 3 a 5. **Ninguno difiere en más de 5.** De los 307.200, solo 220 difieren en más de 2, todos en el borde de un objeto o a 1 píxel de él: el filtrado lineal de CorsixTH |
| Lo que dibuja el RDP con el hospital tras las 3.000 horas, con objetos y personas, a 320 × 240 | El rasterizador de referencia, en la misma ROM | **0 píxeles distintos de 76.800**, 4.577 de ellos de entidades |
| La simulación en la N64: 3.000 horas de la semilla 1 | `th64sim hito4` en el PC | **La misma huella** (44923eae) |
| Distancias de la búsqueda en anchura: 40 orígenes y 400 destinos por escenario, en los 40 escenarios | El A* | **640.000 de 640.000 iguales** |
| La pasada de temperatura repartida entre fotogramas: 9.000 horas, avanzando un trozo al azar tras cada tick | La pasada de una vez | La misma huella; 4.979 pasadas las acabó el tick siguiente |
| Hito 3: caminos y caminatas | CorsixTH | Siguen **12.000 de 12.000** caminos y **6.522 de 6.522** ticks iguales |
| Pruebas del núcleo | — | 60.233 comprobaciones, 0 fallos en el PC, con ASan y UBSan, y en la N64. Con ASan y UBSan corren además 3 escenarios de 9.000 horas |

`tools/motor_check.sh` repite en cada ejecución la quinta y la sexta fila, y comprueba que el recorrido del hospital en marcha no salta fotogramas. Las demás se repiten con [`tools/oraculo_hito4/`](../tools/oraculo_hito4/README.md).

### Comparación con CorsixTH

Media y desviación típica por semilla. CorsixTH, 40 semillas; el motor, 400 corridas (10 por semilla, con semillas propias). La t es la de Welch.

| Cifra | CorsixTH | Motor | Diferencia | t |
|---|---:|---:|---:|---:|
| Pacientes que llegan | 28,9 ± 2,3 | 28,4 ± 2,4 | −1,9 % | −1,46 |
| Pasan por la recepción | 24,7 ± 2,2 | 24,2 ± 2,3 | −2,1 % | −1,45 |
| Pasan por la consulta | 23,7 ± 2,9 | 23,1 ± 2,8 | −2,7 % | −1,37 |
| Diagnosticados | 23,1 ± 2,9 | 22,5 ± 2,9 | −2,3 % | −1,12 |
| Se van a casa | 23,1 ± 2,9 | 22,5 ± 2,8 | −2,6 % | −1,23 |
| Llegar andando a la recepción (h) | 1.397 ± 84 | 1.389 ± 82 | −0,6 % | −0,62 |
| Cola de la recepción (h) | 58,5 ± 25,5 | 57,5 ± 24,0 | −1,6 % | −0,23 |
| Esperar la consulta (h) | 322 ± 357 | 302 ± 293 | −6,2 % | −0,35 |
| Consulta (h) | 203,8 ± 79,8 | 199,7 ± 65,4 | −2,0 % | −0,31 |
| **De la consulta a irse (h)** | **23,5 ± 3,3** | **21,1 ± 2,4** | **−10,2 %** | **−4,48** |
| Volver andando al borde (h) | 1.464 ± 88 | 1.442 ± 94 | −1,5 % | −1,51 |
| Cobros de la consulta | 23,1 ± 2,9 | 22,7 ± 2,9 | −2,0 % | −0,97 |
| Cobrado por la consulta ($) | 2.313 ± 290 | 2.266 ± 287 | −2,0 % | −0,97 |
| Sueldos ($) | 1.209 ± 259 | 1.206 ± 250 | −0,2 % | −0,07 |
| Saldo al final, frente al inicial ($) | +734 ± 361 | +689 ± 318 | −6,2 % | −0,76 |

Los 40 escenarios salen de la misma campaña ([`campana.sh`](../tools/oraculo_hito4/campana.sh) con las semillas 1–40). Correr el motor 400 veces tarda 3 min 26 s en el PC.

**De la consulta a irse.** Por paciente, las dos distribuciones tienen los mismos picos y casi las mismas proporciones: 8 horas (12,0 % en CorsixTH y 13,0 % en el motor), 16 (2,3 y 3,0 %), 22–23 (6,6 y 9,9 %) y 24 (66,8 y 70,1 %). Lo que falta en el motor es una cola: el 7,5 % de los pacientes de CorsixTH tarda más de 28 horas (52,8 de media). Sin esos pacientes, la media de CorsixTH es 21,04 horas, y la del motor, 21,03.

La sonda de un paciente ([`sonda_paciente.lua`](../bench/scenarios/sonda_paciente.lua)) enseña la causa, en la semilla 1, paciente 4:
- sale de la consulta en la hora 3.042;
- en la 3.048 empieza a vomitar (`vomit`, que tiene que acabar);
- el jugador arriesga el diagnóstico en la 3.050, pero la acción nueva espera detrás del vómito, que dura hasta la 3.076;
- después el paciente acaba de caminar, busca sala de tratamiento y, con el segundo fax, se va en la 3.093: 51 horas en lugar de 24.

En las 40 semillas, vomitar y orinar suman 1.455 horas-paciente entre la consulta y la salida, y la cola, 2.192 horas de más. El resto es lo que esas acciones interrumpen y repiten (`interruptAndRequeueAction`) y el fax que llega más tarde. El escenario las dejó fuera porque pesan menos del 0,2 % del tiempo de los pacientes, pero retrasan al 7,5 % de ellos en esta fase. Entran con la basura y el bedel.

**El médico llegaba a la consulta antes.** La primera comparación tenía 5 cifras con \|t\| ≥ 2:
- pasan por la consulta, −5,7 % (t = −2,85);
- diagnosticados, t = −3,08;
- se van a casa, t = −3,13;
- cobros de la consulta, t = −3,0;
- de la consulta a irse, t = −3,64.

Las visitas a la consulta duraban 16 horas más en el motor (166 frente a 150 horas en la silla). La sonda del médico enseñó por qué; las cifras son de la semilla 1:
- en CorsixTH el médico pasea por el pasillo unas 1.140 horas, hasta que el primer paciente va hacia la consulta y esta lo llama (`CallsDispatcher`);
- el motor lo mandaba a la consulta en la hora 0, así que se cansaba antes: cansancio medio 0,40 frente a 0,31;
- cansado, va despacio (`slow_animation`) el 13 % del tiempo, frente al 1 %, y las consultas se alargan.

Con las llamadas de personal, las cuatro primeras vuelven al azar. La quinta es la de los vómitos.

| El médico (semillas 1–10) | CorsixTH | Motor |
|---|---:|---:|
| Usa la mesa | 55,1 % | 52,2 % |
| Camina | 33,4 % | 34,7 % |
| Usa el archivador | 6,1 % | 6,0 % |
| Espera de pie | 5,3 % | 7,1 % |
| Va despacio | 0,7 % | 0,0 % |
| Cansancio medio | 0,308 | 0,288 |
| Cada uso de la mesa, mediana | 90 h | 90 h |
| Cada uso de la mesa, media | 99,5 h (495 usos) | 95,9 h (4.867 usos) |
| Del momento en que el paciente se sienta a que lo despacha, mediana | 97 h (234 visitas) | 91 h (2.337 visitas) |

La sonda abre y contesta los faxes igual que la traza: con la misma partida, CorsixTH da el mismo saldo final y la misma huella del generador que en la campaña. En la primera versión no los abría, y en CorsixTH el médico se iba del hospital hacia la hora 4.200: nadie contestaba su petición de subida, y `checkIfWaitedTooLongForRaise` lo despide.

### Rendimiento

Dos medidas en ares con la ROM de pruebas y la semilla 1:
- **3.000 horas del escenario tan deprisa como se pueda**, con la pasada de temperatura entera en cada tick;
- **el hospital en marcha:** 450 fotogramas a 30 fps, con la cámara fija en el hospital y un tick cada 54 ms, como a velocidad Normal. Empieza en la hora 3.000, justo después de un cambio de mes: lo peor para la temperatura (ver abajo).

| 3.000 horas | Media por tick | El tick más largo | Temperatura, por casilla |
|---|---:|---:|---:|
| Todo el mapa en cada tick, como CorsixTH | 29,5 ms | 434,8 ms | — |
| Solo las casillas que pueden cambiar | 22,5 ms | 443,3 ms | 4,13 µs |
| Los datos de cada casilla juntos; el primer paciente, con una búsqueda en anchura | 11,5 ms | 61,2 ms | 2,06 µs |
| Camino rápido si los cuatro vecinos pesan 4 | 8,8 ms | 52,9 ms | 1,53 µs |
| Un puntero por casilla | **5,7 ms** | **44,5 ms** | **0,94 µs** |

| El hospital en marcha (ROM final) | Pasada de temperatura en el tick | Repartida entre fotogramas |
|---|---:|---:|
| Tick, de media | 15,9 ms | 1,9 ms |
| Tick, el más largo | 44,7 ms | 29,4 ms |
| Temperatura entre fotogramas, de media (como mucho) | — | 8,9 ms (13,5) |
| CPU del dibujo por fotograma | 12,6 ms | 12,6 ms |
| Fotograma entero, de media (el más largo) | 22,8 ms (57,7) | 23,0 ms (42,3) |
| Fotogramas que se saltan | 1 | **0** |
| Sprites por fotograma (de entidades) | 471 (45) | 471 (45) |

- **La temperatura era casi todo el tick.** CorsixTH recalcula las 16.384 casillas en cada tick (el 12 % de su CPU en la partida llena, fase 3). Cada cambio de la tabla da la misma huella:
  1. **Solo lo que puede cambiar.** Se recalculan las casillas que cambiaron en la pasada anterior y sus vecinas, más todas las de un tipo si cambia su mezcla (el aire o el radiador). Son 5.319 casillas por tick de media. Sin embargo, durante unas 440 horas tras cada cambio de mes son casi todas: el aire de fuera cambia, y las casillas de fuera se acercan a él un 1 % por hora.
  2. **La caché de datos.** Las dos memorias y los pesos de cada casilla estaban en tres tablas separadas por 32 KiB. Las tres lecturas de una casilla caían en la misma línea de la caché de datos de la VR4300 (8 KiB, de correspondencia directa) y se echaban unas a otras: 390 ciclos por casilla. Juntas en 6 B, la mitad.
  3. **El camino rápido.** Casi todas las casillas tienen los cuatro vecinos con peso 4, y su media es la suma de los cuatro entre 4. Así se ahorran la multiplicación de 64 bits y la lectura de los pesos.
  4. **Un puntero.** El compilador recalculaba la dirección de cada vecina (unas 30 instrucciones por casilla). Con un puntero a la casilla, las vecinas quedan a desplazamientos fijos (±6 y ±768 B). Son 0,94 µs, unos 89 ciclos.
  5. **Repartida entre fotogramas.** Nadie lee la temperatura hasta el tick siguiente, así que la ROM deja la pasada a medias (`temp_defer`) y la sigue tras dibujar cada fotograma, hasta 26 ms después de empezarlo. Si llega el tick siguiente con la pasada sin acabar, la acaba él antes de nada. Da la misma huella.

  Tras un cambio de mes, una pasada entera cuesta unos 15 ms.
- **El primer paciente** buscaba un camino con el A* desde el mostrador a cada uno de los 32 puntos de llegada: 2,1 ms en el PC y unos 380 ms en la N64. Con una búsqueda en anchura, 0,3 ms en el PC.
- **El tick más largo de las 3.000 horas** (44,5 ms, hora 1.616) junta dos cosas:
  - una pasada de temperatura casi entera (16.147 casillas, unos 15 ms);
  - el A* de un paciente que se va a casa: 1,04 millones de instrucciones en el PC, unos 28 ms en la N64.

  Los caminos largos ya estaban pendientes en el hito 3.
- **Frente al presupuesto.** A velocidad Normal hay 5,07 M ciclos por hora de juego, 54 ms ([`04_decision.md`](04_decision.md#presupuesto-de-cpu)):
  - el tick medio (5,7 ms) es el 11 %;
  - las horas tras un cambio de mes, unos 16 ms: el 30 %;
  - sin la temperatura, el tick cuesta 0,71 ms con 10 personas, contando lo fijo: el mapa de bits de lo que hay que recalcular, el mostrador y los faxes.

  A velocidad ×8 (la Z de la ROM), en las horas tras un cambio de mes, la temperatura no cabe: son 8 ticks por fotograma.
- **La caché de instrucciones.** Al crecer `sim.c`, los suelos pasaron de 2,7 a 5,2 ms por fotograma en todos los recorridos, también en el del hito 2, que no simula nada. La causa fue que el código de libdragon va detrás de todo el del motor, y se movió:
  - `dibujar` y `__rdpq_texture_rectangle`, a la que llama en cada sprite, cayeron en las mismas líneas de la caché de instrucciones (16 KiB, de correspondencia directa) y se echaban una a otra en cada llamada;
  - con funciones de 1 KiB y 0,4 KiB, eso pasa en 1 de cada 10 disposiciones del código, más o menos.

  Ahora [`tools/cache_instrucciones.py`](../tools/cache_instrucciones.py) lo detecta en cada ROM y dice cuánto relleno lo evita. El relleno ([`relleno_cache.c`](../motor/platform/n64/relleno_cache.c)) va entre el motor y libdragon: 192 B en la ROM normal y 2.400 B en la de pruebas. `tools/motor_check.sh` falla si hay choques. Con el relleno, los suelos vuelven a 2,6–2,8 ms.

### Memoria y tamaño

| Dato | Valor |
|---|---|
| Simulación (`th_sim`, estática) | 153 KiB: 96 KiB de temperatura (6 B por casilla), 46 KiB de 64 personas y lo demás (objetos, salas, colas, faxes, mapas de bits) |
| Humanoide | 684 B (332 en el hito 3). La cola de 12 acciones de 30 B ocupa 360 |
| Heap con el hospital cargado | 2.813 KiB de 7.025. Las reglas pesan 369 KiB: `definiciones.trg`, entero, son 331 KiB |
| ROM | 13.778.944 B, con el sonido del hito 0 |
| Código (`text`) | 384.580 B (299.052 en el hito 3) |

### Pendiente

- **Las necesidades de los pacientes** (`Patient:tickDay`): vomitar, orinar, bostezar, la basura, la sed y la salud. Es lo único que separa al motor de CorsixTH en la comparación (ver «De la consulta a irse»). Con la basura llegan el bedel y sus llamadas.
- **Los caminos largos.** Un A* de un paciente que se va puede costar unos 28 ms en la N64. Hay que repartirlos o acotarlos. Se mide en el hospital lleno, en el hito 5.
- **La temperatura a velocidades altas.** A ×8, en las horas tras un cambio de mes, se saltan fotogramas. La pasada es un cálculo regular sobre el mapa entero: se podría llevar al RSP.
- **La memoria de cada humanoide.** Con la cola de acciones, 684 B. Con 384 humanoides serían 257 KiB, frente a los 96 KiB del presupuesto de la fase 4. Hay que compactar la acción, que tiene campos que solo usa un tipo.
- **Las reglas en RAM.** `definiciones.trg` se carga entero (331 KiB); basta con las salas, los objetos y las enfermedades del nivel.
- **El despachador de llamadas.** Solo llama a personal para salas que piden a uno. Faltan:
  - reparar, regar y vacunar;
  - las salas con más personal;
  - el `isRoomInDemand` completo.
- **El panel inferior.** El contador usa FONT100M de `QDATAM`; el panel de CorsixTH usa `Font05V` de `QDATA` y un gráfico con el símbolo del dólar. Llega con la interfaz.

## Hito 5 — Medición en la N64

**Hecho.** El hospital del hito 4 con el triple de llegadas: 50 pacientes en el mapa en la hora 3.501 y 69 en la 9.000. Medido en ares con la ROM de medida ([`tools/hito5`](../tools/hito5/README.md)):
- **30 fps sin saltos a velocidad Normal**, con 52 personas y la música sonando. El fotograma cuesta 19,3 ms de CPU de media (el 58 % de 33,3) y 28,2 en el percentil 99.
- **Una hora de juego cuesta 7,5 ms de media**: el 14 % de los 54 ms que tiene a velocidad Normal. El 73 % es la temperatura del mapa.
- **Por persona, 5,7 µs por hora** (537 ciclos). Es un 8 % de los 7.000 ciclos por humanoide que dejaba a la simulación la fase 4.
- **Frente a CorsixTH en el mismo escenario y las mismas horas:** 158 veces menos tiempo en el PC. En la N64, 3,0 ms por hora frente a los unos 511 que estimaría la fase 1 para CorsixTH: unas 170 veces menos.
- **Con 8 MiB sobran 3,83 MiB de heap** con todo cargado: todos los sprites y animaciones, las reglas, el hospital y la música.
- **Lo que no llega:**
  - la velocidad más alta («And then some more», 55,6 horas por segundo): salta 182 de 300 fotogramas, por la temperatura;
  - los caminos largos: hasta 3.747 nodos en un tick, unos 40 ms.
- **El modo demo sin Expansion Pak no cierra** con el código que se proyecta para el juego completo (ver «Tamaño del código y modo demo»).

Lo que vino después (la simulación repartida entre fotogramas y las cargas de textura nuevas) está en [«Después del hito 5»](#después-del-hito-5-los-riesgos-de-cpu): ya no salta ningún fotograma a ninguna velocidad.

La N64 simula exactamente lo mismo que el PC: tras 9.000 horas, la misma huella (853ec05c).

![El hospital del hito 5 en ares, con 53 personas](img/05_hito5.png)

*La ROM normal con el escenario del hito 5 (`make HORAS=3600 ESCENARIO=hito5`), unos minutos de juego después: 53 personas, 594 sprites y 19,6 ms de CPU por fotograma a 30 fps. La cola de la recepción y los bancos llenos esperan a la consulta.*

### El escenario

Es el hospital del hito 4, creado en CorsixTH con 18 llegadas al mes en lugar de 6 (`--th64-ritmo=72`). Una consulta atiende unos 5 al mes, así que la gente se acumula en los bancos y en la cola. Las personas en el mapa (semilla 1):

| Hora | 750 | 1.500 | 2.250 | 3.000 | 3.750 | 4.500 | 6.000 | 7.500 | 9.000 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Personas | 12 | 23 | 35 | 46 | 51 | 55 | 61 | 67 | 69 |

Antes de medir la carga hay que comprobar que el motor se comporta como CorsixTH con ella. **Se compararon 10 semillas de 9.000 horas**, con 10 corridas del motor por semilla. Quedan dentro del azar 13 de las 15 cifras del hito 4:
- **de la consulta a irse:** −16,8 %. Es lo mismo que en el hito 4 (vomitar y orinar, que el motor aún no tiene), con más peso porque hay más pacientes;
- **las llegadas:** −5,2 %, con t = −2,15, en el límite. Con 15 cifras es esperable que alguna pase de 2 por azar.

La semilla 4 no sirve: en su guardado queda una sala sin puerta, de un intento fallido de construir la consulta. Se usó la 11.

**Lo que se arregló por el camino: el médico aprende con el uso.** La primera comparación, con una sola semilla, daba la mitad de visitas a la consulta que CorsixTH (13,7 frente a 28) y visitas el doble de largas (537 frente a 244 horas).
- En la sonda del médico, los dos repartían igual el tiempo: mesa 54–55 %, andar 33–34 %. Lo distinto era la duración de cada visita.
- La causa está en `Doctor:tick`: un médico gana 0,000003 de habilidad por hora mientras usa un objeto en una sala.
- El de este escenario es junior, con habilidad 0,2479. Hacia la hora 2.179 pasa de `DoctorThreshold` (0,25), deja de ser junior y de ir a medio ritmo, y además despacha antes.
- El motor no tenía ese aprendizaje. En el hito 4 no se notaba, porque el médico de la semilla 1 no era junior.

Con el aprendizaje, la consulta pasa de −51 % a +2,8 %. Las 40 semillas del hito 4 siguen dando 14 de 15 cifras dentro del azar, y la huella de la semilla 1 a 3.000 horas no cambia (44923eae).

**Hasta 128 personas.** La simulación tenía sitio para 64, y este escenario llega a 70. El límite no era la memoria sino dos máscaras de 64 bits: quién está en cada sala y quién va hacia cada cola. Ahora son conjuntos de 128 bits, el tope de la demo.

### Cómo se mide

- **La ROM de medida** (`make -C motor MEDIR=1`):
  1. simula tan deprisa como puede hasta la hora 3.501, con 50 pacientes, y escribe una línea por tick con su carga: personas, casillas de temperatura recalculadas, búsquedas de camino y nodos que recorren;
  2. con la música sonando, dibuja 900 fotogramas a velocidad Normal (18,5 horas por segundo), 300 a «Max speed» (27,8) y 300 a «And then some more» (55,6). La cámara se pone donde se ven más personas: 22, con 146 sprites de entidades por fotograma. La temperatura se reparte entre fotogramas, como en la ROM normal;
  3. sigue hasta la hora 9.000 y escribe la huella.

  Con 8 y con 4 MiB.
- **El PC:** `th64sim carga` escribe lo mismo tick a tick. Las 7.611 horas simuladas a toda prisa tienen la misma carga en los dos lados.
- **CorsixTH:** [`medir_hito5.lua`](../bench/scenarios/medir_hito5.lua), que es el `medir.lua` de la fase 1 con el mismo jugador que contesta los faxes. Con Tracy, 3 repeticiones de las horas 3.500 a 4.500, con Lua 5.4.

### La simulación

Las 7.611 horas a toda prisa (todas menos las de los fotogramas):

| Dato | Valor |
|---|---:|
| Tick de media | 7,46 ms |
| Mediana / p90 / p99 | 2,76 / 16,4 / 29,9 ms |
| El tick más largo | 64,6 ms (hora 7.733: una pasada de temperatura entera y un camino de 3.626 nodos) |
| Ticks de más de 33 ms | 56 |

El tiempo de cada tick se explica con una regresión lineal (R² = 0,994):

| Parte | Coste | En ciclos |
|---|---:|---:|
| Fijo por tick (mapa de bits de la temperatura, mostrador, faxes, colas) | 1,19 ms | 112.000 |
| Por persona | 5,7 µs | 537 |
| Por casilla de temperatura recalculada | 0,92 µs | 86 |
| Por nodo de búsqueda de camino | 10,8 µs | 1.014 |
| Por búsqueda (lo que cuesta empezarla) | 70 µs | 6.600 |

- **La temperatura es el 73 % del tiempo.**
  - Se recalculan 5.930 casillas por tick de media.
  - En 1.913 ticks (el 25 %) es la pasada entera, con 16,8 ms de media por tick. Son las unas 440 horas que siguen a cada cambio de mes, cuando cambia el aire de fuera (hito 4).
- **Los caminos son el 7 %.** Hay 4.389 búsquedas en 7.611 horas, con 79 nodos de media.
  - 143 ticks recorren más de 1.000 nodos; esos ticks duran 32,9 ms de media.
  - El que más, 3.747 nodos: unos 40 ms solo en el camino.
- **Las personas, sin más,** cuestan 0,94 ms más 3,5 µs por persona en los 1.144 ticks sin temperatura ni caminos.
- **La N64 frente al PC:** el mismo código tarda entre 310 y 345 veces más en ares que en el PC de medida, según la ejecución. La fase 1 suponía 227 veces para C.

**Frente a CorsixTH y a la fase 1**, en el mismo escenario y las mismas horas (3.500 a 4.500, 49,6 pacientes de media):

| | CorsixTH | Motor | Cociente |
|---|---:|---:|---:|
| PC de medida, por hora de juego | 1,28 ms | 8,1 µs | 158 |
| N64, por hora de juego | ~511 ms, estimado como en la fase 1 (× 400; de 377 a 831 con su rango 295–650) | 3,0 ms medidos (1,24 del tick y 1,72 de temperatura entre fotogramas) | ~170 |
| Estado de la simulación | Heap de Lua vivo: 13,9 MB; hasta 22,4 MB entre recolecciones | 199 KiB de simulación y 369 KiB de reglas, más el mapa (256 KiB) y el buscador (164 KiB) | — |

La fase 1 estimaba para la partida `mediano` (53 pacientes y 16 empleados) 812 ms por hora en la N64, 16 veces la CPU entera. El motor hace este hospital con 3,0 ms por hora en esas horas y con 7,5 ms de media en las 9.000.

### Los fotogramas

| | Normal (18,5 h/s) | Max speed (27,8 h/s) | And then some more (55,6 h/s) |
|---|---:|---:|---:|
| Fotogramas | 900 | 300 | 300 |
| CPU por fotograma, media | **19,3 ms** | 20,4 ms | 42,6 ms |
| p99 / máximo | 28,2 / 44,4 ms | 34,9 / 53,6 ms | 70,8 / 89,5 ms |
| Fotogramas que se saltan (más de 50 ms) | **0** | 2 | 182 |
| Intervalo máximo | 44,7 ms | 54,0 ms | 89,8 ms |
| Dibujo | 15,2 ms | 15,8 ms | 16,1 ms |
| — la lista | 5,66 ms | 5,80 ms | 6,03 ms |
| — suelos y sombras | 2,64 ms | 2,65 ms | 2,68 ms |
| — paredes y entidades | 6,92 ms | 7,33 ms | 7,42 ms |
| Simulación por fotograma | 0,76 ms | 1,22 ms | 22,9 ms |
| Tick de media / el más largo | 1,24 / 26,6 ms | 1,32 / 36,3 ms | 12,4 / 54,7 ms |
| Temperatura entre fotogramas | 1,06 ms | 0,68 ms | 0,21 ms |
| Audio (`mixer_try_play`) | 1,73 ms | 2,19 ms | 2,84 ms |
| Sprites (de entidades) | 551 (146) | 565 (161) | 571 (167) |
| Cargas de textura | 231 | 245 | 251 |

- **A velocidad Normal no salta ningún fotograma.** Seis de los 900 llegan tarde, entre 36,6 y 44,7 ms, cuando coincide un camino largo: se ven como un tirón, pero el ritmo de 30 fps no se pierde.
- **«Max speed» salta 2** por la misma razón: ticks de 36 ms.
- **«And then some more» no va.** Hay 1,85 ticks por fotograma, así que el tick siguiente llega antes de que acabe la pasada de temperatura y tiene que acabarla él: 12,4 ms por tick de media. Con 52 personas la simulación no es el problema; lo es la temperatura.
- **Lo más caro del dibujo son las cargas de textura:** 25 µs cada una (2.400 ciclos), 5,9 ms por fotograma, el 39 % del dibujo. Cada elemento de entidad cuesta 24 µs al enviarlo, casi todo la carga.
- **La lista cuesta 5,7 ms**, frente a 1,7 en el hito 2 con 379 sprites y sin entidades:
  - pasar los objetos y las personas a entidades (`th_vista_sim`) son 0,56 ms;
  - el resto son unos 23 µs por elemento de entidad. Con callgrind en el PC: el 37 % de las instrucciones es el recorrido de las casillas y el orden de las entidades, y un 33 % los elementos de las entidades. Cada fotograma de un humanoide recorre todos sus elementos, también los de capas que no lleva, y los decodifica de la tabla de animaciones de 470 KiB.
- **El RDP** sigue sin poder medirse en ares. El relleno, unos 250.000 píxeles por fotograma, son unos 4 ms a un píxel por ciclo, más las 231 cargas.

### Memoria

Heap tras cada paso de la carga, en KiB:

| Paso | Con 8 MiB | Con 4 MiB |
|---|---:|---:|
| Heap total (lo demás es código, datos y variables estáticas) | 6.865 | 2.769 |
| Arranque: 3 framebuffers, RDP, mandos y fuentes | 600 | 600 |
| `MSPR-0` entera | 1.786 | 1.786 |
| `MBLK-0` | 1.974 | 1.974 |
| Todas las animaciones | 2.443 | 2.443 |
| Escenario | 2.455 | 2.455 |
| `definiciones.trg` entero | 2.786 | **sin memoria** |
| Reglas del nivel | 2.812 | — |
| Música sonando (banco, sintetizador y FORTUNE) | 2.942 | — |
| **Libre al final** | **3.923** | — |

- **Fuera del heap** van 1,30 MiB: el código y los datos (0,48 MiB con el audio) y las variables estáticas (0,75 MiB).
- **Lo estático del motor:**
  - el mapa, 256 KiB;
  - el buscador, 164 KiB;
  - la simulación, 199 KiB: 96 KiB de temperatura y 128 personas de 736 B;
  - la lista de dibujo, 64 KiB, y lo demás.
- **Con 4 MiB**, la ROM de medida se queda sin memoria al cargar `definiciones.trg`. Era lo esperado: carga todos los sprites (1,16 MiB) y todas las animaciones (0,46 MiB), y la demo contaba con cargar solo los del nivel 1 (0,47 y 0,20).

### Tamaño del código y modo demo

Del mapa del enlazador de la ROM normal ([`codigo.py`](../tools/hito5/codigo.py)):

| Parte | Código | Solo lectura | Datos y bss |
|---|---:|---:|---:|
| Motor | 103.936 B | 38.050 B | 768.580 B |
| — de ello, la simulación (`sim`, `sim_rooms`, `humanoid`) | 70.208 B | | |
| libdragon | 155.616 B (222.368 con el audio) | 7.075 B | 19.258 B |
| libc, libm y libgcc | 72.980 B | 2.692 B | 2.204 B |

**Cuánta lógica hay ya** ([`cobertura.py`](../tools/hito5/cobertura.py)):
- las 69 funciones de CorsixTH que nombra el núcleo y las 10 acciones de humanoide suman 3.439 líneas de Lua;
- son el 14 % de las 24.749 de lógica (fase 3);
- la simulación ocupa 70.208 B para ellas: unos 20 B de código por línea de Lua.

Es una cota gruesa: las funciones cuentan enteras aunque el motor haga solo una parte.

**Proyección para el juego completo:**

| Parte | Proyección |
|---|---:|
| Lógica (24.749 líneas) | 0,48 MiB de código y 0,17 de solo lectura, en la proporción del motor |
| Interfaz (19.512 líneas de Lua en CorsixTH; aquí se rehace para mando y 320 × 240, se cuenta la mitad) | 0,19 MiB |
| Vista, dibujo y capa de la N64 | 0,08 MiB |
| **Código del juego** | **unos 0,9 MiB**; la fase 4 estimaba 0,75 para la demo y 1,00 para el juego |

**Presupuesto de la demo con 4 MiB**, el de [`04_construccion.md`](04_construccion.md#modo-demo-sin-expansion-pak) con lo medido:

| Concepto | Fase 4 | Ahora |
|---|---:|---:|
| libdragon y libc: código, datos y pila | 0,39 | 0,32 (medido, con el audio) y la pila |
| Código del juego | 0,75 | ~0,9 (proyectado) |
| Mapa, caminos, entidades y estado | 0,51 | 0,73 (medido: lo estático del motor, con la temperatura y 128 personas) |
| Vídeo (2 framebuffers), RDP y audio | 0,44 | 0,44 |
| Música y efectos | 0,17 | 0,13 (medido: banco, sintetizador y una pieza) |
| Tablas de animación del nivel 1 | 0,20 | 0,20 |
| Sprites y bloques del nivel 1 | 0,65 | 0,65 |
| Reglas | — | 0,10 (recortadas al nivel; enteras son 0,33) |
| Interfaz y textos | 0,55 | 0,55 |
| **Total** | **3,66** | **~4,0** |

**Recomendación: no hacer el modo demo sin Expansion Pak.**
- Con lo medido, el margen de 0,34 MiB desaparece. Lo estático del motor ya ocupa 0,22 MiB más de lo previsto (sobre todo la temperatura, 96 KiB, que la fase 4 no contaba), y el código proyectado, unos 0,15 más.
- Para meterla habría que partir el código en módulos cargables y rehacer partes del motor solo para ella, algo que la fase 4 ya descartaba («ni partir el juego en módulos cargables para ganar 0,1 MiB»).
- El juego completo necesita el Expansion Pak de todos modos.

### Hacia el hospital lleno

La partida `lleno` de la fase 1 tiene 365 personas, 305 objetos y 36 salas. Con lo medido:
- **las personas:** 365 × 5,7 µs son 2,1 ms por hora. En CorsixTH, cada persona del hospital lleno cuesta solo un 15 % más que en este escenario: 11,9 µs frente a 10,4 en el PC, descontando lo fijo de la partida `inicio`;
- **los caminos:** si las búsquedas crecen con los pacientes (de 0,58 a unas 4 por hora) y son más largas (unos 150 nodos), son unos 6,8 ms por hora;
- **la temperatura:** unos 5,4 ms de media, que no dependen de la gente;
- **lo fijo:** 1,2 ms.

En total, unos **15 ms por hora: el 28 % de los 54 ms de la velocidad Normal**, en un motor que todavía no tiene buena parte de la lógica. Con las mismas cuentas, «And then some more» (18 ms por hora) no llega.

Lo que hay que resolver para el juego completo, por orden (lo hecho después, en [«Después del hito 5»](#después-del-hito-5-los-riesgos-de-cpu)):
1. **Los caminos largos.** Un camino de 3.000–4.000 nodos se come un fotograma entero, y en el hospital lleno habrá más. Opciones:
   - repartir la búsqueda entre fotogramas. Cambia cuándo empieza a andar el paciente: habría que medir si se nota en la comparación con CorsixTH;
   - abaratar el nodo (1.014 ciclos, casi todo fallos de caché sobre una tabla de 128 KiB) sin cambiar el orden de visita.
2. **La temperatura a las velocidades altas.** Es un cálculo regular sobre 16.384 casillas: candidata al RSP, que hoy usan solo el RDP y el audio.
3. **El dibujo con mucha gente en pantalla.**
   - Cargas de textura: 2.400 ciclos cada una. La fase 4 medía unos 1.700 por elemento escribiendo los comandos a mano.
   - Los elementos de entidad en la lista: guardar decodificados los fotogramas recientes.

   Con 50 humanoides en pantalla (hito 3), el dibujo ya eran 23,7 ms.

### Comprobaciones

| Qué | Contra qué | Resultado |
|---|---|---|
| El escenario del hito 5: 10 semillas de 9.000 horas, 10 corridas del motor por semilla | La campaña de CorsixTH | **13 de 15 cifras con \|t\| < 2**; las otras dos, arriba |
| El escenario del hito 4 con el aprendizaje del médico: 40 semillas | La campaña del hito 4 | **14 de 15**, como antes |
| 9.000 horas del escenario del hito 5 en la N64 | `th64sim carga` | **La misma huella** (853ec05c) y la misma carga en las 7.611 horas comparables |
| Pruebas del núcleo | — | 60.233 comprobaciones, 0 fallos en el PC, con ASan y UBSan, y en la N64 |

### Pendiente

- **Lo de «Hacia el hospital lleno»:** los caminos largos, la temperatura a velocidades altas y el coste del dibujo con mucha gente. Hecho en parte después: ver «Después del hito 5».
- **Las necesidades de los pacientes** (del hito 4): con más pacientes pesan más en la salida (−16,8 %).
- **Medir en la consola.** Todo es de ares. Con el flashcart hay que repetir esta medida: ares no emula los contadores del RDP, y sus ciclos de memoria pueden ser optimistas.

## Después del hito 5: los riesgos de CPU

**Hecho en parte.** El hito 5 dejaba tres riesgos de CPU para el juego completo («Hacia el hospital lleno»): los caminos largos, la temperatura a las velocidades altas y el dibujo con mucha gente. Medido con la ROM de medida del hito 5, en el mismo escenario y desde las mismas horas:
- **Ningún fotograma salta ni llega tarde a ninguna de las tres velocidades.** La simulación va en una corrutina repartida entre fotogramas. Antes había 2 saltos a «Max speed» y 182 a «And then some more».
- **Los caminos largos ya no se notan:** la búsqueda se suspende cada 128 nodos y sigue en el fotograma siguiente. Lo que se simula no cambia, porque el dibujo enseña el último tick acabado: el paciente echa a andar en el mismo tick que antes. El nodo cuesta lo mismo, unos 1.000 ciclos: son fallos de caché al leer las casillas, y un montículo que no lee los nodos no lo mejora.
- **Las cargas de textura cuestan un 80 % menos:** 6,8 µs frente a 34,6, cargando con `LOAD_BLOCK` sin pasar por `rdpq_tex_upload`. El dibujo a velocidad Normal baja de 18,1 a 10,3 ms por fotograma, con los mismos píxeles.
- **La lista de dibujo cuesta un 23 % menos:** 4,0 ms por fotograma a velocidad Normal, frente a 5,3, con las mismas listas.
- **El fotograma a velocidad Normal cuesta 13,7 ms de CPU**, frente a 19,3 en el hito 5: el 41 % de 33,3.
- **La temperatura sigue limitando la velocidad más alta.** «And then some more» va a 30 fps, pero la simulación avanza 36,2 horas por segundo de las 55,6 que tocan: el 65 %. La fase 4 ya aceptaba limitar las dos velocidades más altas si la CPU no llegaba ([`04_decision.md`](04_decision.md)). Abaratarla de verdad pide el RSP.

La simulación sigue siendo la misma: tras 9.000 horas, la misma huella que el PC (853ec05c), y las pruebas y las comparaciones de píxeles de `motor_check.sh` dan lo mismo que antes.

### La simulación repartida entre fotogramas

En el hito 5, los ticks que tocaban se hacían enteros antes de dibujar. Un tick con un camino de 3.000 nodos (unos 30 ms) o con la pasada de temperatura entera (unos 15 ms) no cabía en el fotograma, y se veía un tirón.

Ahora ([`simulacion.c`](../motor/platform/n64/simulacion.c)):
- **la simulación va en una corrutina** (`coroutine.h` de libdragon). El bucle principal le pide los ticks que tocan por el reloj, dibuja y le da el tiempo que sobra hasta 28 ms después de empezar el fotograma. Lo que no le da tiempo a hacer lo sigue en el fotograma siguiente;
- **el núcleo tiene puntos de pausa** ([`th/pausa.h`](../motor/core/include/th/pausa.h)): después de cada humanoide, al empezar cada búsqueda de camino y cada 128 nodos. Un punto de pausa llama a un gancho que en el PC no existe, así que la simulación es la misma (la huella no cambia);
- **el dibujo no lee la simulación**, que puede estar a medio tick, sino una instantánea de cada tick acabado: las entidades de `th_vista_sim`, el dinero y la fecha. La pasada de temperatura de cada tick se hace después de la instantánea, de 16 en 16 palabras, y siempre antes del tick siguiente.

Cada velocidad dos veces desde la misma hora (la 3.501, con 52 personas), con la carga de texturas nueva:

| | Normal, sin repartir | Normal, repartida | Max speed, sin repartir | Max speed, repartida | And then some more, sin repartir | And then some more, repartida |
|---|---:|---:|---:|---:|---:|---:|
| Fotogramas | 900 | 900 | 300 | 300 | 300 | 300 |
| CPU por fotograma, media | 15,0 ms | 15,1 ms | 15,9 ms | 15,5 ms | 36,5 ms | 25,4 ms |
| p99 / máximo | 25,0 / 39,8 ms | 27,1 / 28,8 ms | 29,2 / 54,7 ms | 28,4 / 29,2 ms | 69,3 / 89,4 ms | 29,0 / 29,2 ms |
| Fotogramas tarde (más de 34 ms) | 3 | **0** | 3 | **0** | 184 | **0** |
| Fotogramas que se saltan (más de 50 ms) | 0 | **0** | 1 | **0** | 12 | **0** |
| Intervalo máximo | 40,1 ms | 33,5 ms | 55,0 ms | 33,5 ms | 89,7 ms | 33,5 ms |
| Horas de juego por segundo (las que tocan) | 18,5 (18,5) | 18,5 | 27,8 (27,8) | 27,8 | 44,5 (55,6) | 34,6 |
| Ticks pendientes, como mucho | — | 1 | — | 2 | — | 212 |

- **A velocidad Normal y a «Max speed» la simulación va al día:** como mucho 1 o 2 ticks por hacer. Se suspende 8 veces en 900 fotogramas a velocidad Normal y 7 en 300 a «Max speed», cuando un tick no cabe en lo que queda del fotograma.
- **El tramo más largo sin un punto de pausa es de 2,0 a 2,5 ms:** lo que, como mucho, un fotograma se puede pasar de los 28 ms.
  - El primero que se midió, de 7,4 ms, era el cambio de mes: al cambiar el aire de fuera, `th_sim_temperature_begin` recorría las 16.384 casillas para marcar las de aire.
  - Ahora combina 512 palabras con dos máscaras, la del aire y la del radiador, que se calculan junto con los pesos de cada casilla. Ocupan 4 KiB más: la simulación pasa a 203 KiB.
- **A «And then some more», repartida, no salta ningún fotograma, pero la simulación se queda atrás:** 344 ticks en 300 fotogramas, 34,6 horas por segundo de las 55,6. Sin repartir hacía más horas (44,5 por segundo), pero a tirones: 184 fotogramas tarde y 12 saltos. Cada fotograma se va en el dibujo (10,8 ms), la temperatura (10,0), la simulación (2,3) y el audio (1,8).
- **Antes de la carga de texturas nueva**, la misma prueba repartida llegaba a 25,7 horas por segundo: 256 ticks.

### Las cargas de textura

`rdpq_tex_upload` manda 5 comandos por carga (`SET_TEXTURE_IMAGE`, dos `SET_TILE`, `LOAD_BLOCK` o `LOAD_TILE` y `SET_TILE_SIZE`), más los `SYNC` que añade rdpq porque el rectángulo anterior usaba la TMEM y el tile, y antes calcula cómo cargar. En ares, cada comando de rdpq cuesta unos 145 ciclos de CPU: la carga nueva son unos 580 ciclos para 4 comandos con sus `SYNC`.

La carga nueva ([`mapa_n64.c`](../motor/platform/n64/mapa_n64.c), `cargar`) aprovecha cómo están los sprites en la hoja:
- **sube el bloque entero del sprite con un `LOAD_BLOCK` de texeles de 16 bits.** Cada fila tiene `ROUND_UP(ancho, 8)` bytes y el relleno es el índice transparente, así que sobra el `LOAD_TILE` que `rdpq_tex_upload` usa cuando el ancho no es múltiplo de 8: el 89 % de los sprites de `MSPR-0` y el 5 % de los bloques de `MBLK-0`;
- **`TILE0` se configura solo cuando cambia el paso**, y su tamaño es el máximo desde el principio del fotograma. Un rectángulo solo lee los texeles de su sprite y, como mucho, la columna de relleno, que es transparente;
- **quedan 2 comandos por carga** (`SET_TEXTURE_IMAGE` y `LOAD_BLOCK`), y un `SET_TILE` cuando cambia el paso.

Con 11 palabras de 64 bits por fila o más, el `LOAD_BLOCK` pierde precisión en las filas impares (el DXT; ver `rdpq_tex.c`). Esos sprites van por `rdpq_tex_upload`, pero en `MBLK-0` y `MSPR-0` no hay ninguno. Los 29 que no caben en la TMEM siguen con `rdpq_tex_blit`.

**Ciclos de CPU por sprite, cargando y dibujando un rectángulo de 8 × 1** (256 sprites de `MSPR-0`, con el RDP libre al empezar):

| Carga | CPU | Hasta que acaban el RSP y el RDP |
|---|---:|---:|
| `rdpq_tex_upload` (antes) | 2.705 | 2.789 |
| Directa (ahora) | **1.044** | 1.170 |
| Los comandos ya hechos de cada sprite, con `rdpq_exec` | 600 | 633 |
| Solo el rectángulo | 464 | 480 |

**En el fotograma, velocidad Normal repartida** (900 fotogramas, 231 cargas y 146 sprites de entidades por fotograma):

| | `rdpq_tex_upload` | Directa | `rdpq_exec` |
|---|---:|---:|---:|
| CPU por fotograma | 22,7 ms | **15,1 ms** | 14,2 ms |
| Dibujo | 18,1 ms | **10,3 ms** | 9,4 ms |
| — suelos y sombras | 2,82 ms | 2,03 ms | 1,98 ms |
| — paredes y entidades | 10,05 ms | 2,97 ms | 2,14 ms |
| Cargas | 8,0 ms (34,6 µs cada una) | **1,57 ms (6,8 µs)** | 0,61 ms (2,6 µs) |
| Por sprite de entidad, sin la lista | 41,8 µs | 12,4 µs | 8,3 µs |

- **Los mismos píxeles.** Las tres cargas dan el mismo fotograma (0 píxeles distintos de 76.800, con 209 cargas). Las comparaciones de `motor_check.sh` con el rasterizador de referencia siguen dando 0 píxeles distintos en los hitos 2, 3 y 4.
- **En el fotograma se gana más de lo que dice la prueba aislada:** 27,8 µs por carga frente a 1.660 ciclos (17,7 µs). No se ha medido por qué.
- **`rdpq_exec` no es la carga normal**, aunque gana otros 0,9 ms. Con él, el RSP espera a que el RDP haya empezado el búfer anterior en cada carga (`RSPQCmd_RdpSetBuffer`), y ares no lo puede decir: ejecuta los comandos del RDP al instante, así que esa espera no aparece en la medida. Queda en la ROM de medida para probarlo en la consola. Un comando propio en el RSP que cargue y dibuje un sprite sería lo siguiente, si hiciera falta.
- **La caché de instrucciones.** [`cache_instrucciones.py`](../tools/cache_instrucciones.py) mira ahora también `cargar` y las funciones de rdpq que llama en cada carga. Al mirarlas, las ROM normal, de pruebas y de horas chocaban con ellas en 1 a 10 líneas. El relleno pasa a 3.520 B en la normal y en la de horas, a 5.728 en la de pruebas y a 5.216 en la de medida. Con la carga nueva, el recorrido del hito 3 con 50 humanoides en pantalla dibuja en 17,4 ms con el choque y en 13,5 sin él; en el hito 3 eran 23,7.

### La lista de dibujo

Con la carga nueva, la lista (`th_view_scene`) pasó a ser lo más caro del dibujo: 5,3 ms de 10,3 a velocidad Normal.

[`th64sim lista`](../motor/README.md) hace en el PC la misma lista que la ROM de medida (la hora 3.501 y la misma cámara) y escribe su huella, la misma en los dos. Con callgrind, cada lista eran 200.800 instrucciones:
- el 37 % `th_view_scene`: el bucle de las casillas y el orden de las entidades;
- el 14 % `add_layer`;
- el 14 % la búsqueda de las entidades de cada casilla, que se llamaba en todas;
- el 23 % los elementos de animación de las entidades.

Cambios, sin que cambie la lista:
- **los tamaños de los bloques se leen una vez por hoja,** no 256 veces en cada lista;
- **los suelos y las paredes, que no llevan recortes ni banderas, van por un camino corto** (`add_block`);
- **antes de buscar las entidades de una casilla se mira su bit** en el mapa de casillas con entidades;
- **los elementos de animación se leen en línea** ([`anim.h`](../motor/core/include/th/anim.h)), y el tamaño de su sprite también;
- **cada entidad lleva su casilla en una tabla aparte** para ordenarlas y buscarlas.

| | Antes | Ahora |
|---|---:|---:|
| Instrucciones por lista en el PC (callgrind) | 200.800 | 139.400 |
| N64, con las 58 entidades del hospital (528 sprites, 123 de entidades) | 4,77 ms | 4,23 ms |
| N64, sin ellas (403 sprites) | 2,22 ms | 1,51 ms |
| N64, lista por fotograma a velocidad Normal, repartida | 5,25 ms | **4,03 ms** |
| N64, leer un elemento de animación (con la caché caliente) | 100 ciclos | 36 ciclos |
| Recorrido del hito 3 con 50 humanoides en pantalla: lista / dibujo | 6,89 / 13,50 ms | 5,13 / 11,86 ms |

- **Las mismas listas.** Las 658 listas de [`huellas_lista.sh`](../tools/hito5/huellas_lista.sh) dan las mismas huellas antes y después: una rejilla de cámaras y horas de los escenarios de los hitos 4 y 5, 344 de ellas con entidades. Las comparaciones de píxeles de `motor_check.sh` siguen dando 0 distintos.
- **Las entidades siguen añadiendo unos 2,7 ms en la N64:** 22 µs por sprite de entidad.
  - En el PC son unas 75.000 instrucciones por lista: unas 31.600 en recorrer los elementos, unas 11.000 en un 17 % más de casillas (con entidades, la vista mira 64 píxeles a cada lado de la pantalla en lugar de 32), 8.300 en añadir sus sprites, y el resto en ordenarlas, buscarlas y redibujar lo que tapa a las animadas.
  - Guardar los elementos ya filtrados por fotograma de animación ayudaría poco: cada persona cambia de fotograma en cada tick, que a velocidad Normal llega cada 1,6 fotogramas.

### Los caminos

Una búsqueda cuesta unos 1.000 ciclos por nodo en la N64 (10,7 µs). En el PC son unas 312 instrucciones por nodo (callgrind, 3.000 horas del escenario del hito 5): el resto son fallos de caché.

- **Se probó guardar el valor de cada nodo en su entrada del montículo,** para no leer los nodos al ordenarlo.
  - Da los mismos caminos: los 12.000 de CorsixTH del hito 3, y las mismas huellas.
  - Pero el montículo es pequeño: 2 niveles de media al sacar un nodo y 0,66 al meterlo.
  - Las búsquedas de las 7.611 horas pasaron de 4,03 a 4,10 s, así que no mejora nada. **Descartado**, y ahorra 32 KiB.
- **Lo que cuesta es leer las casillas:** 16 B cada una, y las vecinas de arriba y de abajo están a 2 KiB. Un mapa aparte con las banderas de paso, 1 B por casilla (16 KiB), cabría mejor en la caché, pero hay que tenerlo al día cada vez que cambia el mapa. No está hecho.

### La temperatura

Cuesta 0,92 µs por casilla recalculada (86 ciclos): 15,1 ms la pasada entera de 16.384 casillas. A velocidad Normal son 1,1 ms por fotograma de media; a «And then some more», con la simulación repartida, 10,0 ms, lo que más gasta.

Lo que se ha probado en la CPU:
- **calcular una vez las palabras uniformes.** Una palabra de 32 casillas de aire en la que la casilla y sus cuatro vecinas valen lo mismo da el mismo resultado en las 32. En las 9.000 horas del escenario son 174.218 de 1.398.130 palabras de pasadas enteras: un 12,5 %. **Descartado.**
- **separar los valores y los pesos en tres tablas de 32 KiB** en lugar de una de 96 KiB con casillas de 6 B. Con el coste de ares (40 ciclos por fallo de caché de datos y 40 más si la línea expulsada está modificada), la memoria pasaría de unos 30 a unos 20 ciclos por casilla, de 86. **Descartado.**
- **el cambio de mes,** que recorría las 16.384 casillas sin pausas: ahora son dos máscaras (ver arriba).

Queda el RSP. Es un cálculo regular sobre valores de 16 bits, 8 casillas por instrucción, y hoy el RSP solo lo usan el RDP y el audio. Para hacerlo hay que:
- dar exactamente los mismos enteros que la CPU, porque la huella tiene que seguir igual. Las divisiones entre 100, 1.000 y la suma de pesos se hacen con multiplicaciones, y hay que comprobar todos los valores posibles;
- coordinarlo con la corrutina de la simulación.

No se hace por ahora: a velocidad Normal y a «Max speed» la temperatura ya cabe.

### Los fotogramas ahora

Con la simulación repartida, la carga de texturas nueva y la lista nueva (la última medida):

| | Normal (18,5 h/s) | Max speed (27,8 h/s) | And then some more (55,6 h/s) |
|---|---:|---:|---:|
| CPU por fotograma: hito 5 → ahora | 19,3 → **13,7 ms** | 20,4 → **14,1 ms** | 42,6 → **25,0 ms** |
| p99 | 28,2 → 26,1 ms | 34,9 → 28,3 ms | 70,8 → 28,8 ms |
| Fotogramas que se saltan | 0 → **0** | 2 → **0** | 182 → **0** |
| Horas de juego por segundo | 18,5 → 18,5 | 27,8 → 27,8 | 40,2 (a tirones) → 36,2 |
| Dibujo | 15,2 → 9,0 ms | 15,8 → 9,3 ms | 16,1 → 9,5 ms |
| — la lista | 5,66 → 4,03 ms | 5,80 → 4,13 ms | 6,03 → 4,23 ms |

La memoria apenas cambia: la simulación ocupa 4 KiB más (las máscaras de la temperatura). En la ROM de medida, con 8 MiB, quedan 3.854 KiB de heap libres con la música sonando, frente a 3.923. El heap es 31 KiB menor (código, relleno y las máscaras), y la propia medida usa 38 KiB más, unos 31 de ellos para los comandos de `rdpq_exec`.

### Pendiente

- **Las entidades en la lista:** unos 2,7 ms con 123 sprites de entidades.
- **Los caminos en el hospital lleno:** ya no se notan, pero cuestan lo mismo. Lo siguiente sería un mapa compacto de banderas de paso.
- **La temperatura en el RSP,** si hacen falta las velocidades altas.
- **En la consola:** medir el RDP y probar `rdpq_exec`, que ares no puede juzgar.

## Música: la N64 frente a fluidsynth

La pregunta: ¿suena la música en la N64 como en el PC? La N64 la toca con MID64 y el sintetizador SF64 de libdragon. La referencia es fluidsynth 2.3.4 con el mismo banco (TimGM6mb). El sintetizador de libdragon no es el reproductor XM (`xm64`, libxm), que sí tiene vibrato y envolventes de volumen y panorama; la música de Theme Hospital es MIDI y va por SF64.

### Cómo se mide

- **La N64.** La ROM [`n64/pruebas/musica`](../n64/pruebas/musica) toca las 8 piezas con la configuración de sonido del motor:
  - 22.050 Hz (la N64 da 22.047);
  - 16 voces;
  - modo GM1.

  No suena por el altavoz: pide al mezclador cada bloque (`mixer_poll`) y lo vuelca en base64 por EMUX. La grabación es exactamente lo que mezcla la consola, aunque ares vaya más deprisa que el tiempo real (si va más deprisa, ares tira el audio que no cabe en la salida, y por eso la salida de audio de ares no sirvió).

  Las 8 piezas son 1.336 s de audio, en 57.530 bloques de 512 muestras, sin que falte ninguno. ares tarda 4 min 15 s.
- **fluidsynth**, a 48 kHz, de cuatro maneras:
  - como en el PC;
  - sin reverberación ni coro;
  - además, sin filtro ni LFO ([`banco_n64.py`](../tools/musica/banco_n64.py)), lo más parecido a lo que hace SF64;
  - además, con 16 voces.
- **Las medidas** ([`comparar.py`](../tools/musica/comparar.py)), con la N64 pasada a 48 kHz:
  - **distancia de timbre:** la diferencia media, en dB, del espectro a largo plazo por tercios de octava entre 100 Hz y 10 kHz, con el nivel igualado;
  - **correlación de las envolventes** de 50 ms: si las notas suben y bajan a la vez y con el mismo peso;
  - el centroide espectral, el desfase y la deriva.

  Los pasos están en [`tools/musica`](../tools/musica/README.md).

El tempo es exacto: en las 8 piezas el desfase es 0 ms y la deriva entre el principio y el final no pasa de 10 ms, que es el paso de la medida.

### Lo que no hace el sintetizador SF64, en este banco

| Qué | Instrumentos del banco recortado (43) | Detalle |
|---|---:|---|
| Filtro paso bajo | 30 | Corte entre ≈ 120 Hz (Synth Strings 10) y ≈ 5,6 kHz; 5 con resonancia de 1,3 a 2,5 dB |
| Envolvente de modulación al filtro | 28 | Abre el filtro de +787 a +10.800 cents al empezar la nota |
| LFO de modulación al volumen | 7 | Hasta 2,5 dB (Tenor Sax) |
| LFO de modulación al tono | 3 | Mute Trumpet 10 cents, Organ 30 18, Tenor Sax 9–10 |
| LFO de modulación al filtro | 2 | Organ 2 y Organ 30 |
| LFO de vibrato | 1 | Tenor Sax, 9 cents |
| Moduladores SF2 | 170 | Casi todos anulan los de por defecto o mandan a reverberación y coro. Solo 15 hacen algo más: la rueda de modulación y la presión de canal mueven de −50 a +10 cents los LFO de 5 instrumentos |
| Reverberación y coro | — | La N64 no tiene. Solo `STEADY` los pide (10 controladores) |
| Envolvente de modulación al tono | 10 | **Sí la hace** la N64 |

- **Lo que dice `audioconv64`.** Cuenta presets, no instrumentos (28). Su aviso de «vibrato LFO (27)» incluye los LFO que tienen frecuencia pero profundidad cero: con profundidad, solo hay uno. La «modulation envelope (3)» es una envolvente sin destino, que no suena en ningún sintetizador.
- **Lo que piden las piezas.** No hay *pitch bend*. La rueda de modulación (CC1, 22 veces en `ATLANTIS`) y la presión de canal (24 en `FORTUNE` y 4 en `MEND`) solo actuarían sobre esos LFO.

Medido en fluidsynth:
- el filtro y los LFO juntos cambian el timbre entre 0,1 y 1,3 dB según la pieza. Sin ellos, el centroide sube de 23 a 163 Hz;
- la reverberación y el coro, entre 0,0 y 0,2 dB;
- limitar a 16 voces, entre 0,0 y 0,2 dB.

### Lo que sí cambiaba: la atenuación de los instrumentos

La primera grabación (banco del hito 0) se apartaba de fluidsynth más de lo que explican el filtro y los LFO:
- distancia de timbre de 0,6 a 3,0 dB;
- correlación de envolventes de 0,740 a 0,988.

`CANDY` era la peor. Tocada canal a canal ([`solo.py`](../tools/musica/solo.py)), el nivel de cada instrumento en la N64 frente a fluidsynth sin filtro ni LFO:

| Canal | Instrumento | Atenuación en el banco (cB) | N64 − fluidsynth |
|---:|---|---:|---:|
| 1 | Synth Bass 1 | 30 | +9,1 dB |
| 6 | Kalimba | 10–17 | +9,2 dB |
| 7 y 8 | Mute Trumpet | 35–155 | +10,3 dB |
| 9 | Percusión | 10–247 | +9,1 dB |
| 2 | Organ 30 + Organ 31 | 245–258 y 112–189 | +2,2 dB |
| 5 | Saw Wave | 132–177 | +1,6 dB |
| 3 | Square Wave | 185–230 | −2,8 dB |

Los instrumentos muy atenuados en el banco suenan de 7 a 12 dB más bajos en la N64 que el resto.

**La causa.** SF64 aplica la atenuación inicial entera, como dice la especificación SF2. fluidsynth la multiplica por 0,4, como el hardware EMU de las Sound Blaster, para el que se hicieron casi todos los bancos General MIDI.

**La comprobación.** Con el banco ya multiplicado por 0,4, la onda cuadrada sube 5,3 dB en fluidsynth. Si fluidsynth aplicara la atenuación entera, habría subido unos 12,6 dB; con su factor 0,4, la previsión es de unos 5,0 dB.

**La corrección.** Está en el conversor ([`sonidos.py`](../tools/conv/sonidos.py), `ATENUACION_N64` y `MARGEN_N64_CB`):
- la atenuación de cada instrumento se multiplica por 0,4;
- cada preset suma 60 cB.

El margen hace falta porque el volumen sube: sin él, `CANDY` pasaría de 0 dBFS en unos 3 dB (en la prueba se recortaron 144 muestras). El banco para fluidsynth no cambia: con `--vistas`, el conversor deja también el recorte sin ajustar, y sigue sonando igual que el banco entero. El SF64 pasa de 713.908 a 713.869 B.

### Resultado

| Pieza | Frente al PC, antes | Frente al PC, ahora | Frente a sin filtro ni LFO, ahora | Pico y nivel ahora |
|---|---|---|---|---|
| `ATLANTIS` | 1,2 dB · 0,969 | 1,1 dB · 0,987 | 0,4 dB · 0,990 | −5,4 · −26,1 dBFS |
| `CANDY` | 3,0 dB · 0,740 | 1,6 dB · 0,938 | 0,4 dB · 0,954 | −4,2 · −21,5 dBFS |
| `DOCTOR` | 1,1 dB · 0,988 | 0,5 dB · 0,995 | 0,3 dB · 0,995 | −7,5 · −25,0 dBFS |
| `FORTUNE` | 1,8 dB · 0,833 | 0,6 dB · 0,953 | 0,2 dB · 0,963 | −5,7 · −25,2 dBFS |
| `MEND` | 0,6 dB · 0,983 | 0,5 dB · 0,993 | 0,4 dB · 0,995 | −5,6 · −24,1 dBFS |
| `NIGHTSH` | 1,2 dB · 0,947 | 0,7 dB · 0,976 | 0,4 dB · 0,982 | −7,7 · −28,2 dBFS |
| `SIXSEVEN` | 0,6 dB · 0,899 | 0,5 dB · 0,951 | 0,4 dB · 0,953 | −5,7 · −24,5 dBFS |
| `STEADY` | 1,8 dB · 0,890 | 0,5 dB · 0,973 | 0,2 dB · 0,977 | −6,1 · −25,9 dBFS |

*Cada celda de comparación da la distancia de timbre y la correlación de envolventes. «PC» es fluidsynth con reverberación y coro. Antes, `SIXSEVEN` saturaba (pico de 32.767). Ahora ninguna pasa de −4,2 dBFS.*

- **Frente a fluidsynth sin filtro ni LFO**, la N64 queda en 0,2–0,4 dB de timbre. Es decir, la síntesis de SF64 es la misma salvo por lo que no implementa.
- **Lo que queda frente al PC** (0,5–1,6 dB) es sobre todo el filtro. En la N64 los instrumentos filtrados suenan más brillantes: hasta +186 Hz de centroide en `ATLANTIS`. El caso más claro es el Synth Bass 1 de `CANDY`: en fluidsynth el filtro parte de ≈ 134 Hz y la envolvente lo abre al atacar cada nota; en la N64 suena la muestra entera.
- **La salida a 22.050 Hz** no deja nada por encima de 11 kHz. En fluidsynth esa banda tiene entre el 0,0 y el 0,6 % de la energía.
- **El vibrato** no importa en esta música: lo usa un instrumento, con 9 cents.

**Decisión: la música se queda en MID64 + SF64**, 0,75 MiB con el banco recortado. Opus estéreo serían 11,37 MiB ([`02_datos.md`](02_datos.md#música)) y no se gana nada que se oiga, salvo el filtro.

Dos cosas se pueden proponer a libdragon:
- una opción de `audioconv64` para la atenuación al estilo EMU (aquí se hace en el conversor);
- un filtro paso bajo de un polo por voz, con la envolvente de modulación, que es lo siguiente que se oye.
