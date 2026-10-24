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
// el principio y lo demás desde el final (al revés); al terminar, lo demás se
// pone detrás de los suelos.
typedef struct {
    th_drawlist *l;
    const th_view *v;
    const th_scene *sc;
    int f, b;               // siguiente hueco de suelo (sube) y de pared (baja)
    int16_t w[256], h[256]; // tamaños de los bloques, leídos una vez por vista
    // Entidades cerca de la pantalla, ordenadas por casilla y orden de dibujo,
    // y un bit por casilla con alguna (para no buscar en las vacías).
    uint16_t ent[TH_VIEW_MAX_ENTITIES];
    int n_ent;
    uint32_t occupied[TH_MAP_WIDTH * TH_MAP_HEIGHT / 32];
} builder;

static int has_entities(const builder *bd, int x, int y)
{
    const unsigned i = (unsigned)(y * TH_MAP_WIDTH + x);
    return (bd->occupied[i >> 5] >> (i & 31)) & 1u;
}

// Añade un sprite si toca la pantalla. Devuelve 0 o -1 si la lista está llena.
static int add_sprite(builder *bd, int wall, int sheet, int sprite, int w, int h, int x, int y, int flags,
                      int clip_y0, int clip_y1)
{
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
    d->sprite = (uint16_t)sprite;
    d->sheet = (uint8_t)sheet;
    d->flags = (uint8_t)flags;
    d->x = (int16_t)x;
    d->y = (int16_t)y;
    d->clip_y0 = (int16_t)clip_y0;
    d->clip_y1 = (int16_t)clip_y1;
    return 0;
}

static int add(builder *bd, int wall, int block, int x, int y, int flags, int clip_y0, int clip_y1)
{
    return add_sprite(bd, wall, TH_SHEET_BLOCKS, block, bd->w[block], bd->h[block], x, y, flags, clip_y0, clip_y1);
}

// Sprite de una capa a la altura que le toca: el pie del sprite queda a la
// altura del pie del rombo de la casilla.
static int add_layer(builder *bd, int wall, int block, int sx, int sy)
{
    if (!block)
        return 0;
    return add(bd, wall, block, sx - bd->v->unit, sy - bd->h[block] + bd->v->unit, 0, 0, 0);
}

// draw_north_wall: la pared norte y, si hay, la sombra sobre ella recortada a
// su altura.
static int add_north_wall(builder *bd, const th_tile *t, int sx, int sy)
{
    const int u = bd->v->unit;
    if (add_layer(bd, 1, t->wall_n, sx, sy) < 0)
        return -1;
    const int h = bd->h[t->wall_n];
    if (t->wall_n && h > 0 && (t->flags & TH_TILE_SHADOW_WALL)) {
        int c0 = sy - h + u + u / 8;
        if (add(bd, 1, TH_BLOCK_SHADOW_WALL, sx - u, sy - 56 * u / 32, TH_DRAW_ALPHA25 | TH_DRAW_CLIP, c0,
                c0 + 82 * u / 32) < 0)
            return -1;
    }
    return 0;
}

// animation::draw y animation_manager::draw_frame: los elementos del
// fotograma que tocan a sus capas, desde la esquina de la casilla más el
// desplazamiento de la entidad.
static int add_entity(builder *bd, const th_view_entity *e, int sx, int sy)
{
    if ((e->flags & (TH_ANIM_ALPHA50 | TH_ANIM_ALPHA75)) == (TH_ANIM_ALPHA50 | TH_ANIM_ALPHA75))
        return 0;
    const th_anims *a = bd->sc->anims;
    const th_sheet *sh = bd->sc->sheets[TH_SHEET_SPRITES];
    const int u = bd->v->unit;
    const int x = sx + e->px * u / 32, y = sy + e->py * u / 32;
    for (uint32_t i = th_anims_frame_list(a, e->frame);; i++) {
        uint16_t n = th_anims_list_entry(a, i);
        if (n == TH_ANIM_LIST_END || n >= a->elements)
            break;
        th_anim_element el;
        th_anims_element(a, n, &el);
        if (el.sprite >= sh->count)
            continue;
        // Los elementos de una capa solo se dibujan si la capa tiene su id. Como
        // CorsixTH, en la capa 5 (cabezas) W1 y B1 sirven también para W2 y B2.
        if (el.layer_id != 0 && e->layers[el.layer] != el.layer_id &&
            !(el.layer == 5 && e->layers[5] - 4 == el.layer_id))
            continue;
        int f = el.flags;
        if ((f & (TH_ANIM_ALPHA50 | TH_ANIM_ALPHA75)) == (TH_ANIM_ALPHA50 | TH_ANIM_ALPHA75))
            continue;
        int w, h;
        th_sheet_size(sh, el.sprite, &w, &h);
        // Desplazamientos crudos con origen en (141, 186), a la escala de la hoja.
        const int ex = ((int)el.x - 141) * u / 32, ey = ((int)el.y - 186) * u / 32;
        int px;
        if (e->flags & TH_ANIM_FLIP_H) {
            px = x - ex - w;
            f ^= TH_ANIM_FLIP_H;
        } else {
            px = x + ex;
        }
        int flags = (f & TH_ANIM_FLIP_H ? TH_DRAW_FLIP_H : 0) | (f & TH_ANIM_FLIP_V ? TH_DRAW_FLIP_V : 0) |
                    (f & TH_ANIM_ALPHA50 ? TH_DRAW_ALPHA50 : 0) | (f & TH_ANIM_ALPHA75 ? TH_DRAW_ALPHA25 : 0);
        if (add_sprite(bd, 1, TH_SHEET_SPRITES, el.sprite, w, h, px, y + ey, flags, 0, 0) < 0)
            return -1;
    }
    return 0;
}

static int tile_key(const th_view_entity *e)
{
    return e->tile_y * TH_MAP_WIDTH + e->tile_x;
}

// ¿Va a antes que b en la lista de su casilla? Por casilla, luego por capa de
// dibujo y, con la misma capa, la que llegó después primero (attach_to_tile).
static int before(const th_view_entity *a, const th_view_entity *b)
{
    if (tile_key(a) != tile_key(b))
        return tile_key(a) < tile_key(b);
    if (a->drawing_layer != b->drawing_layer)
        return a->drawing_layer < b->drawing_layer;
    return a->attach_seq > b->attach_seq;
}

// Primera entidad (en bd->ent) de la casilla (x, y), o n_ent si no hay.
static int first_entity(const builder *bd, int x, int y)
{
    const int key = y * TH_MAP_WIDTH + x;
    int lo = 0, hi = bd->n_ent;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (tile_key(&bd->sc->entities[bd->ent[mid]]) < key)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

// Las entidades de la casilla (x, y) cuya capa de dibujo es 'layer' (o todas
// si es -1), dibujadas en la casilla de pantalla (sx, sy). Devuelve cuántas
// había, o -1 si la lista se llenó.
static int add_tile_entities(builder *bd, int x, int y, int sx, int sy, int layer)
{
    if (x < 0 || y < 0 || x >= TH_MAP_WIDTH || y >= TH_MAP_HEIGHT || !has_entities(bd, x, y))
        return 0;
    const int key = y * TH_MAP_WIDTH + x;
    int n = 0;
    for (int i = first_entity(bd, x, y); i < bd->n_ent; i++) {
        const th_view_entity *e = &bd->sc->entities[bd->ent[i]];
        if (tile_key(e) != key)
            break;
        if (layer >= 0 && e->drawing_layer != layer)
            continue;
        if (add_entity(bd, e, sx, sy) < 0)
            return -1;
        n++;
    }
    return n;
}

static int floor_div(int a, int b)
{
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

// Casillas de la línea s (x + y = s) que pueden tocar la pantalla: [*x0, *x1].
// Sin entidades, un sprite de casilla ocupa como mucho de sx − u a sx + u + 1
// (33 de ancho); con ellas, se mira 4 u a cada lado (CorsixTH, 110 píxeles).
static int line_range(const th_view *v, int s, int margin, int *x0, int *x1)
{
    // sx = u·(2x − s) − v->x; se pide sx + margen > 0 y sx − margen < v->w.
    int lo = floor_div(v->x - margin, v->unit) + s;
    int hi = floor_div(v->x + v->w + margin, v->unit) + s;
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

int th_view_scene(const th_view *v, const th_scene *sc, th_drawlist *out)
{
    const int u = v->unit, half = v->unit / 2;
    const th_sheet *blocks = sc->sheets[TH_SHEET_BLOCKS];
    const th_map *m = sc->map;
    static builder bd;
    bd.l = out;
    bd.v = v;
    bd.sc = sc;
    bd.f = 0;
    bd.b = out->capacity - 1;
    for (int i = 0; i < 256; i++) {
        int w, h;
        th_sheet_size(blocks, i, &w, &h);
        bd.w[i] = (int16_t)w;
        bd.h[i] = (int16_t)h;
    }
    // Líneas cuyo rombo (sy a sy + u) o sprite más alto pueden tocar la
    // pantalla; con entidades, hasta 150 píxeles de 640 × 480 por debajo.
    int below = blocks->max_height, margin = 2 * u;
    if (sc->entity_count > 0) {
        if (below < 150 * u / 32)
            below = 150 * u / 32;
        margin = 4 * u;
    }
    int s0 = floor_div(v->y - u, half);
    int s1 = floor_div(v->y + v->h + below, half) + 1;
    if (s0 < 0)
        s0 = 0;
    if (s1 > TH_MAP_WIDTH + TH_MAP_HEIGHT - 2)
        s1 = TH_MAP_WIDTH + TH_MAP_HEIGHT - 2;

    // Las entidades de esas líneas, ordenadas (por inserción: son pocas).
    bd.n_ent = 0;
    out->redraws = 0;
    memset(bd.occupied, 0, sizeof bd.occupied);
    for (int i = 0; i < sc->entity_count && bd.n_ent < TH_VIEW_MAX_ENTITIES; i++) {
        const th_view_entity *e = &sc->entities[i];
        int s = e->tile_x + e->tile_y;
        if (e->tile_x < 0 || s < s0 || s > s1)
            continue;
        int j = bd.n_ent++;
        while (j > 0 && before(e, &sc->entities[bd.ent[j - 1]])) {
            bd.ent[j] = bd.ent[j - 1];
            j--;
        }
        bd.ent[j] = (uint16_t)i;
        const unsigned k = (unsigned)tile_key(e);
        bd.occupied[k >> 5] |= 1u << (k & 31);
    }

    int first = 1;
    for (int s = s0; s <= s1; s++) {
        int x0, x1;
        if (!line_range(v, s, margin, &x0, &x1))
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
        // Paredes norte de derecha a izquierda.
        for (x = x1, t = t0 + (x1 - x0) * paso, sx = sx0 + 2 * u * (x1 - x0); x >= x0; x--, t -= paso, sx -= 2 * u)
            if (add_north_wall(&bd, t, sx, sy) < 0)
                return -1;
        // De izquierda a derecha: la pared oeste y las entidades de la casilla.
        int prev_needs_redraw = 0;
        for (x = x0, t = t0, sx = sx0; x <= x1; x++, t += paso, sx += 2 * u) {
            const int y = s - x;
            if (add_layer(&bd, 1, t->wall_w, sx, sy) < 0)
                return -1;
            int needs_redraw = t->wall_n && bd.h[t->wall_n] > 0;
            int animated = 0;
            if (bd.n_ent && has_entities(&bd, x, y)) {
                const int key = y * TH_MAP_WIDTH + x;
                for (int i = first_entity(&bd, x, y); i < bd.n_ent; i++) {
                    const th_view_entity *e = &sc->entities[bd.ent[i]];
                    if (tile_key(e) != key)
                        break;
                    if (add_entity(&bd, e, sx, sy) < 0)
                        return -1;
                    animated |= th_anims_is_multi_frame(sc->anims, e->anim);
                    needs_redraw |= e->drawing_layer == TH_LAYER_NORTH_SIDE;
                }
            }
            if (animated && !first) {
                // Lo que tapa a una entidad animada: los objetos del lado sur
                // de la casilla del oeste, los del lado este de la del norte y,
                // si hace falta, la casilla anterior de la línea con su pared
                // norte y lo que hay en ella.
                int redraw = prev_needs_redraw;
                int r = add_tile_entities(&bd, x - 1, y, sx - u, sy - half, TH_LAYER_SOUTH_SIDE);
                if (r < 0)
                    return -1;
                redraw |= r > 0;
                if (add_tile_entities(&bd, x, y - 1, sx + u, sy - half, TH_LAYER_EAST_SIDE) < 0)
                    return -1;
                if (redraw && x - 1 >= 0 && y + 1 < TH_MAP_HEIGHT) {
                    out->redraws++;
                    if (add_north_wall(&bd, th_map_tile_c(m, x - 1, y + 1), sx - 2 * u, sy) < 0 ||
                        add_tile_entities(&bd, x - 1, y + 1, sx - 2 * u, sy, -1) < 0)
                        return -1;
                }
            }
            prev_needs_redraw = needs_redraw;
        }
        first = 0;
    }

    // Las paredes y entidades, que están al final y al revés, pasan detrás de los suelos.
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

int th_view_map(const th_view *v, const th_map *m, const th_sheet *blocks, th_drawlist *out)
{
    th_scene sc = {.map = m, .sheets = {blocks, NULL}};
    return th_view_scene(v, &sc, out);
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
