# Fase 3 — Arquitectura de CorsixTH y qué se puede reutilizar

Fecha: 2026-10-09. Estado: **cerrada, salvo el port de Wii** (código no accesible desde el entorno; ver [abajo](#port-de-wii)). Versión leída: CorsixTH v0.70.1 (`56bd5d0`). Las medidas de CPU usan la partida `lleno` de la fase 1 (304 pacientes, 57 empleados, 303 objetos).

## Conclusión

**CorsixTH es un juego escrito en Lua sobre un motor C++ pequeño.** La simulación entera (pacientes, personal, habitaciones, economía, eventos) está en Lua; el C++ pone el mapa, el pathfinding, las animaciones, el render y el sonido.

| | Ficheros | Líneas de código | Instrucciones de la simulación (callgrind) | Instrucciones de la VM de Lua (muestreo) |
|---|---:|---:|---:|---:|
| C++ (`CorsixTH/Src`, `libs/`) | 67 | 21.259 (2.778 son una tabla de caracteres chinos) | 21,9 % (temperatura 12,1 %, pathfinding 6,9 %) | — |
| Lua, en total | 298 | 104.945 | 76,5 % | 100 % |
| — lógica del juego | 193 | 24.749 | | 80,8 % |
| — plataforma y utilidades | 16 | 5.888 | | 17,8 % (casi todo `class.is` e `ipairs`) |
| — interfaz | 65 | 19.512 | | 1,3 % |
| — textos de idioma (datos) | 24 | 54.796 | | 0 % |

- **El coste está muy concentrado.** Una sola función, `Humanoid:findObjectsInSquare`, se lleva el **47 % de las instrucciones de la VM** de Lua en el hospital lleno, contando lo que llama (43 % en el mediano). Contando solo su propio código, ella, `EntityMap:getObjectsAtCoordinate`, `class.is` e `ipairs` suman el **57 %**. Los tres grupos donde está el coste (humanoides, mapa de entidades y utilidades) son 4.947 líneas: un 20 % de las 24.749 de lógica, que gasta el 78 % de las instrucciones.
- **La lógica está bastante separada de la interfaz.** De los 193 ficheros de lógica, 172 no tocan la UI y 101 no tocan ni UI, ni sonido, ni gráficos. Las 197 líneas de lógica que llaman a la UI están concentradas: 125 en `player_hospital.lua` y `world.lua`.
- **Pero la lógica depende de las animaciones para medir el tiempo:** 34 llamadas a `getAnimLength` en 21 ficheros (17 de acciones de humanoides) y 88 `setAnimation` fuera de la interfaz. Las tablas de animación hacen falta aunque no se dibuje nada (en `DATAM` y `DATA` duran lo mismo, fase 2).
- **El C++ tiene una frontera limpia con SDL**: todo el dibujo pasa por `render_target`, `sprite_sheet` y `palette` (`th_gfx_sdl.{h,cpp}`, 1.609 líneas y 196 referencias a SDL). El mapa (3.000 líneas), el pathfinding (703) y las animaciones (4.092) no llaman a SDL.
- **Lo que no cabe tal cual en la N64 es la memoria del C++:** cada casilla del mapa ocupa 84 B en la N64 y CorsixTH guarda dos copias del mapa (2,63 MiB), más 0,44 MiB de nodos de pathfinding. Y una partida guardada son 0,53–1,20 MiB (33–296 KiB comprimida), frente a 32–128 KiB de memoria de guardado en un cartucho.
- **Con los factores de CPU de la fase 1, ni un híbrido ni el C++ actual caben en la N64.** El hospital lleno necesitaría 34 veces la CPU a velocidad Normal. La fase 1 daba 39 porque aplicaba el factor de Lua a todo; aquí la parte de C++ lleva el suyo. Aunque las partes calientes de Lua pasaran a C a coste cero, seguirían haciendo falta 11. La difusión de temperatura del C++, sola, pide 2,6. Hace falta un motor con algoritmos pensados para la consola, que use la lógica de CorsixTH como especificación ([implicaciones](#implicaciones-para-la-fase-4)).
- CorsixTH v0.70.1 usa **SDL2**, no SDL3 como suponía el plan.

## Diagrama de módulos

![Módulos de CorsixTH: Lua a la izquierda, C++ a la derecha](img/03_modulos.png)

<details>
<summary>Fuente del diagrama (Mermaid)</summary>

```mermaid
flowchart LR
  subgraph LUA["Lua — CorsixTH/Lua"]
    direction TB
    UI["Interfaz<br/>65 fich. · 19.512 l. · 1,3 %"]
    PLAT["Plataforma y servicios<br/>app, audio, graphics, persistance, strings<br/>12 fich. · 5.213 l. · 0,2 %"]
    TXT[("Textos de idioma (datos)<br/>24 fich. · 54.796 l.")]
    subgraph LOGIC["Lógica del juego — 193 fich. · 24.749 l."]
      direction TB
      WORLD["Mundo, hospital y economía<br/>9 fich. · 5.951 l. · 7,0 %"]
      EV["Eventos<br/>4 fich. · 1.024 l. · 1,1 %"]
      ROOM["Habitaciones<br/>24 fich. · 3.110 l. · 0,4 %"]
      DIS["Enfermedades y diagnóstico<br/>43 fich. · 1.369 l. · 0 %"]
      OBJ["Objetos y máquinas<br/>63 fich. · 5.953 l. · 8,4 %"]
      HUM["Humanoides<br/>10 fich. · 3.269 l. · 42,2 %"]
      ACT["Acciones de humanoides<br/>33 fich. · 3.070 l. · 3,4 %"]
      EMAP["Mapa y entity_map<br/>7 fich. · 1.003 l. · 18,3 %"]
    end
    UTIL["class, utility (ipairs)<br/>4 fich. · 675 l. · 17,6 %"]
  end
  subgraph CPP["C++ — CorsixTH/Src · 21.259 l."]
    direction TB
    CBOOT["Arranque y bucle (SDL2)<br/>1.764 l."]
    CGLUE["Enlace con Lua y guardado<br/>2.572 l."]
    CMAP["Mapa y temperatura<br/>3.000 l. · 13,4 % de la CPU"]
    CPATH["Pathfinding<br/>703 l. · 6,9 % de la CPU"]
    CANIM["Hojas, animaciones, fuentes<br/>4.092 l."]
    CREND["Render (SDL2)<br/>1.609 l."]
    CAV["Sonido, música, vídeo<br/>SDL_mixer, FluidSynth, FFmpeg<br/>2.893 l."]
  end
  UI --> PLAT
  UI --> WORLD
  PLAT --> TXT
  WORLD -. "197 líneas de lógica abren ventanas o avisos" .-> UI
  WORLD --> HUM
  ROOM --> ACT
  OBJ --> HUM
  HUM --> ACT
  HUM --> EMAP
  DIS --> HUM
  HUM --> UTIL
  EMAP --> CMAP
  ACT -- "rutas" --> CPATH
  ACT -- "setAnimation, getAnimLength" --> CANIM
  PLAT --> CAV
  PLAT --> CGLUE
  CBOOT --> CGLUE
  CPATH --> CMAP
  CMAP --> CREND
  CANIM --> CREND
```

</details>

En los grupos de Lua, el porcentaje es su parte de las instrucciones de la VM de Lua (coste propio) durante la simulación del hospital lleno. En el C++ es su parte de todas las instrucciones del proceso según callgrind. Las flechas son dependencias que aparecen en el código: símbolos de otro grupo y llamadas a las API de C++. La tabla completa de referencias entre grupos está en `bench/resultados/fase3/lua_modulos.json`. El diagrama se regenera con `npx @mermaid-js/mermaid-cli` a partir de la fuente.

## Núcleo C++

Medido con `tools/arquitectura/cpp.py`: líneas de código sin comentarios ni vacías, referencias a SDL (`SDL_*`, `Mix_*`), a la API de Lua y a FFmpeg.

| Subsistema | Ficheros | Líneas | SDL | API Lua | En la N64 | Por qué |
|---|---:|---:|---:|---:|---|---|
| Hojas de sprites, animaciones y fuentes (`th_gfx`, `th_gfx_font`, `th_lua_anims`, `th_lua_gfx`) | 7 | 4.092 | 3 | 661 | **Reutilizable con cambios** | El decodificador de chunks pasa al conversor del PC (ya está reimplementado en la fase 2). El gestor de animaciones (fotogramas, capas, marcadores, `getAnimLength`) hace falta en la consola tal cual; 1.585 líneas son enlace con Lua |
| Textos y codificaciones (`th_strings`, `th_lua_strings`, tablas CP437/CP936/MIK) | 6 | 3.576 | 0 | 304 | **Reutilizable** el lector de `LANG-x.DAT` y CP437 | La tabla CP936 (2.778 líneas, chino) y la MIK (ruso) sobran |
| Mapa y superposiciones (`th_map`, `th_map_overlays`, `th_lua_map`) | 5 | 3.000 | 0 | 357 | **Reutilizable con cambios** | Carga de `LEVEL.Lx`, banderas, parcelas, sombras y temperatura: C++ sin SDL. Pero cada casilla ocupa 84 B en la N64 (punteros de listas enlazadas, `std::list` de objetos, temperaturas dobles) y hay dos copias del mapa: 2,63 MiB. Hay que rehacer la disposición en memoria |
| Arranque y bucle principal (`main`, `bootstrap`, `sdl_core`, `sdl_wm`, `whereami`) | 12 | 1.764 | 68 | 212 | **Descartable** | Eventos, ventana y temporizador de SDL; en la N64 lo hace libdragon (`joypad`, `display`, `timer`) |
| Render (`th_gfx_sdl`) | 2 | 1.609 | 196 | 6 | **Reescribir con la misma interfaz** | `render_target`, `sprite_sheet`, `palette`, `raw_bitmap`, `cursor`: es la única frontera con el dibujo. Se reimplementa sobre `rdpq` con texturas CI8/CI4 |
| Enlace C++ ↔ Lua (`th_lua`, `th_lua_internal`, `th_lua_ui`, `th`) | 8 | 1.288 | 0 | 396 | **Depende del camino** | Imprescindible si se conserva Lua; sobra con un motor nativo |
| Guardado de partidas (`persist_lua`, `run_length_encoder`) | 4 | 1.284 | 0 | 425 | **Descartable** | Serializa el estado completo de Lua (0,53–1,20 MiB por partida). En la N64 hace falta un formato propio que quepa en 32–128 KiB |
| Música (`midi_player`, `th_lua_midi`, `xmi2mid`) | 5 | 1.021 | 4 | 54 | **`xmi2mid` en el PC; el resto descartable** | `xmi2mid` ya se usa en el conversor (fase 2); la reproducción pasa a MID64 + SF64 de libdragon |
| Vídeo (`th_movie`, `th_lua_movie`) | 3 | 962 | 59 | 36 | **Descartable** | FFmpeg; en la N64, el reproductor de vídeo de libdragon |
| Ficheros (`iso_fs`, `th_lua_iso`, `th_lua_lfs_ext`, `lua_rnc`, `libs/rnc`) | 8 | 944 | 0 | 95 | **RNC en el PC; el resto descartable** | Lectura de la ISO del juego y del sistema de ficheros; en la N64, DFS |
| Sonido (`th_sound`, `th_lua_sound`, `sdl_audio`) | 4 | 910 | 95 | 170 | **Lector en el PC; reproducción descartable** | El lector de `SOUND-x.DAT` pasa al conversor (fase 2); la mezcla pasa al mezclador de libdragon con WAV64 |
| Pathfinding (`th_pathfind`) | 2 | 703 | 0 | 29 | **Reutilizable con cambios** | A* sobre las banderas del mapa, sin SDL. Reserva un nodo de 28 B por casilla (0,44 MiB en la N64) más 64 KiB; habría que compactarlo. `visit_objects` llama a Lua |
| Números aleatorios (`random.c`, Mersenne Twister) | 1 | 106 | 0 | 34 | **Reutilizable** | C puro |
| **Total** | **67** | **21.259** | **425** | **2.779** | | |

Tamaños en memoria medidos compilando las cabeceras de CorsixTH (`tools/arquitectura/tamanos.sh`): con el compilador de la N64 (`mips64-elf-g++`, ABI o64 de libdragon) y en x86-64.

| Estructura | x86-64 | N64 | Cuántas | Total en la N64 |
|---|---:|---:|---:|---:|
| `map_tile` (casilla) | 120 B | 84 B | 2 × 128 × 128 | **2,63 MiB** |
| `path_node` (pathfinding) | 40 B | 28 B | 128 × 128 | 0,44 MiB |
| `level_map` | 160 B | 124 B | 1 | |
| `pathfinder` | 240 B | 144 B | 1 | |

En la casilla, solo los 8 bytes del formato original (`raw`) y las banderas son datos del juego. El resto son estructuras de C++: dos listas enlazadas, que son 24 B de punteros, una `std::list` de objetos y dos temperaturas. Un motor pensado para la N64 puede guardar una casilla en 8–16 B: 128–256 KiB por mapa.

## Lógica en Lua

Medido con `tools/arquitectura/lua_modulos.py`. Cuenta líneas de código, dependencias (símbolos globales de otro grupo) y líneas que tocan la interfaz (ventanas, `UI*`, `.ui`), el sonido (`.audio`, `playSound`, locutor) o los gráficos (`setAnimation`, `setLayer`, `setMood`, `.gfx`, cursores). Los porcentajes son el coste propio en instrucciones de la VM durante la simulación del hospital lleno.

| Grupo | Ficheros | Líneas | Líneas que tocan UI | Sonido | Gráficos | Ficheros sin UI | Instrucciones VM |
|---|---:|---:|---:|---:|---:|---:|---:|
| Entidades: humanoides (`entities/humanoid*`) | 10 | 3.269 | 16 | 3 | 51 | 6 | **42,2 %** |
| Mapa y entidades en el mapa (`map`, `entity_map`, `walls/`) | 7 | 1.003 | 0 | 0 | 1 | 7 | **18,3 %** |
| Utilidades del lenguaje (`class`, `utility`, `strict`, `date`) | 4 | 675 | 0 | 0 | 10 | 4 | **17,6 %** |
| Entidades: objetos y máquinas (`entity`, `entities/object`, `objects/`) | 63 | 5.953 | 15 | 7 | 34 | 58 | 8,4 % |
| Mundo, hospital y economía (`world`, `hospital*`, `queue`, `calls_dispatcher`, `research_department`…) | 9 | 5.951 | 132 | 15 | 7 | 4 | 7,0 % |
| Acciones de humanoides (`humanoid_actions/`) | 33 | 3.070 | 9 | 3 | 127 | 32 | 3,4 % |
| Interfaz (`ui`, `game_ui`, `window`, `dialogs/`) | 65 | 19.512 | 2.280 | 163 | 544 | 1 | 1,3 % |
| Eventos (`epidemic`, `earthquake`, `cheats`, `announcer`) | 4 | 1.024 | 17 | 14 | 3 | 1 | 1,1 % |
| Habitaciones (`room`, `rooms/`) | 24 | 3.110 | 8 | 0 | 24 | 21 | 0,4 % |
| Plataforma y servicios (`app`, `audio`, `graphics`, `persistance`, `strings`…) | 12 | 5.213 | 75 | 72 | 43 | 7 | 0,2 % |
| Enfermedades y diagnóstico (`diseases/`, `diagnosis/`) | 43 | 1.369 | 0 | 0 | 227 | 43 | 0 % |
| Textos de idioma (`languages/`, son datos) | 24 | 54.796 | — | — | — | — | 0 % |

- **Lógica de juego pura** (los ocho grupos de lógica): 193 ficheros y 24.749 líneas. 172 ficheros (13.207 líneas) no nombran la UI; 101 (6.853 líneas) no tocan ni UI, ni sonido, ni gráficos.
- **Dónde está el acoplamiento con la UI:** `hospitals/player_hospital.lua` (79 líneas: mensajes, faxes, consejero), `world.lua` (46: velocidad, teclas, zoom, ventanas), `epidemic.lua` (11), `pickup.lua` (9) y `staff.lua` (7). Son avisos y ventanas que abre la simulación; se pueden sustituir por eventos hacia la UI.
- **Gráficos dentro de la lógica:** son decisiones de aspecto, no de dibujo. Las enfermedades eligen capas (`setLayer`, 227 líneas: cabeza hinchada, ropa…), los humanoides muestran iconos de humor (`setMood`) y las acciones eligen animación y velocidad por casilla. Todo eso se puede traducir a llamadas a un gestor de animaciones nativo.
- **Dependencias más fuertes entre grupos** (referencias a símbolos de otro grupo): Interfaz → Plataforma 1.032 (casi todo `_S`, los textos), Interfaz → Utilidades 238, Habitaciones → Acciones 174, Mundo → Plataforma 168 (`_S`, `_A`), Enfermedades → Plataforma 145 (`_S`), Objetos → Humanoides 95. La lógica apenas nombra clases de la interfaz: 12 referencias desde humanoides y 15 desde objetos (abrir la ficha de un paciente o de una máquina).
- **Clases:** 195 declaraciones `class`, 160 con clase base. La jerarquía de entidades es `Entity` → `Humanoid` → `Patient`, `Staff` (→ `Doctor`, `Nurse`, `Handyman`, `Receptionist`), `Vip`, `Inspector` y `GrimReaper`; y `Entity` → `Object` → `Machine`.

## Dónde se va el tiempo de la simulación

Dos mediciones complementarias sobre la partida `lleno`, sin dibujar frames durante la medición (`bench/scenarios/perfil.lua --th64-frames=0`):

### Dentro de Lua (muestreo)

Un hook de conteo cada 1.000 instrucciones de la VM, 300 horas de juego, 79.986 muestras (unas 267.000 instrucciones de la VM por hora de juego; 72.000 en el hospital mediano). Cuenta instrucciones de Lua, no tiempo de funciones C.

| Función (inclusivo: ella y lo que llama) | Lleno | Mediano |
|---|---:|---:|
| `World:onTick` | 98,2 % | 93,6 % |
| `Staff:tick` (todo el personal) | 59,1 % | 65,3 % |
| `Humanoid:findObjectsInSquare` | **47,2 %** | **43,3 %** |
| `Doctor:tick` | 36,5 % | 37,8 % |
| `Entity:tick` (avance de animaciones y temporizadores) | 19,2 % | 17,5 % |
| `World:onEndDay` | 16,7 % | 11,7 % |
| `EntityMap:getObjectsAtCoordinate` | 15,1 % | 13,3 % |
| `Patient:tick` | 14,4 % | 7,9 % |
| `Patient:tickDay` | 13,7 % | 8,0 % |
| `walk` (acción de andar) | 9,4 % | 8,0 % |
| `class.is` | 8,6 % | 11,8 % |
| `ipairs` (versión en Lua de `utility.lua`) | 8,2 % | 7,5 % |

- **`findObjectsInSquare`** recorre un cuadrado de 5 × 5 casillas, consulta el número de habitación de cada una en C++ y crea tablas nuevas con los objetos encontrados. `Staff:tick` la llama **en cada hora de juego** para que cada empleado busque basura, y además dos veces al día para plantas y objetos agradables; los pacientes, tres veces al día. En el hospital lleno salen unas 78 búsquedas por hora de juego y unas 1.600 instrucciones de la VM cada una, para un efecto pequeño en la felicidad.
- **`ipairs` y `class.is`** son sobrecoste del lenguaje. `utility.lua` sustituye `ipairs` por una versión en Lua para admitir `__ipairs`, que Lua 5.4 ya no tiene. `class.is` es la comprobación de tipos de su sistema de clases. Juntos son el 17 % de las instrucciones, y en C no existirían.

### C++ y VM de Lua (callgrind)

150 horas de juego bajo valgrind/callgrind (`tools/bench_run.sh --callgrind`), con la instrumentación activa solo durante la medición y sin dibujar. Cuenta instrucciones x86 de todo el proceso: **24,6 millones por hora de juego**.

| Dónde | Instrucciones |
|---|---:|
| VM de Lua (`liblua5.4`) | **76,5 %** |
| C++ de CorsixTH | **21,9 %** |
| — temperatura (`level_map::update_temperatures` y `thermal_neighbour`) | 12,1 % |
| — pathfinding (`th_pathfind`) | 6,9 % |
| — resto del mapa, enlace con Lua, animaciones | 2,9 % |
| libc, libstdc++, SDL y otros | 1,6 % |

- **La temperatura es el 12 % de la simulación.** `update_temperatures` recorre las 16.384 casillas del mapa en cada hora de juego, con cuatro vecinas por casilla y aritmética en `double`, para difundir el calor de los radiadores. En PC no se nota; en la N64 sí (ver [implicaciones](#implicaciones-para-la-fase-4)).
- **El pathfinding es poco** (6,9 %): las rutas se calculan al empezar a andar, no en cada paso.
- **Las animaciones apenas cuestan en la simulación** (0,1 %): avanzar fotogramas es barato; lo caro de ellas es dibujarlas, que aquí no se mide.

Los porcentajes de Lua del apartado anterior se reparten dentro de ese 76,5 %.

## Port de Wii

**Estado: el código existe pero no se ha podido leer desde este entorno.**

- **Dónde está.** La página de WiiBrew ([wiibrew.org/wiki/CorsixTH](https://wiibrew.org/wiki/CorsixTH)) enlaza el código del port (autor: tueidj, versión 1.02, licencia MIT) en `http://www.tueidj.net/CorsixTH-wii-src.zip`. El dominio está hoy aparcado. La única copia localizada está en la Wayback Machine (`web.archive.org/web/20190122061740/http://www.tueidj.net/CorsixTH-wii-src.zip`), y `web.archive.org` corta la conexión tanto desde el contenedor como desde la herramienta de descarga web. No hay copia en GitHub ni en los elementos de archive.org.
- **Lo que documenta su autor en WiiBrew:**
  - parte del CorsixTH de cuando el proyecto estaba en Google Code (SVN) e incluye SDL (el port de Tantric, modificado) y Lua listos para compilar;
  - «arreglos de endianness y una caché LRU para los gráficos y los efectos de sonido, para que quepan en la memoria disponible de la Wii»;
  - en la 1.02, «memoria virtual sobre la NAND para la librería de audio actual, que reduce el conjunto de trabajo de ~16 MB a ~512 KB», y caché LRU para los efectos;
  - la música MIDI se sintetiza con TiMidity, y recomienda un juego de instrumentos pequeño «porque no hay mucha memoria»;
  - reescribió el render de vídeo y el de audio (libaesnd) y la entrada (mandos de GameCube, ratón y teclado USB);
  - lo dejó abandonado porque CorsixTH se alejaba del original y fallaba a menudo.
- **Lo que enseña, aun sin el código:** la Wii tiene 88 MiB de RAM (24 + 64), 22 veces la de una N64 sin Expansion Pak. Aun así, el port necesitó cachés LRU para gráficos y sonido y memoria virtual para el archivo de sonido (`SOUND-x.DAT`, 13–16 MiB por idioma, fase 2). Coincide con lo medido aquí y en la fase 1: lo que no cabe son los gráficos decodificados, el sonido y el estado de Lua, no la lógica.
- **Pendiente:** con el ZIP se completa este apartado leyendo la caché LRU (tamaño, política de expulsión, qué guarda), los cambios de endianness (qué ficheros y estructuras) y qué recortó. La Wii es big-endian como la N64, así que esos cambios serían aplicables tal cual a un lector de datos en la consola.

## Tabla de reutilización

Resumen de las dos tablas anteriores, como entrada para la fase 4. «Herramienta» significa que el código se usa en el PC para convertir los datos (fase 2) y no va en la ROM.

| Pieza | Origen | Tamaño | Decisión | Motivo medido |
|---|---|---:|---|---|
| Decodificador de hojas (chunks), RNC, lector de `SOUND-x.DAT`, `xmi2mid`, lector de `LANG-x.DAT` | C++ | ~1.200 l. | **Herramienta** | Ya reimplementados o enlazados en `tools/datos/` |
| Gestor de animaciones (fotogramas, capas, marcadores, duración) | C++ `th_gfx` | ~1.400 l. | **Reutilizar con cambios** | La lógica lo necesita para medir el tiempo (34 `getAnimLength`); sin SDL |
| Render: `render_target`, `sprite_sheet`, `palette` | C++ `th_gfx_sdl` | 1.609 l. | **Reescribir con la misma interfaz** | 196 referencias a SDL; es la única frontera con el dibujo |
| Mapa: carga, banderas, parcelas, sombras | C++ `th_map` | ~2.000 l. | **Reutilizar el algoritmo; rehacer la casilla** | 84 B × 2 copias por casilla = 2,63 MiB en la N64 |
| Temperatura | C++ `th_map` | ~100 l. | **Reutilizar con otra frecuencia** | Recorre las 16.384 casillas en cada hora de juego: el 12,1 % de la simulación |
| Pathfinding | C++ `th_pathfind` | 703 l. | **Reutilizar con nodos compactos** | A* sin SDL; 0,44 MiB de nodos en la N64 |
| Números aleatorios | C `random.c` | 106 l. | **Reutilizar tal cual** | Mersenne Twister en C puro |
| Enlace con Lua | C++ `th_lua*` | 1.288 l. | **Según el camino** | Solo si se conserva Lua |
| Guardado | C++ `persist_lua` | 1.284 l. | **Descartar; formato propio** | 0,53–1,20 MiB por partida frente a 32–128 KiB de guardado |
| Arranque, eventos, sonido, música, vídeo, ficheros | C++ + SDL2, SDL_mixer, FluidSynth, FFmpeg | 5.601 l. | **Descartar** | Lo cubre libdragon (`joypad`, `mixer`, `wav64`, `mid64`, vídeo, DFS) |
| Humanoides, `entity_map`, `class`/`ipairs` | Lua | 4.947 l. | **Reescribir en C** | 78 % de las instrucciones de la VM; `findObjectsInSquare` sola, 47 % |
| Mundo, hospital, objetos, acciones, habitaciones, eventos | Lua | 19.108 l. | **Especificación para C, o Lua en un híbrido** | 20 % de las instrucciones de la VM; casi sin UI (172 de 193 ficheros) |
| Enfermedades y diagnóstico | Lua | 1.369 l. | **Pasar a tablas de datos** | 0 % de CPU; 43 ficheros de definiciones |
| Interfaz | Lua | 19.512 l. | **Rehacer** para mando y 320 × 240 | 2.280 líneas de UI, 544 de gráficos; sirve como referencia de pantallas y flujos |
| Textos de idioma | Lua | 54.796 l. | **Convertir a tablas de cadenas** | Datos; solo los idiomas incluidos (fase 2) |

## Implicaciones para la fase 4

Estimación del coste en la N64 con los factores medidos en la fase 1. Una hora de juego del hospital lleno cuesta 5,07 ms en la máquina de medición. Lua corre unas 400 veces más despacio en la VR4300 y el C unas 227 veces. Aquí ese tiempo se reparte según la medida de callgrind:

| Parte de la hora simulada | En PC | En la N64 (estimado) | Veces la CPU de la N64 a velocidad Normal (18,5 h/s) |
|---|---:|---:|---:|
| VM de Lua (76,5 %) | 3,88 ms | 1.551 ms | 28,7 |
| C++ y bibliotecas (23,5 %) | 1,19 ms | 271 ms | 5,0 |
| — de ello, temperatura | 0,61 ms | 140 ms | 2,6 |
| — de ello, pathfinding | 0,35 ms | 79 ms | 1,5 |
| **Total** | **5,07 ms** | **1.822 ms** | **33,7** |
| Si las partes calientes de Lua (78 %) costaran **cero** | | 610 ms | **11,3** |

- **Camino híbrido (CorsixTH en Lua con las partes calientes en C): no basta.** Aunque los humanoides, `entity_map`, `class.is` e `ipairs` pasaran a C sin coste alguno, el resto de la simulación en Lua más el C++ actual seguirían necesitando unas 11 veces la CPU de la N64 en el hospital lleno. Además, la memoria de Lua no cambiaría: 12,7–17,7 MiB de heap vivo (fase 1).
- **Ni siquiera el C++ actual cabe en el presupuesto.** La temperatura sola, con el algoritmo de CorsixTH, necesitaría 2,6 veces la CPU a velocidad Normal, y el pathfinding 1,5 veces. Un motor para la N64 tiene que **cambiar algoritmos**, no solo de lenguaje:
  - temperatura por habitación o repartida entre varias horas;
  - búsquedas de objetos cercanos con contadores mantenidos al añadir y quitar objetos, en vez de recorrer 25 casillas;
  - actualización de entidades escalonada entre ticks.

  El juego original corría en PC con DOS y en una PlayStation a 33,9 MHz: la misma simulación se puede hacer mucho más barata. CorsixTH, pensado para PC actuales, no lo necesita.
- **La lógica de CorsixTH sirve como especificación.** Son 24.749 líneas, en su mayoría separables de la UI. Lo que vale de ellas son las reglas, los tiempos y las fórmulas, no la implementación. Las tablas de animación hacen falta en cualquier caso, porque fijan cuánto dura cada acción.
- **Memoria.** Las estructuras C++ actuales ya ocuparían 3,1 MiB en la N64: 2,63 MiB de mapa y 0,44 MiB de pathfinding. Para el presupuesto de RAM de la fase 4 hay que partir de un mapa de 8–16 B por casilla, nodos de pathfinding compactos y un formato de guardado propio que quepa en 32–128 KiB.
- **El render es la pieza mejor aislada.** Basta con reimplementar `render_target`, `sprite_sheet` y `palette` sobre `rdpq`, usando las texturas CI8 de `DATAM` que se midieron en la fase 2.

Estas cifras son estimaciones (instrucciones de x86 convertidas con factores de un benchmark); las medirá de verdad el prototipo de la fase 5.

## Limitaciones

- **Instrucciones no son tiempo.** El muestreo de Lua cuenta instrucciones de la VM, y callgrind instrucciones de la CPU x86. En la VR4300, con su caché pequeña y una memoria lenta (fase 1), el reparto real puede cambiar, sobre todo en lo que recorre mucha memoria (temperatura, `entity_map`).
- **El hook de muestreo** se ejecuta dentro de la VM y su propio coste no se cuenta, pero cambia algo el comportamiento de la caché. Sirve para el reparto, no para tiempos absolutos.
- **Clasificación por expresiones regulares.** Las líneas «que tocan la UI, el sonido o los gráficos» se detectan por patrones (`tools/arquitectura/lua_modulos.py`). Se han revisado a mano en varios ficheros, pero es una medida aproximada.
- **Las cifras de C++ por subsistema** dependen de cómo se asignan los ficheros a cada subsistema, que se ha hecho a mano leyendo cada uno.
- **Port de Wii sin leer:** lo que se dice de él sale solo de la página de WiiBrew.

## Cómo reproducirlo

```bash
source tools/env.sh
# Núcleo C++: líneas, SDL, API de Lua y tamaños de las estructuras (x86-64 y N64)
python3 -I tools/arquitectura/cpp.py "$CORSIXTH_DIR" bench/resultados/fase3/cpp.json
tools/arquitectura/tamanos.sh bench/resultados/fase3/tamanos.json
# Perfil de la simulación en Lua (muestreo; unos 15 s por partida)
tools/bench_run.sh bench/scenarios/perfil.lua out/perfil-lleno --load=lleno.sav --th64-save=lleno --th64-frames=0
# Reparto C++ / VM de Lua con callgrind (unos 5 min)
tools/bench_run.sh --callgrind bench/scenarios/perfil.lua out/cg-lleno --load=lleno.sav \
  --th64-save=lleno --th64-every=0 --th64-hours=150 --th64-frames=0
python3 -I tools/arquitectura/callgrind_resumen.py "$(ls -S out/cg-lleno/callgrind.out.* | head -1)" \
  bench/resultados/fase3/callgrind_lleno.json 150
# Mapa de módulos Lua, con el perfil
python3 -I tools/arquitectura/lua_modulos.py "$CORSIXTH_DIR/CorsixTH/Lua" bench/resultados/fase3/lua_modulos.json \
  out/perfil-lleno/perfil.json
```

Resultados en `bench/resultados/fase3/`: `cpp.json`, `tamanos.json`, `lua_modulos.json`, `perfil_lleno.json`, `perfil_mediano.json` y `callgrind_lleno.json`. El fichero de callgrind en bruto se queda fuera del repo.
