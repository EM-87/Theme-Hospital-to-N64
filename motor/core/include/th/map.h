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
// Objetos del fichero del nivel: en la campaña solo hay puertas de la entrada,
// 20 como mucho (nivel 11). Caben en la máscara de th_map_set_parcel_owner.
#define TH_MAP_MAX_MAP_OBJECTS 32
// Casillas con más de un objeto a la vez (objetos de borde juntos y, sobre
// todo, la basura, que se amontona: en el hito 5, sin bedeles, hasta 10 cosas
// en una casilla y 19 casillas así en 9.000 horas): sus objetos del segundo
// en adelante van en una tabla aparte. Caben 16 por casilla y 128 casillas;
// lo que no cabe se cuenta en th_map.thob_lost.
#define TH_MAP_MAX_STACKS 128
#define TH_MAP_STACK_LEN 15

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
    // Los objetos de la casilla, como la lista de map_tile de CorsixTH: el
    // primero (thob, 0 = ninguno) y, si hay más, el índice + 1 de su pila en
    // th_map.stacks (0 = no hay más). Solo los anota la casilla de origen del
    // objeto (World:addObjectToTile).
    uint8_t object, stack;
    uint16_t room; // 0 = ninguna
} th_tile;

// Un objeto del fichero del nivel (los que CorsixTH pasa a World:createMapObjects).
typedef struct th_map_object {
    uint8_t x, y, thob;
    uint8_t flags;  // del fichero: par = norte, impar = oeste (Object:Object, "map object")
    uint8_t placed; // ya está en el mapa (los de parcelas sin dueño esperan a que se compre)
} th_map_object;

typedef struct th_tile_stack {
    uint16_t tile; // índice de la casilla
    uint8_t n;     // cuántos objetos hay aquí (0 = pila libre)
    uint8_t thob[TH_MAP_STACK_LEN];
} th_tile_stack;

typedef struct th_map {
    th_tile tiles[TH_MAP_WIDTH * TH_MAP_HEIGHT]; // por filas: tiles[y * TH_MAP_WIDTH + x]
    uint8_t players;
    uint8_t parcel_count; // incluida la parcela 0 (el exterior)
    uint8_t level;        // nivel de la campaña (1–12) o 0
    uint8_t parcel_owner[TH_MAP_MAX_PARCELS];
    uint16_t parcel_tiles[TH_MAP_MAX_PARCELS];
    uint8_t camera_x, camera_y, heliport_x, heliport_y; // jugador 1
    // Cambia cada vez que se recalculan las sombras, que es después de tocar
    // las capas o las banderas: así la vista girada sabe cuándo rehacerse.
    uint32_t version;
    uint8_t n_map_objects;
    th_map_object map_objects[TH_MAP_MAX_MAP_OBJECTS];
    th_tile_stack stacks[TH_MAP_MAX_STACKS];
    uint16_t thob_lost; // objetos que no cupieron en la lista de su casilla
    // Las casillas que tienen lista en world.objects, un bit por casilla:
    // World:addObjectToTile la crea con el primer objeto o la primera basura
    // que llega y nadie la borra, así que getObjects da {} y no nil aunque se
    // haya quedado vacía. Patient:setTile no tira basura en ellas.
    uint8_t object_lists[TH_MAP_WIDTH * TH_MAP_HEIGHT / 8];
} th_map;

enum {
    TH_MAP_OK = 0,
    TH_MAP_ERR_SHORT = -1,   // faltan bytes
    TH_MAP_ERR_MAGIC = -2,   // no es TMP1
    TH_MAP_ERR_FORMAT = -3,  // tamaño o casilla distintos de lo esperado
    TH_MAP_ERR_PARCELS = -4, // demasiadas parcelas
};

// Carga un nivel TMP1 y lo deja como CorsixTH tras Map:load y
// World:createMapObjects: la parcela p es del jugador p si p <= jugadores y si
// no queda sin dueño, y los objetos del nivel (las puertas de la entrada) se
// colocan en las parcelas con dueño y en la 0. 'level' es el número del nivel
// de la campaña (activa Map:_fixTiles) o 0.
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
// Map:setPlotOwner y World:setPlotOwner: con dueño, se colocan los objetos del
// nivel de la parcela que esperaban. Devuelve cuántas paredes divisorias se
// quitaron (CorsixTH cierra con ellas las salas que quedan abiertas; eso
// llegará con las salas) o -1 si los argumentos no valen.
// En *placed (si no es NULL) queda la máscara de los map_objects que se acaban
// de colocar: el mapa ya tiene sus banderas y su tipo en la lista, pero quien
// llama tiene que crear su th_object (th_object_init_map_object y
// th_object_adopt), que es lo que hace el newObject de World:setPlotOwner.
// CorsixTH los crea en el orden de pairs(delayed_map_objects); aquí van en el
// del fichero.
int th_map_set_parcel_owner(th_map *m, int parcel, int owner, uint32_t *placed);

// La lista de objetos de una casilla (map_tile.objects de CorsixTH).
// Añadir va al final (setCellFlags{thob}); quitar quita el primero igual
// (removeObjectType). th_map_add_thob devuelve -1 si no cabe, y lo cuenta en
// thob_lost.
int th_map_add_thob(th_map *m, int x, int y, int thob);
int th_map_remove_thob(th_map *m, int x, int y, int thob);
int th_map_has_thob(const th_map *m, int x, int y, int thob);
// Cuántos objetos tiene la casilla y el k-ésimo (desde 0).
int th_map_thob_count(const th_map *m, int x, int y);
int th_map_thob_at(const th_map *m, int x, int y, int k);
// Vacía la lista de la casilla (eraseObjectTypes).
void th_map_clear_thobs(th_map *m, int x, int y);

// La lista de world.objects de la casilla: se abre con el primer objeto o la
// primera basura (World:addObjectToTile) y ya no se cierra.
void th_map_open_object_list(th_map *m, int x, int y);
int th_map_has_object_list(const th_map *m, int x, int y);

// Una puerta de la entrada en (x, y): EntranceDoor:setTile de CorsixTH
// (objects/doors/entrance_*_door.lua). thob 59 es la derecha (la maestra) y
// 58 la izquierda; west, si mira al oeste. Con on = 0 la quita. Al ponerla
// abre la lista de world.objects de la casilla.
void th_map_entrance_door(th_map *m, int x, int y, int thob, int west, int on);
// Las casillas de paso de esa puerta además de la suya
// (additional_walkable_tiles): cuántas son y sus desplazamientos.
int th_map_entrance_door_tiles(int thob, int west, int8_t dx[3], int8_t dy[3]);
void th_map_update_pathfinding(th_map *m);
void th_map_update_shadows(th_map *m);
// Huella de las capas, banderas, salas, objetos, listas de world.objects y
// dueños: para comparar el PC con la N64.
uint32_t th_map_hash(const th_map *m);

#endif
