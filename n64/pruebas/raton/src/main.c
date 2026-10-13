// Muestra el estilo del dispositivo en cada puerto y, si es un ratón, el
// movimiento relativo (stick_x/stick_y) y los botones A/B.
#include <libdragon.h>
static const char *nombre(joypad_style_t s)
{
    switch (s) {
    case JOYPAD_STYLE_NONE: return "nada";
    case JOYPAD_STYLE_N64: return "mando N64";
    case JOYPAD_STYLE_GCN: return "mando GameCube";
    case JOYPAD_STYLE_MOUSE: return "RATON N64";
    default: return "?";
    }
}
int main(void)
{
    debug_init_emulog();
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
    joypad_init();
    console_init();
    for (int fr = 0;; fr++) {
        joypad_poll();
        if (fr % 60 == 0) {
            for (int p = 0; p < 2; p++) {
                joypad_inputs_t in = joypad_get_inputs(p);
                debugf("TH64 joy: t=%d puerto %d: %s (id 0x%04x) dx=%d dy=%d A=%d B=%d\n", fr / 60, p + 1,
                       nombre(joypad_get_style(p)), joypad_get_identifier(p), in.stick_x, in.stick_y, in.btn.a, in.btn.b);
            }
        }
        vi_wait_vblank();
    }
}
