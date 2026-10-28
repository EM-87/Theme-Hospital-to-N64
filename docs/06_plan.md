# Fase 6 — Juego base: el nivel 1 jugable

Fecha: 2026-10-27; revisado el 2026-10-28 con lo que decidió Eduardo tras ver el hito 6.2 ([abajo](#lo-que-decidió-eduardo-tras-el-hito-62)). Estado: **en curso**: hitos 6.1 y 6.2 hechos ([`06_juego_base.md`](06_juego_base.md)); el siguiente es el 6.2b, el sol fijo. Eduardo dejó elegir el orden ([abajo](#el-orden-la-interfaz-primero)): la interfaz va primero.

**Objetivo: el nivel 1 de Theme Hospital, de principio a fin, en la ROM.**
- Se empieza con el edificio vacío y 40.000 $.
- Se construye, se contrata y se cura.
- Se gana o se pierde con los objetivos del nivel.
- Con el mando de N64 o el ratón, en inglés y en español, y guardando la partida.
- Con las cuatro vistas, como en RollerCoaster Tycoon, y salas de cualquier forma. Eduardo pidió adelantar estos dos deseos a esta fase (2026-10-27).

Las reglas siguen siendo las de CorsixTH v0.70.1, con las 6 excepciones de la fase 4 ([`04_construccion.md`](04_construccion.md#fidelidad-corsixth-frente-al-original)). Pero el proyecto es **Theme Hospital 64, no CorsixTH 64** (Eduardo, 2026-10-28): CorsixTH es la referencia de código y se quedan sus arreglos, pero donde se aparta del original, como en las ratas, manda el original ([abajo](#theme-hospital-64-no-corsixth-64)).

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
- **la ROM se puede jugar pronto.** Desde el hito 6.4, Eduardo puede probarla en un emulador con mando, sin flashcart;
- **la simulación no pierde nada.** Se sigue comparando con CorsixTH en el PC, con escenarios creados con el constructor del arnés, como en los hitos 4 y 5.

Esto cambia el orden que proponía la fase 4 ([`04_construccion.md`](04_construccion.md#orden-de-trabajo)), que dejaba las ventanas de gestión para el final.

## Las órdenes del jugador

**La interfaz no toca la simulación: le manda órdenes.** Construir una sala, colocar un objeto, contratar, despedir, contestar un fax, pedir un préstamo… Cada orden lleva su tick y el núcleo la aplica al empezarlo. Así:
- **la ROM graba las órdenes de una partida**, y `th64sim` las repite y tiene que dar la misma huella. La comprobación exacta entre la N64 y el PC sigue valiendo con un jugador de verdad;
- **las pruebas pueden jugar solas:** una lista de órdenes escrita a mano juega una partida en el PC y en la ROM;
- **las mismas órdenes se pueden dar en CorsixTH** con el constructor del arnés ([`bench/lib/builder.lua`](../bench/lib/builder.lua)), que pasa por las mismas validaciones que su interfaz. Así se comparan una a una las que se aceptan y las que se rechazan.

## Las cuatro vistas

Eduardo eligió girar con sustitutos donde falta arte (2026-10-27). Lo que hay en los datos (inventario con [`orientaciones.lua`](../bench/scenarios/orientaciones.lua)):

| Qué | Al girar |
|---|---|
| Personas | Ya andan en las cuatro direcciones en el juego original: dos dibujadas y dos en espejo. Sirven en todas las vistas |
| Paredes de las salas | Van por bordes, con cara interior y exterior. En cada vista se eligen igual que en la original |
| Objetos que se colocan en las cuatro orientaciones (13): bancos, sillas, mesas, camas, sofás, la recepción, el radiador, el extintor, la planta, la máquina de bebidas… | Se colocan en las cuatro, pero casi ninguno tiene espalda dibujada: el sur es el este en espejo y el oeste, el norte en espejo. Solo la máquina de bebidas tiene las cuatro dibujadas, y el extintor tres. El espejo cambia de lado lo que no es simétrico, como el ordenador del escritorio (corregido el 2026-10-28; ver [el arte nuevo](#arte-nuevo-las-espaldas-con-ia)) |
| Objetos que solo se colocan de cara a la cámara (40) | Su espalda nunca se dibujó, ni la de la gente usándolos: 406 animaciones de uso con 6.584 fotogramas, más 92 fotogramas en reposo. En las 8 salas del nivel 1 son 13 objetos, con 129 animaciones y 2.613 fotogramas: el váter, el lavabo, el diván y el sillón del psiquiatra, la máquina de hinchar, el armario de la farmacia, el carro de paradas, el biombo, la mesa de billar… |
| Suelos y decorado del mapa | El nivel 1 usa 37 suelos distintos y 31 piezas en las capas de pared: 12 son paredes exteriores, con sus caras y sus ventanas, y 19 son decorado (setos, vallas…). Los que tienen dirección, como bordillos o líneas, necesitan su pieza girada. Hay que clasificarlos uno a uno |
| Sombras | CorsixTH las calcula a partir de las paredes. El hito 6.2 las recalcula con las paredes de cada vista, así que la luz gira con la cámara; Eduardo quiere el sol quieto en el mundo, y lo arregla el hito 6.2b ([abajo](#el-sol-fijo-hito-62b)) |

**Cómo se hace.** El núcleo no sabe nada de la cámara, como se decidió en la fase 4. La vista gira el mundo antes de dibujar: cada casilla, cada pared (con su cara) y cada objeto (con su orientación) pasan a la vista girada, y desde ahí se dibuja con el mismo orden que CorsixTH. El puntero hace el camino inverso para saber qué casilla señala.

**Los sustitutos.**
- Un objeto que quedaría de espaldas se dibuja de frente, y quien lo usa, con la animación de frente: puede verse en el lado equivocado del objeto.
- El conversor aceptará hojas de sprites propias, como ya preveía la fase 4. Si alguien dibuja las espaldas, se añaden sin tocar el motor.
- El inventario da la lista exacta de lo que falta.

**Cómo se comprueba.**
- La vista original sigue igual píxel a píxel: las comparaciones de `motor_check.sh`.
- Una vista girada es el mismo hospital girado, y se puede montar en CorsixTH: el mapa con las casillas, las paredes y los objetos girados, donde exista esa orientación. Su captura se compara píxel a píxel con la del motor.
- Donde hay sustitutos no hay con qué comparar: capturas para Eduardo.

## Salas de cualquier forma

La regla es de Eduardo (2026-10-27): **una sala vale si tiene al menos tantas casillas como el cuadrado mínimo de su tipo, con la forma que se quiera.** En el nivel 1 son:
- 16 casillas (4 × 4): la consulta, la sala de hinchado, la farmacia, los aseos y la sala de personal;
- 25 (5 × 5): la psiquiatría y la sala de diagnóstico;
- 36 (6 × 6): la enfermería.

Además:
- **No hace falta arte nuevo.** En CorsixTH, con las paredes azules, una sala en L quitando cualquiera de sus cuatro cuadrantes tiene las esquinas bien casadas ([`salas_en_l.lua`](../bench/scenarios/salas_en_l.lua)). Quedan por mirar los otros 3 colores de pared, las ventanas y las puertas junto a una esquina.
- **El precio no cambia.** En CorsixTH una sala cuesta lo mismo sea del tamaño que sea: el precio de su tipo más el de sus objetos.
- **Tiene que ser de una sola pieza y sin huecos.** Un hueco sería un trozo de pasillo encerrado al que no se puede llegar.
- **En el núcleo, la sala pasa a ser un conjunto de casillas** (una máscara sobre su rectángulo), como preveía la fase 4. Lo que en CorsixTH usa el ancho y el alto pasa a contar casillas o a mirar la máscara.
- **Las salas rectangulares siguen siendo exactamente las de CorsixTH**, con los mismos números aleatorios: para ellas, la máscara no cambia nada. Las demás formas no tienen con qué compararse.

## Lo que pidió Eduardo el 2026-10-28

**Un menú en la ROM de prueba.** Hoy la ROM abre directamente el hospital del escenario. El menú dejará elegir qué abrir y, durante la partida, volver a él:
- el hospital del hito 4 o el del hito 5;
- el visor de niveles;
- las vistas.

Va con el hito 6.2, para poder probar las vistas giradas. El menú principal del juego sigue en el hito 6.8.

**El modo sin Expansion Pak se mantiene**, aunque sea a costa de recortar. El hito 5 recomendaba no hacerlo: con lo medido y el código proyectado, la demo con 4 MiB sumaba unos 4,0 MiB ([`05_prototipo.md`](05_prototipo.md#tamaño-del-código-y-modo-demo)). Hoy, con 4 MiB, la ROM se para al cargar las reglas: tras las hojas y las animaciones quedan 250 KiB de heap, y las definiciones piden 331 KiB. Eduardo acepta estos recortes en la demo:

| Recorte | Lo que ahorraría, a primera vista |
|---|---:|
| Un modelo de temperatura más barato que el de CorsixTH, por sala o a menos resolución | Unos 100 KiB: hoy son 96 KiB por casilla y 4 KiB de máscaras |
| Sin música | Unos 130 KiB: el banco, el sintetizador y una pieza (hito 5) |
| Menos texto: solo las cadenas que se usan y menos fuentes | Unos 150 KiB: los textos en inglés ocupan hoy 257 KiB, frente a los 100 de la fase 4 |
| **Total** | **unos 0,38 MiB**: de unos 4,0 a unos 3,6 MiB |

A eso se suman las reglas recortadas al nivel 1 (0,10 MiB en lugar de 0,33) y las hojas y animaciones solo del nivel 1, que ya contaba el hito 5.

**Cómo se hace:**
- desde el hito 6.2, cada hito apunta cuánta memoria pide con 8 MiB y cuánto pediría la demo;
- el trabajo de que quepa va en un hito propio al final de la fase (el 6.10), antes de los cambios en libdragon;
- los recortes van solo en la demo: el juego con Expansion Pak sigue como CorsixTH.

## Lo que decidió Eduardo tras el hito 6.2

Eduardo miró las capturas de las cuatro vistas y respondió el 2026-10-28. Las cifras de esta sección salen de los datos del juego: [`tools/arte`](../tools/arte/README.md) para el arte, y los gráficos del CD de PS1 leídos solo como ficheros de datos.

### El sol fijo (hito 6.2b)

**El sol de TH está al suroeste**, y Eduardo lo vio enseguida: «dan la sensación de ser las 2 o 3 de la tarde». En los datos:
- las paredes de borde oeste proyectan sombra hacia el este y hacia el norte (`update_shadows` de CorsixTH);
- las piezas de borde norte están pintadas más claras que las de borde oeste: 169 frente a 129 de brillo medio en las azules (bloques 86 y 87), y 165 frente a 128 en las de ventana (94 y 95).

El hito 6.2 recalculó las sombras con las paredes de cada vista, así que **la luz giraba con la cámara**. Eduardo quiere el sol quieto en el mundo. Se puede hacer sin dibujar nada:
- **Sombras.** Se calculan en el mundo, como en TH, y en cada vista se dibuja su forma girada. Las del suelo son triángulos y medios rombos de la casilla, así que sus versiones giradas se generan exactas. La sombra sobre una pared solo se dibuja si se ve la cara en la que cae.
- **Paredes.** Cada pieza norte espejada tiene la forma de la oeste con la luz de la norte, y al revés. En cada vista se elige la pieza normal o la espejada según la cara que se ve mire al sol o no. Las mitades de las ventanas se cambian como ya se hace al girar.

En cada vista se ven las caras que miran al sur y al este de la vista. Con el sol al suroeste:

| Vista (giro de la cámara) | Caras que se ven (en el mundo) | Paredes |
|---|---|---|
| 0, la original | sur (al sol) y este (a la sombra) | Como en TH |
| 1 | este y norte, a la sombra | Todas a la sombra |
| 2 | norte (a la sombra) y oeste (al sol) | Al revés que en TH |
| 3 | oeste y sur, al sol | Todas al sol |

Si la vista 1 queda demasiado oscura, se prueba un tono intermedio. Los objetos y las personas llevan su luz pintada; con ellos no se puede hacer nada sin arte nuevo, pero se nota mucho menos que en las paredes.

**Cómo se comprueba.**
- La vista original, igual píxel a píxel.
- En las giradas, el oráculo de CorsixTH sigue valiendo para suelos, decorado y objetos. Las paredes también, poniendo en CorsixTH la pieza espejada que toca. Para las sombras ya no sirve, porque CorsixTH las calcula para su propia vista: se comprueba que las casillas con sombra son las mismas en las cuatro vistas.

**Lo que queda para más adelante**, aunque Eduardo bromeó con pedirlo: la sombra según la hora, el día y la noche, las estaciones, el clima… y hasta el cambio de época histórica que, según Eduardo, llegó a plantearse para el original.

### Arte nuevo: las espaldas, con IA

**Lo que falta** ([`espaldas.py`](../tools/arte/espaldas.py)):
- 37 objetos solo tienen norte y este, y 3 más solo norte. Contando a la gente que los usa, son **378 animaciones, 5.711 fotogramas y 2.322 sprites distintos** en `DATA`.
- Hay de todo:
  - el váter son 459 sprites, porque cada tipo de paciente se sienta en él;
  - el fabricante de gelatina, 473;
  - la máquina de hinchar, 285;
  - el armario de la farmacia, la papelera o el negatoscopio, 1 o 2.
- Además, el espejo de los objetos de cuatro orientaciones cambia de lado lo que no es simétrico. Se nota en el escritorio, cuyo ordenador cambia de lado en dos de las cuatro vistas, y probablemente en el mostrador de recepción. Lo confirmarán las capturas.

**Eduardo decide usar IA.** Los sprites de TH no son pixel art dibujado a mano: son renders 3D reducidos a una paleta de 256 colores. Por eso se proponen dos caminos:
- **3D, para los objetos sin nadie usándolos.** Se reconstruye el objeto a partir de sus vistas norte y este con un modelo de imagen a 3D: Hunyuan3D admite varias vistas; TRELLIS, una; también hay servicios como Meshy o Tripo. Después se renderiza en Blender con la cámara de TH (2:1) y luz del suroeste, y el conversor lo reduce a la paleta del juego. Da la geometría y la quiralidad bien, y las dos resoluciones a la vez.
- **2D, para la gente y los retoques.** PixelLab gira sprites a 4 u 8 direcciones, tiene modo isométrico, referencia de estilo y API; Retro Diffusion trabaja dentro de Aseprite y también tiene API.

**Cómo se trabaja.**
- La prueba: el escritorio, el armario de la farmacia y el váter en reposo, por los dos caminos. Eduardo elige con las capturas en el motor.
- La generación se hace fuera de este contenedor, que no tiene GPU. Con una clave de API en el entorno, las pruebas se pueden lanzar desde aquí.
- Lo que se prepara aquí:
  - para cada objeto, un **kit**: sus fotogramas norte y este en las dos resoluciones, la paleta, la huella en la casilla, y la cámara y la luz;
  - el **importador**: de PNG a la paleta, las hojas y las animaciones de la orientación nueva;
  - la **vista previa** en las cuatro vistas con `th64sim`.
- Por tandas:
  1. los objetos sin nadie usándolos, más el escritorio y la recepción;
  2. los que se usan sentado o de pie: váter, lavabo, diván;
  3. las máquinas.
- **Derechos.** Lo que se derive del arte de TH se trata como sus datos: no va al repositorio.

**[CorsixTH-Graphics](https://github.com/CorsixTH/CorsixTH-Graphics)**, que propuso Eduardo, es el intento de la comunidad de sustituir el arte de TH por arte libre. Se quedó en 80 suelos (de Zephyris, CC BY-SA 4.0), un codificador de animaciones (MIT) y la especificación de su formato; el último cambio es de 2021. Sirve como modelo para el importador: describe sprites, fotogramas y elementos en texto, con grupos de hasta cuatro vistas por animación. Sus suelos no sirven para las espaldas. Y deja una lección: dibujar a mano el arte de TH se paró en los suelos.

### Colocar en las cuatro direcciones (hito 6.3)

Los 37 objetos de norte y este se podrán colocar también al sur y al oeste. Su huella, el sitio de uso y los demás datos de esas orientaciones salen girando los que ya tienen, como en las vistas. Mientras no haya espalda, se dibuja el sustituto. Es un añadido de TH64: ni TH ni CorsixTH lo permiten.

### La demo sin Expansion Pak (hito 6.10)

Se suman estos recortes a los de arriba:
- **sin giro**: unos 100 KiB menos, entre el mapa girado (96 KiB) y sus tablas;
- **como mucho unos 10 pacientes por hospital**, ajustándolo con lo que cueste cada uno;
- **solo 2 o 3 niveles**, cuando los haya (en esta fase, el nivel 1).

Como en Perfect Dark, será una sola ROM, que detecta el Expansion Pak al arrancar y abre el juego entero o la demo.

### Menús e interfaz (hito 6.8)

- **La interfaz sigue siendo la de PC**: el panel de `DATAM` y las ventanas de `QDATA`, como hasta ahora.
- **La de PS1 queda como opción para más adelante.**
  - Cambia la interfaz, con elementos flotantes.
  - Sus pantallas de gestión están rehechas a 320 × 256 en color directo, con los personajes de fondo.
  - Su menú trae fuente, banderas de idioma y botones en hojas de 4 bits por píxel.
  - El mando y los iconos de botones son de PlayStation: habría que redibujarlos como el mando de N64.
  - Se sacaría solo de los ficheros de datos del disco de Eduardo, sin desensamblar el ejecutable y sin subirlo al repositorio.
- **La ROM de prueba conserva su menú sencillo**, porque es de pruebas.

### Partidas: cartucho y Controller Pak (hito 6.7)

- **Guardado principal en el cartucho**: FlashRAM de 128 KiB, para varias partidas.
- **El Controller Pak sirve para intercambiar con los amigos** niveles hechos a mano y copias de seguridad. Tiene unos 31 KiB (123 páginas de 256 B, hasta 16 notas).
- libdragon trae el sistema de ficheros del Controller Pak y las memorias del cartucho.
- El Rumble Pak va en la misma ranura que el Controller Pak, así que hay que pedir el cambio, como hacían los juegos de N64.

### Vibración (hito 6.8)

Con el Rumble Pak, y una opción para apagarla, en:
- emergencias y el helicóptero;
- máquinas que explotan;
- terremotos;
- la muerte de un paciente;
- un golpe corto por cada disparo a una rata.

Del nivel 1 entran las máquinas que explotan y las muertes. Lo demás llega con los niveles que lo traen.

### Alta resolución (fuera de esta fase)

El arte original de TH es de 640 × 480; `DATAM` es la versión reducida. Un modo de 640 sería el arte de PC tal cual, con las ventanas de gestión a su tamaño. Es un experimento para cuando el juego sea jugable, medido en la consola real:

| | 320 × 240 (hoy) | 640 × 480 |
|---|---|---|
| Framebuffers | 300 KiB | 1,2 a 1,8 MiB |
| Relleno del RDP por fotograma | unos 4 ms (estimado, fase 5) | unos 16 ms de 33 (estimado) |
| Sprites | 1,30 MiB residentes | 4,72 MiB: hay que leerlos del cartucho con una caché |

La lista de dibujo cuesta lo mismo, porque se ve la misma porción de mapa. El entrelazado parpadea en las líneas finas del isométrico. El paso más barato sería poner a 640 solo las pantallas de gestión, que son estáticas. Si no da, se recorta como en la demo, por ejemplo con menos pacientes.

### Theme Hospital 64, no CorsixTH 64

- CorsixTH es la referencia de código: corrige fallos históricos y termina cosas que el original dejó a medias.
- Donde se aparta del original, manda el original.
- Cada diferencia se apunta en una de tres listas:
  1. **De TH**, donde CorsixTH no lo sigue: por ejemplo, las ratas.
  2. **De CorsixTH**: sus arreglos, que se quedan.
  3. **Añadidos de TH64**: las cuatro vistas, colocar en cuatro direcciones, la vibración, el Controller Pak.
- Donde se siga al original, la comparación con CorsixTH ya no vale en ese punto. Las dudas se resuelven mirando el original en DOSBox, sin desensamblar nada.

**Las ratas.**
- Sus animaciones están en los datos: andar en 8 direcciones y entrar y salir de la ratonera (1908 a 1930).
- CorsixTH las cita en un comentario de `rathole.lua` y tiene los premios por cazarlas, pero no hay ratas.
- Se harán cuando llegue la limpieza (hito 6.5), si el original las saca en el nivel 1.

### El material sin usar

Según [`sin_usar.py`](../tools/arte/sin_usar.py), los datos traen **377 animaciones con fotogramas que ninguna animación de CorsixTH usa**: 1.395 fotogramas. Es un tope, porque alguna puede usarse por un camino indirecto. Solo **71 sprites** no salen en nada usado: casi todo recombina piezas que ya existen, así que costaría poca memoria.

En una primera mirada hay:
- las ratas;
- una bomba con la mecha encendida (4 fotogramas);
- una llave inglesa que gira (8);
- una paciente con pelo excesivo que cambia y acaba en el suelo (37);
- la máquina de ADN con un paciente alienígena (55);
- la pantalla de la recreativa (45).

El paciente del chicle en el banco, que recordaba Eduardo, aún no ha aparecido. Antes de elegir qué terminar, se hará un inventario con cada animación en movimiento.

## Hitos

Cada hito se cierra como en la fase 5: un commit, una captura de ares y sus cifras en `docs/06_juego_base.md`.

| Hito | Qué | Cómo se comprueba |
|---|---|---|
| **6.1** Puntero y panel | Puntero con el stick (con aceleración, y Z para ir rápido), A y B como clics, la vista con los botones C, y el ratón de N64. La base de ventanas y el panel inferior de `DATAM`: botones, dinero, fecha, mensajes y descripciones de los botones | Capturas iguales píxel a píxel al rasterizador de referencia, como el mapa. El panel y el puntero cuestan como mucho 1,5 ms por fotograma |
| **6.2** Las cuatro vistas | Girar la cámara con la cruceta (izquierda y derecha), que hasta ahora solo desplazaba la vista. Girar el mapa, las paredes, las sombras, los objetos y las personas, y el puntero en las cuatro vistas. Clasificar los suelos y el decorado, y poner los sustitutos. El menú de la ROM de prueba | La vista original, igual píxel a píxel. Las giradas, contra el mismo hospital girado montado en CorsixTH. Girar no puede encarecer la lista de dibujo: hoy cuesta 4,7 ms por fotograma a velocidad Normal |
| **6.2b** El sol fijo | Las sombras, calculadas en el mundo y dibujadas giradas; las paredes, con la pieza normal o la espejada según les dé el sol ([arriba](#el-sol-fijo-hito-62b)) | La vista original, igual píxel a píxel. Las giradas, contra CorsixTH con las piezas espejadas, salvo las sombras: las casillas con sombra, las mismas en las cuatro vistas |
| **6.3** Construir | Amueblar el pasillo, el plano de una sala (arrastrar, puertas y ventanas), colocar y girar objetos, editar y vender, en las cuatro vistas. Los objetos de norte y este, también al sur y al oeste. Las salas de cualquier forma: el plano se amplía o se recorta por rectángulos. Las validaciones de CorsixTH, con el bloqueo de zonas en el modo 2 | Una batería de órdenes de construir, aceptadas y rechazadas igual que en CorsixTH, una a una, con las salas rectangulares. Las banderas de las 16.384 casillas iguales después de construir. Lo cobrado, igual al dólar. Las demás formas, con pruebas de la regla de Eduardo |
| **6.4** Personal y avisos: la primera ROM jugable | Contratar y colocar personal, cogerlo y soltarlo. Las ventanas de persona y de cola, los faxes, los mensajes y el consejero, la pausa y las velocidades | Construir a mano, desde la ROM, el hospital del hito 4 y jugarlo. Grabar las órdenes y repetirlas en el PC con la misma huella |
| **6.5** Las salas del nivel 1 | Las 7 salas que faltan, el diagnóstico completo, la curación y la muerte. La enfermera y el bedel (limpiar, regar y reparar), los aseos, la máquina de bebidas y las plantas | Un escenario del nivel 1 completo, creado en CorsixTH: 20 semillas, con el criterio acordado (±15 % en curados y ±7,5 % en saldo), más reputación y muertes |
| **6.6** Economía, gestión y objetivos | El banco, la política del hospital, la gestión de personal, el libro de casos (precios), el mapa de la ciudad, los informes, el valor del hospital, ganar y perder, VIP y epidemias | Las fórmulas de interés, valor y precios, exactas. El resto, estadístico, con las mismas 20 semillas. Legibilidad a 320 × 240: capturas para Eduardo |
| **6.7** Guardar y cargar | Formato propio, solo con el estado de la partida. Se guarda en el cartucho (FlashRAM, 128 KiB, varias partidas); el Controller Pak sirve para copiar partidas y llevarlas a otra consola, así que cada una debería caber en unos 31 KiB | Guardar, cargar y seguir da la misma huella que no guardar, también tras pasar por el Controller Pak. El tamaño se mide con el hospital del hito 5 |
| **6.8** Menús, opciones, sonido y vibración | Menú principal y partida nueva con la interfaz de PC, las opciones acordadas en la fase 4, inglés y español, el locutor con subtítulos, los efectos de sonido en la partida y el Rumble Pak | Los textos de los dos idiomas, sin cadenas que falten. Las opciones, en la partida guardada. La vibración, en los sucesos de la lista |
| **6.9** El nivel 1 entero | Jugar el nivel 1 hasta ganarlo, con una lista de órdenes y en la ROM | Gana en la ROM y en el PC con la misma huella. Memoria, CPU y fotogramas con el hospital lleno del final, frente a los presupuestos de la fase 4 |
| **6.10** Sin Expansion Pak | El nivel 1 con 4 MiB, con los recortes que acepta Eduardo: temperatura más barata, sin música, menos texto, sin giro y unos 10 pacientes como mucho. Solo las hojas, las animaciones y las reglas del nivel 1. Una sola ROM, que detecta el Expansion Pak al arrancar | La ROM juega el nivel 1 con 4 MiB en ares, sin quedarse sin memoria. Lo que se recorta cambia la simulación (la temperatura), así que la demo tiene su propia huella y su propia comparación con CorsixTH |

**Presupuestos:**
- **CPU.** A velocidad Normal y con el hospital del hito 5, el fotograma cuesta hoy 14,1 ms de los 33,3. Con una ventana de gestión abierta tiene que seguir a 30 fps sin saltos.
- **RAM.** La fase 4 reservaba 0,50 MiB para la interfaz: el panel, el reloj, los menús, las fuentes y una ventana de gestión abierta. Hoy quedan 3,8 MiB de heap libres con 8 MiB.

## Lo que no entra en la fase 6

- **Lo que el nivel 1 no tiene:** investigación, formación, quirófano, emergencias, terremotos y los niveles 2 a 12.
- **Los vídeos.** Van fuera de la partida, así que pueden esperar a la fase siguiente.
- **Lo que se dejó para el final:**
  - los cambios en libdragon;
  - el último deseo de Eduardo: pediatría y maternidad, con arte nuevo.
- **Lo que pidió Eduardo el 2026-10-28 para después:**
  - el modo de alta resolución, como experimento;
  - la interfaz de PS1, como opción;
  - el editor de niveles que CorsixTH dejó a medias, para compartir niveles por el Controller Pak;
  - la sombra según la hora, el día y la noche, las estaciones y el clima.
- **El arte nuevo con IA va en paralelo:** las herramientas se preparan durante la fase, y cada tanda entra cuando esté lista, sin tocar el motor.

## Riesgos

| Riesgo | Cómo se reduce | Cuándo se sabe |
|---|---|---|
| **El puntero con el stick resulta incómodo** | Aceleración ajustable, atajos con los botones, y el ratón de N64. Eduardo lo prueba en un emulador con mando | Hito 6.4 |
| **Los sustitutos de las vistas giradas se notan demasiado** | Lista exacta del arte que falta; el conversor admite sprites nuevos para cambiarlos sin tocar el motor | Hito 6.2: se notan, y Eduardo pide las espaldas |
| **El arte hecho con IA no casa con el de TH** | Los sprites de TH son renders 3D: para los objetos, reconstruir en 3D y renderizar con la cámara y la luz de TH; la paleta la pone el conversor. Probar los dos caminos con tres objetos antes de seguir | Con la primera prueba |
| **Los derechos del arte nuevo** | Lo que se deriva del arte de TH se trata como sus datos: fuera del repositorio | Desde la primera prueba |
| **Suelos y decorado sin pieza girada** | Clasificarlos uno a uno: los que no tienen dirección valen tal cual; para los demás, la pieza más parecida o arte nuevo | Hito 6.2 |
| **Las salas de forma libre no tienen con qué compararse** | Las rectangulares siguen exactas frente a CorsixTH; para las demás, pruebas de la regla y partidas largas sin atascos | Hito 6.3 |
| **Las ventanas de gestión no se leen a 320 × 240** | Las ventanas de `QDATA` reducidas a la mitad; si no se leen, se rehace la maquetación, como hizo la PS1 | Hito 6.6 |
| **La partida guardada no cabe en 32 KiB** | Guardar solo el estado. La temperatura (96 KiB en RAM) cambia despacio de una casilla a otra y debería comprimirse bien | Hito 6.7 |
| **El esfuerzo:** unas 14.700 líneas de interfaz y 4.500 de lógica | Se avanza por lo que pide el nivel 1, no por pantallas sueltas | A lo largo de la fase |
| **La demo no cabe en 4 MiB ni con los recortes** | Medir la memoria en cada hito, no solo al final. Si no llega, se recortan más cosas, con Eduardo | Hito 6.10, pero se ve venir antes |
| **ares frente a la consola real:** todo sigue medido en ares | Las ROM de medida escriben en pantalla. Se prueban en la consola cuando llegue el flashcart | Cuando llegue |

## Lo que hará falta de Eduardo

- **En el hito 6.2:** mirar capturas de las vistas giradas y decir si los sustitutos se pueden aceptar. Hecho el 2026-10-28: ver [arriba](#lo-que-decidió-eduardo-tras-el-hito-62).
- **En el hito 6.2b:** mirar las cuatro vistas con el sol fijo, sobre todo la que queda a la sombra.
- **Con el arte nuevo:** generar las pruebas con la IA (o dejar una clave de API en el entorno para lanzarlas desde aquí) y elegir el camino.
- **En el hito 6.4:** probar la ROM en un emulador con mando y decir qué tal va el puntero. Además, decidir si «acciones en pausa» y «recordar el contenido de las salas» vienen activadas por defecto (pendiente de la fase 4).
- **En el hito 6.6:** mirar las capturas de las ventanas de gestión y decir si se leen.
- **Cuando llegue el flashcart:** pasar las ROM de pruebas, de medida y normal en la consola.
