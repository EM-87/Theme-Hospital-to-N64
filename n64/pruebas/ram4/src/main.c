// Fase 4b (modo demo): RAM libre con 4 MiB según las opciones de vídeo y audio.
// Variantes en compilación:
//   NBUF   framebuffers (2 o 3)
//   ZBUF   1 = pide un z-buffer con display_get_zbuf()
//   AFREQ  frecuencia de audio (22050 o 32000)
//   MIXCH  canales del mezclador
//   MUS    0 sin música; 1 MIDI (MID64 + SF64 TimGM6mb, MUSV voces);
//          2 pista Opus estéreo en streaming; 3 pista VADPCM estéreo 22.050 Hz
//   SFXN   efectos sonando a la vez (wav64 VADPCM mono 22.050 Hz)
//   SFFIT  1 = al final carga y cierra dos bancos SF64 para ver su coste en RAM
// Al final mide el mayor bloque que malloc puede dar y la suma de bloques.
#include <libdragon.h>
#include <malloc.h>

#ifndef NBUF
#define NBUF 3
#endif
#ifndef ZBUF
#define ZBUF 0
#endif
#ifndef AFREQ
#define AFREQ 32000
#endif
#ifndef MIXCH
#define MIXCH 16
#endif
#ifndef MUS
#define MUS 1
#endif
#ifndef MUSV
#define MUSV 12
#endif
#ifndef SFXN
#define SFXN 4
#endif
#ifndef SFFIT
#define SFFIT 0
#endif

static int heap_used(void)
{
    heap_stats_t h;
    sys_get_heap_stats(&h);
    return h.used;
}

static void step(const char *what)
{
    heap_stats_t h;
    sys_get_heap_stats(&h);
    debugf("TH64 ram4: %-44s usado %7d B (%5d KiB)  libre %7d B (%5d KiB)\n",
           what, h.used, h.used / 1024, h.total - h.used, (h.total - h.used) / 1024);
}

// Mayor bloque que da malloc (búsqueda binaria) y suma de bloques sacando
// siempre el mayor posible hasta que quede menos de 1 KiB.
static void probe_free(void)
{
    static void *blocks[64];
    int n = 0, total = 0, largest = 0;
    while (n < 64) {
        int lo = 0, hi = 8 * 1024 * 1024;
        while (hi - lo > 64) {
            int mid = (lo + hi) / 2;
            // volatile: GCC elimina un malloc+free sin uso y da p por no nulo
            void * volatile p = malloc(mid);
            if (p) { free(p); lo = mid; } else hi = mid;
        }
        if (lo < 1024) break;
        blocks[n] = malloc(lo);
        if (!blocks[n]) break;
        if (n == 0) largest = lo;
        total += lo;
        n++;
    }
    for (int i = 0; i < n; i++) free(blocks[i]);
    debugf("TH64 ram4: malloc mayor bloque %d B (%d KiB), suma %d B (%d KiB) en %d bloques\n",
           largest, largest / 1024, total, total / 1024, n);
}

static void pump(int ms)
{
    uint32_t t0 = TICKS_READ();
    while (TICKS_DISTANCE(t0, TICKS_READ()) < (int)TICKS_FROM_MS(ms))
        mixer_try_play();
}

int main(void)
{
    debug_init_emulog();
    debugf("TH64 ram4: config NBUF=%d ZBUF=%d AFREQ=%d MIXCH=%d MUS=%d MUSV=%d SFXN=%d\n",
           NBUF, ZBUF, AFREQ, MIXCH, MUS, MUSV, SFXN);
    debugf("TH64 ram4: RDRAM %d KiB, Expansion Pak %s\n", get_memory_size() / 1024,
           is_memory_expanded() ? "si" : "no");
    step("arranque");
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, NBUF, GAMMA_NONE, FILTERS_RESAMPLE);
    step("display");
#if ZBUF
    display_get_zbuf();
    step("z-buffer");
#endif
    rdpq_init();
    step("rdpq");
    joypad_init();
    dfs_init(DFS_DEFAULT_LOCATION);
    asset_init_compression(2);
    step("joypad, dfs, aPLib");
    audio_init(AFREQ, AUDIO_DEFAULT_LATENCY);
    debugf("TH64 ram4: frecuencia real %d Hz\n", audio_get_frequency());
    step("audio");
    mixer_init(MIXCH);
    wav64_init_compression(1);
#if MUS == 2
    wav64_init_compression(3);
#endif
    step("mixer");
    int base_audio = heap_used();
    int a = heap_used();

#if MUS == 1
    sf64_bank_t *bank = sf64_load("rom:/gm.sf64");
    debugf("TH64 ram4: sf64_load(TimGM6mb) %d B\n", heap_used() - a);
    a = heap_used();
    sf64_synth_t *synth = sf64_synth_create(bank);
    sf64_synth_set_channels(synth, 0, MUSV, MIXER_PRIORITY_MUSIC);
    for (int i = 0; i < MUSV; i++)
        mixer_ch_set_limits(i, 0, 96000, 0);
    mid64player_t *mid = mid64player_load("rom:/musica.mid64");
    mid64player_set_loop(mid, true);
    mid64player_play(mid, sf64_synth_midi_target(synth));
    debugf("TH64 ram4: synth + mid64 cargados %d B\n", heap_used() - a);
    a = heap_used();
    int maxpoly = 0;
    for (int k = 0; k < 40; k++) {
        pump(100);
        int poly = 0;
        for (int i = 0; i < MUSV; i++) if (mixer_ch_playing(i)) poly++;
        if (poly > maxpoly) maxpoly = poly;
    }
    debugf("TH64 ram4: musica 4 s: max %d voces a la vez, bufers de voz %d B\n", maxpoly, heap_used() - a);
    step("musica MIDI sonando");
#elif MUS == 2 || MUS == 3
    static wav64_t mus;
    wav64_open(&mus, MUS == 2 ? "rom:/musica_opus.wav64" : "rom:/musica_vadpcm.wav64");
    wav64_set_loop(&mus, true);
    // Opus decodifica siempre a 48 kHz: hay que subir el límite del canal
    mixer_ch_set_limits(0, 0, 48000, 0);
    wav64_play(&mus, 0);
    pump(2000);
    debugf("TH64 ram4: musica en streaming %d B\n", heap_used() - a);
    step(MUS == 2 ? "musica Opus sonando" : "musica VADPCM sonando");
#endif
    int first_sfx = (MUS == 1) ? MUSV : (MUS ? 2 : 0);
#if SFXN > 0
    a = heap_used();
    static wav64_t sfx[SFXN];
    for (int i = 0; i < SFXN; i++) {
        wav64_open(&sfx[i], "rom:/tono.wav64");
        wav64_set_loop(&sfx[i], true);
        wav64_play(&sfx[i], first_sfx + i);
    }
    pump(500);
    debugf("TH64 ram4: %d efectos %d B (%d B por canal)\n", SFXN, heap_used() - a, (heap_used() - a) / SFXN);
    step("efectos sonando");
#endif
    debugf("TH64 ram4: audio en total (tras mixer_init) %d B (%d KiB)\n",
           heap_used() - base_audio, (heap_used() - base_audio) / 1024);
    probe_free();
#if SFFIT
    const char *banks[] = { "rom:/gm.sf64", "rom:/florestan-full.sf64" };
    for (int i = 0; i < 2; i++) {
        a = heap_used();
        sf64_bank_t *b = sf64_load(banks[i]);
        debugf("TH64 ram4: sf64_load(%s) %d B, %d presets\n", banks[i],
               heap_used() - a, sf64_preset_count(b));
        sf64_close(b);
    }
#endif
    debugf("TH64 ram4: fin\n");
    for (;;) {
        mixer_try_play();
        surface_t *disp = display_try_get();
        if (disp) {
            rdpq_attach_clear(disp, NULL);
            rdpq_detach_show();
        }
    }
}
