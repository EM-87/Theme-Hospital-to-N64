// Comprobación: RAM de audio_init y mixer_init por separado, con 16 y 32 canales,
// y tras la primera llamada a mixer_try_play.
#include <libdragon.h>
static int used(void) { heap_stats_t h; sys_get_heap_stats(&h); return h.used; }
int main(void) {
    debug_init_isviewer();
    int h0 = used();
    audio_init(32000, 4);
    int h1 = used();
    mixer_init(NCH);
    int h2 = used();
    for (int i = 0; i < 50; i++) { mixer_try_play(); wait_ms(5); }
    int h3 = used();
    debugf("AUDIORAM canales=%d audio_init %d B, mixer_init %d B, tras mixer_try_play %d B, total %d B\n",
           NCH, h1 - h0, h2 - h1, h3 - h2, h3 - h0);
    debugf("AUDIORAM fin\n");
    for (;;) {}
}
