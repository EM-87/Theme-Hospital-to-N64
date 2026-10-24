// Humanoides: la parte de Entity y Humanoid de CorsixTH (entity.lua,
// entities/humanoid.lua) que hace falta para que alguien camine por el mapa,
// con las acciones de caminar (humanoid_actions/walk.lua) y de quedarse
// quieto (idle.lua). Puertas, salas y colas llegarán con el hito 4.
//
// El tiempo va por ticks del mundo: a velocidad Normal, uno cada 54 ms. En
// cada tick, el humanoide avanza un fotograma de su animación y su
// desplazamiento en píxeles, y después corre su temporizador; al vencer, la
// acción en curso decide el siguiente paso. Los desplazamientos están en
// píxeles de 640 × 480, como en CorsixTH; la vista los escala.
#ifndef TH_HUMANOID_H
#define TH_HUMANOID_H

#include <stdint.h>

#include "th/anim.h"
#include "th/map.h"
#include "th/path.h"

#define TH_HUMANOID_PATH_MAX 1024 // pasos de un camino guardado (2 bits cada uno)
#define TH_HUMANOID_QUEUE 4

enum { TH_DIR_NORTH, TH_DIR_EAST, TH_DIR_SOUTH, TH_DIR_WEST };

// Animaciones de caminar y de estar quieto de una clase de humanoide (la
// tabla walk_anims de humanoid.lua).
typedef struct th_walk_anims {
    const char *name;
    uint16_t walk_north, walk_east, idle_north, idle_east;
} th_walk_anims;

const th_walk_anims *th_walk_anims_find(const char *humanoid_class);

enum { TH_ACTION_NONE, TH_ACTION_IDLE, TH_ACTION_WALK };

typedef struct th_action {
    uint8_t kind;
    uint8_t x, y; // destino de la acción de caminar
    uint8_t must_happen;
} th_action;

// Lo que comparten las entidades: el mapa, el buscador de caminos, las
// animaciones y el contador con que se ordenan las entidades de una casilla.
typedef struct th_world {
    th_map *map;
    th_pathfinder *pf;
    const th_anims *anims;
    uint32_t attach_seq;
} th_world;

typedef struct th_humanoid {
    // Lo que se dibuja (animation de CorsixTH).
    int16_t tile_x, tile_y; // -1 = fuera del mapa
    int16_t px, py;         // desplazamiento desde la casilla
    int16_t sx, sy;         // velocidad: lo que suma cada tick
    uint16_t anim;
    uint8_t anim_flags; // TH_ANIM_*
    uint8_t drawing_layer;
    uint32_t frame;
    uint32_t attach_seq; // cuándo entró en su casilla (el último va primero)
    uint8_t layers[TH_ANIM_LAYERS];
    // Lógica.
    const th_walk_anims *walk;
    uint8_t last_dir;
    uint8_t timer_fn;
    uint16_t timer;
    th_action queue[TH_HUMANOID_QUEUE];
    uint8_t queue_len;
    // Camino de la acción de caminar en curso: casilla de partida y la
    // dirección de cada paso.
    uint8_t walk_interruptible;
    uint8_t path_truncated; // el camino no cabía: al acabarlo, se vuelve a buscar
    uint8_t cur_x, cur_y;   // casilla del camino en path_index
    uint16_t path_len, path_index;
    uint8_t path_dirs[TH_HUMANOID_PATH_MAX / 4];
} th_humanoid;

// World:newEntity("…", 2) y Humanoid:setType(clase): animación 2 y, como la
// cola está vacía, la acción de quedarse quieto.
void th_humanoid_init(th_world *w, th_humanoid *h, const th_walk_anims *walk);
void th_humanoid_set_tile(th_world *w, th_humanoid *h, int x, int y);
void th_humanoid_set_layer(th_humanoid *h, int layer, int id);
// Humanoid:setNextAction y queueAction (al final de la cola).
void th_humanoid_set_next_action(th_world *w, th_humanoid *h, th_action a);
void th_humanoid_queue_action(th_humanoid *h, th_action a);
// Entity:tick.
void th_humanoid_tick(th_world *w, th_humanoid *h);

static inline th_action th_action_walk(int x, int y)
{
    return (th_action){TH_ACTION_WALK, (uint8_t)x, (uint8_t)y, 0};
}

static inline th_action th_action_idle(void)
{
    return (th_action){TH_ACTION_IDLE, 0, 0, 0};
}

#endif
