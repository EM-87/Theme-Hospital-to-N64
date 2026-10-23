// Punto de entrada del motor en la N64.
//
// Hito 1 de la fase 5: carga la paleta y una hoja de sprites que genera el
// conversor (tools/conv) y los dibuja con el RDP. Al arrancar comprueba que lo
// que dibuja el RDP coincide píxel a píxel con lo que dice la hoja y la paleta.
// Después muestra un visor: cruceta izquierda/derecha cambia de sprite.
//
// Con -DTH_PRUEBAS ejecuta antes las pruebas del núcleo (las mismas que en el
// PC) y, si corre en un emulador con EMUX (ares en modo Homebrew), cierra el
// emulador tras las comprobaciones (tools/motor_check.sh).
#include <libdragon.h>
#include <stdarg.h>

#include "graficos.h"
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
#define COLOR_FONDO RGBA32(24, 72, 64, 255)

static const char *estilo_mando(joypad_style_t s)
{
    switch (s) {
    case JOYPAD_STYLE_N64: return "mando de N64";
    case JOYPAD_STYLE_MOUSE: return "raton de N64";
    case JOYPAD_STYLE_GCN: return "mando de GameCube";
    default: return "nada";
    }
}

// Primer sprite "de buen tamaño" que cabe en la TMEM: el de la comprobación.
static int sprite_de_prueba(const hoja_t *h)
{
    for (int i = 0; i < h->num; i++) {
        const hoja_entrada_t *e = &h->tabla[i];
        int paso = (e->ancho + 7) & ~7;
        if (e->ancho >= 16 && e->alto >= 24 && paso * e->alto <= 2048)
            return i;
    }
    return 0;
}

// Dibuja el sprite a escala 1 sobre el fondo, espera al RDP y compara cada
// píxel del framebuffer con el color que le toca según la hoja y la paleta.
// Devuelve el número de píxeles distintos (-1 si no se pudo comprobar).
static int comprobar_dibujo(const hoja_t *h, const paletas_t *p, int indice, int *comparados)
{
    const int X = 20, Y = 20;
    surface_t s = hoja_superficie(h, indice);
    if (!s.width)
        return -1;
    surface_t *pantalla = display_get();
    rdpq_attach(pantalla, NULL);
    rdpq_set_mode_fill(COLOR_FONDO);
    rdpq_fill_rectangle(0, 0, 320, 240);
    graficos_modo_sprites(p->colores[PALETA_NORMAL]);
    hoja_dibujar(h, indice, X, Y, 1, false);
    rdpq_detach_wait();
    data_cache_hit_invalidate(pantalla->buffer, pantalla->stride * pantalla->height);
    const uint16_t fondo = color_to_packed16(COLOR_FONDO);
    const uint16_t *fb = pantalla->buffer;
    const uint8_t *px = s.buffer;
    int distintos = 0;
    *comparados = 0;
    for (int j = 0; j < s.height; j++) {
        for (int i = 0; i < s.width; i++) {
            uint8_t idx = px[j * s.stride + i];
            uint16_t esperado = idx == 255 ? fondo : p->colores[PALETA_NORMAL][idx];
            uint16_t real = fb[(Y + j) * (pantalla->stride / 2) + X + i];
            // El bit bajo del framebuffer de 16 bits es cobertura, no color.
            if ((real & 0xFFFE) != (esperado & 0xFFFE))
                distintos++;
            (*comparados)++;
        }
    }
    display_show(pantalla);
    return distintos;
}

int main(void)
{
    debug_init(DEBUG_FEATURE_LOG_USB | DEBUG_FEATURE_LOG_EMU);
    const int ram_kib = get_memory_size() / 1024;
    const bool expansion = is_memory_expanded();
    debugf("TH64 motor %s: RDRAM %d KiB, Expansion Pak %s\n", TH64_VERSION, ram_kib, expansion ? "si" : "no");

#ifdef TH_PRUEBAS
    int fallos = th_run_core_tests();
    debugf("TH64 pruebas: nucleo, %d fallos\n", fallos);
#endif

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    joypad_init();
    asset_init_compression(2);
    const bool dfs_ok = dfs_init(DFS_DEFAULT_LOCATION) == DFS_ESUCCESS;
    rdpq_text_register_font(FUENTE_DEPURACION, rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));

    hoja_t hoja = {0};
    paletas_t paletas = {0};
    const bool assets = dfs_ok && paletas_cargar(&paletas, "rom:/paleta.tpl") &&
                        hoja_cargar(&hoja, "rom:/datam/mspr-0.tsh");
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    debugf("TH64 motor: heap usado %d KiB de %d KiB; DFS %s; assets %s\n", heap.used / 1024, heap.total / 1024,
           dfs_ok ? "ok" : "no encontrado", assets ? "ok" : "no encontrados (tools/conv/conv.py)");

    int actual = 0;
    if (assets) {
        actual = sprite_de_prueba(&hoja);
        int comparados = 0;
        int distintos = comprobar_dibujo(&hoja, &paletas, actual, &comparados);
        debugf("TH64 hito1: sprite %d de %d (%dx%d): %d pixeles comparados, %d distintos\n", actual, hoja.num,
               hoja.tabla[actual].ancho, hoja.tabla[actual].alto, comparados, distintos);
    }
#ifdef TH_PRUEBAS
    debugf("TH64 pruebas: fin, %d fallos\n", fallos);
    if (emux_detect(0))
        emux_ioctl_exit();
#endif

    for (uint32_t fotograma = 0;; fotograma++) {
        joypad_poll();
        joypad_buttons_t pulsado = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        if (assets) {
            if (pulsado.d_right || pulsado.c_right)
                actual = (actual + 1) % hoja.num;
            if (pulsado.d_left || pulsado.c_left)
                actual = (actual + hoja.num - 1) % hoja.num;
            if (pulsado.d_up)
                actual = (actual + 100) % hoja.num;
            if (pulsado.d_down)
                actual = (actual + hoja.num - 100) % hoja.num;
        }
        surface_t *pantalla = display_get();
        rdpq_attach_clear(pantalla, NULL);
        rdpq_set_mode_fill(COLOR_FONDO);
        rdpq_fill_rectangle(0, 0, 320, 240);
        if (assets) {
            // El sprite a escala 3 con las tres paletas, y reflejado.
            for (int v = 0; v < PALETAS_MAX; v++) {
                graficos_modo_sprites(paletas.colores[v]);
                hoja_dibujar(&hoja, actual, 16 + 76 * v, 40, 3, false);
            }
            graficos_modo_sprites(paletas.colores[PALETA_NORMAL]);
            hoja_dibujar(&hoja, actual, 16 + 76 * 3, 40, 3, true);
            // Una tira con los sprites siguientes a escala 1.
            int x = 16;
            for (int i = actual; i < hoja.num && x < 300; i++) {
                if (!hoja.tabla[i].ancho)
                    continue;
                hoja_dibujar(&hoja, i, x, 196 - hoja.tabla[i].alto, 1, false);
                x += hoja.tabla[i].ancho + 2;
            }
        }
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 16, "TH64 %s - %s", TH64_VERSION,
                         estilo_mando(joypad_get_style(JOYPAD_PORT_1)));
        if (assets)
            rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 28, "MSPR-0 sprite %d/%d (%dx%d)  normal/gris/rojo/espejo",
                             actual, hoja.num, hoja.tabla[actual].ancho, hoja.tabla[actual].alto);
        else
            rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 28, "Sin assets: ejecuta tools/conv/conv.py");
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 220, "RAM %d KiB  heap %d/%d KiB  fotograma %lu", ram_kib,
                         heap.used / 1024, heap.total / 1024, (unsigned long)fotograma);
        rdpq_detach_show();
    }
}
