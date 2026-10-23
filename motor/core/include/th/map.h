// Mapa de un nivel: casillas, parcelas, tránsito y sombras.
// Sigue a level_map de CorsixTH (th_map.cpp) y a Map de su map.lua: carga,
// dueño de cada parcela (las que no son del jugador se ven como césped),
// paredes divisorias entre parcelas, banderas de tránsito, sombras y las
// correcciones que CorsixTH hace al cargar los niveles de la campaña.
// Los niveles llegan en el formato TMP1 del conversor (tools/conv/niveles.py).
#ifndef TH_MAP_H
#define TH_MAP_H

#include <stddef.h>
#include <stdint.h>

#define TH_MAP_WIDTH 128
#define TH_MAP_HEIGHT 128
#define TH_MAP_MAX_PARCELS 64
#define TH_MAP_MAX_PLAYERS 4

// Banderas de una casilla. Las 7 primeras son las del fichero del nivel.
enum {
    TH_TILE_PASSABLE = 1u << 0,
    TH_TILE_HOSPITAL = 1u << 1,
    TH_TILE_BUILDABLE = 1u << 2,
    TH_TILE_BUILDABLE_N = 1u << 3,
    TH_TILE_BUILDABLE_E = 1u << 4,
    TH_TILE_BUILDABLE_S = 1u << 5,
    TH_TILE_BUILDABLE_W = 1u << 6,
    TH_TILE_TRAVEL_N = 1u << 7,
    TH_TILE_TRAVEL_E = 1u << 8,
    TH_TILE_TRAVEL_S = 1u << 9,
    TH_TILE_TRAVEL_W = 1u << 10,
    TH_TILE_SHADOW_HALF = 1u << 11, // bloque 75 sobre el suelo
    TH_TILE_SHADOW_FULL = 1u << 12, // bloque 74 sobre el suelo
    TH_TILE_SHADOW_WALL = 1u << 13, // bloque 156 sobre la pared
    TH_TILE_TALL_N = 1u << 14,      // objeto alto junto a la pared norte (sombras)
    TH_TILE_TALL_W = 1u << 15,
    TH_TILE_ROOM = 1u << 16,
    TH_TILE_DOOR_N = 1u << 17,
    TH_TILE_DOOR_W = 1u << 18,
    TH_TILE_DO_NOT_IDLE = 1u << 19,
};

#define TH_TILE_FILE_FLAGS 0x7fu
#define TH_TILE_BUILDABLE_SIDES \
    (TH_TILE_BUILDABLE_N | TH_TILE_BUILDABLE_E | TH_TILE_BUILDABLE_S | TH_TILE_BUILDABLE_W)

// 16 B por casilla: el mapa entero ocupa 256 KiB (docs/04_decision.md).
typedef struct th_tile {
    uint8_t floor, wall_n, wall_w; // bloques de la hoja MBLK; 0 = nada
    uint8_t parcel;
    uint32_t flags;
    // Tal como vienen en el nivel (original_cells de CorsixTH): se restauran
    // al comprar la parcela. Las paredes originales incluyen las divisorias.
    uint8_t orig_floor, orig_wall_n, orig_wall_w, orig_flags;
    uint8_t object, object_dir; // objeto del mapa (thob) y su dirección
    uint16_t room;              // 0 = ninguna
} th_tile;

typedef struct th_map {
    th_tile tiles[TH_MAP_WIDTH * TH_MAP_HEIGHT]; // por filas: tiles[y * TH_MAP_WIDTH + x]
    uint8_t players;
    uint8_t parcel_count; // incluida la parcela 0 (el exterior)
    uint8_t level;        // nivel de la campaña (1–12) o 0
    uint8_t parcel_owner[TH_MAP_MAX_PARCELS];
    uint16_t parcel_tiles[TH_MAP_MAX_PARCELS];
    uint8_t camera_x, camera_y, heliport_x, heliport_y; // jugador 1
} th_map;

enum {
    TH_MAP_OK = 0,
    TH_MAP_ERR_SHORT = -1,   // faltan bytes
    TH_MAP_ERR_MAGIC = -2,   // no es TMP1
    TH_MAP_ERR_FORMAT = -3,  // tamaño o casilla distintos de lo esperado
    TH_MAP_ERR_PARCELS = -4, // demasiadas parcelas
};

// Carga un nivel TMP1 y lo deja como CorsixTH tras Map:load: la parcela p es
// del jugador p si p <= jugadores y si no queda sin dueño. 'level' es el
// número del nivel de la campaña (activa Map:_fixTiles) o 0.
int th_map_load(th_map *m, const uint8_t *data, size_t len, int level);

static inline th_tile *th_map_tile(th_map *m, int x, int y)
{
    return &m->tiles[y * TH_MAP_WIDTH + x];
}

static inline const th_tile *th_map_tile_c(const th_map *m, int x, int y)
{
    return &m->tiles[y * TH_MAP_WIDTH + x];
}

int th_map_parcel_owner(const th_map *m, int parcel);
// Map:setPlotOwner. Devuelve cuántas paredes divisorias se quitaron
// (CorsixTH cierra con ellas las salas que quedan abiertas; eso llegará con
// las salas) o -1 si los argumentos no valen.
int th_map_set_parcel_owner(th_map *m, int parcel, int owner);
void th_map_update_pathfinding(th_map *m);
void th_map_update_shadows(th_map *m);
// Huella de las capas, banderas y dueños: para comparar el PC con la N64.
uint32_t th_map_hash(const th_map *m);

#endif
