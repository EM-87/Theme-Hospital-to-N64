// Prueba de audio para TH64: RAM y CPU del mezclador con efectos VADPCM
// (wav64) y música MIDI (MID64 + SF64, sf64_synth), en tres escenarios.
// La CPU se mide como tiempo dentro de mixer_try_play() frente al tiempo total;
// "espera RSP" es la parte de ese tiempo que la CPU pasa esperando al RSP.
#include <libdragon.h>

// Contabilidad interna de libdragon (src/accounting_internal.h), la que usa
// examples/audioplayer. Se declaran aquí para no incluir cabeceras internas.
extern uint64_t acct_get_ticks(int cat);
#define ACCT_CAT_RSP  2
#define ACCT_CAT_RSPQ 4
#define ACCT_CAT_PI   7

#define VOICES 16
#define SFX     8

static int used(void)
{
    heap_stats_t h;
    sys_get_heap_stats(&h);
    return h.used;
}

static volatile uint32_t sink;
static uint32_t base_units;
// Cuenta unidades de trabajo fijo (~1 ms cada una) completadas en "segundos",
// llamando a mixer_try_play() entre unidad y unidad (como haría el bucle del juego).
static uint32_t trabajo(float segundos, bool audio)
{
    uint32_t t0 = get_ticks(), units = 0;
    while (TICKS_DISTANCE(t0, get_ticks()) < (int)(segundos * TICKS_PER_SECOND)) {
        for (int k = 0; k < 20000; k++) sink += k;
        units++;
        if (audio) mixer_try_play();
    }
    return units;
}
static void medir2(const char *nombre)
{
    uint32_t u = trabajo(6, true);
    debugf("TH64 audio: %-34s trabajo %lu u (base %lu): el audio consume %5.2f %% de la CPU\n",
           nombre, (unsigned long)u, (unsigned long)base_units, 100.0 * (1.0 - (double)u / base_units));
}
static void medir(const char *nombre, float segundos)
{
    uint32_t t0 = get_ticks();
    uint64_t in = 0, rsp = 0, pi = 0;
    while (TICKS_DISTANCE(t0, get_ticks()) < (int)(segundos * TICKS_PER_SECOND)) {
        uint64_t r0 = acct_get_ticks(ACCT_CAT_RSP) + acct_get_ticks(ACCT_CAT_RSPQ);
        uint64_t p0 = acct_get_ticks(ACCT_CAT_PI);
        uint32_t a = get_ticks();
        mixer_try_play();
        uint32_t b = get_ticks();
        in += b - a;
        rsp += acct_get_ticks(ACCT_CAT_RSP) + acct_get_ticks(ACCT_CAT_RSPQ) - r0;
        pi += acct_get_ticks(ACCT_CAT_PI) - p0;
    }
    uint32_t tot = TICKS_DISTANCE(t0, get_ticks());
    debugf("TH64 audio: %-34s CPU en mixer_try_play %5.2f %% (espera RSP %5.2f %%, espera PI %5.2f %%)\n",
           nombre, 100.0 * in / tot, 100.0 * rsp / tot, 100.0 * pi / tot);
}

int main(void)
{
    debug_init_emulog();
    dfs_init(DFS_DEFAULT_LOCATION);
    int h0 = used();
    audio_init(32000, 4);
    mixer_init(32);
    int h1 = used();
    debugf("TH64 audio: audio_init(32 kHz, 4 búferes) + mixer_init(32): %d KiB\n", (h1 - h0) / 1024);

    wav64_t *sfx = wav64_load("rom:/sfx.wav64", NULL);
    wav64_set_loop(sfx, true);
    int h2 = used();
    debugf("TH64 audio: wav64_load (VADPCM 22 kHz, 2 s): %d B\n", h2 - h1);

    sf64_bank_t *bank = sf64_load("rom:/TimGM6mb.sf64");
    int h3 = used();
    debugf("TH64 audio: sf64_load TimGM6mb: %d KiB (fichero SF64 de 1775 KiB)\n", (h3 - h2) / 1024);
    sf64_synth_t *synth = sf64_synth_create(bank);
    sf64_synth_set_mode(synth, SF64_MODE_GM1);
    // Las muestras del SF2 son de 44,1 kHz y el tono las sube: sin esto, mixer.c:593 aborta
    // ("frequency 44100.0 exceeds configured limit 31995.0").
    for (int i = 0; i < VOICES; i++) mixer_ch_set_limits(i, 16, 96000, 0);
    sf64_synth_set_channels(synth, 0, VOICES, MIXER_PRIORITY_MUSIC);
    int h4 = used();
    debugf("TH64 audio: sf64_synth_create + %d voces: %d KiB\n", VOICES, (h4 - h3) / 1024);
    mid64player_t *mid = mid64player_load("rom:/FORTUNE.mid64");
    mid64player_set_loop(mid, true);
    int h5 = used();
    debugf("TH64 audio: mid64player_load FORTUNE: %d KiB; duración %lu ms\n", (h5 - h4) / 1024,
           (unsigned long)mid64player_get_duration_ms(mid));

    base_units = trabajo(6, false);
    medir2("mezclador sin canales");
    // Escenario 1: solo efectos
    wav64_play(sfx, VOICES);
    medir2("1 efecto VADPCM 22 kHz");
    for (int i = 1; i < SFX; i++) wav64_play(sfx, VOICES + i);
    medir2("8 efectos VADPCM 22 kHz");
    int h6 = used();
    debugf("TH64 audio: heap tras sonar 8 efectos: +%d KiB\n", (h6 - h5) / 1024);
    for (int i = 0; i < SFX; i++) mixer_ch_stop(VOICES + i);

    wav64_t *sfxnh = wav64_load("rom:/sfxnh.wav64", NULL);
    wav64_set_loop(sfxnh, true);
    for (int i = 0; i < SFX; i++) wav64_play(sfxnh, VOICES + i);
    medir2("8 efectos VADPCM sin Huffman");
    for (int i = 0; i < SFX; i++) mixer_ch_stop(VOICES + i);

    // Escenario 2: solo música
    mid64player_play(mid, sf64_synth_midi_target(synth));
    medir2("MIDI FORTUNE, 16 voces SF64");
    int h7 = used();
    debugf("TH64 audio: heap tras 10 s de MIDI: +%d KiB\n", (h7 - h6) / 1024);

    // Escenario 3: música + efectos
    for (int i = 0; i < SFX; i++) wav64_play(sfx, VOICES + i);
    medir2("MIDI 16 voces + 8 efectos");
    for (int i = 0; i < SFX; i++) wav64_play(sfxnh, VOICES + i);
    medir2("MIDI 16 voces + 8 efectos sin Huffman");
    int h8 = used();
    debugf("TH64 audio: heap tras música + efectos: +%d KiB; total audio %d KiB\n", (h8 - h7) / 1024, (h8 - h0) / 1024);
    int act = 0;
    for (int i = 0; i < 32; i++) act += mixer_ch_playing(i);
    debugf("TH64 audio: canales sonando ahora: %d\n", act);
    debugf("TH64 audio: fin\n");
    for (;;) mixer_try_play();
}
