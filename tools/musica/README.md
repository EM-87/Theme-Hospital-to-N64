# Música: la N64 frente a fluidsynth (fase 5)

Comparan lo que suena en la N64 (MID64 + el sintetizador SF64 de libdragon, con la configuración de sonido del motor) con lo que sintetiza fluidsynth con el mismo banco (TimGM6mb). Los resultados están en [`docs/05_prototipo.md`](../../docs/05_prototipo.md#música-la-n64-frente-a-fluidsynth).

Ninguna salida entra en el repositorio: la música sale de los datos del juego. Todo va a `$TH64_WORK/resultados/musica`.

```bash
source tools/env.sh
make -C motor && make -C n64/pruebas/musica
tools/musica/grabar_ares.sh      # la N64 en ares: unos 4 min para los 22 min de música
tools/musica/fluidsynth.sh       # las referencias de fluidsynth: menos de 1 min
tools/musica/comparar_todo.sh    # cifras pieza a pieza y un A/B en MP3 por pieza
```

| Fichero | Qué hace |
|---|---|
| [`n64/pruebas/musica`](../../n64/pruebas/musica) | ROM que toca todos los `.mid64` de su sistema de ficheros y vuelca por EMUX (modo Homebrew de ares) cada bloque que mezcla la consola. No depende de que ares vaya al ritmo del tiempo real. Con `make MUSICA=<dir>` toca los de otro directorio |
| `grabar_ares.sh [dir]` | Ejecuta la ROM y rehace un WAV por pieza a 22.047 Hz (`<pieza>_n64.wav`) y otro a 48 kHz (`<pieza>_n6448.wav`, con soxr) |
| `decodificar.py <isviewer.log> <dir>` | Pasa el volcado a WAV y comprueba que no falte ningún bloque |
| `fluidsynth.sh [dir]` | Sintetiza cada pieza a 48 kHz como en el PC (`_pc`), sin reverberación ni coro (`_seco`), además sin filtro ni LFO (`_sinlfo`) y además con 16 voces (`_sinlfo16`) |
| `banco_n64.py <in.sf2> <out.sf2>` | Copia de un SF2 sin lo que no hace el sintetizador SF64: filtro, envolvente de modulación al filtro y LFO |
| `comparar.py <a.wav> <b.wav>` | Nivel, centroide espectral, energía por bandas, distancia de timbre (espectro a largo plazo por tercios de octava, con el nivel igualado), correlación de las envolventes de 50 ms, desfase y deriva. Con `--ab` escribe un fragmento de cada una para escuchar |
| `comparar_todo.sh [dir]` | `comparar.py` de la N64 contra las cuatro referencias, en cada pieza, y los A/B (`ab/<pieza>.mp3`: 25 s de la N64, 1 s de silencio y lo mismo en fluidsynth, al mismo nivel) |
| `solo.py <in.mid> <prefijo>` | Un MIDI por canal, con todos los controladores pero solo las notas de ese canal, para medir cada instrumento por separado |
