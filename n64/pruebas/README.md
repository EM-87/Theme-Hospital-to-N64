# ROMs de medida de la fase 4

Son las ROMs que sostienen las cifras de [`docs/04_construccion.md`](../../docs/04_construccion.md). **No son código del motor**: cada una mide una cosa y escribe el resultado por `debugf`. Las fuentes están tal como se ejecutaron. Las salidas resumidas están en [`bench/resultados/fase4/`](../../bench/resultados/fase4/).

Antes de nada:

```bash
source tools/env.sh
```

Cada ROM se compila con `make -C n64/pruebas/<dir>` y se ejecuta con `tools/run_ares.sh`. Ejemplo:

```bash
make -C n64/pruebas/hilos
ARES_SETTINGS="General/HomebrewMode=true" tools/run_ares.sh n64/pruebas/hilos/th64hilos.z64 out/hilos 60
```

`ARES_SETTINGS="General/HomebrewMode=true"` hace falta en las ROMs que usan `debug_init_emulog()`, EMUX o `profile.h` (`calibracion`, `sim`, `depuracion`, `hilos`, `dso`, `guardado`, `rom_grande`). Con `--no-expansion`, ares arranca con 4 MiB.

## Qué mide cada una

| Directorio | Qué mide | Resultado |
|---|---|---|
| `calibracion/` | Modelo de ciclos de ares: bucle de ALU, lectura de 1 MiB con fallos limpios, escritura, lectura con fallos sucios y aciertos (contadores EMUX; modo Homebrew) | 45 ciclos por línea con fallo limpio, 85 con sucio, 5 por vuelta con acierto |
| `sim/` | Ciclos de un módulo de prueba (A* sobre 128 × 128 y 361 entidades) con `TICKS_READ`, `profile.h` y EMUX. Con `-DSALIR_AL_FINAL` cierra ares desde la ROM. `SIM_OPT=-Os` compila el módulo con `-Os`. También sirve de objetivo para GDB | 715 ciclos por nodo con `-O2` y 683 con `-Os`; huella igual que en el PC (`host/main_host.c`) |
| `cpp/` | Tamaño de código de C frente a C++ con y sin excepciones, RTTI, STL e `iostream` (`make ROMNAME=<variante> EXTRA_CXXFLAGS=…`; la versión en C con `make -f c_base.mk`) | +88 KB solo por enlazar C++; +421 KB con `iostream` |
| `depuracion/` | Pantalla de `assertf` con backtrace y validador del RDP (`rdpq_debug_start`) | Mensajes y backtrace con fichero:línea |
| `rom_grande/` | `debugf` con una ROM de más de 64 MiB. Antes de compilar: `head -c 67108864 /dev/urandom > filesystem/relleno.bin` | Sin modo Homebrew no sale nada; con él, sí |
| `hilos/` | Coste de un hilo (`kernel.h`) y de una corrutina (`coroutine.h`) | 6.763 y 1.604 ciclos por cambio |
| `dso/` | `dlopen` y `dlclose` de un módulo pequeño y de uno con 548 KB de `bss` | 1,1 ms y 81 ms; la RAM se libera al cerrar |
| `dso_grande/` | El núcleo de Lua 5.5 como DSO frente a enlazado en el ejecutable (`make` y `make estatico`; usa las fuentes de `$TH64_WORK/src/lua55`) | +31 % de RAM como DSO; hueco de 236 KiB tras el primer `dlopen` |
| `guardado/` | EEPROM, SRAM, FlashRAM y Controller Pak: una ROM por tipo (`save_<tipo>.z64`), ejecutada dos veces con `Paths/Saves=<dir>/` en `ARES_SETTINGS` | Persistencia y tamaños máximos |
| `graficos/` | CPU y RSP+RDP de 1.000 dibujos de sprites CI8 por varias vías, espejo, cambio de paleta, `rdpq_sprite_blit` y texto con una fuente de TH | `rdpq_tex_blit` 3.425–4.248 ciclos; a mano ~1.630; solo el rectángulo, 173 |
| `audio/` | RAM y CPU del mezclador con efectos VADPCM (con y sin Huffman) y música MID64 + SF64 | 8 efectos: 16,7 % de CPU con Huffman, 2,96 % sin él |
| `audio_init/` | RAM de `audio_init` y `mixer_init` por separado (`make DEFS=-DNCH=16`) | `mixer_init` reserva 136 KiB solo porque inicia `rspq`; en el juego ya lo ha hecho `rdpq` |
| `video/` | `fmv_play` con H.264: fotogramas mostrados, fps y pico de RAM, con su propio display o con el del juego | Sin saltos a 15 y 25 fps; +1,6 a +2,9 MiB de pico |
| `raton/` | Detección del ratón de N64 conectado en ares desde el menú (`run_mouse.sh` lo hace con `xdotool`) | Detectado en caliente como `JOYPAD_STYLE_MOUSE` |
| `ram4/` | RAM libre con 4 MiB según framebuffers, z-buffer, frecuencia de audio y formato de la música. Variantes en `variantes.txt` (`make V=<nombre> DEFS="…"`) | De 2.668 a 3.268 KiB libres |
| `bfs/` | Recorrido en anchura de 10.000 casillas, el peor caso de la comprobación de zonas bloqueadas | 19,0 ms; 9,0 ms con un mapa de bits |
| `nucleo/` | Núcleo de ejemplo en C portable (pool, Q16.16, xorshift32) con 60.045 comprobaciones; `CMakeLists.txt` para el PC (`-DTH_SANITIZE=ON` para ASan y UBSan) y `Makefile.n64` para la ROM; `bugs/` con tres errores a propósito; `gen_ccdb.py` genera `compile_commands.json` | Misma huella en el PC y en la N64; coste por operación numérica |

## Ficheros de `filesystem/` que hay que generar

Ninguno está en el repositorio. Unos son sintéticos y otros salen de los datos del juego o de bancos de sonido con su propia licencia.

- **`graficos/`:**
  - `ci8_impar.sprite`: PNG sintético de [`tools/construccion/sprites/gen_png.py`](../../tools/construccion/sprites/gen_png.py), convertido con `mksprite -c 0 -f CI8`;
  - `font01v.font64`: fuente de TH convertida con [`tools/construccion/fuentes/th2bmfont.py`](../../tools/construccion/fuentes/th2bmfont.py) y luego `mkfont -c 0 --format CI4`. Necesita los datos del juego.
- **`audio/`:**
  - `sfx.wav64`: un seno de 2 s a 22.050 Hz convertido con `audioconv64`; `sfxnh.wav64`, lo mismo con `--wav-compress "vadpcm,huffman=false"`;
  - `TimGM6mb.sf64`: `audioconv64` sobre `/usr/share/sounds/sf2/TimGM6mb.sf2` (paquete `timgm6mb-soundfont`, GPL-2);
  - `FORTUNE.mid64`: la música del juego pasada a MIDI con `tools/datos/xmi2mid` y luego a MID64 con `audioconv64`.
- **`video/`:** vídeos sintéticos de 12 s a 320 × 240, generados con `ffmpeg -f lavfi -i testsrc=size=320x240:rate=15` (y `rate=25`) con un tono, y convertidos con `videoconv64 --codec h264 --quality 70`.
- **`ram4/`:**
  - `tono.wav64`: seno sintético;
  - `musica_opus.wav64` y `musica_vadpcm.wav64`: 20 s de acordes sintéticos convertidos con `--wav-compress 3` y `--wav-compress 1 --wav-resample 22050`;
  - `gm.sf64`: TimGM6mb;
  - `florestan-full.sf64`: el banco de `examples/audioplayer` de libdragon;
  - `musica.mid64`: una pieza del juego.
- **`dso/` y `dso_grande/`:** los `.dso` los genera su propio `Makefile`.
