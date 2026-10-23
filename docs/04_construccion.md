# Fase 4 — Cómo se construye el motor

Fecha: 2026-10-12. Estado: **evaluación, pendiente de revisión con Eduardo** junto con [`04_decision.md`](04_decision.md).

Responde a tres cosas que Eduardo planteó al revisar la decisión:
- «¿Has evaluado ya cómo construirlo? ¿Herramientas y eso?»
- la pregunta 4: qué reglas seguir, si CorsixTH o el original;
- la pregunta 1: un modo demo sin Expansion Pak, «como el Perfect Dark».

**Cómo se ha hecho.** Se investigaron cinco temas por separado:
1. herramientas;
2. APIs de libdragon y formatos;
3. método de trabajo y pruebas;
4. fidelidad de CorsixTH al original;
5. modo demo.

Cada tema tiene experimentos propios: ROMs ejecutadas en ares v148 y programas en el PC.

**Revisión.**
- **Herramientas y APIs:** cada tema lo revisó después otro agente, que repitió las comprobaciones intentando refutarlas. Confirmó 26 de 36 hallazgos y corrigió 10. Las correcciones ya están aplicadas aquí; las más importantes son:
  - la RAM del audio;
  - el modelo de ciclos de ares;
  - `-Os` y el backtrace;
  - de dónde sale el coste de C++;
  - el coste del dibujo con APIs estables;
  - los elementos que de verdad se dibujan por fotograma.
- **Método, fidelidad y modo demo:** sus revisiones se cortaron por el límite de gasto. Sus cifras se han repetido a mano, ejecutando otra vez los scripts y leyendo las salidas:
  - las líneas de datos;
  - las 126 definiciones;
  - las llamadas a `math.random`;
  - el aspecto de los pacientes;
  - la dispersión entre semillas;
  - las 0 curas de `mediano`;
  - las opciones y los recuentos de CorsixTH;
  - la RAM con 4 MiB;
  - los sprites del nivel 1;
  - las sumas de los presupuestos.

Las fuentes de las pruebas están en [`n64/pruebas/`](../n64/pruebas/) y [`tools/construccion/`](../tools/construccion/), y sus salidas en `bench/resultados/fase4/` ([cómo reproducirlo](#cómo-reproducirlo)).

**Los ciclos de CPU son los del reloj emulado por ares.** El modelo de ares cobra:
- un ciclo por instrucción, más otro por cada lectura o escritura que acierta en la caché de datos;
- 40 ciclos por cada fallo de la caché de datos, 80 si además hay que escribir una línea modificada;
- 48 por cada fallo de la caché de instrucciones.

Medido con una ROM de calibración (`n64/pruebas/calibracion`), leer 1 MiB cuesta 45 ciclos por línea si las líneas que salen de la caché están limpias, y 85 si estaban modificadas. La fase 1 hablaba de unos 70: su medida mezclaba lecturas y escrituras. Ya está corregido en [`01_mediciones.md`](01_mediciones.md); el factor de CPU de esa fase se midió aparte y no cambia.

ares no modela los bloqueos del pipeline ni la competencia por la RDRAM con el RSP y el RDP. Es determinista: la misma ROM da los mismos ciclos con un margen de ±2. Pero el modo Homebrew los cambia en un 0,4 %, así que las comparaciones tienen que usar siempre el mismo modo. Sirve para comparar versiones del código; los valores absolutos probablemente son optimistas y habrá que confirmarlos en una consola.

## En breve

1. **C (gnu17) con el `n64.mk` de libdragon.** C++ también funciona, pero tal cual añade 88 KB de código, que en la N64 vive en la RAM; con `iostream`, 421 KB. Esos 88 KB solo desaparecen compilando sin asserts (`NDEBUG`) y con un `operator new` que no lance excepciones. Como CorsixTH se traduce desde Lua y libdragon es C, C++ no compensa ese cuidado.
2. **Tres capas:**
   - un **núcleo de simulación en C portable**, que compila igual en el PC y en la N64 y da la misma huella de estado en los dos;
   - una **capa de plataforma**: libdragon en la consola, y un ejecutable sin pantalla en el PC;
   - un **conversor** en Python y Lua.
3. **Los datos de la lógica se generan, no se traducen a mano.** Entre el 34 y el 39 % de las líneas de los directorios de objetos, habitaciones, enfermedades y diagnóstico de CorsixTH son tablas de datos. Sus 126 definiciones se cargan y se vuelcan a JSON sin un solo fallo. A mano se reescribe el comportamiento, que se concentra en las acciones y los humanoides.
4. **Pruebas en tres niveles:**
   - **oráculos exactos** donde no hay azar. Por ejemplo, la duración de las 5.186 animaciones coincide en 5.183, y las 3 restantes son ajustes conocidos de CorsixTH;
   - **comparación estadística** con CorsixTH donde hay azar;
   - **la misma huella** en el PC y en la N64.
5. **Depuración en ares:**
   - con GDB (`gdb-multiarch` instalado con apt, porque el de la toolchain no arranca en Ubuntu 24.04);
   - en «modo Homebrew», que da `debugf` sin límite de tamaño de ROM, perfiles con fallos de caché y salida automática para pruebas;
   - con la pantalla de error de libdragon, que trae backtrace con fichero y línea.
6. **Dibujo por una vía más barata que `rdpq_tex_blit`.** Ciclos de CPU por sprite:
   - `rdpq_tex_blit`: 3.497–4.248;
   - cargar la textura y dibujar un rectángulo con APIs estables: 1.758–2.542;
   - la misma carga escrita a mano: unos 1.630.

   Se empieza por las APIs estables y se mide en el hito 2. Los sprites van en un formato propio por hoja: 1,34 MiB, frente a 3,96 MiB como `.sprite` sueltos.
7. **Audio:**
   - efectos en VADPCM **sin** la compresión Huffman que trae activada `audioconv64`: con ocho efectos sonando se pasa del 16,7 % de CPU al 3,0 %;
   - la música en MIDI, con un banco de sonidos recortado a los instrumentos que usa el juego.
8. **Hardware para probar:**
   - un **SummerCart64**, unos 70–99 $;
   - un **Expansion Pak** de segunda mano, unos 40–70 $;
   - el ratón de N64 es caro y raro (un solo anuncio encontrado, a 401 $). Se puede probar antes en ares con el ratón del PC.
9. **Reglas:** CorsixTH v0.70.1 como especificación, con 6 excepciones ([fidelidad](#fidelidad-corsixth-frente-al-original)).
10. **Modo demo con 4 MiB:**
    - **contenido:** el nivel 1 completo, el mismo que usó la demo de PC;
    - **viabilidad:** cabe con un margen estimado de 0,34 MiB, que desaparece si el código del juego ocupa lo que estima la fase 4;
    - **propuesta:** hacer ya lo que sale barato y decidir la demo con la medida del hito 5 ([modo demo](#modo-demo-sin-expansion-pak)).

## Herramientas

| Para qué | Herramienta | ¿Comprobado aquí? | Dato |
|---|---|---|---|
| Compilar la ROM | `mips64-elf-gcc` 16.2 con `n64.mk` | Sí | 0,86 s desde cero y 0,55 s tras tocar un fichero (ROM de prueba) |
| Compilar y probar el núcleo en el PC | gcc 13.3 o clang 18, CMake y CTest; macros de prueba propias (`th_test.h`, 40 líneas) | Sí | 60.045 comprobaciones en 40 ms |
| Errores de memoria en el PC | ASan y UBSan de gcc; valgrind | Sí | ASan ve desbordamientos; valgrind, lecturas sin inicializar; ASan+UBSan multiplican el tiempo por 9,6 |
| Comportamiento indefinido en la propia N64 | `-fsanitize=undefined -fsanitize-trap=undefined` | Sí | +1.856 B de código; ares se para en la línea exacta |
| Análisis estático | `gcc -fanalyzer` (PC y N64); `clang-tidy`, solo para el núcleo | Sí | clang no entiende el ABI `o64` de libdragon |
| Emulador | ares v148 en modo Homebrew | Sí | Ciclos reproducibles a ±2 |
| Depurador | Servidor GDB de ares y `gdb-multiarch` 15.1 | Sí | Puntos de ruptura, variables, structs, paso a paso, watchpoints. Sin IPv6 hace falta `DebugServer/UseIPv4=true`, que escucha en todas las interfaces y sin contraseña: solo detrás de un cortafuegos |
| Perfilado | `profile.h` con EMUX: ciclos y fallos de caché por bloque | Sí | — |
| Validar el RDP | `rdpq_debug_start()` en la ROM; `rdpvalidate` sin consola | Sí | `rdpvalidate` devuelve 0 aunque encuentre errores: hay que buscar «ERROR» en su salida |
| Assets | `mkasset`, `audioconv64`, `videoconv64`, `mkfont` | Sí | — |
| Integración continua | GitHub Actions con la imagen `ghcr.io/dragonminded/libdragon:preview` fijada por digest | Borrador sin ejecutar | Compilar libdragon y sus herramientas sobre la imagen: unos 54 s sin otra carga, frente a 432 s del `tools/setup.sh` completo. Falta comprobar que su GCC es la misma que la local (imagen del 2026-09-14, paquete local del 2026-10-09) |
| Consola real | SummerCart64 con `sc64deployer` o UNFLoader | No (no hay flashcart) | libdragon envía `debugf` por USB a 64drive, EverDrive y SummerCart64 |

### El día a día

1. Escribir la lógica en el núcleo y probarla en el PC: pruebas unitarias, sanitizers y el ejecutable sin pantalla.
2. Compararla con CorsixTH, con el arnés de la fase 1 como oráculo.
3. Compilar la ROM y ejecutarla en ares en modo Homebrew, leyendo `debugf` y los perfiles.
4. Si algo falla, leer la pantalla de error o abrir GDB contra ares.
5. Una vez por hito, en la consola con el SummerCart64: comparar ciclos, fps y RAM con los de ares.

### Detalles que conviene saber

- **Las APIs preview de libdragon no avisan.** En la rama `preview`, `n64.mk` fija `LIBDRAGON_PREVIEW ?= 2`: todas las APIs preview se pueden usar sin ningún aviso. Es lo contrario de lo que decía `docs/00_entorno.md`, ya corregido; el cambio es del commit `ea505c352` de libdragon, del 2026-08-21. El motor usará muchas a propósito: el mezclador, `wav64`, `mid64`, `sf64`, el vídeo, `profile.h` y los hilos. Con `LIBDRAGON_PREVIEW = 1` habría un aviso en cada uso, que no rompe la compilación y acabaría siendo ruido. Así que se deja el 2, y un trabajo de integración continua con `LIBDRAGON_PREVIEW=1` lista las APIs preview usadas para revisarlas al actualizar libdragon.
- **No hay que pasar `CFLAGS` por la línea de `make`.** Así se pierden los flags de `n64.mk` y la compilación falla. Para opciones por fichero se usa `objetivo.o: CFLAGS += …`.
- **`-O2` frente a `-Os`.** Con un módulo de prueba de 362 líneas (A* sobre 128 × 128 y un pool de 384 entidades):
  - `-Os` ocupa un 17,6 % menos (3.136 frente a 3.808 B). Pero una parte es relleno: `-Os` desactiva la alineación de funciones a 32 B que pone `n64.mk`, y de la que depende el backtrace de la pantalla de error. El código en sí baja un 15 %;
  - en ares fue un 3–4,5 % más rápido, pero ares no modela los bloqueos del pipeline, que es justo lo que optimiza `-O2`;
  - LTO solo ahorra 384 B.

  **Se usa `-O2`.** `-Os` solo en módulos concretos, recuperando la alineación (`-falign-functions=32`) y midiendo en consola.
- **Tipos.** Con el ABI `o64` de libdragon, `long` y los punteros ocupan 4 B; en el PC, 8. En las estructuras que se guardan solo se usan tipos de `stdint` y comprobaciones de tamaño con `_Static_assert`.

  Coste en ares, en ciclos por operación:

  | Tipo | Multiplicar | Dividir |
  |---|---:|---:|
  | Entero de 32 bits | 6 | 40 |
  | Coma fija Q16.16 | 16 | 72 |
  | `float` | 11 | 33 |
  | `double` | 12 | 60 |

  - El estado de la simulación va en enteros y coma fija, que dan la misma huella en el PC y en la N64.
  - El `float` cambia de resultado entre las dos plataformas: `n64.mk` incluye `-freciprocal-math`, que convierte `x/100.0f` en `x*0.01f`. Se queda para el dibujo y el audio.
  - Nada de `double`.
- **Ni hilos ni corrutinas para las entidades.** Cada cambio cuesta 6.763 ciclos con un hilo y 1.604 con una corrutina. Con una corrutina por humanoide, el hospital lleno gastaría unos 579.000 ciclos por pasada: un tercio de cada evento de 18 ms a velocidad Normal. Cada humanoide será una máquina de estados.
- **Módulos cargables (DSO).** Funcionan:
  - un módulo pequeño carga en 1,1 ms y libera su RAM al cerrarse;
  - pero ocupa en RAM un 31 % más que el mismo código enlazado en el ejecutable, porque los DSO no eliminan código sin usar y se compilan con `-G 0`;
  - el primer `dlopen` deja un hueco de 236 KiB en el heap.

  Sirven para las ventanas de gestión, una cargada cada vez, no para separar «el juego completo».
- **Guardado.** Medido en ares, con persistencia entre dos arranques:
  - EEPROM de 512 B y 2 KiB;
  - SRAM de 32 KiB: la API de libdragon no pasa de ahí aunque la cabecera diga más, y ares no admite la SRAM de 96 KiB;
  - FlashRAM de 128 KiB;
  - Controller Pak, con 31.488 B útiles.

  Una trampa: sondear la FlashRAM después de detectar 32 KiB de SRAM corrompe la SRAM. El motor lee el tipo de guardado declarado en la cabecera y no sondea.
- **EMUX sin modo Homebrew.** Sin ese modo, `emux_prof_read` no devuelve 0, sino basura (el código de la métrica): hay que comprobar antes `emux_detect()`. Además, `profile_dump` puede provocar una excepción de coma flotante en bancos de pruebas que no esperan nunca al RSP ni al vídeo.
- **Precios** consultados el 2026-10-10 y el 2026-10-13, casi todos de fuentes secundarias:

  | Qué | Precio |
  |---|---|
  | SummerCart64 | 70–99 $ |
  | EverDrive-64 X7 | ~149 $ |
  | EverDrive-64 Pro | ~169 $ |
  | Expansion Pak suelto, usado | 40 $ |
  | Expansion Pak completo, usado | 70 $ |
  | Ratón de N64 | Un único anuncio, a 401 $ |

  - El SummerCart64 conviene comprarlo por los canales que enlaza su web oficial, porque hay clones.
  - El EverDrive-64 Pro todavía no aparece en el código USB de libdragon del commit fijado.

## APIs de libdragon y lo que genera el conversor

### Sprites y dibujo

- **TMEM.** En CI8, `rdpq` usa la mitad baja de la TMEM para los píxeles (2.048 B) y la alta para la paleta. De los 4.601 sprites de `DATAM`:
  - solo 29 (0,6 %) pasan de 2.048 B; `rdpq_tex_blit` los parte solo en tiras, como mucho 3;
  - el tamaño mediano es 11 × 13, el p90 24 × 30 y el máximo 73 × 74.
- **Formato del conversor.** Un fichero por hoja:
  - una tabla de 8 B por sprite (desplazamiento, ancho, alto y flags);
  - los píxeles en CI8, con cada fila rellenada a 8 B con el índice transparente;
  - una paleta de 256 colores RGBA5551, más las paletas «fantasma» ya calculadas.

  Ocupa 1,34 MiB. Como `.sprite` de `mksprite` serían 3,96 MiB, porque cada uno lleva 648 B fijos, paleta incluida. Además, el formato `.sprite` es interno y versionado. `mksprite` sí conserva bien los índices y la paleta de un PNG indexado (probado), por si hace falta.
- **Animaciones.** Cada fotograma de Theme Hospital es una lista de elementos. En `DATAM`:
  - **elementos en la lista de un fotograma:** mediana 14, p90 28 y máximo 47;
  - **elementos que de verdad se dibujan:** mediana 8, p90 16 y máximo 28. Cada elemento pertenece a una capa (ropa, pelo…) y solo se dibujan los de la capa elegida;
  - **espejo horizontal:** el 29,1 % de los elementos dibujados, ponderado por uso;
  - **semitransparencia al 50 %:** el 13,6 %;
  - **sombras del mapa:** van al 75 %.

  El modo COPY del RDP, el rápido, no admite ni espejo ni transparencias: el hospital se dibuja en el modo estándar de un ciclo. Con un rectángulo con `x0 > x1` sale el espejo (comprobado).
- **Coste en CPU por sprite, en ares:**

  | Cómo | Ciclos de CPU |
  |---|---:|
  | `rdpq_sprite_blit` (sube la paleta en cada llamada) | 4.862–5.743 |
  | `rdpq_tex_blit`, 16 × 13 | 3.497–4.248 |
  | `rdpq_tex_blit` grabado en un bloque `rspq` | 3.642–4.850 (no ahorra) |
  | `rdpq_tex_upload` + `rdpq_texture_rectangle` (APIs estables) | 1.758–2.542 |
  | La misma carga, escrita a mano con comandos del RDP | 1.626–1.634 |
  | Solo el rectángulo, con la textura ya cargada | 173 |

  Las horquillas vienen de compilar varias veces: cambia la disposición del código en la caché. Parte del coste de `rdpq_tex_blit` (unos 240 ciclos) son las funciones anidadas de GCC, que vacían la caché en cada llamada.

  **Decisión: no usar `rdpq_tex_blit` para los elementos de las animaciones.** Primero, la vía estable (cargar la textura y dibujar un rectángulo), medida en el hito 2; la versión a mano solo ahorra entre un 8 y un 36 % más, y obliga a llevar a mano el estado de la TMEM.

  Solo el suelo y sus sombras se pueden grabar en un bloque `rspq`. Las paredes se dibujan intercaladas con humanoides y objetos, en orden de pintor (`th_map.cpp`), y el bloque guarda coordenadas absolutas: hay que regrabarlo al mover la cámara. Si aun así la CPU no llega, el siguiente paso es un comando propio en el RSP que dibuje un fotograma de TH entero.
- **La paleta de las fuentes.** `rdpq_font` sube su propia paleta en la posición 0 de la TLUT, y `rdpq_sprite_blit` y el vídeo hacen lo mismo. Hay que volver a subir la paleta del juego después de cada tanda de texto, o dibujar el texto al final.
- **Efectos de los pacientes.**
  - La radiación hace brillar en verde: en el RDP es multiplicar la textura por un color.
  - La gelatinitis dibuja el sprite en franjas horizontales que ondulan: varios rectángulos por sprite.
- **Fantasmas.** CorsixTH solo recolorea con tablas los objetos y empleados que se están colocando, y las puertas o ventanas no válidas. La ropa y la piel van por capas. En la N64 basta con cambiar la paleta antes de esos elementos: entre 170 y 570 ciclos más cada uno.

### Texto

- `mkfont` convierte fuentes BMFont en formato de texto. Una fuente de TH convertida así (FONT01V) ocupa 6.056 B en CI4 y se dibuja bien con `rdpq_text_print` (comprobado en captura).
- El texto de libdragon va en UTF-8: el conversor pasa los textos de CP437 a UTF-8.
- **Español.** Las fuentes de TH tienen todos los caracteres que usa `LANG-4.DAT`. Les faltan Á, Í, Ó y Ú, que CorsixTH usa 4 veces en su español: o se cambian por A, I y O, o se dibujan esos glifos.
- **Coste.** Una línea de 44 caracteres cuesta 0,29–0,35 ms con `rdpq_text_print`, y 0,17 ms ya maquetada con `rdpq_paragraph`. Una ventana de gestión con 30 líneas serían 8,7–10,6 ms por fotograma con `rdpq_text_print`, y 5,0 ms ya maquetada (más 0,22 ms por párrafo al maquetarla una vez). El texto fijo se maqueta una vez o se dibuja en una superficie aparte.

### Audio

- **Mezclador.** Admite 32 canales como máximo. Mezcla en el RSP, pero la compresión Huffman de `wav64`, activada por defecto, se expande en la CPU.

  | Escenario | CPU |
  |---|---:|
  | 8 efectos VADPCM con Huffman | 16,7 % |
  | 8 efectos VADPCM sin Huffman | 2,96 % (y un 10 % más de cartucho) |
  | Música MIDI con 16 voces | 5,0–5,7 % |
  | Música más 8 efectos, sin Huffman | 9,1 % |

  **Decisión:** `--wav-compress vadpcm,huffman=false`. Queda por medir el códec ULC de libdragon, que promete más velocidad que Opus.
- **Música.** MID64 más SF64. Hace falta `mixer_ch_set_limits` en los canales del sintetizador: sin él, la primera nota aborta.
- **RAM.** Se midieron 643 KiB para todo el audio con el banco TimGM6mb completo, pero no todo es nuevo:
  - 136 KiB son las colas de `rspq`, que `mixer_init` reserva solo si nadie lo ha hecho antes. En el juego ya las reserva `rdpq`: comprobado en `n64/pruebas/audio_init` y en `mixer.c:357`;
  - 20 KiB son los búferes de `audio_init`.

  Las dos cosas ya están en la ROM base de la fase 4. Lo que añaden la música y los efectos son **unos 487 KiB**, y el banco solo se lleva 353. Las 8 piezas de TH usan 28 instrumentos: recortado a ellos, el banco ocuparía unos 101 KiB (estimado, no medido), y la música y los efectos, unos 0,23 MiB. El conversor generará el banco recortado.

### Vídeo

- `fmv_play` reproduce H.264 con su sonido. Antes hay que registrar el códec con `video_register_codec`.
- **Rendimiento.** Con vídeos sintéticos de 320 × 240 a 15 y 25 fps, en ares se mostraron todos los fotogramas, sin saltos. Con contenido real, el vídeo de ejemplo de libdragon (320 × 176 a 24 fps) gasta entre el 32 y el 56 % de la CPU.
- **Memoria.** Mientras suena un vídeo hacen falta entre 1,6 y 2,3 MiB más sobre la pantalla del juego, y 2,8 MiB si `fmv_play` abre la suya. Al terminar se libera todo.
- **Detalles:**
  - el sonido del vídeo va por defecto al canal 0 del mezclador, que es del sintetizador: hay que darle otro canal o parar la música;
  - al terminar, quita cualquier límite de fps que hubiera puesto el juego.
- **Decisión:** los vídeos solo fuera de la partida (intro, fin de nivel), o liberando antes la caché de sprites, sobre la pantalla de 16 bits del juego (`disable_display_init`).
- **Pendiente:** repetirlo con los vídeos reales de TH.

### Mandos y ratón

- `joypad` presenta el ratón de N64 como un mando de estilo `JOYPAD_STYLE_MOUSE`: el stick trae el movimiento relativo, y A y B son los botones.
- ares v148 lo emula, y libdragon lo detectó al conectarlo con la ROM en marcha. Se conecta desde el menú de ares; no hay opción de línea de órdenes.
- **Hay que acumular el movimiento en cada barrido de pantalla.** libdragon lee los mandos en cada uno, y `joypad_poll` solo copia la última lectura. El ratón informa del movimiento desde la lectura anterior, así que un juego que lea a 30 fps perdería la mitad del movimiento.
- El movimiento no se ha probado, porque ares corre sin dispositivo de entrada.

## Método: estructura del motor y pruebas

### Estructura

| Capa | Contenido | Reglas |
|---|---|---|
| `core/` | La simulación: tablas generadas, mapa (casilla de 16 B), A*, animaciones (fotogramas, marcadores, duración), entidades, acciones como máquinas de estado, habitaciones, hospital y economía, mundo y planificador, eventos, guardado | C17 portable, sin libdragon. Solo tipos de `stdint`, enteros y Q16.16. Pools de tamaño fijo recorridos por índice. Sin `malloc` después de iniciar |
| `platform/n64/` | Dibujo, mandos, sonido, sistema de ficheros, temporizador | libdragon |
| `platform/pc/` | `th64sim`: carga un nivel convertido y un escenario, y escribe el mismo volcado que el oráculo de CorsixTH | Sin pantalla; opcionalmente una ventana con SDL2 |
| `tools/conv/` | Conversor: gráficos, sonido, textos, niveles y tablas de reglas | Python, más Lua para leer las definiciones de CorsixTH |
| `tests/` | Pruebas unitarias, huella PC↔N64 y comparación con el oráculo | — |

**Dos generadores de números aleatorios.** CorsixTH usa un único Mersenne Twister, de 2.498 B de estado, para todo:
- la simulación;
- los sonidos de fondo, que se llevan el 17 % de los números;
- el dibujo, que gasta 2 números por fotograma aunque no haya terremoto.

El motor separa un generador de simulación pequeño (xorshift32 o PCG, de 4–8 B, que va en la partida guardada) de otro estético. Así la simulación no depende de cuántos fotogramas se dibujan.

### Qué se genera y qué se escribe

- **Se genera** con el conversor:
  - 126 definiciones: 60 objetos, 23 habitaciones, 34 enfermedades y 9 diagnósticos. Se cargan con `lua5.4` de serie y unos stubs mínimos, sin fallos;
  - 272 marcadores de animación y 3 duraciones ajustadas;
  - `base_config.lua` (1.138 entradas);
  - el aspecto de cada paciente (`initPatient`): 33 de 34 enfermedades se pueden convertir en una tabla, y la de los huesos rotos necesita un caso especial;
  - los textos, como identificadores.
- **Se escribe a mano** el comportamiento:
  - unas 7.800 líneas en esos directorios, el 62 % de ellas en acciones y humanoides;
  - lo que queda de las 24.749 de lógica: el mundo, el hospital y el resto.

  Cada función de C cita el `fichero.lua:línea` de CorsixTH que reproduce. `findObjectsInSquare`, el 47 % del Lua de CorsixTH, se sustituye por contadores.

### Cómo se comprueba

- **Exacta, CorsixTH contra CorsixTH.** Con la misma partida y sin dibujar, CorsixTH es reproducible bit a bit:
  - 3 ejecuciones de 2.000 horas de juego en el hospital lleno, 2 en el mediano, e incluso con Lua 5.5 frente a 5.4;
  - y eso pese a que el orden de `pairs` cambia en cada ejecución (hay 175 `pairs(` en la lógica).

  Sirve para validar cada escenario y el arnés. Dibujando, la simulación se separa a partir de la hora 11.
- **Exacta, motor en el PC contra motor en la N64.** El mismo C da la misma huella en los dos: probado con gcc, clang y la ROM en ares.
- **Exacta en lo que no tiene azar:**
  - duración de las animaciones (5.183 de 5.186 iguales; las 3 distintas son los ajustes conocidos);
  - las definiciones volcadas;
  - las banderas de las 16.384 casillas;
  - distancias de pathfinding;
  - fórmulas de precios y sueldos.
- **Estadística, motor contra CorsixTH.** Paso a paso no se puede: cualquier cambio en el orden de las llamadas al generador separa las partidas en pocas horas.

  Con 10 semillas y 30 días de juego en el hospital lleno:
  - la dispersión es de 9,1 ± 1,5 curados y 16.639 ± 1.410 $ de saldo;
  - con N = 10 se distinguen diferencias de ±1,9 curados (21 %) o ±1.765 $ (10,6 %);
  - cada ejecución cuesta unos 15 s.

  El umbral de aceptación hay que acordarlo.
- **Qué se vuelca en cada caso:**
  - por hora, unos 30 campos (saldo, reputación, curados, muertes, pacientes, colas…), con sus nombres exactos en `hospital.lua` y `world.lua` ya localizados;
  - por día, el balance y el libro de casos de cada enfermedad;
  - una traza de llegadas, diagnósticos, curas, muertes y salidas, que se captura sin alterar la simulación.

**Un problema descubierto: las partidas de referencia de la fase 1 no sirven como oráculo de reglas.**
- En `mediano` no se cura nadie en 2.000 horas de juego: el mismo paciente pasa 300 h en la consulta con certeza de diagnóstico 0.
- En `lleno`, ninguna de las 50 llegadas de un mes llega a diagnosticarse.

Valen para medir carga, que es para lo que se usaron en las fases 1 y 3, pero no para comparar reglas. Hay que crear escenarios propios por hito y validarlos antes de usarlos (curas por día mayores que cero, ninguna consulta atascada). La causa no se ha investigado.

### Orden de trabajo

Primero lo que tiene oráculo exacto; después, lo que necesita uno estadístico.

| Hito | Qué | Cómo se comprueba |
|---|---|---|
| 0 | Conversor: definiciones, `base_config`, aspecto de los pacientes, tablas de animación, hojas de sprites, un mapa de prueba, textos | Oráculos exactos (duraciones, ida y vuelta del JSON); sprites comparados con el decodificador de la fase 2 |
| 1 | La ROM carga los assets y dibuja un sprite con su paleta | Captura de ares frente a un PNG del conversor |
| 2 | Mapa del núcleo y dibujo con scroll | Banderas de las 16.384 casillas frente a CorsixTH; fps en ares |
| 3 | Animaciones y A* compacto | Duraciones; distancias de camino; ciclos por ruta |
| 4 | Bucle mínimo: recepción, consulta, cobro y salida | Estadística con N semillas y traza de eventos, sobre un escenario validado |
| 5 | Medición en la N64 | Ciclos por humanoide y por hora de juego frente a 5,07 M por hora |

Después, hasta tener algo jugable, en este orden:
1. construcción de salas;
2. personal;
3. diagnóstico y tratamiento;
4. economía;
5. confort (sed, aseos, basura y temperatura);
6. investigación;
7. eventos;
8. objetivos del nivel;
9. guardado;
10. ventanas de gestión.

## Fidelidad: CorsixTH frente al original

Respuesta a la pregunta 4 («Supongo que Corsix habrá mejorado, evalúa»): **en parte**. CorsixTH corrige errores del original y reproduce muchas reglas a propósito, contrastadas con el original (las velocidades se midieron en DOSBox). Pero le falta contenido, y algunas fórmulas son suyas.

Las fuentes:
- el changelog de CorsixTH: de sus más de 800 entradas, solo unas 20–30 citan el original, según el criterio de búsqueda;
- 157 commits que lo mencionan;
- los comentarios del código;
- la wiki, contrastada siempre con el código;
- los textos del propio juego (`LANG-0.DAT`), que confirman qué mecánicas existían.

Se han clasificado 71 diferencias:

| Clase | Cuántas | Ejemplos |
|---|---:|---|
| (a) Errores del original que CorsixTH corrige | 9 | La emergencia 9 del nivel 7 trae una enfermedad que aún no existe; criterios del nivel 8 al revés; ADN alienígena desactivado en los niveles 11 y 12; banderas erróneas en casillas del mapa; dos fallos del mapa de la ciudad |
| (b) Del original, que falta o se aproxima | 20 | Hospitales rivales (solo son nombres: el jugador recibe un 25 % fijo de pacientes por un factor de reputación); ratas a tiros; ola de vómitos; 12 categorías de premios; 83 de los 197 campos de los `.SAM` sin leer; calefacción, diagnóstico y formación con fórmulas propias |
| (c) Añadidos de CorsixTH | 18 | Velocidad «Speed Up»; tirar pacientes al suelo con el clic derecho; tres modos de bloquear zonas; construcción libre; comodidades de interfaz |
| (d) Del original, reproducido a propósito | 24 | Sin años bisiestos y 50 horas por día; velocidades; terremotos; VIP; no se gana con préstamos pendientes; cansancio y ascensos del `.SAM` |

**Recomendación: seguir CorsixTH v0.70.1 por defecto**, incluidas sus correcciones y reproducciones, con 6 excepciones:

| # | Excepción | Por qué |
|---|---|---|
| 1 | Las correcciones de los niveles, en el conversor | Llegan ya corregidas: 0 RAM y 0 CPU en la consola. Son 5 ficheros `.level` de CorsixTH (MIT) y la corrección de banderas del mapa |
| 2 | No tirar pacientes al suelo con el clic derecho | No es del original, y en la N64 el clic derecho es el botón B: habría caídas cada vez que se cancela algo sobre un paciente |
| 3 | Quitar «Speed Up» | Va a 444 horas de juego por segundo, 24 veces la Normal; con el presupuesto de la fase 4 quedarían 0,21 M ciclos por hora. Además, Z ya es el puntero rápido |
| 4 | Calefacción con un modelo propio y barato (por sala o repartido entre ticks) | La de CorsixTH ya no es la original («slightly different to the original», según su wiki) y es el 12 % de su CPU. Se conserva lo visible: radiadores, exterior por meses, caras de frío y calor, coste diario |
| 5 | Bloqueo de zonas fijo en el modo 2, el del original | La comprobación recorre el hospital al colocar un objeto: 19 ms en ares para 10.000 casillas, 9 ms con un mapa de bits. Se hace al cambiar de casilla, no en cada fotograma |
| 6 | Rivales como en CorsixTH, sin sus pantallas | El reparto fijo cuesta 0 CPU. Se quitan derivar pacientes, el Salón de la fama y de la vergüenza, la tarta de población y los premios regionales: sin rivales no harían nada |

**Más adelante, opcional:** completar los premios que faltan, porque los valores ya están en los `.SAM`, y la ola de vómitos. No se harán las ratas a tiros, las abducciones ni el multijugador.

**Opciones en la N64:**
- **Reglas**, con los valores por defecto de CorsixTH:
  - acciones con el juego en pausa: no, como el original;
  - conceder las subidas de sueldo al caducar la petición: no;
  - retirar salas destruidas: no;
  - recordar el contenido medio de cada sala: no.
- **Interfaz:** consejero, temblor en los terremotos, colores de temperatura, reloj de 24 h y subtítulos del locutor.
- **Fijas, sin menú:**
  - las tres opciones del ADN alienígena y las fracturas en mujeres, porque a los datos originales les faltan esas animaciones;
  - bloqueo de zonas en el modo 2;
  - sin construcción libre.

La tabla completa de diferencias está en `bench/resultados/fase4/diferencias.tsv`. En la fase 5, cada sistema anotará qué filas sigue y de cuáles se aparta.

## Modo demo sin Expansion Pak

Respuesta a la pregunta 1 («Si es posible como el Perfect Dark, un modo demo sin Expansion Pak»).

**Precedentes.**
- **Perfect Dark:** sin el Pak solo deja el multijugador de 1 y 2 jugadores y los desafíos. Según Wikipedia, que cita la caja, es «aproximadamente el 35 %» del juego.
- **StarCraft 64:** pide el Pak para las misiones de Brood War.

**Detección.** `is_memory_expanded()` de libdragon. En ares se prueba con `tools/run_ares.sh … --no-expansion`, que da 4.096 KiB.

**Contenido: el nivel 1 completo.** Es el nivel de la demo de PC de Bullfrog, y el más pequeño:
- 767 casillas construibles y ninguna parcela que comprar;
- 8 habitaciones y 6 enfermedades;
- sin investigación.

CorsixTH trae ya unas reglas de demo sobre ese mapa (`Levels/demo.level`, MIT), pero son suyas, no las de Bullfrog. El nivel 2 añade investigación, parcelas y otro tipo de paciente.

| Nivel | Casillas iniciales / por comprar | Habitaciones | Enfermedades |
|---|---|---:|---:|
| 1 | 767 / 0 | 8 | 6 |
| 2 | 299 / 579 | 11 | 9 |
| 6 | 648 / 1.062 | 17 | 19 |
| 12 | 301 / 3.697 | 23 | 33 |

**Medidas con 4 MiB, en ares.** Mayor bloque libre al final, sin el código del juego:

| Configuración | Mayor bloque libre |
|---|---:|
| La de la fase 4 (3 framebuffers, 32 kHz, música MIDI con el banco entero) | 2.668 KiB |
| 2 framebuffers, 22 kHz, MIDI | 2.824 KiB |
| 2 framebuffers, música en streaming Opus | 3.120 KiB |
| 2 framebuffers, 22 kHz, música en streaming VADPCM | 3.256 KiB |
| 2 framebuffers, sin música | 3.268 KiB |

Por partes:
- **2 framebuffers en vez de 3:** ahorran 150 KiB.
- **Audio a 22 kHz en vez de 32:** solo 6 KiB.
- **Banco de sonidos completo:** 353 KiB.

**Lo que necesita el nivel 1** (análisis estático de las animaciones que puede usar):
- 402 animaciones;
- 2.046 sprites: 0,47 MiB en CI8, frente a 0,92 MiB de todo lo que usa CorsixTH;
- tablas de animación de 0,20 MiB, frente a 0,48.

Con eso, todos los sprites del nivel caben residentes, sin leer del cartucho mientras se juega.

**Presupuesto de la demo, con 4 MiB:**

| Concepto | Demo (4 MiB) | Juego completo (8 MiB) | Origen |
|---|---:|---:|---|
| Código, datos y pila de la ROM base | 0,39 | 0,40 | Medido |
| Vídeo (2 framebuffers), RDP y audio a 22 kHz | 0,44 | 0,59 | Medido |
| Código del juego | 0,75 | 1,00 | Estimado |
| Música y efectos (banco recortado; en la demo, 8 voces y 4 efectos) | 0,17 | 0,23 | En parte medido |
| Tablas de animación | 0,20 | 0,48 | Estimado (nivel 1) |
| Sprites y bloques, residentes | 0,65 | 1,30 | Estimado (nivel 1) |
| Interfaz | 0,45 | 0,50 | En parte medido |
| Mapa y pathfinding | 0,38 | 0,38 | Diseño |
| Entidades (128 humanoides, 256 objetos) | 0,05 | 0,15 | Diseño |
| Textos de un idioma | 0,10 | 0,10 | Medido |
| Estado de la partida | 0,08 | 0,10 | Estimado |
| **Total** | **3,66** | | |
| **Margen** | **0,34** | | |

- Con música en streaming VADPCM en vez de MIDI, el total baja a 3,48 MiB (margen 0,52).
- Si el código ocupa 1,00 MiB, como estima la fase 4, sube a 3,91 MiB (margen 0,09), y la demo no cierra.

El tope de 128 humanoides deja margen: la partida `mediano`, un hospital mayor que el del nivel 1, tuvo 69 de media.

**Recomendación: no comprometer la demo hasta el hito 5**, cuando se sepa cuánto ocupa el código. Mientras tanto, hacer lo que sale barato y también le sirve al juego completo:
- reservas de memoria (humanoides, objetos, caché de sprites, colas) según `is_memory_expanded()` al arrancar, sin arrays estáticos dimensionados para 8 MiB;
- sprites y animaciones agrupados por nivel en el conversor;
- banco de sonidos recortado;
- pruebas automáticas en ares con y sin `--no-expansion`, que fallen si el heap libre baja de un umbral.

Lo que **no** se hará: imponer al juego completo los topes de la demo, ni partir el juego en módulos cargables para ganar 0,1 MiB. Lo exclusivo de los niveles 2–12 es solo un 10 % del código.

**Coste propio de la demo:**
- la pantalla de elección de modo;
- dos juegos de tamaños de reserva;
- todas las pruebas de memoria por duplicado;
- una partida completa del nivel 1 con 4 MiB en cada versión.

En hardware hace falta una consola sin Expansion Pak (con el Jumper Pak) además de la que lo tiene.

## Lo que cambia en el presupuesto de la fase 4

- **RAM con 8 MiB.** La música y los efectos ocupan más de los 0,20 MiB estimados:
  - 0,47 MiB con el banco de sonidos entero;
  - unos 0,23 MiB con el banco recortado (estimado).

  El presupuesto pasa de 5,20 a 5,23 MiB, con 2,77 de margen; con el banco entero, 5,47 y 2,53. Actualizado en [`04_decision.md`](04_decision.md#presupuesto-de-ram).
- **CPU: el dibujo cuenta.** Un ejemplo con 400 elementos dibujados por fotograma a 30 fps:
  - con `rdpq_tex_blit`, 46 M ciclos por segundo: el 49 % de la CPU;
  - con carga y rectángulo de las APIs estables, 21–30 M: un 22–33 %;
  - escrito a mano, 20 M: el 21 %.

  El audio gasta otro 8–9 % (efectos sin Huffman más MIDI). Sin `rdpq_tex_blit`, con el suelo en bloques `rspq` y el texto ya maquetado, a la simulación le quedan unos 3 M ciclos por hora de juego a velocidad Normal: algo más que los 2,5 M que suponía la fase 4. Cuántos elementos se dibujan de verdad se mide en los hitos 2 y 3.

## Riesgos nuevos o que cambian

| Riesgo | Gravedad | Cómo se reduce | Cuándo se sabe |
|---|---|---|---|
| CPU del dibujo | Alta | Carga y rectángulo con APIs estables, o a mano; el suelo en bloques `rspq`; 20 fps si hace falta; comando propio en el RSP como último recurso | Hitos 2 y 3 |
| Escenarios de comparación: las partidas de referencia no curan a nadie | Media | Escenarios propios validados por hito | Antes del hito 4 |
| Ciclos de ares optimistas | Media | Confirmar en consola con el SummerCart64 | Al tener flashcart |
| Dependencia de APIs preview (mezclador, `wav64`, `mid64`, `sf64`, vídeo) | Media | Commit de libdragon fijado; lista de APIs preview usadas en la integración continua; actualizar solo a propósito | Continuo |
| La demo consume tiempo de la fase 5 | Media | No comprometerla hasta el hito 5; solo lo barato | Hito 5 |
| Glifos del español (Á, Í, Ó, Ú) | Baja | Sustituirlos o dibujarlos en el conversor | Hito 0 |

## Limitaciones

- Todo está medido en ares. No hay flashcart para probar en consola.
- La revisión independiente solo se completó en dos de los cinco temas: herramientas y APIs (`bench/resultados/fase4/construccion/revision_*.json`). En método, fidelidad y modo demo, las cifras se repitieron a mano, pero nadie ha intentado refutar sus recomendaciones.
- El código del motor completo no existe: los tamaños de código (1,00 y 0,75 MiB) son estimaciones, y son lo que decide la demo.
- El banco de sonidos recortado y su RAM (~101 KiB) son un ajuste lineal sobre dos bancos medidos, no una medida.
- El vídeo se midió con contenido sintético.
- La clasificación de fidelidad se apoya en el código, el changelog, los commits y los textos del juego, no en el original jugado en DOSBox. Las fronteras entre clases pueden variar en ±3.
- El análisis de los sprites del nivel 1 es estático: puede faltar alguna animación elegida en tiempo de ejecución, y cuenta todas las variantes de ropa.

## Cómo reproducirlo

Ver [`n64/pruebas/README.md`](../n64/pruebas/README.md) (ROMs de medida) y [`tools/construccion/README.md`](../tools/construccion/README.md) (scripts del PC). Las salidas resumidas están en `bench/resultados/fase4/`.
