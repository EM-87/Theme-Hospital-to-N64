#include "th/map.h"

#include <string.h>

#define CELL_BYTES 10
#define HEADER_BYTES 32

// Capas, como tile_layer de CorsixTH: el número entra en el bloque de las
// paredes divisorias (layer + 141 sin dueño, layer + 143 con dueño).
enum { LAYER_NORTH = 1, LAYER_WEST = 2 };

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

int th_map_parcel_owner(const th_map *m, int parcel)
{
    if (parcel < 0 || parcel >= m->parcel_count)
        return 0;
    return m->parcel_owner[parcel];
}

void th_map_update_pathfinding(th_map *m)
{
    const uint32_t travel = TH_TILE_TRAVEL_N | TH_TILE_TRAVEL_E | TH_TILE_TRAVEL_S | TH_TILE_TRAVEL_W;
    th_tile *t = m->tiles;
    for (int y = 0; y < TH_MAP_HEIGHT; y++) {
        for (int x = 0; x < TH_MAP_WIDTH; x++, t++) {
            t->flags |= travel;
            if (x == 0)
                t->flags &= ~TH_TILE_TRAVEL_W;
            else if (x == TH_MAP_WIDTH - 1)
                t->flags &= ~TH_TILE_TRAVEL_E;
            if (y == 0)
                t->flags &= ~TH_TILE_TRAVEL_N;
            else if (y == TH_MAP_HEIGHT - 1)
                t->flags &= ~TH_TILE_TRAVEL_S;
            // Una pared corta el paso por los dos lados.
            if (t->wall_n) {
                t->flags &= ~TH_TILE_TRAVEL_N;
                if (y != 0)
                    t[-TH_MAP_WIDTH].flags &= ~TH_TILE_TRAVEL_S;
            }
            if (t->wall_w) {
                t->flags &= ~TH_TILE_TRAVEL_W;
                if (x != 0)
                    t[-1].flags &= ~TH_TILE_TRAVEL_E;
            }
        }
    }
}

// Para las sombras, una casilla tiene pared si hay un objeto alto (o una
// puerta) en ese lado o si el bloque está entre 82 y 164.
static int is_wall(uint8_t block, int tall)
{
    return tall || (block >= 82 && block <= 164);
}

void th_map_update_shadows(th_map *m)
{
    static uint32_t versiones; // distinta en cada llamada, también entre mapas
    m->version = ++versiones;
    const uint32_t shadows = TH_TILE_SHADOW_HALF | TH_TILE_SHADOW_FULL | TH_TILE_SHADOW_WALL;
    th_tile *t = m->tiles;
    for (int y = 0; y < TH_MAP_HEIGHT; y++) {
        for (int x = 0; x < TH_MAP_WIDTH; x++, t++) {
            t->flags &= ~shadows;
            int tall_w = (t->flags & TH_TILE_TALL_W) != 0;
            if (!is_wall(t->wall_w, tall_w))
                continue;
            t->flags |= TH_TILE_SHADOW_HALF;
            if (is_wall(t->wall_n, (t->flags & TH_TILE_TALL_N) != 0)) {
                t->flags |= TH_TILE_SHADOW_WALL;
            } else if (y != 0) {
                // La sombra se proyecta sobre la casilla del norte y dobla la
                // esquina. Como en CorsixTH, la pared oeste del vecino se mira
                // con la bandera «alto» de esta casilla, no la suya.
                th_tile *n = t - TH_MAP_WIDTH;
                n->flags |= TH_TILE_SHADOW_FULL;
                if (x != 0 && !is_wall(n->wall_w, tall_w))
                    n[-1].flags |= TH_TILE_SHADOW_FULL;
            }
        }
    }
}

// addRemoveDividerWalls de th_map.cpp: entre dos casillas de hospital de
// parcelas distintas, pone una pared divisoria si los dueños son distintos y
// la quita si son el mismo y una de las dos es la parcela que cambia.
static int divider_wall(th_map *m, th_tile *t, int coord, int delta, int layer, int parcel)
{
    if (coord <= 0)
        return 0;
    const th_tile *o = t - delta;
    if (!(t->orig_flags & TH_TILE_HOSPITAL) || !(o->orig_flags & TH_TILE_HOSPITAL) || t->parcel == o->parcel)
        return 0;
    uint8_t *wall = layer == LAYER_NORTH ? &t->wall_n : &t->wall_w;
    int owner = th_map_parcel_owner(m, t->parcel);
    if (owner != th_map_parcel_owner(m, o->parcel)) {
        *wall = (uint8_t)(layer + (owner ? 143 : 141));
    } else if (t->parcel == parcel || o->parcel == parcel) {
        *wall = 0;
        return 1;
    }
    return 0;
}

// Suelos de interior (son hospital) y suelos por los que se puede andar.
static int is_hospital_floor(uint8_t f)
{
    switch (f) {
    case 16: case 17: case 18: case 19: case 20: case 21: case 22: case 23:
    case 66: case 70: case 76:
        return 1;
    default:
        return 0;
    }
}

static int is_passable_floor(uint8_t f)
{
    // Interior, caminos de hormigón, helipuerto y carreteras.
    return is_hospital_floor(f) || f == 4 || f == 5 || f == 15 || (f >= 41 && f <= 58);
}

// Map:_fixTiles: corrige banderas mal puestas en los niveles de la campaña.
static void fix_tiles(th_map *m)
{
    if (m->level == 0)
        return;
    for (int i = 0; i < TH_MAP_WIDTH * TH_MAP_HEIGHT; i++) {
        th_tile *t = &m->tiles[i];
        if (!is_hospital_floor(t->floor))
            t->flags &= ~TH_TILE_HOSPITAL;
        if (!is_passable_floor(t->floor))
            t->flags &= ~(TH_TILE_PASSABLE | TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_SIDES);
    }
    if (m->level == 6) {
        // Coordenadas de map.lua menos 1: (56, 71) y (58, 72).
        th_map_tile(m, 55, 70)->flags |= TH_TILE_HOSPITAL | TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_SIDES;
        th_map_tile(m, 57, 71)->flags &= ~TH_TILE_PASSABLE;
    }
}

// level_map::set_parcel_owner sin las llamadas de Lua.
static int set_owner(th_map *m, int parcel, int owner)
{
    int removed = 0;
    m->parcel_owner[parcel] = (uint8_t)owner;
    th_tile *t = m->tiles;
    for (int y = 0; y < TH_MAP_HEIGHT; y++) {
        for (int x = 0; x < TH_MAP_WIDTH; x++, t++) {
            if (t->parcel == parcel) {
                if (owner != 0) {
                    t->floor = t->orig_floor;
                    t->wall_n = t->orig_wall_n;
                    t->wall_w = t->orig_wall_w;
                    t->flags = t->orig_flags;
                } else {
                    // Césped segado a rayas y, cada 8 casillas, un adorno.
                    t->floor = (uint8_t)(((x & 1) << 1) + 1);
                    t->wall_n = 0;
                    t->wall_w = 0;
                    t->flags = 0;
                    if (((x | y) & 7) == 0)
                        t->wall_n = (uint8_t)(192 + (x ^ y) % 9);
                }
            }
            removed += divider_wall(m, t, x, 1, LAYER_WEST, parcel);
            removed += divider_wall(m, t, y, TH_MAP_WIDTH, LAYER_NORTH, parcel);
        }
    }
    th_map_update_pathfinding(m);
    th_map_update_shadows(m);
    return removed;
}

// Map:setPlotOwner: el dueño, el tránsito otra vez y _fixTiles.
static int set_parcel_owner(th_map *m, int parcel, int owner)
{
    int removed = set_owner(m, parcel, owner);
    th_map_update_pathfinding(m);
    fix_tiles(m);
    return removed;
}

// World:_createMapObject: se colocan los objetos del nivel de la parcela 0 y de
// las parcelas con dueño, en el orden del fichero; los demás esperan a que se
// compre su parcela. Solo las puertas de la entrada se pueden crear así
// (supports_creation_for_map); con otro thob, CorsixTH avisa y no hace nada.
static void place_map_objects(th_map *m, int parcel)
{
    for (int i = 0; i < m->n_map_objects; i++) {
        th_map_object *o = &m->map_objects[i];
        const int p = th_map_tile(m, o->x, o->y)->parcel;
        if (o->placed || (parcel >= 0 && p != parcel) || (p != 0 && th_map_parcel_owner(m, p) == 0))
            continue;
        if (o->thob == 58 || o->thob == 59)
            th_map_entrance_door(m, o->x, o->y, o->thob, o->flags & 1, 1);
        o->placed = 1;
    }
}

int th_map_set_parcel_owner(th_map *m, int parcel, int owner)
{
    if (parcel <= 0 || parcel >= m->parcel_count || owner < 0 || owner > TH_MAP_MAX_PLAYERS)
        return -1;
    int removed = set_parcel_owner(m, parcel, owner);
    // World:setPlotOwner: los objetos del nivel que esperaban a la parcela.
    if (owner != 0)
        place_map_objects(m, parcel);
    th_map_update_shadows(m);
    return removed;
}

static th_tile_stack *stack_of(th_map *m, const th_tile *t)
{
    return t->stack ? &m->stacks[t->stack - 1] : NULL;
}

int th_map_thob_count(const th_map *m, int x, int y)
{
    const th_tile *t = th_map_tile_c(m, x, y);
    return t->object ? 1 + (t->stack ? m->stacks[t->stack - 1].n : 0) : 0;
}

int th_map_thob_at(const th_map *m, int x, int y, int k)
{
    const th_tile *t = th_map_tile_c(m, x, y);
    if (k == 0)
        return t->object;
    if (!t->stack || k > m->stacks[t->stack - 1].n)
        return 0;
    return m->stacks[t->stack - 1].thob[k - 1];
}

int th_map_has_thob(const th_map *m, int x, int y, int thob)
{
    const int n = th_map_thob_count(m, x, y);
    for (int k = 0; k < n; k++)
        if (th_map_thob_at(m, x, y, k) == thob)
            return 1;
    return 0;
}

int th_map_add_thob(th_map *m, int x, int y, int thob)
{
    th_tile *t = th_map_tile(m, x, y);
    if (!t->object) {
        t->object = (uint8_t)thob;
        return 0;
    }
    th_tile_stack *st = stack_of(m, t);
    if (!st) {
        for (int i = 0; i < TH_MAP_MAX_STACKS && !st; i++)
            if (m->stacks[i].n == 0) {
                st = &m->stacks[i];
                st->tile = (uint16_t)(y * TH_MAP_WIDTH + x);
                t->stack = (uint8_t)(i + 1);
            }
        if (!st) {
            m->thob_lost++;
            return -1;
        }
    }
    if (st->n >= TH_MAP_STACK_LEN) {
        m->thob_lost++;
        return -1;
    }
    st->thob[st->n++] = (uint8_t)thob;
    return 0;
}

int th_map_remove_thob(th_map *m, int x, int y, int thob)
{
    th_tile *t = th_map_tile(m, x, y);
    th_tile_stack *st = stack_of(m, t);
    const int n = th_map_thob_count(m, x, y);
    for (int k = 0; k < n; k++) {
        if (th_map_thob_at(m, x, y, k) != thob)
            continue;
        // Se corre la lista desde k.
        for (int j = k; j < n - 1; j++) {
            const int next = th_map_thob_at(m, x, y, j + 1);
            if (j == 0)
                t->object = (uint8_t)next;
            else
                st->thob[j - 1] = (uint8_t)next;
        }
        if (n == 1) {
            t->object = 0;
        } else if (--st->n == 0) {
            t->stack = 0;
        }
        return 1;
    }
    return 0;
}

int th_map_entrance_door_tiles(int thob, int west, int8_t dx[3], int8_t dy[3])
{
    static const int8_t slave[2][1][2] = {{{0, -1}}, {{-1, 0}}};
    static const int8_t master[2][3][2] = {{{0, -1}, {1, 0}, {1, -1}}, {{-1, 0}, {0, 1}, {-1, 1}}};
    const int is_master = thob == 59, n = is_master ? 3 : 1;
    west = west != 0;
    for (int k = 0; k < n; k++) {
        dx[k] = is_master ? master[west][k][0] : slave[west][k][0];
        dy[k] = is_master ? master[west][k][1] : slave[west][k][1];
    }
    return n;
}

void th_map_entrance_door(th_map *m, int x, int y, int thob, int west, int on)
{
    int8_t dx[3], dy[3];
    const int n = th_map_entrance_door_tiles(thob, west, dx, dy);
    const uint32_t tall = west ? TH_TILE_TALL_W : TH_TILE_TALL_N;
    if (on)
        th_map_add_thob(m, x, y, thob); // Object.setTile, antes que las banderas
    th_tile *t = th_map_tile(m, x, y);
    t->flags = on ? (t->flags | tall | TH_TILE_DO_NOT_IDLE) & ~TH_TILE_BUILDABLE
                  : (t->flags | TH_TILE_BUILDABLE) & ~(tall | TH_TILE_DO_NOT_IDLE);
    for (int k = 0; k < n; k++) {
        const int ox = x + dx[k], oy = y + dy[k];
        if (ox < 0 || oy < 0 || ox >= TH_MAP_WIDTH || oy >= TH_MAP_HEIGHT)
            continue;
        th_tile *o = th_map_tile(m, ox, oy);
        o->flags = on ? (o->flags | TH_TILE_DO_NOT_IDLE) & ~TH_TILE_BUILDABLE
                      : (o->flags | TH_TILE_BUILDABLE) & ~TH_TILE_DO_NOT_IDLE;
    }
    if (!on)
        th_map_remove_thob(m, x, y, thob);
}

int th_map_load(th_map *m, const uint8_t *data, size_t len, int level)
{
    if (len < HEADER_BYTES)
        return TH_MAP_ERR_SHORT;
    if (memcmp(data, "TMP1", 4) != 0)
        return TH_MAP_ERR_MAGIC;
    if (rd16(data + 4) != TH_MAP_WIDTH || rd16(data + 6) != TH_MAP_HEIGHT || rd16(data + 10) < CELL_BYTES)
        return TH_MAP_ERR_FORMAT;
    size_t cell = rd16(data + 10);
    if (len < HEADER_BYTES + cell * TH_MAP_WIDTH * TH_MAP_HEIGHT)
        return TH_MAP_ERR_SHORT;
    int parcels = data[9];
    if (parcels < 1 || parcels > TH_MAP_MAX_PARCELS)
        return TH_MAP_ERR_PARCELS;

    memset(m, 0, sizeof(*m));
    m->players = data[8] < 1 ? 1 : data[8] > TH_MAP_MAX_PLAYERS ? TH_MAP_MAX_PLAYERS : data[8];
    m->parcel_count = (uint8_t)parcels;
    m->level = (uint8_t)level;
    m->camera_x = (uint8_t)rd16(data + 12);
    m->camera_y = (uint8_t)rd16(data + 14);
    m->heliport_x = (uint8_t)rd16(data + 16);
    m->heliport_y = (uint8_t)rd16(data + 18);

    const uint8_t *p = data + HEADER_BYTES;
    for (int i = 0; i < TH_MAP_WIDTH * TH_MAP_HEIGHT; i++, p += cell) {
        th_tile *t = &m->tiles[i];
        if (p[8] >= parcels)
            return TH_MAP_ERR_PARCELS;
        t->floor = t->orig_floor = p[0];
        t->wall_n = p[1];
        t->wall_w = p[2];
        t->flags = t->orig_flags = p[3] & TH_TILE_FILE_FLAGS;
        t->orig_wall_n = p[4];
        t->orig_wall_w = p[5];
        t->parcel = p[8];
        m->parcel_tiles[t->parcel]++;
        // Los objetos del fichero no van a la casilla: se colocan después, como
        // World:createMapObjects (load_from_th_file deja vacía la lista).
        if (p[6] && m->n_map_objects < TH_MAP_MAX_MAP_OBJECTS) {
            th_map_object *o = &m->map_objects[m->n_map_objects++];
            o->x = (uint8_t)(i % TH_MAP_WIDTH);
            o->y = (uint8_t)(i / TH_MAP_WIDTH);
            o->thob = p[6];
            o->flags = p[7];
        }
    }
    m->parcel_tiles[0] = 0;
    // Al cargar, todas las parcelas salvo el exterior son del jugador 1.
    for (int i = 1; i < parcels; i++)
        m->parcel_owner[i] = 1;
    // load_from_th_file calcula aquí el tránsito sin contar las plantas del
    // estanque como pared, pero Map:load lo recalcula enseguida contándolas
    // (en un mapa con al menos una parcela, como todos los de la campaña).
    th_map_update_pathfinding(m);
    th_map_update_shadows(m);

    // Map:load: cada jugador recibe su parcela; las demás quedan sin dueño.
    for (int i = 1; i < parcels; i++)
        set_parcel_owner(m, i, i <= m->players ? i : 0);
    fix_tiles(m);
    // World:createMapObjects, ya con el mundo creado.
    place_map_objects(m, -1);
    th_map_update_shadows(m);
    return TH_MAP_OK;
}

uint32_t th_map_hash(const th_map *m)
{
    uint32_t h = 2166136261u;
#define MIX(v)                       \
    do {                             \
        h ^= (uint32_t)(v);          \
        h *= 16777619u;              \
    } while (0)
    for (int i = 0; i < TH_MAP_WIDTH * TH_MAP_HEIGHT; i++) {
        const th_tile *t = &m->tiles[i];
        MIX(t->floor | (t->wall_n << 8) | (t->wall_w << 16) | ((uint32_t)t->parcel << 24));
        MIX(t->flags);
        MIX(t->room);
        const int n = th_map_thob_count(m, i % TH_MAP_WIDTH, i / TH_MAP_WIDTH);
        for (int k = 0; k < n; k++)
            MIX(th_map_thob_at(m, i % TH_MAP_WIDTH, i / TH_MAP_WIDTH, k));
    }
    for (int i = 0; i < m->parcel_count; i++)
        MIX(m->parcel_owner[i]);
#undef MIX
    return h;
}
