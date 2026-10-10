// Medida de la fase 4: cuánta RAM queda para el juego después de inicializar
// lo que cualquier versión necesitará de libdragon (vídeo a 320x240 con triple
// buffer, RDP, mandos, sistema de ficheros, descompresores y audio con
// mezclador). Escribe por ISViewer el heap tras cada paso.
#include <libdragon.h>

static void step(const char *what)
{
    heap_stats_t h;
    sys_get_heap_stats(&h);
    debugf("TH64 ram: %-40s usado %5d KiB  libre %5d KiB  total %5d KiB\n",
           what, h.used / 1024, (h.total - h.used) / 1024, h.total / 1024);
}

int main(void)
{
    debug_init_emulog();
    debugf("TH64 ram: RDRAM %d KiB, Expansion Pak %s\n", get_memory_size() / 1024,
           is_memory_expanded() ? "si" : "no");
    step("arranque");
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    step("display 320x240 16 bpp, 3 buffers");
    rdpq_init();
    step("rdpq");
    joypad_init();
    step("joypad");
    dfs_init(DFS_DEFAULT_LOCATION);
    step("dfs");
    asset_init_compression(2);
    asset_init_compression(3);
    step("descompresores aPLib y Shrinkler");
    audio_init(32000, AUDIO_DEFAULT_LATENCY);
    step("audio 32 kHz");
    mixer_init(16);
    step("mixer 16 canales");
    wav64_init_compression(3);
    step("decodificador Opus");
    debugf("TH64 ram: fin\n");
    for (;;) {
        surface_t *disp = display_get();
        rdpq_attach_clear(disp, NULL);
        rdpq_detach_show();
    }
}
