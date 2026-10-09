# Theme Hospital → Nintendo 64 — Plan de arranque

## Contexto

Objetivo: llevar Theme Hospital (Bullfrog, 1997) a N64 sobre **libdragon**, usando **CorsixTH** (reimplementación open source, MIT) como referencia y los **datos originales del juego** (GOG/EA), que cada usuario aporta. No se distribuye ningún asset propietario.

Por qué tiene sentido: la N64 casi no tiene juegos de gestión fuera de Japón, y el hardware alcanza (el original corría con 8 MB en PC y existió una versión de PS1 con 2 MB).

Riesgo principal: **CorsixTH no cabe tal cual.** El port de Wii (≈88 MB de RAM) ya necesitó una caché LRU para gráficos y sonido. La N64 tiene 4 MB, u 8 MB con Expansion Pak.

Hipótesis de trabajo (a validar en la fase 1): **camino 2 híbrido**. Motor propio en C/C++ para libdragon que lee los datos de DOS. La lógica Lua de CorsixTH sirve como especificación y no se ejecuta. Se reutilizan partes del núcleo C++ de CorsixTH si resultan aprovechables.

La alternativa (**camino 1**) es ejecutar el Lua de CorsixTH en N64 cambiando el backend SDL3 por libdragon. Solo se considera si las mediciones lo permiten.

Objetivo de hardware: **N64 + Expansion Pak (8 MB)**, 320×240, cartucho de hasta 64 MB.

## Fuentes

- CorsixTH: https://github.com/CorsixTH/CorsixTH
- Port de Wii de CorsixTH: https://wiibrew.org/wiki/CorsixTH (big-endian como N64; incluye correcciones de endianness y una caché LRU)
- libdragon: https://github.com/DragonMinded/libdragon (C++ con excepciones soportado; resolución arbitraria con `display_init`)
- Referencia de estructura "motor libre + datos originales" en N64: https://github.com/meeq/AnotherWorld-N64 (backend N64 como opción de CMake junto a SDL2)
- Emulador de desarrollo: ares (fiel al hardware)

## Reglas del proyecto

- No subir al repositorio ningún archivo de datos de Theme Hospital. Los datos van en una ruta local, configurada por variable de entorno (`TH_DATA_DIR`) e ignorada en `.gitignore`.
- Cada fase termina con un documento de hallazgos en `docs/` que incluya números concretos, no impresiones.
- No empezar la fase 5 (código N64) sin haber cerrado la fase 4 (decisión).

---

## Fase 0 — Entorno

1. Clonar CorsixTH y compilarlo en Linux con soporte de Tracy (feature `tracy` de vcpkg) y símbolos de depuración.
2. Configurar `TH_DATA_DIR` con los datos de GOG. Si vienen en el instalador, extraerlos con `innoextract`.
3. Instalar la toolchain de libdragon (rama estable y unstable/preview; documentar cuál se elige y por qué), ares y heaptrack o valgrind massif.
4. Compilar y ejecutar en ares un "hola mundo" de libdragon y un ejemplo de los incluidos.

**Entregable:** `docs/00_entorno.md` con versiones exactas y comandos reproducibles.

## Fase 1 — Medir CorsixTH en PC

Objetivo: saber cuánto pesa la lógica y cuánto los assets.

1. Preparar **tres partidas guardadas de referencia**: inicio de nivel, hospital mediano (unos 50 pacientes) y hospital lleno (el máximo realista de la campaña, con emergencia o epidemia activa). Guardarlas en `bench/saves/` (son ficheros nuestros, no datos del juego).
2. Para cada partida, medir:
   - Heap de Lua: `collectgarbage("count")` muestreado cada N ticks, con pico y media.
   - Heap de C++: heaptrack/massif, desglosado por subsistema (gráficos, mapa, sonido, otros).
   - Tiempo por tick de simulación, separado del render, con Tracy.
   - Número de entidades activas (pacientes, personal, objetos).
3. Estimar el coste en N64 con un factor de CPU documentado. VR4300 a 93,75 MHz frente a la CPU de medición; dejar explícitas las suposiciones.

**Entregable:** `docs/01_mediciones.md` con tablas de pico y media por escenario, y una conclusión: ¿cabría el estado de la simulación en ≤4 MB, dejando el resto para gráficos y audio?

## Fase 2 — Inventario de los datos originales

1. Listar todos los archivos de `TH_DATA_DIR` con su tamaño, agrupados por tipo: sprites/animaciones, tablas, mapas/niveles, sonido, música, vídeo y texto.
2. Usando el código de carga de CorsixTH como referencia de formatos, documentar cada formato: compresión, paletas, estructura de animaciones.
3. Calcular cuánto ocuparía cada grupo en el cartucho:
   - tal cual,
   - comprimido con la compresión de assets de libdragon,
   - reescalado a 320×240 (el original va a 640×480).
4. Proponer qué se queda fuera si hace falta. Primeros candidatos: vídeos FMV y música en su formato original (alternativa: MIDI o XM).

**Entregable:** `docs/02_datos.md` con la tabla de tamaños y un presupuesto de cartucho frente a 64 MB.

## Fase 3 — Leer el código

1. **Núcleo C++ de CorsixTH** (`CorsixTH/Src`): qué subsistemas están en C++ (mapa, renderizado de sprites, pathfinding, audio...) y cuáles están acoplados a SDL3. Para cada uno, indicar si es reutilizable en N64, reutilizable con cambios o descartable.
2. **Lógica Lua de CorsixTH**: mapa de módulos (humanoides, habitaciones, enfermedades, economía, eventos, UI) con sus dependencias. Dejar señalado qué es lógica de juego pura y qué es UI.
3. **Port de Wii**: localizar el código fuente (enlazado desde WiiBrew) y documentar la caché LRU, los parches de endianness y qué recortes hizo para caber.

**Entregable:** `docs/03_arquitectura.md` con un diagrama de módulos y una tabla de reutilización.

## Fase 4 — Decisión

Con los datos de las fases 1 a 3, redactar `docs/04_decision.md`:

- Camino 1 (Lua en N64), camino 2 (motor propio) o híbrido, justificado con números.
- Presupuesto de RAM: simulación, caché de gráficos, framebuffers, audio y pila/heap de Lua si aplica.
- Recortes de diseño necesarios: tamaño máximo de hospital, número máximo de pacientes simultáneos, resolución.
- Esquema de controles: stick como cursor, botones C para cámara y menús, y decidir si se da soporte al N64 Mouse.
- Riesgos abiertos.

**Aquí se para y se revisa con Eduardo antes de seguir.**

## Fase 5 — Prototipos en N64 (solo tras aprobar la fase 4)

Hitos pequeños y verificables en ares, en este orden:

1. **Lectura de datos**: la ROM carga los datos originales desde el filesystem de libdragon (DFS) y muestra un sprite con su paleta correcta.
2. **Mapa**: renderizar el mapa isométrico de un nivel con scroll mediante el stick a 30 fps estables.
3. **Animación**: un humanoide animado que camina por el mapa siguiendo un pathfinding.
4. **Bucle mínimo**: un recepcionista, una consulta de GP, pacientes que llegan, hacen cola, son diagnosticados y se van, con contador de dinero.
5. **Medición en N64**: RAM libre, tiempo por tick y fps con 50 pacientes. Comparar con la estimación de la fase 1.

Cada hito se cierra con un commit, una captura de ares y una nota en `docs/05_prototipo.md` con las cifras.

---

## Fuera de alcance por ahora

Multijugador, hospitales rivales con IA (CorsixTH tampoco los tiene completos), vídeos FMV, editor de mapas y localización a otros idiomas.
