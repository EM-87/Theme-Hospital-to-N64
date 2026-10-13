// Prueba de depuración: validador de rdpq (rdpq_debug) y assertf con pantalla de error.
#include <libdragon.h>

__attribute__((noinline)) static void comprueba_paciente(int id, int salud)
{
    assertf(salud >= 0, "paciente %d con salud negativa (%d)", id, salud);
}

int main(void)
{
    debug_init_emulog();
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    rdpq_debug_start();                 // activa el validador del RDP
    for (int frame = 0; frame < 3; frame++) {
        surface_t *fb = display_get();
        rdpq_attach_clear(fb, NULL);
        if (frame == 1) rdpq_debug_log(true);
        rdpq_set_mode_fill(RGBA32(0, 128, 0, 255));
        rdpq_fill_rectangle(10, 10, 100, 100);
        // Error a propósito: combinador con textura y tile 0 sin configurar.
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_TEX);
        rdpq_texture_rectangle(TILE0, 120, 10, 152, 42, 0, 0);
        if (frame == 1) rdpq_debug_log(false);
        rdpq_detach_show();
        rspq_wait();
        debugf("TH64 dbg: frame %d dibujado\n", frame);
    }
    debugf("TH64 dbg: ahora falla un assert\n");
    comprueba_paciente(42, -5);
    for (;;) {}
}
