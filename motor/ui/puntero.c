#include "puntero.h"

void th_pointer_init(th_pointer *p, int w, int h)
{
    p->w = (int16_t)w;
    p->h = (int16_t)h;
    p->x = (int32_t)(w / 2) << 4;
    p->y = (int32_t)(h / 2) << 4;
    p->borde = 0;
}

static void limitar(th_pointer *p)
{
    const int32_t mx = ((int32_t)p->w - 1) << 4, my = ((int32_t)p->h - 1) << 4;
    if (p->x < 0)
        p->x = 0;
    if (p->x > mx)
        p->x = mx;
    if (p->y < 0)
        p->y = 0;
    if (p->y > my)
        p->y = my;
}

// Velocidad de un eje en dieciseisavos de píxel por fotograma: 0 en la zona
// muerta y de 16 a 128 (1 a 8 píxeles) con el cuadrado de la inclinación.
static int32_t eje(int s)
{
    enum { MUERTA = 8, MAXIMO = 85 };
    const int a = s < 0 ? -s : s;
    if (a <= MUERTA)
        return 0;
    const int32_t t = a > MAXIMO ? MAXIMO - MUERTA : a - MUERTA; // 1–77
    const int32_t v = 16 + 112 * t * t / ((MAXIMO - MUERTA) * (MAXIMO - MUERTA));
    return s < 0 ? -v : v;
}

void th_pointer_stick(th_pointer *p, int sx, int sy, int rapido)
{
    int32_t vx = eje(sx), vy = -eje(sy); // en la pantalla, y crece hacia abajo
    if (rapido) {
        vx = vx * 5 / 2;
        vy = vy * 5 / 2;
    }
    p->x += vx;
    p->y += vy;
    limitar(p);
}

void th_pointer_move(th_pointer *p, int dx, int dy)
{
    p->x += (int32_t)dx << 4;
    p->y += (int32_t)dy << 4;
    limitar(p);
}

void th_pointer_edge_scroll(th_pointer *p, int *dx, int *dy)
{
    const int x = th_pointer_x(p), y = th_pointer_y(p);
    *dx = x <= 0 ? -1 : x >= p->w - 1 ? 1 : 0;
    *dy = y <= 0 ? -1 : y >= p->h - 1 ? 1 : 0;
    if (!*dx && !*dy) {
        p->borde = 0;
        return;
    }
    // tick_scroll_mult en centésimas: 100 + 2 por tick, unos 11 ticks cada 6 fotogramas.
    int mult = 100 + 2 * (p->borde * 11 / 6);
    if (mult > 200)
        mult = 200;
    else
        p->borde++;
    const int v = 13 * mult / 200;
    *dx *= v;
    *dy *= v;
}
