#include "th/path.h"

#include <string.h>

static int is_seen(const th_pathfinder *pf, unsigned i)
{
    return (pf->seen[i >> 5] >> (i & 31)) & 1u;
}

static void set_seen(th_pathfinder *pf, unsigned i)
{
    pf->seen[i >> 5] |= 1u << (i & 31);
}

static int is_visited(const th_pathfinder *pf, unsigned i)
{
    return (pf->visited[i >> 5] >> (i & 31)) & 1u;
}

static void set_visited(th_pathfinder *pf, unsigned i)
{
    pf->visited[i >> 5] |= 1u << (i & 31);
}

static int value(const th_pathfinder *pf, unsigned i)
{
    return (int)pf->nodes[i].distance + pf->nodes[i].guess;
}

// Distancia de Manhattan al destino; las otras búsquedas no tienen destino
// (dest_x == TH_PATH_NONE) y su estimación es 0.
static int16_t guess(const th_pathfinder *pf, unsigned i)
{
    if (pf->dest_x == TH_PATH_NONE)
        return 0;
    int dx = (int)(i % TH_MAP_WIDTH) - pf->dest_x, dy = (int)(i / TH_MAP_WIDTH) - pf->dest_y;
    return (int16_t)((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
}

// pathfinder::open_heap_promote
static void promote(th_pathfinder *pf, unsigned n)
{
    unsigned i = pf->nodes[n].open_idx;
    while (i > 0) {
        unsigned parent = (i - 1) / 2, p = pf->heap[parent];
        if (value(pf, p) <= value(pf, n))
            break;
        pf->nodes[p].open_idx = (uint16_t)i;
        pf->heap[i] = (uint16_t)p;
        pf->heap[parent] = (uint16_t)n;
        i = parent;
    }
    pf->nodes[n].open_idx = (uint16_t)i;
}

static void push(th_pathfinder *pf, unsigned n)
{
    pf->nodes[n].open_idx = (uint16_t)pf->heap_size;
    pf->heap[pf->heap_size++] = (uint16_t)n;
    promote(pf, n);
}

// pathfinder::pop_from_open_heap, con sus mismas comparaciones.
static unsigned pop(th_pathfinder *pf)
{
    unsigned result = pf->heap[0], n = pf->heap[--pf->heap_size];
    if (pf->heap_size == 0)
        return result;
    pf->heap[0] = (uint16_t)n;
    unsigned i = 0, min = 0, left = 1;
    const int v = value(pf, n);
    while (left < pf->heap_size) {
        min = i;
        unsigned right = (i + 1) * 2, swap = 0;
        int minvalue = v;
        unsigned t = pf->heap[left];
        if (value(pf, t) < minvalue) {
            min = left;
            minvalue = value(pf, t);
            swap = t;
        }
        if (right < pf->heap_size) {
            t = pf->heap[right];
            if (value(pf, t) < minvalue) {
                min = right;
                swap = t;
            }
        }
        if (min == i)
            break;
        pf->nodes[swap].open_idx = (uint16_t)i;
        pf->heap[i] = (uint16_t)swap;
        pf->heap[min] = (uint16_t)n;
        i = min;
        left = i * 2 + 1;
    }
    pf->nodes[n].open_idx = (uint16_t)min;
    return result;
}

// abstract_pathfinder::record_neighbour_if_passable: desde una casilla
// transitable solo se pasa a otra transitable.
static void record(th_pathfinder *pf, const th_map *m, unsigned n, int passable, unsigned nb)
{
    if (nb == pf->start || is_visited(pf, nb))
        return;
    if (passable && !(m->tiles[nb].flags & TH_TILE_PASSABLE))
        return;
    uint16_t d = (uint16_t)(pf->nodes[n].distance + 1);
    if (!is_seen(pf, nb)) {
        set_seen(pf, nb);
        pf->nodes[nb].prev = (uint16_t)n;
        pf->nodes[nb].distance = d;
        pf->nodes[nb].guess = guess(pf, nb);
        push(pf, nb);
    } else if (d < pf->nodes[nb].distance) {
        pf->nodes[nb].prev = (uint16_t)n;
        pf->nodes[nb].distance = d;
        promote(pf, nb);
    }
}

int th_path_find(th_pathfinder *pf, const th_map *m, int sx, int sy, int dx, int dy)
{
    pf->dest = TH_PATH_NONE;
    pf->expanded = 0;
    if (dx < 0 || dy < 0 || dx >= TH_MAP_WIDTH || dy >= TH_MAP_HEIGHT || sx < 0 || sy < 0 ||
        sx >= TH_MAP_WIDTH || sy >= TH_MAP_HEIGHT)
        return -1;
    const unsigned target = (unsigned)(dy * TH_MAP_WIDTH + dx);
    if (!(m->tiles[target].flags & TH_TILE_PASSABLE))
        return -1;
    pf->dest_x = (uint16_t)dx;
    pf->dest_y = (uint16_t)dy;
    memset(pf->seen, 0, sizeof(pf->seen));
    memset(pf->visited, 0, sizeof(pf->visited));
    pf->heap_size = 0;
    unsigned n = (unsigned)(sy * TH_MAP_WIDTH + sx);
    pf->start = (uint16_t)n;
    set_seen(pf, n);
    pf->nodes[n].prev = TH_PATH_NONE;
    pf->nodes[n].distance = 0;
    pf->nodes[n].guess = guess(pf, n);
    for (;;) {
        if (n == target) {
            pf->dest = (uint16_t)n;
            return pf->nodes[n].distance;
        }
        pf->expanded++;
        // search_neighbours: oeste, este, norte y sur. Las banderas de
        // tránsito ya impiden salir del mapa y atravesar paredes.
        const uint32_t f = m->tiles[n].flags;
        const int passable = (f & TH_TILE_PASSABLE) != 0;
        if (f & TH_TILE_TRAVEL_W)
            record(pf, m, n, passable, n - 1);
        if (f & TH_TILE_TRAVEL_E)
            record(pf, m, n, passable, n + 1);
        if (f & TH_TILE_TRAVEL_N)
            record(pf, m, n, passable, n - TH_MAP_WIDTH);
        if (f & TH_TILE_TRAVEL_S)
            record(pf, m, n, passable, n + TH_MAP_WIDTH);
        if (pf->heap_size == 0)
            return -1;
        n = pop(pf);
    }
}

// abstract_pathfinder::init para las búsquedas sin destino.
static unsigned init_search(th_pathfinder *pf, int sx, int sy)
{
    pf->dest = TH_PATH_NONE;
    pf->dest_x = pf->dest_y = TH_PATH_NONE;
    pf->expanded = 0;
    memset(pf->seen, 0, sizeof(pf->seen));
    memset(pf->visited, 0, sizeof(pf->visited));
    pf->heap_size = 0;
    const unsigned n = (unsigned)(sy * TH_MAP_WIDTH + sx);
    pf->start = (uint16_t)n;
    set_seen(pf, n);
    set_visited(pf, n);
    pf->nodes[n].prev = TH_PATH_NONE;
    pf->nodes[n].distance = 0;
    pf->nodes[n].guess = 0;
    return n;
}

// Vecino en la dirección d (0 norte, 1 este, 2 sur, 3 oeste), o -1 si las
// banderas no dejan ir. Las dos búsquedas sin destino no cruzan puertas.
static int neighbour_no_doors(const th_map *m, unsigned n, uint32_t f, int d)
{
    static const uint32_t travel[4] = {TH_TILE_TRAVEL_N, TH_TILE_TRAVEL_E, TH_TILE_TRAVEL_S, TH_TILE_TRAVEL_W};
    if (!(f & travel[d]))
        return -1;
    const unsigned nb = d == 0 ? n - TH_MAP_WIDTH : d == 1 ? n + 1 : d == 2 ? n + TH_MAP_WIDTH : n - 1;
    return (int)nb;
    (void)m;
}

static int door_blocks(const th_map *m, unsigned n, unsigned nb, uint32_t f, int d)
{
    switch (d) {
    case 0: return (f & TH_TILE_DOOR_N) != 0;
    case 1: return (m->tiles[nb].flags & TH_TILE_DOOR_W) != 0;
    case 2: return (m->tiles[nb].flags & TH_TILE_DOOR_N) != 0;
    default: return (f & TH_TILE_DOOR_W) != 0;
    }
    (void)n;
}

// Orden de search_neighbours: oeste, este, norte, sur.
static const int search_order[4] = {3, 1, 0, 2};

int th_path_idle_tile(th_pathfinder *pf, const th_map *m, int sx, int sy, int n, int *ox, int *oy)
{
    if (sx < 0 || sy < 0 || sx >= TH_MAP_WIDTH || sy >= TH_MAP_HEIGHT)
        return 0;
    unsigned node = init_search(pf, sx, sy);
    int possible = -1;
    for (;;) {
        set_visited(pf, node);
        const uint32_t f = m->tiles[node].flags;
        if (!(f & TH_TILE_DO_NOT_IDLE) && (f & TH_TILE_PASSABLE) && (f & TH_TILE_HOSPITAL)) {
            if (n == 0) {
                *ox = (int)(node % TH_MAP_WIDTH);
                *oy = (int)(node / TH_MAP_WIDTH);
                return 1;
            }
            possible = (int)node;
            n--;
        }
        int best = -1, best_d2 = 0;
        for (int k = 0; k < 4; k++) {
            const int d = search_order[k];
            const int nb = neighbour_no_doors(m, node, f, d);
            if (nb < 0 || is_visited(pf, (unsigned)nb))
                continue;
            if (!door_blocks(m, node, (unsigned)nb, f, d))
                record(pf, m, node, (f & TH_TILE_PASSABLE) != 0, (unsigned)nb);
            // El vecino más cerca del origen (en línea recta) pasa delante.
            if (is_seen(pf, (unsigned)nb)) {
                const int dx = nb % TH_MAP_WIDTH - sx, dy = nb / TH_MAP_WIDTH - sy;
                const int d2 = dx * dx + dy * dy;
                if (best < 0 || d2 < best_d2) {
                    best = nb;
                    best_d2 = d2;
                }
            }
        }
        if (pf->heap_size == 0)
            break;
        if (best >= 0) {
            pf->nodes[best].guess = (int16_t)-pf->nodes[best].distance;
            promote(pf, (unsigned)best);
        }
        node = pop(pf);
    }
    if (possible >= 0) {
        *ox = possible % TH_MAP_WIDTH;
        *oy = possible / TH_MAP_WIDTH;
        return 1;
    }
    return 0;
}

int th_path_visit_objects(th_pathfinder *pf, const th_map *m, int sx, int sy, int thob, int max_dist,
                          th_path_visit_fn fn, void *ctx)
{
    if (sx < 0 || sy < 0 || sx >= TH_MAP_WIDTH || sy >= TH_MAP_HEIGHT)
        return 0;
    unsigned node = init_search(pf, sx, sy);
    for (;;) {
        set_visited(pf, node);
        const uint32_t f = m->tiles[node].flags;
        for (int k = 0; k < 4; k++) {
            const int d = search_order[k];
            const int nb = neighbour_no_doors(m, node, f, d);
            if (nb < 0)
                continue;
            if (m->tiles[nb].object == thob) {
                const int r = fn(ctx, nb % TH_MAP_WIDTH, nb / TH_MAP_WIDTH, d, pf->nodes[node].distance);
                if (r)
                    return r;
            }
            if (pf->nodes[node].distance < max_dist && !door_blocks(m, node, (unsigned)nb, f, d))
                record(pf, m, node, (f & TH_TILE_PASSABLE) != 0, (unsigned)nb);
        }
        if (pf->heap_size == 0)
            return 0;
        node = pop(pf);
    }
}

int th_path_get(const th_pathfinder *pf, uint8_t *xs, uint8_t *ys, int max)
{
    if (pf->dest == TH_PATH_NONE)
        return -1;
    int len = pf->nodes[pf->dest].distance + 1;
    if (len > max)
        return -1;
    for (unsigned n = pf->dest; n != TH_PATH_NONE; n = pf->nodes[n].prev) {
        int k = pf->nodes[n].distance;
        xs[k] = (uint8_t)(n % TH_MAP_WIDTH);
        ys[k] = (uint8_t)(n / TH_MAP_WIDTH);
    }
    return len;
}
