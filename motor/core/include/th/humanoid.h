// Humanoides: la parte de Entity y Humanoid de CorsixTH (entity.lua,
// entities/humanoid.lua) con su cola de acciones (humanoid_actions/*.lua).
//
// La cola funciona como la de CorsixTH: la acción en curso es la primera;
// setNextAction deja las que tienen que acabar (must_happen), descarta las
// demás y marca las que quedan delante para interrumpirlas; queueAction en la
// posición 0 arranca la acción en el acto. Las funciones de Lua que cuelgan de
// una acción (on_interrupt, el temporizador, loop_callback...) son aquí
// números que se despachan con un switch, así que todo el estado cabe en la
// estructura y se puede guardar.
//
// Sin simulación (th_world.sim == NULL) solo hay caminar y quedarse quieto,
// sin puertas: es lo del hito 3. Con simulación (th/sim.h) están todas las
// acciones del bucle del hito 4: aparecer, buscar la recepción, hacer cola,
// usar un objeto, llamar a una puerta, deambular, buscar una sala y atender
// el mostrador.
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
#define TH_HUMANOID_QUEUE 12
#define TH_NONE 0xFF // índice nulo de humanoide, objeto o sala

enum { TH_DIR_NORTH, TH_DIR_EAST, TH_DIR_SOUTH, TH_DIR_WEST };

// Animaciones de una clase de humanoide: caminar y quieto (walk_anims de
// humanoid.lua), las de las puertas (door_animations) y, de los pacientes, las
// de vomitar, orinar, bostezar, dar golpecitos con el pie y mirar el reloj
// (vomit_anim, pee_anim...); 0 = no tiene.
typedef struct th_walk_anims {
    const char *name;
    uint16_t walk_north, walk_east, idle_north, idle_east;
    uint16_t door_leaving, door_entering, knock_north, knock_east, swing_leaving, swing_entering;
    uint16_t vomit, pee, yawn, tap_foot, check_watch;
} th_walk_anims;

const th_walk_anims *th_walk_anims_find(const char *humanoid_class);
const th_walk_anims *th_walk_anims_at(unsigned i); // NULL al acabar la tabla

enum {
    TH_ACTION_NONE,
    TH_ACTION_IDLE,
    TH_ACTION_WALK,
    TH_ACTION_SPAWN,
    TH_ACTION_SEEK_RECEPTION,
    TH_ACTION_QUEUE,
    TH_ACTION_USE_OBJECT,
    TH_ACTION_KNOCK_DOOR,
    TH_ACTION_MEANDER,
    TH_ACTION_SEEK_ROOM,
    TH_ACTION_STAFF_RECEPTION,
    TH_ACTION_CALL_CHECKPOINT, // obj: sala; val: identidad de la llamada
    TH_ACTION_ANSWER_CALL,
    // Una animación entera y se acaba (vomit.lua, pee.lua, yawn.lua,
    // tap_foot.lua, check_watch.lua); vomitar y orinar dejan basura.
    TH_ACTION_VOMIT,
    TH_ACTION_PEE,
    TH_ACTION_YAWN,
    TH_ACTION_TAP_FOOT,
    TH_ACTION_CHECK_WATCH,
};

// Banderas de una acción (flags): las que pone quien la crea.
enum {
    TH_AF_MUST_HAPPEN = 1u << 0,
    TH_AF_TODO_INTERRUPT = 1u << 1,
    TH_AF_TODO_HIGH = 1u << 2, // todo_interrupt == "high"
    TH_AF_UNINTERRUPTIBLE = 1u << 3,
    TH_AF_IS_LEAVING = 1u << 4,
    TH_AF_IS_ENTERING = 1u << 5,
    TH_AF_TRIMMED = 1u << 6,       // walk: acabar al cambiar de ruta
    TH_AF_TRUNCATE_HIGH = 1u << 7, // walk: solo se corta con prioridad alta
};

// Estado interno (state).
enum {
    TH_AS_DONE_INIT = 1u << 0,   // queue, seek_room
    TH_AS_DONE_WALK = 1u << 1,   // seek_room
    TH_AS_TREATMENT = 1u << 2,   // seek_room: busca una sala de tratamiento
    TH_AS_IN_QUEUE = 1u << 3,    // queue: is_in_queue
    TH_AS_STARTED = 1u << 4,     // walk: on_restart ya puesto
    TH_AS_SAVED_MH = 1u << 5,    // walk: saved_must_happen
    TH_AS_DONE_KNOCK = 1u << 6,  // walk
    TH_AS_PROLONGED = 1u << 7,   // use_object: prolonged_usage
    TH_AS_DO_WALK = 1u << 8,     // use_object: walk_in_to_use
    TH_AS_CAN_IDLE = 1u << 9,    // meander
    TH_AS_DIAG_ROOM = 1u << 10,  // seek_room: diagnosis_room puesto
    TH_AS_NOTIFY = 1u << 11,     // walk: on_next_tile_set pendiente (paso de puerta)
    TH_AS_NO_PROLONG = 1u << 12, // use_object: prolonged_usage == false
    TH_AS_MSG_SENT = 1u << 13,   // seek_room: message_sent (el fax ya se mandó)
    TH_AS_RESTARTED = 1u << 14,  // walk: saved_must_happen ~= nil (se ha vuelto a empezar)
};

typedef struct th_action {
    uint16_t serial; // identidad (en Lua, la propia tabla): la pone la cola al meterla
    uint8_t kind;
    uint8_t flags;     // TH_AF_*
    uint16_t state;    // TH_AS_*
    uint8_t interrupt; // manejador on_interrupt; 0 = ninguno
    uint8_t x, y;      // destino (walk) o posición de la cola (queue)
    uint8_t obj;       // objeto: el que se usa, el de la cola, la puerta a la que se llama, el mostrador
    uint8_t aux;       // walk: reserve_on_resume; queue: reserve_when_done; spawn: punto; seek_room: diagnosis_exists
    uint8_t dir;       // idle y knock_door: dirección (TH_NONE = la del último paso); spawn: 1 = desaparecer
    int8_t phase;      // use_object: fase (-6..6, 100 = fin); knock_door: estado
    uint8_t cb;        // use_object: loop_callback / after_use
    int16_t count;     // idle, meander: count (-1 = sin)
    int16_t val;       // use_object con el callback del GP: desk_use_time
    uint8_t face_x, face_y; // queue: casilla a la que mirar (TH_NONE = la de la cola)
    uint8_t bench_dist;     // queue: current_bench_distance (TH_NONE = de pie)
    uint8_t old_x, old_y, new_x, new_y; // use_object
    uint8_t room_type;      // seek_room: tipo de sala (índice en las definiciones)
    uint8_t diag_idx;       // seek_room: diagnosis_room (desde 0)
    uint8_t ntf_from, ntf_to; // walk: salas a avisar al cruzar la puerta
    uint8_t mirror;         // use_object: mirror_flags
} th_action;

struct th_sim;

// Lo que comparten las entidades: el mapa, el buscador de caminos, las
// animaciones, el contador con que se ordenan las entidades de una casilla y,
// si la hay, la simulación del hospital.
typedef struct th_world {
    th_map *map;
    th_pathfinder *pf;
    const th_anims *anims;
    uint32_t attach_seq;
    struct th_sim *sim;
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
    uint8_t slow_animation, skip_next_tick; // Entity: los cansados van a medio ritmo
    uint8_t speed;                          // 0 lento, 1 normal, 2 rápido
    uint8_t user_of;                        // objeto que usa (TH_NONE)
    uint8_t in_room;                        // sala (número) en la que está; 0 = ninguna
    th_action queue[TH_HUMANOID_QUEUE];
    uint8_t queue_len;
    uint16_t serial_seq;
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
// Humanoid:setNextAction (high = prioridad alta) y queueAction (pos < 0: al
// final; pos == 0 la arranca).
void th_humanoid_set_next_action_p(th_world *w, th_humanoid *h, th_action a, int high);
void th_humanoid_queue_action_at(th_world *w, th_humanoid *h, th_action a, int pos);
void th_humanoid_finish_action(th_world *w, th_humanoid *h);
// Patient:interruptAndRequeueAction(current, pos): vuelve a poner en la
// posición pos la acción en curso, rehecha (quieto, caminar o, si no, buscar
// sala), y la interrumpe.
void th_humanoid_interrupt_and_requeue(th_world *w, th_humanoid *h, int pos);
// Entity:tick.
void th_humanoid_tick(th_world *w, th_humanoid *h);
// Entity:setTimer: con slow_animation, el doble de ticks.
void th_humanoid_set_timer(th_humanoid *h, unsigned ticks, unsigned fn);
// La acción en curso (NULL si la cola está vacía).
static inline th_action *th_humanoid_action(th_humanoid *h)
{
    return h->queue_len ? &h->queue[0] : 0;
}

static inline void th_humanoid_set_next_action(th_world *w, th_humanoid *h, th_action a)
{
    th_humanoid_set_next_action_p(w, h, a, 0);
}

static inline void th_humanoid_queue_action(th_humanoid *h, th_action a)
{
    if (h->queue_len < TH_HUMANOID_QUEUE) {
        a.serial = ++h->serial_seq;
        h->queue[h->queue_len++] = a;
    }
}

static inline th_action th_action_new(int kind)
{
    th_action a = {0};
    a.kind = (uint8_t)kind;
    a.obj = a.aux = a.dir = a.face_x = a.face_y = a.bench_dist = TH_NONE;
    a.count = -1;
    return a;
}

static inline th_action th_action_walk(int x, int y)
{
    th_action a = th_action_new(TH_ACTION_WALK);
    a.x = (uint8_t)x;
    a.y = (uint8_t)y;
    return a;
}

static inline th_action th_action_idle(void)
{
    return th_action_new(TH_ACTION_IDLE);
}

#endif
