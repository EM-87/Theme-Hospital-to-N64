// La música de Theme Hospital como suena en la N64, con la
// configuración de sonido del motor (motor/platform/n64/main.c,
// comprobar_sonido): 22.050 Hz, mezclador de 32 canales, 16 voces del
// sintetizador SF64 en modo GM1.
//
// No se toca por el altavoz: la ROM pide al mezclador cada bloque de audio
// (mixer_poll, lo mismo que hace mixer_try_play para la salida de audio, con
// el mismo tamaño de bloque) y lo vuelca en base64 por EMUX (modo Homebrew de
// ares). Así la grabación es exactamente lo que mezcla la consola, sin
// depender de que ares vaya al ritmo del tiempo real (si va más deprisa,
// tira el audio que no cabe en la salida). tools/musica/grabar_ares.sh la
// ejecuta y tools/musica/decodificar.py rehace los WAV.
//
// Toca todos los .mid64 de su sistema de ficheros, en orden alfabético: por
// defecto las 8 piezas del motor; con `make MUSICA=<dir>`, los de otro
// directorio (por ejemplo una pieza separada por canales con
// tools/musica/solo.py), que debe tener también banco.sf64.
//
// Cada pieza suena su duración más 2 s (las notas que aún suenan): el
// reproductor no dice cuándo acaba, y mid64player_tell_ms se queda en el
// último suceso MIDI.
#include <libdragon.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PIEZAS 64

static int por_nombre(const void *a, const void *b)
{
    return strcmp(a, b);
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// "P " + base64 de n bytes + "\n", en memoria sin caché (ares la lee
// directamente de la RAM).
static void volcar(const uint8_t *p, int n, char *linea)
{
    int k = 0;
    linea[k++] = 'P';
    linea[k++] = ' ';
    for (int i = 0; i < n; i += 3) {
        const uint32_t v = (uint32_t)p[i] << 16 | (uint32_t)(i + 1 < n ? p[i + 1] : 0) << 8 | (i + 2 < n ? p[i + 2] : 0);
        linea[k++] = B64[v >> 18 & 63];
        linea[k++] = B64[v >> 12 & 63];
        linea[k++] = i + 1 < n ? B64[v >> 6 & 63] : '=';
        linea[k++] = i + 2 < n ? B64[v & 63] : '=';
    }
    linea[k++] = '\n';
    emux_logn(linea, k);
}

int main(void)
{
    debug_init(DEBUG_FEATURE_LOG_EMU);
    if (!(emux_detect(1) & EMUX_FEAT1_LOG)) { // subcódigo 1: las funciones 0x20–0x3F
        debugf("TH64 musica: hace falta el modo Homebrew de ares (EMUX)\n");
        for (;;) {
        }
    }
    dfs_init(DFS_DEFAULT_LOCATION);
    audio_init(22050, AUDIO_DEFAULT_LATENCY);
    mixer_init(32);
    sf64_bank_t *banco = sf64_load("rom:/banco.sf64");
    sf64_synth_t *synth = sf64_synth_create(banco);
    sf64_synth_set_mode(synth, SF64_MODE_GM1);
    for (int i = 0; i < 16; i++)
        mixer_ch_set_limits(i, 16, 96000, 0);
    sf64_synth_set_channels(synth, 0, 16, MIXER_PRIORITY_MUSIC);
    const int bloque = audio_get_buffer_length();
    const int frecuencia = audio_get_frequency();
    int16_t *pcm = malloc_uncached(bloque * 4);
    char *linea = malloc_uncached(4 + (bloque * 4 + 2) / 3 * 4 + 4);
    debugf("TH64 musica: %d presets; %d Hz, bloques de %d muestras\n", sf64_preset_count(banco), frecuencia, bloque);
    static char piezas[MAX_PIEZAS][64];
    int n = 0;
    dir_t d;
    for (int r = dir_findfirst("rom:/", &d); r == 0; r = dir_findnext("rom:/", &d)) {
        const size_t l = strlen(d.d_name);
        if (d.d_type == DT_REG && l > 6 && l - 6 < sizeof piezas[0] && !strcmp(d.d_name + l - 6, ".mid64")) {
            assertf(n < MAX_PIEZAS, "más de %d piezas", MAX_PIEZAS);
            memcpy(piezas[n], d.d_name, l - 6);
            piezas[n++][l - 6] = 0;
        }
    }
    qsort(piezas, n, sizeof piezas[0], por_nombre);
    for (int k = 0; k < n; k++) {
        char ruta[80];
        snprintf(ruta, sizeof ruta, "rom:/%.63s.mid64", piezas[k]);
        mid64player_t *mid = mid64player_load(ruta);
        mid64player_set_loop(mid, false);
        const uint32_t dur = mid64player_get_duration_ms(mid);
        debugf("TH64 musica: %s empieza (%lu ms)\n", piezas[k], (unsigned long)dur);
        mid64player_play(mid, sf64_synth_midi_target(synth));
        const int64_t total = (int64_t)(dur + 2000) * frecuencia / 1000;
        for (int64_t hechas = 0; hechas < total; hechas += bloque) {
            mixer_poll(pcm, bloque);
            volcar((const uint8_t *)pcm, bloque * 4, linea);
        }
        mid64player_stop(mid);
        mid64player_close(mid);
        // Que no quede nada sonando para la pieza siguiente.
        for (int i = 0; i < 4; i++)
            mixer_poll(pcm, bloque);
        debugf("TH64 musica: %s acaba\n", piezas[k]);
    }
    debugf("TH64 musica: fin\n");
    emux_ioctl_exit();
    for (;;) {
    }
}
