// Punto de entrada del motor en la N64.
//
// Por ahora (entorno de la fase 5): inicia vídeo, RDP, mandos y sistema de
// ficheros, informa por debugf de la memoria y dibuja una pantalla de estado.
// Con -DTH_PRUEBAS ejecuta antes las pruebas del núcleo (las mismas que en el
// PC) y, si corre en un emulador con EMUX (ares en modo Homebrew), cierra el
// emulador al terminar para las pruebas automáticas (tools/motor_check.sh).
#include <libdragon.h>
#include <stdarg.h>

#include "th/version.h"

#ifdef TH_PRUEBAS
#include "th_test.h"

static void log_debugf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    debugf("TH64 pruebas: %s", buf);
}

void (*th_test_log)(const char *fmt, ...) = log_debugf;
#endif

enum { FUENTE_DEPURACION = 1 };

static const char *estilo_mando(joypad_style_t s)
{
    switch (s) {
    case JOYPAD_STYLE_N64: return "mando de N64";
    case JOYPAD_STYLE_MOUSE: return "raton de N64";
    case JOYPAD_STYLE_GCN: return "mando de GameCube";
    default: return "nada";
    }
}

int main(void)
{
    debug_init(DEBUG_FEATURE_LOG_USB | DEBUG_FEATURE_LOG_EMU);
    const int ram_kib = get_memory_size() / 1024;
    const bool expansion = is_memory_expanded();
    debugf("TH64 motor %s: RDRAM %d KiB, Expansion Pak %s\n", TH64_VERSION, ram_kib, expansion ? "si" : "no");

#ifdef TH_PRUEBAS
    int fallos = th_run_core_tests();
    debugf("TH64 pruebas: fin, %d fallos\n", fallos);
    if (emux_detect(0))
        emux_ioctl_exit();
#endif

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    joypad_init();
    const bool dfs_ok = dfs_init(DFS_DEFAULT_LOCATION) == DFS_ESUCCESS;
    rdpq_text_register_font(FUENTE_DEPURACION, rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));

    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    debugf("TH64 motor: heap usado %d KiB de %d KiB; DFS %s\n", heap.used / 1024, heap.total / 1024,
           dfs_ok ? "ok" : "no encontrado");

    for (uint32_t fotograma = 0;; fotograma++) {
        joypad_poll();
        surface_t *pantalla = display_get();
        rdpq_attach_clear(pantalla, NULL);
        rdpq_set_mode_fill(RGBA32(24, 72, 64, 255));
        rdpq_fill_rectangle(0, 0, 320, 240);
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 24, "Theme Hospital 64 - motor %s", TH64_VERSION);
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 44, "RDRAM %d KiB, Expansion Pak: %s", ram_kib,
                         expansion ? "si" : "no");
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 56, "Heap: %d de %d KiB", heap.used / 1024,
                         heap.total / 1024);
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 68, "DFS: %s", dfs_ok ? "ok" : "no encontrado");
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 80, "Puerto 1: %s",
                         estilo_mando(joypad_get_style(JOYPAD_PORT_1)));
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 92, "Fotograma %lu", (unsigned long)fotograma);
        rdpq_detach_show();
    }
}
