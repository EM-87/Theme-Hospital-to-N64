# Fase 6 — Juego base: el nivel 1 jugable

Fecha: 2026-10-27. Estado: **plan**. Eduardo dejó elegir el orden ([abajo](#el-orden-la-interfaz-primero)): la interfaz va primero.

**Objetivo: el nivel 1 de Theme Hospital, de principio a fin, en la ROM.**
- Se empieza con el edificio vacío y 40.000 $.
- Se construye, se contrata y se cura.
- Se gana o se pierde con los objetivos del nivel.
- Con el mando de N64 o el ratón, en inglés y en español, y guardando la partida.

Las reglas siguen siendo las de CorsixTH v0.70.1, con las 6 excepciones de la fase 4 ([`04_construccion.md`](04_construccion.md#fidelidad-corsixth-frente-al-original)).

## Lo que trae el nivel 1

Censo de CorsixTH al cargar el nivel 1 (`FULL01.SAM` y `base_config.lua`), con [`censo_nivel.lua`](../bench/scenarios/censo_nivel.lua):

| Qué | En el nivel 1 |
|---|---|
| Salas (8) | Consulta (GP) 2.500 $, psiquiatría 2.500, sala de hinchado 4.000, farmacia 1.500, sala de diagnóstico general 1.000, enfermería 2.000, aseos 1.500 y sala de personal 1.500 (precios con sus objetos). Todas disponibles desde el principio |
| Objetos de pasillo (7) | Mostrador de recepción 150 $, banco 40, máquina de bebidas 500, extintor 25, radiador 20, planta 5 y papelera 5 |
| Enfermedades (6) | Cabeza hinchada (sala de hinchado), invisibilidad, resfriado poco común, almorranas apiladas y los «squits» (farmacia), personalidad televisiva múltiple (psiquiatría). Solo la invisibilidad empieza descubierta. 4 de las 6 son contagiosas |
| Personal (4 tipos) | Médicos, enfermeras, bedeles y recepcionistas. Al empezar hay 8, 8, 3 y 8 candidatos; el nivel cambia esas cifras en los meses 3, 5, 9, 13 y 15 |
| Para ganar | Reputación 300, saldo 1.000 $, el 40 % curados, 10 curados y el hospital valorado en 55.000 $ |
| Para perder | Reputación por debajo de 200, saldo por debajo de −20.000 $ o más del 50 % de muertos |
| Eventos | Visitas de VIP (cada 120 a 180 días) y epidemias. Sin emergencias, terremotos ni investigación |

**Qué tiene ya el motor:**
- recepción, consulta, cobro y salida;
- el médico, con su cansancio y lo que aprende;
- las llamadas de personal para la consulta;
- las necesidades de los pacientes, sin aseos, bebidas, plantas, bedel ni muerte.

El núcleo cita 83 funciones de CorsixTH y 15 de sus acciones: unas 3.800 líneas de Lua. Lo que falta para el nivel 1:

| Parte | Líneas de Lua en CorsixTH (sin comentarios) |
|---|---:|
| Las 7 salas que faltan | 796, más `room.lua` (892) |
| Enfermera y bedel | 209, más las acciones que usan |
| Las 17 acciones de humanoide que faltan | 1.699 |
| Epidemias y VIP | 534 y 413 |
| Interfaz que usa el nivel 1: base de ventanas, panel, construir, personal, avisos, ventanas de gestión, menús y opciones | 14.724 de las 19.830 de toda la interfaz |

La interfaz es casi todo el trabajo. Por eso el orden importa.

## El orden: la interfaz primero

Había dos órdenes posibles:
- **A. La interfaz primero** y, después, la simulación que falta del nivel 1.
- **B. Toda la simulación del nivel 1 primero**, comparada con CorsixTH en el PC, y la interfaz al final.

Se elige **A**:
- **los controles son lo último del diseño sin probar.** El puntero con el stick y los botones de la fase 4 solo se pueden juzgar jugando;
- **la ROM se puede jugar pronto.** Desde el hito 6.3, Eduardo puede probarla en un emulador con mando, sin flashcart;
- **la simulación no pierde nada.** Se sigue comparando con CorsixTH en el PC, con escenarios creados con el constructor del arnés, como en los hitos 4 y 5.

Esto cambia el orden que proponía la fase 4 ([`04_construccion.md`](04_construccion.md#orden-de-trabajo)), que dejaba las ventanas de gestión para el final.

## Las órdenes del jugador

**La interfaz no toca la simulación: le manda órdenes.** Construir una sala, colocar un objeto, contratar, despedir, contestar un fax, pedir un préstamo… Cada orden lleva su tick y el núcleo la aplica al empezarlo. Así:
- **la ROM graba las órdenes de una partida**, y `th64sim` las repite y tiene que dar la misma huella. La comprobación exacta entre la N64 y el PC sigue valiendo con un jugador de verdad;
- **las pruebas pueden jugar solas:** una lista de órdenes escrita a mano juega una partida en el PC y en la ROM;
- **las mismas órdenes se pueden dar en CorsixTH** con el constructor del arnés ([`bench/lib/builder.lua`](../bench/lib/builder.lua)), que pasa por las mismas validaciones que su interfaz. Así se comparan una a una las que se aceptan y las que se rechazan.

## Hitos

Cada hito se cierra como en la fase 5: un commit, una captura de ares y sus cifras en `docs/06_juego_base.md`.

| Hito | Qué | Cómo se comprueba |
|---|---|---|
| **6.1** Puntero y panel | Puntero con el stick (con aceleración, y Z para ir rápido), A y B como clics, la vista con los botones C, y el ratón de N64. La base de ventanas y el panel inferior de `DATAM`: botones, dinero, fecha, mensajes y descripciones de los botones | Capturas iguales píxel a píxel al rasterizador de referencia, como el mapa. El panel y el puntero cuestan como mucho 1,5 ms por fotograma |
| **6.2** Construir | Amueblar el pasillo, el plano de una sala (arrastrar, puertas y ventanas), colocar y girar objetos, editar y vender. Las validaciones de CorsixTH, con el bloqueo de zonas en el modo 2 | Una batería de órdenes de construir, aceptadas y rechazadas igual que en CorsixTH, una a una. Las banderas de las 16.384 casillas iguales después de construir. Lo cobrado, igual al dólar |
| **6.3** Personal y avisos: la primera ROM jugable | Contratar y colocar personal, cogerlo y soltarlo. Las ventanas de persona y de cola, los faxes, los mensajes y el consejero, la pausa y las velocidades | Construir a mano, desde la ROM, el hospital del hito 4 y jugarlo. Grabar las órdenes y repetirlas en el PC con la misma huella |
| **6.4** Las salas del nivel 1 | Las 7 salas que faltan, el diagnóstico completo, la curación y la muerte. La enfermera y el bedel (limpiar, regar y reparar), los aseos, la máquina de bebidas y las plantas | Un escenario del nivel 1 completo, creado en CorsixTH: 20 semillas, con el criterio acordado (±15 % en curados y ±7,5 % en saldo), más reputación y muertes |
| **6.5** Economía, gestión y objetivos | El banco, la política del hospital, la gestión de personal, el libro de casos (precios), el mapa de la ciudad, los informes, el valor del hospital, ganar y perder, VIP y epidemias | Las fórmulas de interés, valor y precios, exactas. El resto, estadístico, con las mismas 20 semillas. Legibilidad a 320 × 240: capturas para Eduardo |
| **6.6** Guardar y cargar | Formato propio, solo con el estado de la partida: ≤ 32 KiB si es posible (SRAM o Controller Pak) y ≤ 128 KiB como mucho (FlashRAM) | Guardar, cargar y seguir da la misma huella que no guardar. El tamaño se mide con el hospital del hito 5 |
| **6.7** Menús, opciones y sonido | Menú principal, partida nueva, las opciones acordadas en la fase 4, inglés y español, el locutor con subtítulos y los efectos de sonido en la partida | Los textos de los dos idiomas, sin cadenas que falten. Las opciones, en la partida guardada |
| **6.8** El nivel 1 entero | Jugar el nivel 1 hasta ganarlo, con una lista de órdenes y en la ROM | Gana en la ROM y en el PC con la misma huella. Memoria, CPU y fotogramas con el hospital lleno del final, frente a los presupuestos de la fase 4 |

**Presupuestos:**
- **CPU.** A velocidad Normal y con el hospital del hito 5, el fotograma cuesta hoy 14,1 ms de los 33,3. Con una ventana de gestión abierta tiene que seguir a 30 fps sin saltos.
- **RAM.** La fase 4 reservaba 0,50 MiB para la interfaz: el panel, el reloj, los menús, las fuentes y una ventana de gestión abierta. Hoy quedan 3,8 MiB de heap libres con 8 MiB.

## Lo que no entra en la fase 6

- **Lo que el nivel 1 no tiene:** investigación, formación, quirófano, emergencias, terremotos y los niveles 2 a 12.
- **Los vídeos.** Van fuera de la partida, así que pueden esperar a la fase siguiente.
- **Lo que se dejó para el final:**
  - los cambios en libdragon;
  - el modo demo sin Expansion Pak;
  - los deseos de Eduardo: girar la cámara, salas con esquinas interiores, pediatría y maternidad.

## Riesgos

| Riesgo | Cómo se reduce | Cuándo se sabe |
|---|---|---|
| **El puntero con el stick resulta incómodo** | Aceleración ajustable, atajos con la cruceta y los botones, y el ratón de N64. Eduardo lo prueba en un emulador con mando | Hito 6.3 |
| **Las ventanas de gestión no se leen a 320 × 240** | Las ventanas de `QDATA` reducidas a la mitad; si no se leen, se rehace la maquetación, como hizo la PS1 | Hito 6.5 |
| **La partida guardada no cabe en 32 KiB** | Guardar solo el estado. La temperatura (96 KiB en RAM) cambia despacio de una casilla a otra y debería comprimirse bien | Hito 6.6 |
| **El esfuerzo:** unas 14.700 líneas de interfaz y 4.500 de lógica | Se avanza por lo que pide el nivel 1, no por pantallas sueltas | A lo largo de la fase |
| **ares frente a la consola real:** todo sigue medido en ares | Las ROM de medida escriben en pantalla. Se prueban en la consola cuando llegue el flashcart | Cuando llegue |

## Lo que hará falta de Eduardo

- **En el hito 6.3:** probar la ROM en un emulador con mando y decir qué tal va el puntero. Además, decidir si «acciones en pausa» y «recordar el contenido de las salas» vienen activadas por defecto (pendiente de la fase 4).
- **En el hito 6.5:** mirar las capturas de las ventanas de gestión y decir si se leen.
- **Cuando llegue el flashcart:** pasar las ROM de pruebas, de medida y normal en la consola.
