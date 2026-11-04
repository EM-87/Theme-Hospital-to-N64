// Objetos sobre el mapa: ocupar y desocupar las casillas de su huella como
// CorsixTH. Sigue a Object:setTile, occupyTilesByObjectFootprintAt y
// deoccupyTilesByObjectFootprintAt (entities/object.lua), Door:setTile
// (objects/door.lua), SwingDoor y EntranceDoor (objects/doors/) y
// World:resetSideObjects, isTilePartOfNearbyObject, addObjectToTile y
// removeObjectFromTile (world.lua).
//
// Lo que CorsixTH guarda en cada casilla son dos listas: la de los tipos de
// objeto del mapa (map_tile.objects, en th_map) y la de los objetos de Lua
// cuyo origen es esa casilla (world.objects). La segunda no se guarda: es la de
// los objetos con ese origen, en el orden de th_object.seq.
//
// La basura (Litter) todavía no entra en el mapa: la simulación la lleva en su
// propia lista (th_sim.litter).
#ifndef TH_OBJECTS_H
#define TH_OBJECTS_H

#include <stdint.h>

#include "th/humanoid.h"
#include "th/map.h"
#include "th/path.h"
#include "th/rules.h"

// Tipos de objeto que la lógica distingue; los demás son TH_OBJ_OTHER.
enum {
    TH_OBJ_OTHER,
    TH_OBJ_DOOR,
    TH_OBJ_BENCH,
    TH_OBJ_RECEPTION_DESK,
    TH_OBJ_DESK,
    TH_OBJ_CABINET,
    TH_OBJ_CHAIR,
    TH_OBJ_RADIATOR,
};

// Cómo ocupa casillas: la clase de Lua (campo class de la definición), con
// las que no cambian setTile como TH_OCLASS_OBJECT.
enum {
    TH_OCLASS_OBJECT,
    TH_OCLASS_SIDE,          // SideObject: radiador, extintor y papelera
    TH_OCLASS_DOOR,          // Door: la puerta de una sala
    TH_OCLASS_SWING_DOOR,    // SwingDoor: las dos hojas de la puerta doble
    TH_OCLASS_ENTRANCE_DOOR, // EntranceDoor: las puertas de la entrada
};

// th_object.oflags
enum {
    TH_OBJF_ON_MAP = 1,        // tiene casilla (tile_x ~= nil en CorsixTH)
    TH_OBJF_SIDE_PASSABLE = 2, // set_passable_flags: un objeto de borde que cortó el paso
    TH_OBJF_MASTER = 4,        // is_master: la hoja derecha de las puertas dobles y de la entrada
};

typedef struct th_object {
    uint8_t type;      // TH_OBJ_*
    uint8_t thob;      // número de objeto del mapa (object_type.thob)
    uint8_t x, y, dir; // dir: TH_DIR_*; x, y: el origen, si oflags lleva TH_OBJF_ON_MAP
    uint8_t room;      // sala (número) o 0
    uint8_t user;      // humanoide que lo usa (TH_NONE)
    uint8_t reserved_for;
    uint8_t queue;        // cola de las puertas y los mostradores (TH_NONE)
    uint8_t receptionist; // mostrador: quien lo atiende
    uint16_t queue_advance_timer;
    uint16_t times_used;
    uint8_t oclass; // TH_OCLASS_*
    uint8_t oflags; // TH_OBJF_*
    th_rnode def;   // objetos.<id> en las definiciones
    uint32_t seq;   // orden de llegada a su casilla (world.objects): el menor va primero
} th_object;

// Lo que necesitan las funciones de ocupar: el mapa, el buscador de caminos
// (isTilePartOfNearbyObject e isReachableFromHospital), las definiciones y los
// objetos, en el orden de world.entities. seq cuenta las llegadas a una
// casilla (World:addObjectToTile).
typedef struct th_objects {
    th_map *map;
    th_pathfinder *pf;
    const th_rules *defs;
    th_object *obj;
    int n_obj;
    uint32_t *seq;
} th_objects;

// Rellena la parte de th_object que usa este módulo (oclass, la maestra de
// las puertas dobles, thob y def) a partir de la definición; el objeto queda
// fuera del mapa. Devuelve -1 si la definición no vale.
int th_object_init(th_object *o, const th_rules *defs, th_rnode def, int dir);

// Object:setTile (y las de Door, SwingDoor y EntranceDoor): quita el objeto de
// donde estaba y, si x >= 0, lo pone con origen en (x, y), con las banderas
// que cambian las dos cosas. Las sombras se recalculan si es una puerta, como
// en CorsixTH; para los demás, las recalcula quien llama cuando acabe.
void th_object_set_tile(th_objects *w, int i, int x, int y);

// World:resetSideObjects: vuelve a poner en su sitio los objetos de borde,
// casilla a casilla. CorsixTH recorre las casillas en el orden de pairs() de
// una tabla; aquí van por índice. Solo cambia algo si dos objetos de borde
// cortan el mismo borde desde las dos casillas.
void th_objects_reset_side(th_objects *w);

// World:isTilePartOfNearbyObject: 1 si (x, y) es casilla de paso de un objeto
// cuyo origen está a distance pasos o menos (sin cruzar puertas).
int th_objects_part_of_nearby(th_objects *w, int x, int y, int distance);

// El primer objeto con origen en (x, y) (World:getObject sin tipo), o -1.
int th_objects_first_at(const th_objects *w, int x, int y);

// Las casillas de paso del objeto (getWalkableTiles): devuelve cuántas,
// TH_OBJECT_MAX_WALKABLE como mucho.
#define TH_OBJECT_MAX_WALKABLE 16
int th_object_walkable_tiles(const th_objects *w, int i, int *xs, int *ys);

// Para los objetos que ya están en el mapa sin haber pasado por
// th_object_set_tile (las puertas de la entrada, que pone th_map_load): les
// da su casilla y su sitio en la lista de la casilla, sin tocar el mapa.
void th_object_adopt(th_objects *w, int i, int x, int y);

#endif
