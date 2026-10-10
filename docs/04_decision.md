# Fase 4 — Decisión

Fecha: 2026-10-10. Estado: **propuesta, pendiente de revisión con Eduardo**. Según el plan, aquí se para: la fase 5 no empieza hasta que esto se apruebe.

Este documento solo usa cifras ya medidas en las fases 1–3 y una medida nueva (la RAM que deja libre libdragon, más abajo). Cuando una cifra es una estimación, se dice. 1 MiB = 1.048.576 bytes.

## La decisión, en breve

1. **Camino 2: motor propio en C sobre libdragon.** La lógica de CorsixTH se usa como especificación (reglas, fórmulas, tiempos), no como código que ejecutar. Ni CorsixTH tal cual ni un híbrido con Lua caben en la N64 (tabla siguiente).
2. **Expansion Pak obligatorio (8 MiB).** Con 4 MiB el presupuesto no cierra, ni con la caché de sprites más pequeña razonable. Esto es lo primero que tiene que confirmar Eduardo.
3. **320 × 240 a 16 bits, con los gráficos de `DATAM`.** Las ventanas de gestión se reducen a la mitad en el conversor del PC. El RDP dibuja cada sprite como textura CI8; nada de componer el frame por CPU como el port de Wii.
4. **Simulación con algoritmos propios.** CorsixTH gasta el 47 % de su Lua en buscar objetos cercanos y el 12 % de la CPU en recalcular la temperatura de todo el mapa. El motor nuevo lleva contadores incrementales y reparte el trabajo entre ticks. Objetivo de partida: el hospital `lleno` de la fase 1 (304 pacientes, 57 empleados) a velocidad Normal. Si no llega, se recorta en este orden: velocidades altas, luego el máximo de pacientes.
5. **Mandos de N64 como puntero (stick) más botones, y ratón de N64.** Es el esquema del port de Wii, que funcionó sin tocar la interfaz; libdragon admite el ratón de N64.
6. **Los datos los pone cada usuario.** Un conversor en el PC genera los assets de la ROM a partir de su copia de Theme Hospital. No se distribuye ninguna ROM con datos del juego.

## Los números que deciden

| Dato | Valor | Fuente |
|---|---|---|
| Heap vivo de Lua en CorsixTH | 12,7–17,7 MiB (picos de 26–43 MiB) | Fase 1 |
| Datos C++ de CorsixTH en PC | 15,5–18,2 MiB | Fase 1 |
| Lua en la VR4300 frente al PC de medida | ~400 veces más lento (295–650); C, ~227 | Fase 1 (ares) |
| CorsixTH en la N64, hospital lleno, velocidad Normal | **34 veces** la CPU disponible | Fase 3 |
| … si lo caro de Lua costara cero | **11 veces** | Fase 3 |
| … solo la temperatura del C++ actual | 2,6 veces | Fase 3 |
| Casillas del mapa de CorsixTH compiladas para N64 | 84 B × 2 copias × 16.384 = **2,63 MiB** | Fase 3 |
| Partida guardada de CorsixTH | 0,53–1,20 MiB (33–296 KiB con zlib) | Fase 3 |
| Memoria de guardado de un cartucho | 0,5–2 KiB (EEPROM), 32 KiB (SRAM, Controller Pak), 128 KiB (FlashRAM) | — |
| Juego completo en cartucho | 6,4 MiB mínimo; 22,8 con voz y vídeos; 38,2 con todo | Fase 2 |
| **RAM libre tras inicializar libdragon** | **3.077 KiB (4 MiB) / 7.173 KiB (8 MiB)** | Esta fase |

### RAM que deja libdragon (medida nueva)

`n64/rambase` es una ROM de medida, no un prototipo. Inicializa lo que cualquier versión del juego necesita y escribe el heap libre tras cada paso. Ejecutada en ares, con y sin Expansion Pak (`bench/resultados/fase4/rambase.txt`):

| Paso | Heap usado | Libre con 4 MiB | Libre con 8 MiB |
|---|---:|---:|---:|
| Arranque (código + datos + pila fuera del heap: 411 KiB) | 1 KiB | 3.684 KiB | 7.780 KiB |
| Vídeo 320 × 240, 16 bits, 3 framebuffers | +450 KiB | 3.234 KiB | 7.330 KiB |
| RDP (`rdpq`) | +136 KiB | 3.098 KiB | 7.194 KiB |
| Mandos, sistema de ficheros, descompresores aPLib y Shrinkler | +0 | 3.098 KiB | 7.194 KiB |
| Audio a 32 kHz | +20 KiB | 3.077 KiB | 7.173 KiB |
| Mezclador de 16 canales y decodificador Opus | +0 al iniciar (reservan al sonar) | 3.077 KiB | 7.173 KiB |

Esta ROM ocupa 346 KiB entre código, datos y variables (`text`, `data` y `bss`); el código solo, 298 KiB. Un juego entero tendrá bastante más, y el código también vive en la RAM.

## Por qué el camino 2

| | Camino 1: CorsixTH (Lua + C++) en la N64 | Híbrido: Lua para la lógica, C para lo caro | **Camino 2: motor propio en C** |
|---|---|---|---|
| RAM | 12,7–17,7 MiB de Lua + 3,1 MiB de C++ solo en mapa y pathfinding: **no cabe ni en 8 MiB** | Lua sigue en 12,7 MiB: **no cabe** | Estructuras a medida: cabe en 8 MiB (presupuesto abajo) |
| CPU, hospital lleno, Normal | 34 veces la disponible | ≥ 11 veces (con lo caliente gratis) | Hay que medirla; el presupuesto es de 5,07 M ciclos por hora de juego (abajo) |
| Qué se aprovecha | Todo, pero no funciona | La lógica en Lua, más lenta de lo necesario | La lógica como especificación; formatos, animaciones, pathfinding y mapa como algoritmos (fase 3) |
| Esfuerzo | Bajo, pero inútil | Alto: mantener el puente Lua–C y aun así recortar | El mayor: reescribir 24.749 líneas de lógica y la interfaz |

Los dos primeros caminos fallan por memoria antes de llegar a la CPU. El tercero es el único que puede caber, y sus riesgos son de esfuerzo y de CPU, no de imposibilidad.

### Qué es «motor propio» en concreto

- **C sobre libdragon** (rama `preview`, commit fijado en la fase 0): `rdpq` para el dibujo, `mixer` + `wav64` para los efectos y la voz, MID64 + SF64 para la música, `joypad` para mandos y ratón, DFS para los assets.
- **La simulación se reescribe** a partir de la lógica Lua de CorsixTH:
  - pacientes, personal, habitaciones, enfermedades, economía y eventos;
  - con las tablas de animación originales para los tiempos (fase 3: `getAnimLength`);
  - con números en coma fija o `float`, nunca `double`.
- **Se reutilizan algoritmos de CorsixTH** donde valen (fase 3): la carga del mapa, el A* del pathfinding con nodos compactos, la difusión de temperatura (con otra frecuencia) y el Mersenne Twister.
- **Los datos se convierten en el PC** (fase 2): sprites de `DATAM` a CI8, sonido a WAV64 VADPCM, música a MID64 + SF64, textos y mapas a formatos binarios propios, ya en big-endian. La consola no decodifica RLE ni convierte bytes.

## Presupuesto de RAM

Con Expansion Pak. Lo «medido» viene de la tabla anterior o de las fases 2 y 3; lo «estimado» se confirmará en la fase 5.

| Concepto | MiB | Origen |
|---|---:|---|
| Código, datos y pila de la ROM base (fuera del heap) | 0,40 | Medido |
| Vídeo (3 framebuffers), RDP y audio | 0,59 | Medido |
| Código del juego, además del de la ROM base | 1,00 | Estimado (la ROM `luabench`, con un intérprete Lua entero, ocupa 0,31 MiB) |
| Búferes del mezclador, sintetizador MIDI | 0,20 | Estimado |
| Tablas de animación de `DATAM` | 0,48 | Medido (fase 2; la PS1 lo dejó en 0,29) |
| Sprites y bloques de `DATAM` en CI8, todos residentes | 1,30 | Medido (fase 2) |
| Interfaz: panel, reloj, menús, fuentes y una ventana de gestión abierta | 0,50 | Medido en parte (fase 2: 0,29 de panel, reloj, menús y punteros de `DATAM`; 0,06 de fuentes de `QDATAM`; 0,10 la mayor ventana reducida; el resto, fuentes de `QDATA`) |
| Mapa: 16.384 casillas de 16 B | 0,25 | Diseño (CorsixTH: 2,63 MiB) |
| Pathfinding: nodos de 8 B | 0,13 | Diseño (CorsixTH: 0,44 MiB) |
| Entidades: 384 humanoides × 256 B, 512 objetos × 64 B, basura, colas | 0,15 | Diseño |
| Textos de un idioma | 0,10 | Medido (fase 2: 93–109 KiB) |
| Estado de la partida (hospital, economía, investigación, eventos) | 0,10 | Estimado |
| **Total** | **5,20** | |
| **Margen sobre 8 MiB** | **2,80** | Fragmentación, picos, cachés de sonido |

**Con 4 MiB no cierra.** Los 5,20 MiB habría que bajarlos así:
- doble buffer en vez de triple (−0,15);
- una caché de sprites de 0,5 MiB en vez de todos residentes (−0,80);
- las tablas de animación recompactadas como en la PS1 (−0,19);
- la interfaz en 0,35 MiB, con las ventanas de gestión dentro de la caché de sprites (−0,15).

Aun así quedarían 3,91 MiB de 4: unos 90 KiB de margen, y con el código sin medir. Además, la caché de sprites leería del cartucho mientras se juega. Cualquier desvío rompe un presupuesto así: no se recomienda como objetivo.

## Presupuesto de CPU

- **Cuánta hay.** La VR4300 corre a 93,75 MHz. A velocidad Normal el juego simula 18,5 horas por segundo, así que **cada hora de juego dispone de 5,07 millones de ciclos** para todo: simulación, preparar el dibujo, audio y mandos. Si la simulación se queda con la mitad, son unos 2,5 M ciclos por hora: unos 7.000 por humanoide en el hospital lleno.
- **Cuánto gasta CorsixTH.** 24,6 millones de instrucciones x86 por hora en ese hospital (fase 3), unas 68.000 por humanoide. Frente a 2,5 M ciclos son 10 veces más, y la VR4300 no ejecuta una instrucción por ciclo cuando falla la caché (unos 70 ciclos por fallo, fase 1). El motor nuevo tiene que hacer el mismo trabajo con **unas 10–20 veces menos instrucciones**.
- **Por qué parece alcanzable:**
  - se pierde el intérprete: el 76,5 % del tiempo de CorsixTH es la VM de Lua;
  - el 47 % de lo que ejecuta esa VM es una búsqueda que se puede sustituir por contadores;
  - el 12 % de la CPU es una temperatura que se puede recalcular por habitación o por partes.

  El original corría en PC con DOS y en una PlayStation a 33,9 MHz. Es una estimación de orden de magnitud: lo decide el hito 5 de la fase 5.

## Recortes de diseño

| Qué | Decisión | Por qué |
|---|---|---|
| Resolución | 320 × 240, 16 bits por píxel, triple buffer | `DATAM` está hecho para esa escala (fase 2); 0,45 MiB de framebuffers |
| Gráficos | `DATAM` para el juego y el panel; ventanas de gestión de `QDATA` reducidas a la mitad; fuentes de `QDATAM` | Fase 2; la PS1 también rehízo sus pantallas a 320 de ancho |
| Tamaño del hospital | Los mapas originales enteros (128 × 128), sin recortar | Con casillas de 16 B el mapa ocupa 0,25 MiB |
| Pacientes y personal simultáneos | Capacidad fija de 384 humanoides y 512 objetos; objetivo de rendimiento, el hospital `lleno` (361 humanoides) | El límite real lo pone la CPU; se ajusta con la medida del hito 5 |
| Velocidades del juego | Se mantienen las cinco; «Max speed» y «And then some more» se limitan si la CPU no llega | A velocidad Normal hay 5,07 M ciclos por hora; con «Max speed» (27,8 h/s), 3,4 M; con «And then some more» (55,6 h/s), 1,7 M |
| Partidas guardadas | Formato binario propio, objetivo ≤ 32 KiB (SRAM o Controller Pak) y como mucho 128 KiB (FlashRAM) | Las de CorsixTH ocupan 0,53–1,20 MiB |
| Idiomas | Inglés y español, con voz | 9,8 MiB de sonido (fase 2); cada idioma más suma unos 3,5 MiB |
| Vídeos | Opcionales, en H.264 (8 MiB); se deciden al final, según el sitio y el rendimiento | Fase 2 |
| Fuera | Multijugador, hospitales rivales con IA, editor de mapas, niveles extra | Ya fuera de alcance en el plan |

## Controles

Propuesta para el mando de N64, a validar en el prototipo. La interfaz de Theme Hospital está pensada para ratón, así que el mando hace de ratón y unos botones dan atajos, como en el port de Wii.

| Mando de N64 | Función |
|---|---|
| Stick | Mueve el puntero, con aceleración |
| A | Clic izquierdo: seleccionar, colocar, pulsar botones |
| B | Clic derecho: cancelar, coger, girar (lo que haga el botón derecho en cada sitio) |
| Botones C | Desplazar la vista del hospital (arriba, abajo, izquierda, derecha) |
| Cruceta | Desplazar la vista (alternativa) o recorrer botones y menús |
| Z (mantenido) | Puntero rápido |
| R | Abrir y cerrar el panel inferior |
| L | Ir al último aviso (fax, mensaje) |
| Start | Pausa y menú del juego |

**Ratón de N64: sí.** libdragon lo lee como un mando más (`JOYPAD_STYLE_MOUSE`), y con una interfaz de ratón el coste es pequeño: un movimiento relativo para el puntero y dos botones. Es la forma más cercana al original.

## Riesgos abiertos

| Riesgo | Gravedad | Cómo se reduce | Cuándo se sabe |
|---|---|---|---|
| **La simulación no llega a velocidad Normal en el hospital lleno** | Alta | Algoritmos incrementales; repartir el trabajo entre ticks; recortes del apartado anterior | Hitos 4 y 5 de la fase 5 |
| **Esfuerzo.** Hay que reescribir 24.749 líneas de lógica y rehacer 19.512 de interfaz | Alta | Bucle mínimo primero (hito 4); avanzar por sistemas, no por pantallas | A lo largo de la fase 5 |
| Fidelidad de las reglas: CorsixTH no es idéntico al original (el autor del port de Wii lo abandonó por eso) | Media | Tomar CorsixTH como referencia y anotar las diferencias conocidas | Al implementar cada sistema |
| Relleno del RDP con muchos sprites solapados a 320 × 240 | Media | Texturas CI8 o CI4, recortes por casilla, medir en el hito 2 | Hitos 2 y 3 |
| Legibilidad de las ventanas de gestión a 320 × 240 | Media | Retocar en el conversor o rehacer la maquetación, como la PS1 | Al convertir la interfaz |
| Formato de guardado en 32–128 KiB | Media | Guardar solo el estado, sin animaciones ni cachés | Al implementar el guardado |
| ares frente a la consola real (sin SummerCart todavía) | Media | Las ROM escriben sus medidas en pantalla para leerlas en la consola | Cuando haya flashcart |
| Licencias: datos del juego, SoundFont (TimGM6mb es GPL-2) y `wii_vm.c` (todos los derechos reservados) | Media | Conversor para que cada uno use sus datos; SoundFont compatible; no se usa nada de `wii_vm` | Antes de publicar nada |
| Cambios en la rama `preview` de libdragon | Baja | Commit fijado en la fase 0; actualizar solo a propósito | Continuo |
| Rendimiento de H.264 para los vídeos | Baja | Los vídeos son opcionales | Al final |

## Cambios al plan de la fase 5

- **Hito 0 nuevo: el conversor.** Antes del hito 1, un conversor en el PC (Python, a partir de `tools/datos/`) genera en big-endian:
  - las hojas de `DATAM` en CI8 con su paleta;
  - las tablas de animación;
  - un mapa de prueba.

  El hito 1 («la ROM carga los datos y muestra un sprite con su paleta») carga esos assets y no los originales.
- **Hito 5:** además de RAM libre, tiempo por tick y fps con 50 pacientes, medir los ciclos por humanoide y por hora de juego, para extrapolar al hospital lleno con el presupuesto de 5,07 M ciclos por hora.

## Preguntas para Eduardo

1. ¿**Expansion Pak obligatorio**? Es la recomendación. ¿Tienes uno para probar en tu N64?
2. **Idiomas:** ¿inglés y español con voz, o solo uno?
3. **Vídeos:** ¿se intentan (8 MiB y algo de riesgo) o se dejan fuera desde el principio?
4. **Referencia de reglas:** ¿CorsixTH tal como está, o corregir hacia el original cuando se sepa que difieren?
5. **Controles:** ¿te encaja el esquema propuesto? ¿Tienes un ratón de N64 para probarlo?

Con las respuestas, y si apruebas el camino 2, empieza la fase 5 por el hito 0.
