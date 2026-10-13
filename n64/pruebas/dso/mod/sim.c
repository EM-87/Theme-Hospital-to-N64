// Módulo representativo del motor: A* en una rejilla de 128x128 con nodos
// compactos (8 B) y un bucle de actualización de entidades sobre un pool fijo.
// Sirve para medir tamaño de código (-O2 frente a -Os) y ciclos en la VR4300.
// Es código original escrito para esta prueba; no deriva de CorsixTH ni de wii_vm.
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "sim.h"

// ---------------------------------------------------------------- mapa
// Casilla de 4 B: flags de paso, habitación y coste de recorrerla.
typedef struct {
    uint8_t  flags;     // TILE_*
    uint8_t  room;      // 0 = pasillo
    uint16_t cost;      // coste base para el A* (10 = normal)
} tile_t;

enum {
    TILE_PASSABLE = 1 << 0,
    TILE_WALL_N   = 1 << 1,
    TILE_WALL_E   = 1 << 2,
    TILE_WALL_S   = 1 << 3,
    TILE_WALL_W   = 1 << 4,
    TILE_DOOR     = 1 << 5,
};

tile_t sim_map[MAP_N];

// ---------------------------------------------------------------- A*
// Nodo compacto de 8 B. "gen" evita limpiar los 16.384 nodos en cada búsqueda.
typedef struct {
    uint16_t parent;
    uint16_t g;
    uint16_t f;
    uint8_t  state;     // 1 abierto, 2 cerrado (válido solo si gen coincide)
    uint8_t  gen;
} node_t;

static node_t nodes[MAP_N];
static uint8_t cur_gen;

// Montículo binario de claves empaquetadas (f << 16 | índice), con borrado perezoso.
#define HEAP_MAX (MAP_N * 2)
static uint32_t heap[HEAP_MAX];
static int heap_n;

static inline void heap_push(uint32_t key)
{
    if (heap_n >= HEAP_MAX) return;
    int i = heap_n++;
    while (i > 0) {
        int p = (i - 1) >> 1;
        if (heap[p] <= key) break;
        heap[i] = heap[p];
        i = p;
    }
    heap[i] = key;
}

static inline uint32_t heap_pop(void)
{
    uint32_t top = heap[0];
    uint32_t last = heap[--heap_n];
    int i = 0;
    for (;;) {
        int c = 2 * i + 1;
        if (c >= heap_n) break;
        if (c + 1 < heap_n && heap[c + 1] < heap[c]) c++;
        if (last <= heap[c]) break;
        heap[i] = heap[c];
        i = c;
    }
    heap[i] = last;
    return top;
}

static inline int heuristic(int x0, int y0, int x1, int y1)
{
    int dx = x0 > x1 ? x0 - x1 : x1 - x0;
    int dy = y0 > y1 ? y0 - y1 : y1 - y0;
    return 10 * (dx + dy);
}

// ¿Se puede pasar de la casilla a a la vecina en la dirección dir (0 N, 1 E, 2 S, 3 O)?
static inline bool can_step(int a, int b, int dir)
{
    static const uint8_t wall_out[4] = { TILE_WALL_N, TILE_WALL_E, TILE_WALL_S, TILE_WALL_W };
    static const uint8_t wall_in[4]  = { TILE_WALL_S, TILE_WALL_W, TILE_WALL_N, TILE_WALL_E };
    if (!(sim_map[b].flags & TILE_PASSABLE)) return false;
    if (sim_map[a].flags & wall_out[dir]) return false;
    if (sim_map[b].flags & wall_in[dir]) return false;
    return true;
}

int sim_nodes_expanded;

// Devuelve la longitud del camino escrito en out (de destino a origen), o -1.
int sim_astar(int sx, int sy, int tx, int ty, uint16_t *out, int max_out)
{
    static const int8_t dxs[4] = { 0, 1, 0, -1 };
    static const int8_t dys[4] = { -1, 0, 1, 0 };
    int start = sy * MAP_W + sx, goal = ty * MAP_W + tx;

    if (++cur_gen == 0) {           // vuelta del contador: limpiar de verdad
        memset(nodes, 0, sizeof(nodes));
        cur_gen = 1;
    }
    heap_n = 0;
    node_t *n0 = &nodes[start];
    n0->gen = cur_gen; n0->state = 1; n0->g = 0; n0->parent = start;
    n0->f = heuristic(sx, sy, tx, ty);
    heap_push(((uint32_t)n0->f << 16) | start);

    while (heap_n > 0) {
        uint32_t key = heap_pop();
        int cur = key & 0xFFFF;
        node_t *nc = &nodes[cur];
        if (nc->state == 2 || (key >> 16) > nc->f) continue;   // entrada obsoleta
        nc->state = 2;
        sim_nodes_expanded++;
        if (cur == goal) {
            int len = 0;
            for (int i = goal; len < max_out; i = nodes[i].parent) {
                out[len++] = i;
                if (i == start) break;
            }
            return len;
        }
        int cx = cur & (MAP_W - 1), cy = cur >> 7;
        for (int d = 0; d < 4; d++) {
            int nx = cx + dxs[d], ny = cy + dys[d];
            if ((unsigned)nx >= MAP_W || (unsigned)ny >= MAP_H) continue;
            int nb = ny * MAP_W + nx;
            if (!can_step(cur, nb, d)) continue;
            node_t *nn = &nodes[nb];
            uint32_t g = nc->g + sim_map[nb].cost;
            if (g > 0xFFFF) continue;
            if (nn->gen == cur_gen) {
                if (nn->state == 2 || g >= nn->g) continue;
            } else {
                nn->gen = cur_gen;
            }
            nn->state = 1;
            nn->g = g;
            nn->parent = cur;
            uint32_t f = g + heuristic(nx, ny, tx, ty);
            nn->f = f > 0xFFFF ? 0xFFFF : f;
            heap_push(((uint32_t)nn->f << 16) | nb);
        }
    }
    return -1;
}

// ---------------------------------------------------------------- entidades
enum { ENT_FREE, ENT_IDLE, ENT_WALK, ENT_QUEUE, ENT_USE, ENT_LEAVE };

typedef struct {
    uint16_t x, y;          // casilla actual
    uint16_t tx, ty;        // destino
    uint8_t  state;
    uint8_t  kind;          // 0 paciente, 1 médico, 2 enfermera, 3 conserje
    uint8_t  room;
    uint8_t  anim_frame;
    uint16_t path_off;      // en path_pool
    uint8_t  path_len;
    uint8_t  path_pos;
    uint16_t timer;         // ticks restantes en el estado actual
    int16_t  health;
    int16_t  happiness;
    int16_t  thirst;
    int16_t  warmth;
    uint16_t next_free;
    uint32_t money;
    uint8_t  pad[30];       // hasta 64 B: hueco para el resto del estado
} entity_t;

_Static_assert(sizeof(entity_t) == 64, "entity_t debe ocupar 64 B");

entity_t sim_ents[MAX_ENTS];
static uint16_t free_head;
static int live_count;
static uint16_t path_pool[MAX_ENTS * PATH_SLOT];
static uint16_t room_occupancy[256];   // contadores incrementales por habitación
static uint32_t rng_state = 0x1234567u;
uint32_t sim_stats[8];

static inline uint32_t rng(void)
{
    uint32_t x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rng_state = x;
}

void sim_init_map(void)
{
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            tile_t *t = &sim_map[y * MAP_W + x];
            t->flags = TILE_PASSABLE; t->room = 0; t->cost = 10;
        }
    // Habitaciones de 8x8 en una cuadrícula, con una puerta al sur.
    for (int ry = 0; ry < 6; ry++)
        for (int rx = 0; rx < 6; rx++) {
            int x0 = 8 + rx * 19, y0 = 8 + ry * 19, id = 1 + ry * 6 + rx;
            for (int y = y0; y < y0 + 8; y++)
                for (int x = x0; x < x0 + 8; x++) {
                    tile_t *t = &sim_map[y * MAP_W + x];
                    t->room = id;
                    if (y == y0)     t->flags |= TILE_WALL_N;
                    if (y == y0 + 7) t->flags |= TILE_WALL_S;
                    if (x == x0)     t->flags |= TILE_WALL_W;
                    if (x == x0 + 7) t->flags |= TILE_WALL_E;
                }
            tile_t *door = &sim_map[(y0 + 7) * MAP_W + x0 + 3];
            door->flags = (door->flags & ~TILE_WALL_S) | TILE_DOOR;
            sim_map[(y0 + 8) * MAP_W + x0 + 3].flags &= ~TILE_WALL_N;
        }
    // Algo de terreno caro y obstáculos sueltos.
    for (int i = 0; i < 600; i++) {
        int idx = rng() % MAP_N;
        if (sim_map[idx].room == 0) {
            if (i & 1) sim_map[idx].flags &= ~TILE_PASSABLE;
            else       sim_map[idx].cost = 25;
        }
    }
}

void sim_init_ents(void)
{
    memset(sim_ents, 0, sizeof(sim_ents));
    memset(room_occupancy, 0, sizeof(room_occupancy));
    for (int i = 0; i < MAX_ENTS; i++) sim_ents[i].next_free = i + 1;
    free_head = 0;
    live_count = 0;
}

static int pick_free_tile(int room, uint16_t *ox, uint16_t *oy)
{
    for (int tries = 0; tries < 64; tries++) {
        int idx = rng() % MAP_N;
        if ((sim_map[idx].flags & TILE_PASSABLE) && (room < 0 || sim_map[idx].room == room)) {
            *ox = idx & (MAP_W - 1); *oy = idx >> 7;
            return 1;
        }
    }
    return 0;
}

int sim_spawn(int kind)
{
    if (free_head >= MAX_ENTS) return -1;
    int id = free_head;
    entity_t *e = &sim_ents[id];
    free_head = e->next_free;
    memset(e, 0, sizeof(*e));
    e->kind = kind;
    e->state = ENT_IDLE;
    e->health = 1000; e->happiness = 800; e->thirst = 0; e->warmth = 500;
    e->path_off = id * PATH_SLOT;
    if (!pick_free_tile(0, &e->x, &e->y)) { e->x = 2; e->y = 2; }
    live_count++;
    return id;
}

static void despawn(int id)
{
    entity_t *e = &sim_ents[id];
    if (e->room) room_occupancy[e->room]--;
    e->state = ENT_FREE;
    e->next_free = free_head;
    free_head = id;
    live_count--;
}

static void set_room(entity_t *e, int room)
{
    if (e->room == room) return;
    if (e->room) room_occupancy[e->room]--;
    if (room) room_occupancy[room]++;
    e->room = room;
}

static void start_walk(entity_t *e, int tx, int ty)
{
    int len = sim_astar(e->x, e->y, tx, ty, &path_pool[e->path_off], PATH_SLOT);
    if (len <= 0) { e->state = ENT_IDLE; e->timer = 10; sim_stats[1]++; return; }
    e->tx = tx; e->ty = ty;
    e->path_len = len; e->path_pos = len - 1;
    e->state = ENT_WALK;
    sim_stats[0]++;
}

static void update_one(int id)
{
    entity_t *e = &sim_ents[id];
    e->anim_frame = (e->anim_frame + 1) & 15;
    e->thirst += e->kind == 0 ? 2 : 1;
    int idx = e->y * MAP_W + e->x;
    e->warmth += (sim_map[idx].room ? 3 : -1);
    if (e->warmth > 1000) e->warmth = 1000;
    if (e->thirst > 900) e->happiness -= 2;
    if (e->timer) { e->timer--; return; }

    switch (e->state) {
    case ENT_IDLE: {
        int room = 1 + rng() % 36;
        uint16_t x, y;
        if (pick_free_tile(room, &x, &y)) start_walk(e, x, y);
        else e->timer = 5;
        break;
    }
    case ENT_WALK: {
        uint16_t next = path_pool[e->path_off + e->path_pos];
        e->x = next & (MAP_W - 1); e->y = next >> 7;
        set_room(e, sim_map[next].room);
        if (e->path_pos == 0) {
            e->state = room_occupancy[e->room] > 4 ? ENT_QUEUE : ENT_USE;
            e->timer = 20 + rng() % 40;
        } else {
            e->path_pos--;
        }
        break;
    }
    case ENT_QUEUE:
        e->happiness -= 3;
        e->state = room_occupancy[e->room] > 4 ? ENT_QUEUE : ENT_USE;
        e->timer = 10;
        break;
    case ENT_USE:
        e->thirst = 0;
        e->health += e->kind == 0 ? 50 : 0;
        e->money += 120;
        e->state = (e->kind == 0 && e->health > 1400) ? ENT_LEAVE : ENT_IDLE;
        sim_stats[2]++;
        break;
    case ENT_LEAVE:
        sim_stats[3]++;
        despawn(id);
        break;
    }
}

void sim_tick(void)
{
    for (int i = 0; i < MAX_ENTS; i++)
        if (sim_ents[i].state != ENT_FREE) update_one(i);
}

int sim_live(void) { return live_count; }

uint32_t sim_checksum(void)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < MAX_ENTS; i++) {
        const entity_t *e = &sim_ents[i];
        uint32_t v = e->x | (e->y << 8) | ((uint32_t)e->state << 16) | ((uint32_t)e->room << 24);
        h = (h ^ v) * 16777619u;
        h = (h ^ (uint16_t)e->happiness) * 16777619u;
    }
    for (int i = 0; i < 4; i++) h = (h ^ sim_stats[i]) * 16777619u;
    return h;
}
