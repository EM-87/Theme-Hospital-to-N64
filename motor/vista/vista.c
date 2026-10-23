#include "vista.h"

#include <string.h>

static uint32_t rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

int th_sheet_init(th_sheet *s, const uint8_t *data, size_t len)
{
    if (len < 16 || memcmp(data, "TSH1", 4) != 0)
        return -1;
    s->data = data;
    s->count = rd16(data + 4);
    s->table = rd32(data + 8);
    s->pixels = rd32(data + 12);
    if (s->table + 8u * s->count > len || s->pixels > len)
        return -1;
    s->max_height = 0;
    for (int i = 0; i < s->count; i++) {
        const uint8_t *e = data + s->table + 8 * i;
        uint16_t w = rd16(e + 4), h = rd16(e + 6);
        if (w && s->pixels + rd32(e) + (size_t)th_sheet_stride(w) * h > len)
            return -1;
        if (h > s->max_height)
            s->max_height = h;
    }
    return 0;
}

void th_sheet_size(const th_sheet *s, int i, int *w, int *h)
{
    if (i < 0 || i >= s->count) {
        *w = *h = 0;
        return;
    }
    const uint8_t *e = s->data + s->table + 8 * i;
    *w = rd16(e + 4);
    *h = rd16(e + 6);
    if (!*w || !*h)
        *w = *h = 0;
}

const uint8_t *th_sheet_pixels(const th_sheet *s, int i)
{
    return s->data + s->pixels + rd32(s->data + s->table + 8 * i);
}

// La lista se llena por los dos extremos en una sola pasada: los suelos desde
// el principio y las paredes desde el final (al revés); al terminar, las
// paredes se ponen detrás de los suelos.
typedef struct {
    th_drawlist *l;
    const th_view *v;
    int f, b;               // siguiente hueco de suelo (sube) y de pared (baja)
    int16_t w[256], h[256]; // tamaños de los bloques, leídos una vez por vista
} builder;

// Añade un sprite si toca la pantalla. Devuelve 0 o -1 si la lista está llena.
static int add(builder *bd, int wall, int sprite, int x, int y, int flags, int clip_y0, int clip_y1)
{
    const int w = bd->w[sprite], h = bd->h[sprite];
    if (!w)
        return 0;
    int y0 = y, y1 = y + h;
    if (flags & TH_DRAW_CLIP) {
        if (clip_y0 > y0)
            y0 = clip_y0;
        if (clip_y1 < y1)
            y1 = clip_y1;
    }
    if (x >= bd->v->w || x + w <= 0 || y0 >= bd->v->h || y1 <= 0 || y0 >= y1)
        return 0;
    if (bd->f > bd->b)
        return -1;
    th_draw *d = wall ? &bd->l->items[bd->b--] : &bd->l->items[bd->f++];
    d->sprite = (uint8_t)sprite;
    d->flags = (uint8_t)flags;
    d->x = (int16_t)x;
    d->y = (int16_t)y;
    d->clip_y0 = (int16_t)clip_y0;
    d->clip_y1 = (int16_t)clip_y1;
    return 0;
}

// Sprite de una capa a la altura que le toca: el pie del sprite queda a la
// altura del pie del rombo de la casilla.
static int add_layer(builder *bd, int wall, int block, int sx, int sy)
{
    if (!block)
        return 0;
    return add(bd, wall, block, sx - bd->v->unit, sy - bd->h[block] + bd->v->unit, 0, 0, 0);
}

static int floor_div(int a, int b)
{
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

// Casillas de la línea s (x + y = s) que pueden tocar la pantalla: [*x0, *x1].
// Un sprite de casilla ocupa como mucho de sx − u a sx + u + 1 (33 de ancho).
static int line_range(const th_view *v, int s, int *x0, int *x1)
{
    // sx = u·(2x − s) − v->x; se pide sx + u + 1 > 0 y sx − u < v->w.
    int lo = floor_div(v->x - 2 * v->unit, v->unit) + s;
    int hi = floor_div(v->x + v->w + 2 * v->unit, v->unit) + s;
    *x0 = floor_div(lo, 2);
    *x1 = floor_div(hi, 2) + 1;
    if (*x0 < s - (TH_MAP_HEIGHT - 1))
        *x0 = s - (TH_MAP_HEIGHT - 1);
    if (*x0 < 0)
        *x0 = 0;
    if (*x1 > s)
        *x1 = s;
    if (*x1 > TH_MAP_WIDTH - 1)
        *x1 = TH_MAP_WIDTH - 1;
    return *x0 <= *x1;
}

int th_view_map(const th_view *v, const th_map *m, const th_sheet *blocks, th_drawlist *out)
{
    const int u = v->unit, half = v->unit / 2;
    builder bd = {.l = out, .v = v, .f = 0, .b = out->capacity - 1};
    for (int i = 0; i < 256; i++) {
        int w, h;
        th_sheet_size(blocks, i, &w, &h);
        bd.w[i] = (int16_t)w;
        bd.h[i] = (int16_t)h;
    }
    // Líneas cuyo rombo (sy a sy + u) o sprite más alto pueden tocar la pantalla.
    int s0 = floor_div(v->y - u, half);
    int s1 = floor_div(v->y + v->h + blocks->max_height, half) + 1;
    if (s0 < 0)
        s0 = 0;
    if (s1 > TH_MAP_WIDTH + TH_MAP_HEIGHT - 2)
        s1 = TH_MAP_WIDTH + TH_MAP_HEIGHT - 2;

    for (int s = s0; s <= s1; s++) {
        int x0, x1;
        if (!line_range(v, s, &x0, &x1))
            continue;
        // En la línea s, la casilla x está en (u·(2x − s) − v->x, u/2·s − v->y).
        const int sy = half * s - v->y;
        const int sx0 = u * (2 * x0 - s) - v->x;
        const th_tile *t0 = th_map_tile_c(m, x0, s - x0);
        const int paso = 1 - TH_MAP_WIDTH; // x + 1, y − 1
        const th_tile *t;
        int x, sx;
        // Suelos y sombras del suelo (draw_floor), de izquierda a derecha.
        for (x = x0, t = t0, sx = sx0; x <= x1; x++, t += paso, sx += 2 * u) {
            if (add_layer(&bd, 0, t->floor, sx, sy) < 0)
                return -1;
            int sombra = t->flags & TH_TILE_SHADOW_FULL   ? TH_BLOCK_SHADOW_FULL
                         : t->flags & TH_TILE_SHADOW_HALF ? TH_BLOCK_SHADOW_HALF
                                                          : 0;
            if (sombra && add(&bd, 0, sombra, sx - u, sy, TH_DRAW_ALPHA25, 0, 0) < 0)
                return -1;
        }
        // Paredes norte de derecha a izquierda, con la sombra sobre la pared
        // recortada a la altura de esta (draw_north_wall).
        for (x = x1, t = t0 + (x1 - x0) * paso, sx = sx0 + 2 * u * (x1 - x0); x >= x0; x--, t -= paso, sx -= 2 * u) {
            if (add_layer(&bd, 1, t->wall_n, sx, sy) < 0)
                return -1;
            const int h = bd.h[t->wall_n];
            if (t->wall_n && h > 0 && (t->flags & TH_TILE_SHADOW_WALL)) {
                int c0 = sy - h + u + u / 8;
                if (add(&bd, 1, TH_BLOCK_SHADOW_WALL, sx - u, sy - 56 * u / 32, TH_DRAW_ALPHA25 | TH_DRAW_CLIP, c0,
                        c0 + 82 * u / 32) < 0)
                    return -1;
            }
        }
        // Paredes oeste de izquierda a derecha.
        for (x = x0, t = t0, sx = sx0; x <= x1; x++, t += paso, sx += 2 * u)
            if (add_layer(&bd, 1, t->wall_w, sx, sy) < 0)
                return -1;
    }

    // Las paredes, que están al final y al revés, pasan detrás de los suelos.
    const int nw = out->capacity - 1 - bd.b;
    th_draw *w = &out->items[bd.b + 1];
    for (int i = 0, j = nw - 1; i < j; i++, j--) {
        th_draw tmp = w[i];
        w[i] = w[j];
        w[j] = tmp;
    }
    memmove(&out->items[bd.f], w, (size_t)nw * sizeof(th_draw));
    out->floor_count = bd.f;
    out->count = bd.f + nw;
    return out->count;
}

void th_view_clamp(th_view *v)
{
    // El centro de la pantalla, en coordenadas del mundo, dentro del rombo:
    // |cx| / u + cy / (u/2) <= alto del mapa (en casillas), con cy >= |cx| / 2.
    const int u = v->unit, half = v->unit / 2;
    int cx = v->x + v->w / 2, cy = v->y + v->h / 2;
    const int lim_x = u * (TH_MAP_WIDTH - 1);
    if (cx < -lim_x)
        cx = -lim_x;
    if (cx > lim_x)
        cx = lim_x;
    int ax = cx < 0 ? -cx : cx;
    int cy_min = ax / 2;                                        // borde superior del rombo
    int cy_max = half * (TH_MAP_WIDTH + TH_MAP_HEIGHT - 2) - ax / 2; // borde inferior
    if (cy < cy_min)
        cy = cy_min;
    if (cy > cy_max)
        cy = cy_max;
    v->x = cx - v->w / 2;
    v->y = cy - v->h / 2;
}
