// Hola mundo de la fase 0: comprueba la toolchain, la RAM que ve la consola
// (4 MiB, u 8 MiB con Expansion Pak) y la salida de depuración por ISViewer.
#include <libdragon.h>

int main(void)
{
    debug_init_emulog();

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    rdpq_text_register_font(1, rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));

    const int ram_kib = get_memory_size() / 1024;
    const bool expanded = is_memory_expanded();
    heap_stats_t heap;
    sys_get_heap_stats(&heap);

    debugf("TH64 hola: RDRAM %d KiB, Expansion Pak %s, heap %d/%d KiB\n",
           ram_kib, expanded ? "si" : "no", heap.used / 1024, heap.total / 1024);

    uint32_t last_ms = get_ticks_ms();
    for (uint32_t frame = 0;; frame++) {
        surface_t *disp = display_get();
        rdpq_attach_clear(disp, NULL);
        rdpq_text_printf(NULL, 1, 24, 40, "Theme Hospital N64 - fase 0");
        rdpq_text_printf(NULL, 1, 24, 64, "RDRAM: %d KiB", ram_kib);
        rdpq_text_printf(NULL, 1, 24, 76, "Expansion Pak: %s", expanded ? "si" : "no");
        rdpq_text_printf(NULL, 1, 24, 88, "Heap: %d / %d KiB", heap.used / 1024, heap.total / 1024);
        rdpq_text_printf(NULL, 1, 24, 112, "Frame: %lu", frame);
        rdpq_detach_show();

        // Reloj de la consola (en ares, el emulado), no el del ordenador anfitrión.
        if (frame > 0 && frame % 300 == 0) {
            uint32_t now_ms = get_ticks_ms();
            debugf("TH64 hola: frame %lu, %.1f fps\n", frame, 300000.0f / (now_ms - last_ms));
            last_ms = now_ms;
        }
    }
}
