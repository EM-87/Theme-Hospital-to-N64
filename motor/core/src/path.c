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

static unsigned value(const th_pathfinder *pf, unsigned i)
{
    return (unsigned)pf->nodes[i].distance + pf->nodes[i].guess;
}

static uint16_t guess(const th_pathfinder *pf, unsigned i)
{
    int dx = (int)(i % TH_MAP_WIDTH) - pf->dest_x, dy = (int)(i / TH_MAP_WIDTH) - pf->dest_y;
    return (uint16_t)((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
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
    const unsigned v = value(pf, n);
    while (left < pf->heap_size) {
        min = i;
        unsigned right = (i + 1) * 2, minvalue = v, swap = 0;
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
    if (nb == pf->start)
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
