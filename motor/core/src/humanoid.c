#include "th/humanoid.h"

#include <string.h>

#include "th/sim.h"

// walk_anims de entities/humanoid.lua (CorsixTH): caminar al norte y al este,
// quieto mirando al norte y al este, y las puertas: salir y entrar, llamar
// (norte y este) y las puertas batientes. El sur y el oeste son los mismos en
// espejo. th_sim_init comprueba que coinciden con las reglas (caminar, puertas).
// Los pacientes llevan además vomit_anim, pee_anim, yawn_anim, tap_foot_anim y
// check_watch_anim (las mismas tablas de humanoid.lua).
static const th_walk_anims walk_table[] = {
    {"Standard Male Patient", 16, 18, 24, 26, 182, 184, 286, 288, 2040, 2042, 2056, 2244, 368, 2960, 2964},
    {"Gowned Male Patient", 406, 408, 414, 416, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Stripped Male Patient", 818, 820, 826, 828, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Stripped Male Patient 2", 818, 820, 826, 828, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Stripped Male Patient 3", 818, 820, 826, 828, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Alternate Male Patient", 2704, 2706, 2712, 2714, 2748, 2750, 2764, 2766, 0, 0, 4476, 4472, 0, 360, 364},
    {"Slack Male Patient", 1484, 1486, 1492, 1494, 1524, 1526, 2764, 1494, 0, 0, 4324, 4328, 0, 0, 4060},
    {"Slack Female Patient", 0, 2, 8, 10, 258, 260, 294, 296, 2864, 2866, 0, 4744, 0, 0, 0},
    {"Transparent Male Patient", 1064, 1066, 1072, 1074, 1104, 1106, 1120, 1074, 0, 0, 4384, 4848, 0, 0, 0},
    {"Standard Female Patient", 0, 2, 8, 10, 258, 260, 294, 296, 2864, 2866, 3184, 4744, 4864, 4464, 4468},
    {"Gowned Female Patient", 2876, 2878, 2884, 2886, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Stripped Female Patient", 834, 836, 842, 844, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Stripped Female Patient 2", 834, 836, 842, 844, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Stripped Female Patient 3", 834, 836, 842, 844, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Transparent Female Patient", 3012, 3014, 3020, 3022, 3052, 3054, 3068, 3070, 0, 0, 4452, 4852, 0, 0, 0},
    {"Chewbacca Patient", 858, 860, 866, 868, 3526, 3528, 4150, 4152, 0, 0, 4138, 4178, 0, 0, 0},
    {"Elvis Patient", 978, 980, 986, 988, 3634, 3636, 4868, 4870, 0, 0, 1034, 970, 0, 0, 0},
    {"Invisible Patient", 1642, 1644, 1840, 1842, 1796, 1798, 4192, 4194, 0, 0, 4204, 4208, 0, 0, 0},
    {"Alien Male Patient", 3598, 3600, 3606, 3608, 182, 184, 286, 288, 3626, 3628, 0, 0, 0, 0, 0},
    {"Alien Female Patient", 3598, 3600, 3606, 3608, 258, 260, 294, 296, 3626, 3628, 0, 0, 0, 0, 0},
    {"Doctor", 32, 34, 40, 42, 670, 672, 0, 0, 4750, 4752, 0, 0, 0, 0, 0},
    {"Surgeon", 2288, 2290, 2296, 2298, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Nurse", 1206, 1208, 1650, 1652, 3264, 3266, 0, 0, 3272, 3274, 0, 0, 0, 0, 0},
    {"Handyman", 1858, 1860, 1866, 1868, 3286, 3288, 0, 0, 3518, 3520, 0, 0, 0, 0, 0},
    {"Receptionist", 3668, 3670, 3676, 3678, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"VIP", 266, 268, 274, 276, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Inspector", 266, 268, 274, 276, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"Grim Reaper", 994, 996, 1002, 1004, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

// Funciones del temporizador (las que Lua pasa a setTimer).
enum {
    TIMER_NONE,
    TIMER_WALK,           // action_walk_tick
    TIMER_FINISH,         // Humanoid:finishAction
    TIMER_WALK_DOOR,      // action_walk_tick_door
    TIMER_IDLE,           // action_idle_timer
    TIMER_USE,            // action_use_object_tick
    TIMER_KNOCK,          // action_knock_door_tick
    TIMER_DESPAWN,        // action_spawn_despawn
    TIMER_RECEPTION_IDLE, // action_staff_reception_idle_phase
    TIMER_RECEPTION_LEFT, // final del paso al dejar el mostrador
    TIMER_NEED_END,       // action_vomit_end, action_pee_end, action_yawn_end...
};

// Manejadores on_interrupt.
enum {
    INT_NONE,
    INT_IDLE,
    INT_WALK,
    INT_QUEUE,
    INT_USE,
    INT_SEEK_ROOM,
    INT_RECEPTION,
    INT_RECEPTION_EARLY,
};

// Callbacks de use_object.
enum { CB_NONE, CB_GP_DESK };

// Pasos de walk.lua a velocidad normal: 8 ticks por casilla, 4 y 2 píxeles por tick.
#define WALK_TICKS 8
#define WALK_DX 4
#define WALK_DY 2

const th_walk_anims *th_walk_anims_find(const char *humanoid_class)
{
    for (unsigned i = 0; i < sizeof walk_table / sizeof walk_table[0]; i++)
        if (strcmp(walk_table[i].name, humanoid_class) == 0)
            return &walk_table[i];
    return NULL;
}

const th_walk_anims *th_walk_anims_at(unsigned i)
{
    return i < sizeof walk_table / sizeof walk_table[0] ? &walk_table[i] : NULL;
}

// Entity:setAnimation: solo cambia (y vuelve al primer fotograma) si cambian
// la animación o las banderas.
static void set_animation(th_world *w, th_humanoid *h, unsigned anim, unsigned flags)
{
    if (anim == h->anim && flags == h->anim_flags)
        return;
    h->anim = (uint16_t)anim;
    h->anim_flags = (uint8_t)flags;
    h->frame = th_anims_first_frame(w->anims, anim);
}

void th_humanoid_set_tile(th_world *w, th_humanoid *h, int x, int y)
{
    // Patient:setTile: la basura que tiran por el camino.
    if (w->sim && th_sim_person(w->sim, h)->kind == TH_HUM_PATIENT)
        th_sim_patient_set_tile(w->sim, th_sim_index(w->sim, h), x, y);
    h->tile_x = (int16_t)x;
    h->tile_y = (int16_t)y;
    h->attach_seq = ++w->attach_seq;
}

static void set_tile_position_speed(th_world *w, th_humanoid *h, int x, int y, int px, int py, int sx, int sy)
{
    th_humanoid_set_tile(w, h, x, y);
    h->px = (int16_t)px;
    h->py = (int16_t)py;
    h->sx = (int16_t)sx;
    h->sy = (int16_t)sy;
}

void th_humanoid_set_timer(th_humanoid *h, unsigned ticks, unsigned fn)
{
    h->timer = (uint16_t)ticks;
    h->timer_fn = (uint8_t)fn;
    if (h->slow_animation && ticks) {
        h->skip_next_tick = 1;
        h->timer = (uint16_t)(ticks * 2);
    }
}

static th_person *person(const th_world *w, th_humanoid *h)
{
    return th_sim_person(w->sim, h);
}

static int who(const th_world *w, const th_humanoid *h)
{
    return th_sim_index(w->sim, h);
}

static int is_patient(const th_world *w, th_humanoid *h)
{
    return w->sim && person(w, h)->kind == TH_HUM_PATIENT;
}

// Humanoid:getRoom: la sala en la que ha entrado o la de su casilla.
static th_room *get_room(th_world *w, th_humanoid *h)
{
    if (!w->sim)
        return NULL;
    if (h->in_room)
        return th_sim_room(w->sim, h->in_room);
    return h->tile_x < 0 ? NULL : th_sim_room_at(w->sim, h->tile_x, h->tile_y);
}

static th_action *find_serial(th_humanoid *h, uint16_t serial, int *idx)
{
    for (int i = 0; i < h->queue_len; i++)
        if (h->queue[i].serial == serial) {
            if (idx)
                *idx = i;
            return &h->queue[i];
        }
    return NULL;
}

static void start_action(th_world *w, th_humanoid *h);
static void call_interrupt(th_world *w, th_humanoid *h, th_action *a, int high);

void th_humanoid_finish_action(th_world *w, th_humanoid *h)
{
    if (h->queue_len) {
        memmove(&h->queue[0], &h->queue[1], (h->queue_len - 1u) * sizeof(th_action));
        h->queue_len--;
    }
    start_action(w, h);
}

void th_humanoid_queue_action_at(th_world *w, th_humanoid *h, th_action a, int pos)
{
    if (pos < 0 || pos >= h->queue_len) {
        th_humanoid_queue_action(h, a);
        if (pos == 0)
            start_action(w, h);
        return;
    }
    if (h->queue_len >= TH_HUMANOID_QUEUE)
        h->queue_len--; // no debería pasar: se pierde la última
    a.serial = ++h->serial_seq;
    memmove(&h->queue[pos + 1], &h->queue[pos], (unsigned)(h->queue_len - pos) * sizeof(th_action));
    h->queue[pos] = a;
    h->queue_len++;
    if (pos == 0)
        start_action(w, h);
}

// Lo que setNextAction hace con una acción que descarta.
static void on_removed(th_world *w, th_humanoid *h, const th_action *a)
{
    th_sim *s = w->sim;
    if (!s)
        return;
    if ((a->kind == TH_ACTION_USE_OBJECT || a->kind == TH_ACTION_STAFF_RECEPTION) && a->obj != TH_NONE &&
        s->obj[a->obj].reserved_for == who(w, h))
        s->obj[a->obj].reserved_for = TH_NONE;
    if (a->kind == TH_ACTION_CALL_CHECKPOINT)
        th_sim_call_checkpoint_removed(s, who(w, h), a->obj, a->val);
    if (a->kind == TH_ACTION_WALK && (a->flags & TH_AF_IS_ENTERING)) {
        th_room *r = th_sim_room_at(s, a->x, a->y);
        th_sim_unexpect_from_room(s, who(w, h), r);
        if (r && a->aux != TH_NONE && s->obj[a->aux].reserved_for == who(w, h)) {
            s->obj[a->aux].reserved_for = TH_NONE;
            th_sim_room_try_advance_queue(s, r);
        }
    }
}

void th_humanoid_set_next_action_p(th_world *w, th_humanoid *h, th_action a, int high)
{
    // Las acciones que tienen que acabar se quedan; el resto se descarta y la
    // nueva va detrás.
    int i = 0;
    while (i < h->queue_len && (h->queue[i].flags & TH_AF_MUST_HAPPEN))
        i++;
    const int interrupted = i > 0;
    while (h->queue_len > i) {
        th_action removed = h->queue[--h->queue_len];
        on_removed(w, h, &removed);
    }
    if (i >= TH_HUMANOID_QUEUE)
        i = TH_HUMANOID_QUEUE - 1;
    a.serial = ++h->serial_seq;
    h->queue[i] = a;
    h->queue_len = (uint8_t)(i + 1);
    if (!interrupted) {
        start_action(w, h);
        return;
    }
    for (int j = 0; j < i; j++)
        h->queue[j].flags |= TH_AF_TODO_INTERRUPT | (high ? TH_AF_TODO_HIGH : 0);
    th_action *cur = &h->queue[0];
    if (!(cur->flags & TH_AF_UNINTERRUPTIBLE) && cur->interrupt) {
        // Hito 3 (sin simulación): action_walk_interrupt corta el camino tras
        // la casilla a la que va.
        call_interrupt(w, h, cur, high);
    }
}

// ---------------------------------------------------------------------------
// Quieto (idle.lua)

static void idle_anim(th_world *w, th_humanoid *h, int dir)
{
    const th_walk_anims *a = h->walk;
    switch (dir) {
    case TH_DIR_NORTH: set_animation(w, h, a->idle_north, 0); break;
    case TH_DIR_EAST: set_animation(w, h, a->idle_east, 0); break;
    case TH_DIR_SOUTH: set_animation(w, h, a->idle_east, TH_ANIM_FLIP_H); break;
    default: set_animation(w, h, a->idle_north, TH_ANIM_FLIP_H); break;
    }
}

static void idle_start(th_world *w, th_humanoid *h, th_action *a)
{
    idle_anim(w, h, a->dir != TH_NONE ? a->dir : h->last_dir);
    // humanoid.th:setTile(map, x, y, layer): vuelve a entrar en su casilla.
    if (h->tile_x >= 0)
        h->attach_seq = ++w->attach_seq;
    h->sx = h->sy = 0;
    if (a->count >= 0) {
        th_humanoid_set_timer(h, (unsigned)a->count, TIMER_IDLE);
        a->flags |= TH_AF_MUST_HAPPEN;
    }
    if ((a->flags & TH_AF_MUST_HAPPEN) && !a->interrupt)
        a->interrupt = INT_IDLE;
}

// ---------------------------------------------------------------------------
// Caminar (walk.lua)

static int step_dir(const th_humanoid *h, unsigned i)
{
    return (h->path_dirs[i >> 2] >> ((i & 3) * 2)) & 3;
}

static const int8_t step_dx[4] = {0, 1, 0, -1}, step_dy[4] = {-1, 0, 1, 0};

static uint32_t flags_at(const th_world *w, int x, int y)
{
    return th_map_tile_c(w->map, x, y)->flags;
}

// Casilla del camino en el paso i (>= path_index).
static void path_tile(const th_humanoid *h, unsigned i, int *x, int *y)
{
    int cx = h->cur_x, cy = h->cur_y;
    for (unsigned k = h->path_index; k < i; k++) {
        const int d = step_dir(h, k);
        cx += step_dx[d];
        cy += step_dy[d];
    }
    *x = cx;
    *y = cy;
}

static int navigate_door(th_world *w, th_humanoid *h, int x1, int y1, int dir);

// action_walk_raw: devuelve 1 si ha dejado pendiente un aviso de paso de
// puerta (on_next_tile_set).
static int walk_raw(th_world *w, th_humanoid *h, int x1, int y1, int x2, int y2, int doors, unsigned timer_fn)
{
    const th_walk_anims *a = h->walk;
    int factor = 1, quantity = WALK_TICKS;
    if (h->speed == 2) {
        factor = 2;
        quantity = 4;
    }
    const int dx = WALK_DX * factor, dy = WALK_DY * factor;
    if (x1 != x2) {
        if (x1 < x2) {
            if (doors && (flags_at(w, x2, y2) & TH_TILE_DOOR_W))
                return navigate_door(w, h, x1, y1, TH_DIR_EAST);
            h->last_dir = TH_DIR_EAST;
            set_animation(w, h, a->walk_east, 0);
            set_tile_position_speed(w, h, x2, y2, -32, -16, dx, dy);
        } else {
            if (doors && (flags_at(w, x1, y1) & TH_TILE_DOOR_W))
                return navigate_door(w, h, x1, y1, TH_DIR_WEST);
            h->last_dir = TH_DIR_WEST;
            set_animation(w, h, a->walk_north, TH_ANIM_FLIP_H);
            set_tile_position_speed(w, h, x1, y1, 0, 0, -dx, -dy);
        }
    } else if (y1 < y2) {
        if (doors && (flags_at(w, x2, y2) & TH_TILE_DOOR_N))
            return navigate_door(w, h, x1, y1, TH_DIR_SOUTH);
        h->last_dir = TH_DIR_SOUTH;
        set_animation(w, h, a->walk_east, TH_ANIM_FLIP_H);
        set_tile_position_speed(w, h, x2, y2, 32, -16, -dx, dy);
    } else {
        if (doors && (flags_at(w, x1, y1) & TH_TILE_DOOR_N))
            return navigate_door(w, h, x1, y1, TH_DIR_NORTH);
        h->last_dir = TH_DIR_NORTH;
        set_animation(w, h, a->walk_north, 0);
        set_tile_position_speed(w, h, x1, y1, 0, 0, dx, -dy);
    }
    th_humanoid_set_timer(h, (unsigned)quantity, timer_fn);
    return 0;
}

// El on_next_tile_set de navigateDoor: avisa a las salas a ambos lados de la
// puerta, ya cruzada.
static void notify_rooms(th_world *w, th_humanoid *h, int from, int to)
{
    th_sim *s = w->sim;
    th_room *r = from ? th_sim_room(s, from) : NULL;
    if (r)
        th_sim_room_on_leave(s, r, who(w, h));
    r = to ? th_sim_room(s, to) : NULL;
    if (r)
        th_sim_room_on_enter(s, r, who(w, h));
}

// Llama al aviso pendiente de la acción (si lo hay) y lo borra.
static void fire_notify(th_world *w, th_humanoid *h, th_action *a)
{
    if (!(a->state & TH_AS_NOTIFY))
        return;
    a->state &= (uint16_t)~TH_AS_NOTIFY;
    notify_rooms(w, h, a->ntf_from, a->ntf_to);
}

static void walk_start(th_world *w, th_humanoid *h);

static int room_num_at(const th_world *w, int x, int y)
{
    return th_map_tile_c(w->map, x, y)->room;
}

// action_walk_tick: arranca el paso siguiente o termina la acción.
static void walk_tick(th_world *w, th_humanoid *h)
{
    th_action *a = &h->queue[0];
    const uint16_t serial = a->serial;
    const int x1 = h->cur_x, y1 = h->cur_y;
    if (h->path_index >= h->path_len) {
        set_tile_position_speed(w, h, x1, y1, 0, 0, 0, 0);
        if (h->path_truncated) {
            walk_start(w, h);
            return;
        }
        if (a->state & TH_AS_NOTIFY) {
            fire_notify(w, h, a);
            a = find_serial(h, serial, NULL);
            if (!a)
                return;
        }
        th_humanoid_finish_action(w, h);
        return;
    }
    const int d = step_dir(h, h->path_index);
    const int x2 = x1 + step_dx[d], y2 = y1 + step_dy[d];
    // Si la casilla siguiente ha dejado de ser transitable (o una sala ha
    // aparecido en el camino), se busca otro camino.
    const uint32_t here = flags_at(w, x1, y1), there = flags_at(w, x2, y2);
    int recalc = !(there & TH_TILE_PASSABLE) && (here & TH_TILE_PASSABLE);
    if (!recalc && w->sim && room_num_at(w, x1, y1) != room_num_at(w, x2, y2)) {
        recalc = (there & TH_TILE_ROOM) != 0;
        if (recalc && (th_sim_object_at(w->sim, x1, y1, TH_OBJ_DOOR) != TH_NONE ||
                       th_sim_object_at(w->sim, x2, y2, TH_OBJ_DOOR) != TH_NONE)) {
            int ex, ey;
            path_tile(h, h->path_len, &ex, &ey);
            if (room_num_at(w, ex, ey) == room_num_at(w, x2, y2))
                recalc = (a->flags & TH_AF_TRIMMED) != 0;
        }
    }
    if (recalc && (here & TH_TILE_PASSABLE)) {
        set_tile_position_speed(w, h, x1, y1, 0, 0, 0, 0);
        if (a->state & TH_AS_NOTIFY) {
            fire_notify(w, h, a);
            a = find_serial(h, serial, NULL);
            if (!a)
                return;
        }
        if (a->flags & TH_AF_TRIMMED) {
            th_humanoid_finish_action(w, h);
            return;
        }
        walk_start(w, h);
        return;
    }
    // El aviso pendiente es el del paso anterior: se llama después de este paso.
    const int had = (a->state & TH_AS_NOTIFY) != 0;
    const uint8_t of = a->ntf_from, ot = a->ntf_to;
    const uint16_t idx_before = h->path_index;
    const int fresh = walk_raw(w, h, x1, y1, x2, y2, w->sim != NULL, TIMER_WALK);
    a = find_serial(h, serial, NULL);
    if (a && h->queue[0].serial == serial && h->path_index == idx_before) {
        // action.path_index = path_index + 1 (también si la puerta ha mandado hacer cola)
        h->path_index++;
        h->cur_x = (uint8_t)x2;
        h->cur_y = (uint8_t)y2;
    }
    if (had) {
        if (a && !fresh)
            a->state &= (uint16_t)~TH_AS_NOTIFY;
        notify_rooms(w, h, of, ot);
    }
}

// Guarda el camino que acaba de encontrar el A*: de la meta hacia atrás por «prev».
static void store_path(th_world *w, th_humanoid *h, int len)
{
    h->path_truncated = len > TH_HUMANOID_PATH_MAX;
    const int kept = h->path_truncated ? TH_HUMANOID_PATH_MAX : len;
    memset(h->path_dirs, 0, sizeof h->path_dirs);
    for (unsigned n = w->pf->dest; w->pf->nodes[n].prev != TH_PATH_NONE; n = w->pf->nodes[n].prev) {
        const unsigned p = w->pf->nodes[n].prev, i = w->pf->nodes[p].distance;
        if (i >= (unsigned)kept)
            continue;
        int d = n == p + 1 ? TH_DIR_EAST : n + 1 == p ? TH_DIR_WEST : n > p ? TH_DIR_SOUTH : TH_DIR_NORTH;
        h->path_dirs[i >> 2] |= (uint8_t)(d << ((i & 3) * 2));
    }
    h->path_len = (uint16_t)kept;
    h->path_index = 0;
    h->cur_x = (uint8_t)h->tile_x;
    h->cur_y = (uint8_t)h->tile_y;
}

// action_walk_start (también es su on_restart).
static void walk_start(th_world *w, th_humanoid *h)
{
    th_action *a = &h->queue[0];
    int len = h->tile_x < 0 ? -1 : th_path_find(w->pf, w->map, h->tile_x, h->tile_y, a->x, a->y);
    if (len <= 0) {
        // Sin camino, o ya está allí: quieto un tick (con la acción de quieto
        // de CorsixTH, que mira la dirección y must_happen de esta) y se acaba.
        idle_anim(w, h, h->last_dir);
        if (h->tile_x >= 0)
            h->attach_seq = ++w->attach_seq;
        h->sx = h->sy = 0;
        if ((a->flags & TH_AF_MUST_HAPPEN) && !a->interrupt)
            a->interrupt = INT_IDLE;
        th_humanoid_set_timer(h, 1, TIMER_FINISH);
        return;
    }
    store_path(w, h, len);
    h->walk_interruptible = 1;
    a->interrupt = INT_WALK;
    if (a->state & TH_AS_STARTED) {
        if (a->flags & TH_AF_MUST_HAPPEN)
            a->state |= TH_AS_SAVED_MH;
        else
            a->state &= (uint16_t)~TH_AS_SAVED_MH;
        a->state |= TH_AS_RESTARTED;
    }
    a->state |= TH_AS_STARTED;
    a->flags |= TH_AF_MUST_HAPPEN;
    if (w->sim && a->aux != TH_NONE && !(a->flags & TH_AF_TODO_INTERRUPT))
        w->sim->obj[a->aux].reserved_for = (uint8_t)who(w, h);
    walk_tick(w, h);
}

// action_walk_interrupt
static void walk_interrupt(th_world *w, th_humanoid *h, th_action *a, int high)
{
    if ((a->flags & TH_AF_TRUNCATE_HIGH) && !high) {
        a->interrupt = INT_WALK;
        return;
    }
    // El camino se corta tras la casilla a la que va.
    h->walk_interruptible = 0;
    h->path_len = h->path_index;
    h->path_truncated = 0;
    th_sim *s = w->sim;
    if (s) {
        if (a->aux != TH_NONE && s->obj[a->aux].reserved_for == who(w, h)) {
            s->obj[a->aux].reserved_for = TH_NONE;
            th_sim_room_try_advance_queue(s, th_sim_room(s, s->obj[a->aux].room));
        }
        th_sim_unexpect_from_room(s, who(w, h), th_sim_room_at(s, a->x, a->y));
    }
    if (high) {
        const unsigned fn = h->timer_fn;
        h->timer = 0;
        h->timer_fn = TIMER_NONE;
        if (fn == TIMER_WALK)
            walk_tick(w, h);
        else if (fn == TIMER_WALK_DOOR) {
            if (s && h->user_of != TH_NONE)
                th_sim_object_remove_user(s, h->user_of, who(w, h));
            h->user_of = TH_NONE;
            walk_tick(w, h);
        } else if (fn == TIMER_FINISH)
            th_humanoid_finish_action(w, h);
    }
}

// navigateDoor
static int navigate_door(th_world *w, th_humanoid *h, int x1, int y1, int dir)
{
    th_sim *s = w->sim;
    const int me = who(w, h);
    th_action *a = &h->queue[0];
    const uint16_t serial = a->serial;
    int dx = x1, dy = y1;
    if (dir == TH_DIR_EAST)
        dx++;
    else if (dir == TH_DIR_SOUTH)
        dy++;
    const int door = th_sim_object_at(s, dx, dy, TH_OBJ_DOOR);
    if (door == TH_NONE)
        return 0; // puerta batiente: no las hay en el hito 4
    th_object *dob = &s->obj[door];
    th_sim_queue_unexpect(s, dob->queue, me);
    th_room *room = th_sim_room(s, dob->room);
    const int entering = room && get_room(w, h) != room;
    if ((dob->user != TH_NONE) || (dob->reserved_for != TH_NONE && dob->reserved_for != me) ||
        (entering && !th_sim_room_can_enter(s, room, me))) {
        if (dob->reserved_for == me) {
            dob->reserved_for = TH_NONE;
            th_sim_room_try_advance_queue(s, room);
        }
        set_tile_position_speed(w, h, x1, y1, 0, 0, 0, 0);
        const th_queue *q = &s->queues[dob->queue];
        int pos = 0;
        if (entering && q->size == 0 && !th_sim_room_patient_count(s, room) && dob->user == TH_NONE &&
            dob->reserved_for == TH_NONE && is_patient(w, h) && th_sim_room_requires_staff(s, room)) {
            th_action k = th_action_new(TH_ACTION_KNOCK_DOOR);
            k.obj = (uint8_t)door;
            k.dir = (uint8_t)dir;
            th_humanoid_queue_action_at(w, h, k, 0);
            pos = 1;
        }
        th_action qa = th_action_new(TH_ACTION_QUEUE);
        qa.x = (uint8_t)x1;
        qa.y = (uint8_t)y1;
        qa.obj = (uint8_t)door;
        qa.aux = (uint8_t)door;
        if (!entering)
            qa.flags |= TH_AF_IS_LEAVING;
        th_humanoid_queue_action_at(w, h, qa, pos);
        a = find_serial(h, serial, NULL);
        if (a) {
            if (a->state & TH_AS_SAVED_MH)
                a->flags |= TH_AF_MUST_HAPPEN;
            else
                a->flags &= (uint8_t)~TH_AF_MUST_HAPPEN;
            a->aux = (uint8_t)door;
        }
        return 0;
    }
    if (a->aux != TH_NONE) {
        a->aux = TH_NONE;
    } else if (entering && !(a->state & TH_AS_DONE_KNOCK) && is_patient(w, h) &&
               th_sim_room_requires_staff(s, room)) {
        set_tile_position_speed(w, h, x1, y1, 0, 0, 0, 0);
        a->aux = (uint8_t)door;
        a->state |= TH_AS_DONE_KNOCK;
        th_action k = th_action_new(TH_ACTION_KNOCK_DOOR);
        k.obj = (uint8_t)door;
        k.dir = (uint8_t)dir;
        th_humanoid_queue_action_at(w, h, k, 0);
        return 0;
    }
    const th_walk_anims *an = h->walk;
    set_tile_position_speed(w, h, dx, dy, 0, 0, 0, 0);
    h->user_of = (uint8_t)door;
    th_sim_object_set_user(s, door, me);
    unsigned anim, flags = 0;
    int to_x = dx, to_y = dy;
    if (dir == TH_DIR_NORTH || dir == TH_DIR_WEST) {
        anim = an->door_leaving;
        if (dir == TH_DIR_WEST) {
            flags = TH_ANIM_FLIP_H;
            to_x--;
        } else {
            to_y--;
        }
    } else {
        anim = an->door_entering;
        if (dir == TH_DIR_SOUTH)
            flags = TH_ANIM_FLIP_H;
    }
    set_animation(w, h, anim, flags);
    const int duration = th_sim_anim_length(s, anim);
    h->last_dir = (uint8_t)dir;
    a->state |= TH_AS_NOTIFY;
    a->ntf_from = (uint8_t)room_num_at(w, x1, y1);
    a->ntf_to = (uint8_t)room_num_at(w, to_x, to_y);
    if (entering)
        h->in_room = room->num;
    h->path_index++;
    h->cur_x = (uint8_t)(x1 + step_dx[dir]);
    h->cur_y = (uint8_t)(y1 + step_dy[dir]);
    th_humanoid_set_timer(h, (unsigned)duration, TIMER_WALK_DOOR);
    return 1;
}

// ---------------------------------------------------------------------------
// Aparecer y desaparecer (spawn.lua)

static void spawn_start(th_world *w, th_humanoid *h, th_action *a)
{
    th_sim *s = w->sim;
    const int x = s->spawn[a->aux].x, y = s->spawn[a->aux].y;
    const int despawn = a->dir == 1;
    if (despawn && (h->tile_x != x || h->tile_y != y)) {
        th_action wk = th_action_walk(x, y);
        wk.flags |= a->flags & TH_AF_MUST_HAPPEN;
        th_humanoid_queue_action_at(w, h, wk, 0);
        return;
    }
    a->flags |= TH_AF_MUST_HAPPEN;
    int d = s->spawn[a->aux].dir;
    if (!despawn)
        d = (d + 2) & 3;
    const th_walk_anims *an = h->walk;
    unsigned anim, flag = 0;
    int sx, sy;
    const int duration = 10 * 2;
    switch (d) {
    case TH_DIR_EAST: anim = an->walk_east, sx = 4, sy = 2; break;
    case TH_DIR_WEST: anim = an->walk_north, flag = TH_ANIM_FLIP_H, sx = -4, sy = -2; break;
    case TH_DIR_SOUTH: anim = an->walk_east, flag = TH_ANIM_FLIP_H, sx = -4, sy = 2; break;
    default: anim = an->walk_north, sx = 4, sy = -2; break;
    }
    h->last_dir = (uint8_t)d;
    set_animation(w, h, anim, flag);
    int px = 0, py = 0;
    if (!despawn) {
        px = -sx * duration;
        py = -sy * duration;
        th_humanoid_set_timer(h, duration, TIMER_FINISH);
    } else {
        th_humanoid_set_timer(h, duration, TIMER_DESPAWN);
    }
    set_tile_position_speed(w, h, x, y, px, py, sx, sy);
}

// ---------------------------------------------------------------------------
// Buscar la recepción (seek_reception.lua)

#define CAN_JOIN_QUEUE_DISTANCE 6

static int can_join_queue_at(th_world *w, int x, int y, int dist)
{
    const th_tile *t = th_map_tile_c(w->map, x, y);
    return (t->flags & TH_TILE_HOSPITAL) && !(t->flags & TH_TILE_ROOM) && dist <= CAN_JOIN_QUEUE_DISTANCE &&
           th_map_parcel_owner(w->map, t->parcel) == 1;
}

static void seek_reception_start(th_world *w, th_humanoid *h, th_action *a)
{
    th_sim *s = w->sim;
    int best = TH_NONE, score = 0, best_dist = 0, bx = 0, by = 0;
    for (int i = 0; i < s->n_desks; i++) {
        const int d = s->desks[i];
        const th_object *o = &s->obj[d];
        if (o->receptionist == TH_NONE && o->reserved_for == TH_NONE)
            continue;
        int x, y;
        th_sim_use_tile(s, d, 0, &x, &y);
        const int dist = th_path_find(w->pf, w->map, h->tile_x, h->tile_y, x, y);
        if (dist < 0)
            continue;
        const int sc = dist + th_sim_desk_usage_score(s, d);
        if (best == TH_NONE || sc < score) {
            score = sc;
            best = d;
            best_dist = dist;
            bx = x;
            by = y;
        }
    }
    th_person *p = person(w, h);
    if (best == TH_NONE) {
        // Sin mostrador: deambular hasta que haya uno (en el hito 4 no pasa).
        th_action m = th_action_new(TH_ACTION_MEANDER);
        m.count = 1;
        m.flags |= a->flags & TH_AF_MUST_HAPPEN;
        if (p->u.p.waiting < 0)
            p->u.p.waiting = 5;
        th_humanoid_queue_action_at(w, h, m, 0);
        return;
    }
    p->u.p.waiting = -1;
    if (can_join_queue_at(w, h->tile_x, h->tile_y, best_dist)) {
        th_action q = th_action_new(TH_ACTION_QUEUE);
        q.x = (uint8_t)bx;
        q.y = (uint8_t)by;
        q.obj = (uint8_t)best;
        q.flags |= a->flags & TH_AF_MUST_HAPPEN;
        int fx, fy;
        th_sim_use_tile(s, best, 1, &fx, &fy);
        q.face_x = (uint8_t)fx;
        q.face_y = (uint8_t)fy;
        th_humanoid_set_next_action(w, h, q);
        return;
    }
    th_action wk = th_action_walk(bx, by);
    wk.flags |= TH_AF_TRIMMED | (a->flags & TH_AF_MUST_HAPPEN);
    th_humanoid_queue_action_at(w, h, wk, 0);
    th_action *wa = &h->queue[0];
    if (wa->kind != TH_ACTION_WALK || wa->interrupt != INT_WALK)
        return; // sin camino
    // trimQueuingTail: el final del camino se recorta mientras se pueda hacer
    // cola desde la casilla.
    const unsigned len = h->path_len;
    unsigned keep = len;
    for (unsigned j = len - 1; j >= 1 && j >= h->path_index; j--) {
        int x, y;
        path_tile(h, j, &x, &y);
        if (!can_join_queue_at(w, x, y, (int)(len - j)))
            break;
        keep = j;
    }
    h->path_len = (uint16_t)keep;
    int ex, ey;
    path_tile(h, keep, &ex, &ey);
    wa->x = (uint8_t)ex;
    wa->y = (uint8_t)ey;
}

// ---------------------------------------------------------------------------
// Hacer cola (queue.lua)

// interrupt_head: n es el índice (desde 0) de la acción hasta la que se
// interrumpe; los usos de objetos de por medio se quitan.
static void interrupt_head(th_world *w, th_humanoid *h, int n)
{
    while (n > 0) {
        th_action *a = &h->queue[n];
        if (a->kind == TH_ACTION_USE_OBJECT) {
            if (a->obj != TH_NONE && w->sim->obj[a->obj].reserved_for == who(w, h))
                w->sim->obj[a->obj].reserved_for = TH_NONE;
            memmove(&h->queue[n], &h->queue[n + 1], (unsigned)(h->queue_len - n - 1) * sizeof(th_action));
            h->queue_len--;
        } else {
            a->flags |= TH_AF_TODO_INTERRUPT;
        }
        n--;
    }
    th_action *head = &h->queue[0];
    if (head->interrupt)
        call_interrupt(w, h, head, 0);
}

// action_queue_find_idle: índice de la acción de quieto justo antes de la cola.
static int find_idle_before(th_humanoid *h, uint16_t qserial)
{
    for (int i = 0; i + 1 < h->queue_len; i++)
        if (h->queue[i].kind == TH_ACTION_IDLE && h->queue[i + 1].serial == qserial)
            return i;
    return -1;
}

// Las dos devuelven cuántas acciones quedan antes de la de cola.
static int finish_standing(th_world *w, th_humanoid *h, uint16_t qserial)
{
    const int i = find_idle_before(h, qserial);
    if (i < 0)
        return -1;
    interrupt_head(w, h, i);
    int idx;
    return find_serial(h, qserial, &idx) ? idx : -1;
}

static int leave_bench(th_world *w, th_humanoid *h, uint16_t qserial)
{
    for (int i = 0; i + 1 < h->queue_len; i++) {
        if (h->queue[i].serial == qserial)
            return finish_standing(w, h, qserial);
        if (h->queue[i].kind == TH_ACTION_USE_OBJECT && h->queue[i + 1].serial == qserial) {
            interrupt_head(w, h, i);
            break;
        }
    }
    int idx;
    return find_serial(h, qserial, &idx) ? idx : -1;
}

static int dir_towards(int x, int y, int fx, int fy)
{
    if (fy < y)
        return TH_DIR_NORTH;
    if (fy > y)
        return TH_DIR_SOUTH;
    if (fx > x)
        return TH_DIR_EAST;
    if (fx < x)
        return TH_DIR_WEST;
    return TH_NONE;
}

static int is_leaving(const th_humanoid *h)
{
    return h->queue_len && (h->queue[0].flags & TH_AF_IS_LEAVING);
}

// action_queue_on_change_position
void th_sim_on_change_queue_position(th_sim *s, int whom, th_action *qa)
{
    th_world *w = &s->w;
    th_humanoid *h = &s->hum[whom].h;
    if (!(qa->state & TH_AS_IN_QUEUE))
        return;
    const uint16_t qserial = qa->serial;
    th_person *p = &s->hum[whom];
    const th_queue *q = &s->queues[s->obj[qa->obj].queue];
    int must_stand = p->kind != TH_HUM_PATIENT || (qa->flags & TH_AF_IS_LEAVING) ||
                     s->dis[p->u.p.disease].must_stand;
    if (!must_stand)
        for (int i = 1; i <= q->bench_threshold; i++) {
            const int k = q->size - q->reported + i - 1;
            if (k >= 0 && k < q->size && q->member[k] == whom) {
                must_stand = 1;
                break;
            }
        }
    if (!must_stand) {
        if (qa->bench_dist != TH_NONE && qa->bench_dist <= 4)
            return;
        const int dist = th_path_find(w->pf, w->map, h->tile_x, h->tile_y, qa->x, qa->y);
        if (dist < 0)
            return;
        const int standing = qa->bench_dist == TH_NONE;
        // max(2 · distancia / 3, 4) o 10, redondeado hacia arriba en getFreeBench.
        int look = standing ? 10 : (2 * dist + 2) / 3;
        if (!standing && look < 4)
            look = 4;
        int bx, by, nd;
        const int bench = th_sim_free_bench(s, qa->x, qa->y, look, &bx, &by, &nd);
        if (bench != TH_NONE) {
            const int prior = standing ? finish_standing(w, h, qserial) : leave_bench(w, h, qserial);
            if (prior < 0)
                return;
            qa = find_serial(h, qserial, NULL);
            qa->bench_dist = (uint8_t)nd;
            th_action wk = th_action_walk(bx, by);
            wk.flags |= TH_AF_MUST_HAPPEN;
            th_action use = th_action_new(TH_ACTION_USE_OBJECT);
            use.obj = (uint8_t)bench;
            use.flags |= TH_AF_MUST_HAPPEN;
            th_humanoid_queue_action_at(w, h, wk, prior);
            th_humanoid_queue_action_at(w, h, use, prior + 1);
            s->obj[bench].reserved_for = (uint8_t)whom;
            return;
        }
        if (!standing)
            return;
    }
    // De pie, en su sitio de la cola.
    int standing_index = 0;
    th_room *our_room = get_room(w, h);
    for (int i = 0; i < q->size && q->member[i] != whom; i++) {
        th_humanoid *o = &s->hum[q->member[i]].h;
        const th_action *oq = th_sim_queue_action_of(s, q->member[i], s->obj[qa->obj].queue);
        if (oq && oq->bench_dist == TH_NONE && get_room(w, o) == our_room)
            standing_index++;
    }
    int ix, iy, fx, fy;
    if (!th_sim_idle_tile(s, qa->x, qa->y, standing_index, &ix, &iy))
        return;
    if (standing_index == 0) {
        fx = qa->face_x != TH_NONE ? qa->face_x : qa->x;
        fy = qa->face_y != TH_NONE ? qa->face_y : qa->y;
    } else if (!th_sim_idle_tile(s, qa->x, qa->y, standing_index - 1, &fx, &fy)) {
        return;
    }
    const int idle_dir = dir_towards(ix, iy, fx, fy);
    if (qa->bench_dist == TH_NONE) {
        const int idle_index = find_idle_before(h, qserial);
        if (idle_index < 0)
            return;
        h->queue[idle_index].dir = (uint8_t)idle_dir;
        th_action wk = th_action_walk(ix, iy);
        wk.flags |= TH_AF_MUST_HAPPEN | (is_leaving(h) ? TH_AF_IS_LEAVING : 0);
        th_humanoid_queue_action_at(w, h, wk, idle_index);
    } else {
        qa->bench_dist = TH_NONE;
        const int prior = leave_bench(w, h, qserial);
        if (prior < 0)
            return;
        th_action wk = th_action_walk(ix, iy);
        wk.flags |= TH_AF_MUST_HAPPEN;
        th_action idle = th_action_idle();
        idle.dir = (uint8_t)idle_dir;
        idle.flags |= TH_AF_MUST_HAPPEN;
        th_humanoid_queue_action_at(w, h, wk, prior);
        th_humanoid_queue_action_at(w, h, idle, prior + 1);
    }
}

// action_queue_on_leave: la cola lo deja pasar.
void th_sim_on_leave_queue(th_sim *s, int whom, th_action *qa)
{
    th_world *w = &s->w;
    th_humanoid *h = &s->hum[whom].h;
    qa->state &= (uint16_t)~TH_AS_IN_QUEUE;
    if (qa->aux != TH_NONE)
        s->obj[qa->aux].reserved_for = (uint8_t)whom;
    int idx;
    if (find_serial(h, qa->serial, &idx))
        interrupt_head(w, h, idx);
}

static void queue_start(th_world *w, th_humanoid *h, th_action *a)
{
    th_sim *s = w->sim;
    if (a->state & TH_AS_DONE_INIT)
        return;
    a->state |= TH_AS_DONE_INIT | TH_AS_IN_QUEUE;
    a->flags |= TH_AF_MUST_HAPPEN;
    a->interrupt = INT_QUEUE;
    const uint16_t serial = a->serial;
    const int q = s->obj[a->obj].queue;
    th_sim_queue_unexpect(s, q, who(w, h));
    th_sim_queue_push(s, q, who(w, h));
    th_action idle = th_action_idle();
    idle.flags |= TH_AF_MUST_HAPPEN | (is_leaving(h) ? TH_AF_IS_LEAVING : 0);
    th_humanoid_queue_action_at(w, h, idle, 0);
    a = find_serial(h, serial, NULL);
    if (a)
        th_sim_on_change_queue_position(s, who(w, h), a);
    if (s->queues[q].door != TH_NONE)
        th_sim_room_try_advance_queue(s, th_sim_room(s, s->obj[s->queues[q].door].room));
}

// ---------------------------------------------------------------------------
// Usar un objeto (use_object.lua)

static const char *const phase_names[] = {"begin_use", "begin_use_2", "begin_use_3", "begin_use_4", "begin_use_5",
                                          "in_use",    "finish_use",  "finish_use_2", "finish_use_3", "finish_use_4",
                                          "finish_use_5"};

// Tabla de animaciones de la fase (-5..5) para el objeto, o 0.
static th_rnode phase_anims(const th_sim *s, const th_action *a, int phase)
{
    const th_object *o = &s->obj[a->obj];
    static const char *const dirs[] = {"north", "east", "south", "west"};
    static const uint8_t mirror[] = {TH_DIR_WEST, TH_DIR_SOUTH, TH_DIR_EAST, TH_DIR_NORTH};
    th_rnode ua = th_rules_get(s->defs, o->def, "usage_animations");
    th_rnode t = th_rules_get(s->defs, ua, dirs[o->dir]);
    if (!t)
        t = th_rules_get(s->defs, ua, dirs[mirror[o->dir]]);
    return th_rules_get(s->defs, t, phase_names[phase + 5]);
}

// action_use_next_phase
static int use_next_phase(const th_sim *s, const th_action *a, int phase)
{
    phase++;
    if (phase < -6)
        phase = -6;
    if (phase == -6 && !(a->state & TH_AS_DO_WALK))
        phase++;
    while (phase >= -5 && phase <= 5 && !phase_anims(s, a, phase))
        phase++;
    if (phase == 6 && (!(a->state & TH_AS_DO_WALK)))
        phase++;
    if (phase > 6)
        phase = 100;
    return phase;
}

static th_rnode orientation(const th_sim *s, const th_object *o)
{
    static const char *const dirs[] = {"north", "east", "south", "west"};
    return th_rules_get(s->defs, th_rules_get(s->defs, o->def, "orientations"), dirs[o->dir]);
}

// Par (x, y) de una lista [x, y] o de un mapa {#1 = x, #2 = y}.
static int pair_at(const th_rules *r, th_rnode n, int *x, int *y)
{
    if (!n)
        return 0;
    if (th_rules_type(r, n) == TH_RULES_LIST) {
        th_rnode first = th_rules_at(r, n, 0);
        if (th_rules_type(r, first) == TH_RULES_LIST || th_rules_type(r, first) == TH_RULES_MAP)
            return pair_at(r, first, x, y); // render_attach_position partido: el primero
        *x = th_rules_int(r, first, 0);
        *y = th_rules_int(r, th_rules_at(r, n, 1), 0);
        return 1;
    }
    *x = th_rules_int(r, th_rules_geti(r, n, 1), 0);
    *y = th_rules_int(r, th_rules_geti(r, n, 2), 0);
    return 1;
}

static void use_phase(th_world *w, th_humanoid *h, th_action *a, int phase);

static void finish_using(th_sim *s, th_humanoid *h, int obj)
{
    th_sim_object_remove_user(s, obj, th_sim_index(s, h));
    h->user_of = TH_NONE;
}

// action_use_object_tick
static void use_tick(th_world *w, th_humanoid *h)
{
    th_sim *s = w->sim;
    th_action *a = &h->queue[0];
    const uint16_t serial = a->serial;
    const int old = a->phase;
    int phase = old;
    if (old == -6) {
        th_sim_object_set_user(s, a->obj, who(w, h));
        h->user_of = a->obj;
    }
    if (phase != 0 || !(a->state & TH_AS_PROLONGED) || !a->interrupt) {
        phase = use_next_phase(s, a, phase);
    } else if (a->cb == CB_GP_DESK) {
        th_sim_gp_loop_callback(s, who(w, h), a);
        a = find_serial(h, serial, NULL);
        if (!a)
            return;
    }
    if (old <= 5 && phase > 5)
        finish_using(s, h, a->obj);
    if (phase == 100) {
        set_tile_position_speed(w, h, a->old_x, a->old_y, 0, 0, 0, 0);
        if (th_rules_bool(s->defs, th_rules_get(s->defs, s->obj[a->obj].def, "dynamic_info"), 0))
            s->obj[a->obj].times_used++;
        th_humanoid_finish_action(w, h);
    } else {
        use_phase(w, h, a, phase);
    }
}

static void use_phase(th_world *w, th_humanoid *h, th_action *a, int phase)
{
    th_sim *s = w->sim;
    const th_object *o = &s->obj[a->obj];
    h->user_of = TH_NONE;
    a->phase = (int8_t)phase;
    if (phase == -6) {
        walk_raw(w, h, a->old_x, a->old_y, a->new_x, a->new_y, 0, TIMER_USE);
        return;
    }
    if (phase == 6) {
        walk_raw(w, h, a->new_x, a->new_y, a->old_x, a->old_y, 0, TIMER_USE);
        return;
    }
    th_rnode at = phase_anims(s, a, phase);
    th_rnode an = th_rules_get(s->defs, at, h->walk->name);
    if (!an) {
        use_tick(w, h); // sin animación para esta clase (los bedeles en CorsixTH)
        return;
    }
    int anim_length = 1, is_list = 0;
    unsigned flags = a->mirror;
    unsigned anim;
    const int type = th_rules_type(s->defs, an);
    if (type == TH_RULES_LIST && th_rules_len(s->defs, an) > 1) {
        is_list = 1;
        an = th_rules_at(s->defs, an, (uint32_t)th_rng_range(&s->rng, 0, (int32_t)th_rules_len(s->defs, an) - 1));
    }
    if (th_rules_type(s->defs, an) == TH_RULES_MAP) {
        // {anim, length = n, mirror = true, ...}
        anim_length = th_rules_int(s->defs, th_rules_get(s->defs, an, "length"), 1);
        if (th_rules_bool(s->defs, th_rules_get(s->defs, an, "mirror"), 0))
            flags ^= TH_ANIM_FLIP_H;
        anim = (unsigned)th_rules_int(s->defs, th_rules_geti(s->defs, an, 1), 0);
    } else if (th_rules_type(s->defs, an) == TH_RULES_LIST) {
        anim = (unsigned)th_rules_int(s->defs, th_rules_at(s->defs, an, 0), 0);
    } else {
        anim = (unsigned)th_rules_int(s->defs, an, 0);
    }
    set_animation(w, h, anim, flags);
    // setHumanoidTileSpeed
    th_rnode spec = orientation(s, o);
    int tx = o->x, ty = o->y, ox = 0, oy = 0, ax = 0, ay = 0;
    if (th_rules_bool(s->defs, th_rules_get(s->defs, spec, "use_animate_from_use_position"), 0)) {
        tx = a->old_x;
        ty = a->old_y;
    } else if (pair_at(s->defs, th_rules_get(s->defs, spec, "render_attach_position"), &ox, &oy)) {
        tx += ox;
        ty += oy;
    }
    pair_at(s->defs, th_rules_get(s->defs, spec, "animation_offset"), &ax, &ay);
    int bx = 0, by = 0;
    pair_at(s->defs, th_rules_get(s->defs, spec, "added_animation_offset_while_in_use"), &bx, &by);
    set_tile_position_speed(w, h, tx, ty, ax + bx, ay + by, 0, 0);
    h->user_of = a->obj;
    const int frames = th_sim_anim_length(s, anim);
    const int length = (anim_length > 1 ? anim_length : 1) * frames;
    if (phase != 0 || is_list || length != 1 || !(a->state & TH_AS_PROLONGED) || !a->interrupt || a->cb)
        th_humanoid_set_timer(h, (unsigned)length, TIMER_USE);
}

static void use_start(th_world *w, th_humanoid *h, th_action *a)
{
    th_sim *s = w->sim;
    th_object *o = &s->obj[a->obj];
    a->old_x = (uint8_t)h->tile_x;
    a->old_y = (uint8_t)h->tile_y;
    a->interrupt = INT_USE;
    a->flags |= TH_AF_MUST_HAPPEN;
    static const char *const dirs[] = {"north", "east", "south", "west"};
    th_rnode ua = th_rules_get(s->defs, o->def, "usage_animations");
    a->mirror = th_rules_get(s->defs, ua, dirs[o->dir]) ? 0 : TH_ANIM_FLIP_H;
    th_rnode spec = orientation(s, o);
    int fx, fy;
    if (pair_at(s->defs, th_rules_get(s->defs, spec, "finish_use_position"), &fx, &fy)) {
        a->old_x = (uint8_t)(o->x + fx);
        a->old_y = (uint8_t)(o->y + fy);
    }
    int wx, wy;
    if (pair_at(s->defs, th_rules_get(s->defs, spec, "walk_in_tile"), &wx, &wy)) {
        a->new_x = (uint8_t)(o->x + wx);
        a->new_y = (uint8_t)(o->y + wy);
    } else {
        a->new_x = o->x;
        a->new_y = o->y;
    }
    if (!(a->state & (TH_AS_PROLONGED | TH_AS_NO_PROLONG)) && phase_anims(s, a, -5) && phase_anims(s, a, 0) &&
        phase_anims(s, a, 1))
        a->state |= TH_AS_PROLONGED;
    if (th_rules_bool(s->defs, th_rules_get(s->defs, o->def, "walk_in_to_use"), 0)) {
        a->state |= TH_AS_DO_WALK;
    } else {
        th_sim_object_set_user(s, a->obj, who(w, h));
        h->user_of = a->obj;
    }
    use_phase(w, h, a, use_next_phase(s, a, -100));
}

// action_use_object_interrupt
static void use_interrupt(th_world *w, th_humanoid *h, th_action *a, int high)
{
    th_sim *s = w->sim;
    if (a->flags & TH_AF_UNINTERRUPTIBLE)
        return;
    if (high) {
        if (h->user_of != TH_NONE)
            finish_using(s, h, a->obj);
        else if (s->obj[a->obj].reserved_for == who(w, h))
            s->obj[a->obj].reserved_for = TH_NONE;
        h->timer = 0;
        h->timer_fn = TIMER_NONE;
        set_tile_position_speed(w, h, a->old_x, a->old_y, 0, 0, 0, 0);
        th_humanoid_finish_action(w, h);
    } else if (h->timer_fn == TIMER_NONE) {
        th_humanoid_set_timer(h, 1, TIMER_USE);
    }
}

// ---------------------------------------------------------------------------
// Llamar a la puerta (knock_door.lua)

static void knock_start(th_world *w, th_humanoid *h, th_action *a)
{
    th_sim *s = w->sim;
    a->phase = 0;
    a->flags |= TH_AF_MUST_HAPPEN;
    const int east = a->dir == TH_DIR_EAST || a->dir == TH_DIR_SOUTH;
    const unsigned anim = east ? h->walk->knock_east : h->walk->knock_north;
    const unsigned mirror = (a->dir == TH_DIR_WEST || a->dir == TH_DIR_SOUTH) ? TH_ANIM_FLIP_H : 0;
    set_animation(w, h, anim, mirror);
    set_tile_position_speed(w, h, h->tile_x, h->tile_y, 0, 0, 0, 0);
    th_humanoid_set_timer(h, (unsigned)th_sim_anim_length(s, anim), TIMER_KNOCK);
    h->user_of = a->obj;
    th_sim_object_set_user(s, a->obj, who(w, h));
    a->phase = 1;
}

static void knock_tick(th_world *w, th_humanoid *h)
{
    th_sim *s = w->sim;
    const int door = h->user_of;
    th_sim_object_remove_user(s, door, who(w, h));
    h->user_of = TH_NONE;
    th_sim_room_try_advance_queue(s, th_sim_room(s, s->obj[door].room));
    th_humanoid_finish_action(w, h);
}

// ---------------------------------------------------------------------------
// Deambular (meander.lua)

static void meander_start(th_world *w, th_humanoid *h, th_action *a)
{
    th_sim *s = w->sim;
    if (th_sim_staff_meander(s, who(w, h), (a->flags & TH_AF_MUST_HAPPEN) != 0))
        return;
    int x, y;
    if (!th_sim_idle_tile(s, h->tile_x, h->tile_y, th_rng_range(&s->rng, 1, 24), &x, &y)) {
        x = h->tile_x;
        y = h->tile_y;
    }
    if (x == h->tile_x && y == h->tile_y) {
        if (h->queue_len == 1)
            th_humanoid_queue_action(h, th_action_idle());
        th_humanoid_finish_action(w, h);
        return;
    }
    if (a->flags & TH_AF_TODO_INTERRUPT) {
        th_humanoid_finish_action(w, h);
        return;
    }
    if (a->count >= 0) {
        if (a->count == 0) {
            th_humanoid_finish_action(w, h);
            return;
        }
        a->count--;
    }
    th_action next;
    if (a->state & TH_AS_CAN_IDLE) {
        a->state &= (uint16_t)~TH_AS_CAN_IDLE;
        next = th_action_idle();
        next.count = (int16_t)th_rng_range(&s->rng, 5, 40);
    } else {
        a->state |= TH_AS_CAN_IDLE;
        next = th_action_walk(x, y);
    }
    next.flags |= a->flags & TH_AF_MUST_HAPPEN;
    th_humanoid_queue_action_at(w, h, next, 0);
}

// ---------------------------------------------------------------------------
// Buscar una sala (seek_room.lua)

static void goto_room(th_world *w, th_humanoid *h, th_room *r, int diag_idx)
{
    th_sim *s = w->sim;
    th_person *p = person(w, h);
    p->u.p.waiting = -1;
    p->u.p.next_room = r->num;
    if (diag_idx >= 0 && diag_idx < p->u.p.n_diag) {
        memmove(&p->u.p.diag[diag_idx], &p->u.p.diag[diag_idx + 1], (unsigned)(p->u.p.n_diag - diag_idx - 1));
        p->u.p.n_diag--;
    }
    th_humanoid_set_next_action(w, h, th_sim_room_enter_action(s, r, who(w, h)));
    th_sim_call_for_staff(s, r); // si la sala no tiene el personal que necesita
}

// action_seek_room_find_room
static th_room *find_room(th_world *w, th_humanoid *h, th_action *a, int *diag_idx)
{
    th_sim *s = w->sim;
    th_person *p = person(w, h);
    *diag_idx = -1;
    if (a->state & TH_AS_DIAG_ROOM) {
        int r = th_sim_find_room_near(s, who(w, h), a->room_type);
        if (r != TH_NONE) {
            *diag_idx = a->diag_idx;
            return &s->rooms[r];
        }
        uint8_t check[TH_SIM_DIAG_ROOMS];
        int n = p->u.p.n_diag;
        for (int i = 0; i < n; i++)
            check[i] = (uint8_t)i;
        while (n > 0) {
            const int k = th_rng_range(&s->rng, 0, n - 1);
            const int type = p->u.p.diag[check[k]];
            r = th_sim_find_room_near(s, who(w, h), type);
            if (r != TH_NONE) {
                a->diag_idx = check[k];
                a->room_type = (uint8_t)type;
                *diag_idx = a->diag_idx;
                return &s->rooms[r];
            }
            memmove(&check[k], &check[k + 1], (unsigned)(n - k - 1));
            n--;
        }
        return NULL;
    }
    if (a->state & TH_AS_TREATMENT) {
        const th_disease *d = &s->dis[p->u.p.disease];
        for (int i = 0; i < d->n_treat; i++)
            if (th_sim_find_room_near(s, who(w, h), d->treat[i]) == TH_NONE)
                return NULL;
    }
    const int r = th_sim_find_room_near(s, who(w, h), a->room_type);
    return r == TH_NONE ? NULL : &s->rooms[r];
}

// action_seek_room_no_diagnosis_room_found: devuelve 1 si la acción sigue.
static int no_diagnosis_room(th_world *w, th_humanoid *h)
{
    th_sim *s = w->sim;
    th_person *p = person(w, h);
    const int me = who(w, h);
    if (p->u.p.diagnosis_progress < s->hosp.send_home) {
        th_sim_go_home(s, me, TH_HOME_KICKED);
        return 0;
    }
    const th_casebook *cb = th_sim_casebook(s, s->dis[p->u.p.disease].id);
    if (p->u.p.diagnosis_progress < s->hosp.guess_cure || !cb || !cb->discovered) {
        p->u.p.waiting = 60;
        th_sim_send_fax(s, me, TH_FAX_NO_DIAGNOSIS);
        return 1;
    }
    th_sim_set_diagnosed(s, me);
    if (th_sim_agrees_to_pay(s, me, s->dis[p->u.p.disease].id)) {
        th_action sr = th_action_new(TH_ACTION_SEEK_ROOM);
        sr.room_type = s->dis[p->u.p.disease].treat[0];
        sr.state |= TH_AS_TREATMENT;
        th_humanoid_queue_action_at(w, h, sr, 1);
    } else {
        th_sim_go_home(s, me, TH_HOME_OVER_PRICED);
        return 0;
    }
    th_humanoid_finish_action(w, h);
    return 0;
}

static void seek_room_start(th_world *w, th_humanoid *h, th_action *a)
{
    th_sim *s = w->sim;
    th_person *p = person(w, h);
    const uint16_t serial = a->serial;
    if (a->flags & TH_AF_TODO_INTERRUPT) {
        th_humanoid_finish_action(w, h);
        return;
    }
    int diag_idx;
    th_room *r = find_room(w, h, a, &diag_idx);
    if (r) {
        goto_room(w, h, r, diag_idx);
        return;
    }
    if (!(a->state & TH_AS_DONE_INIT)) {
        a->state |= TH_AS_DONE_INIT;
        a->flags |= TH_AF_MUST_HAPPEN;
        a->interrupt = INT_SEEK_ROOM;
    }
    if (!(a->state & TH_AS_DONE_WALK)) {
        int valid = 1;
        if (a->state & TH_AS_MSG_SENT) {
            // message_sent: la rehízo interruptAndRequeueAction, el fax ya se mandó.
        } else if (p->u.p.diagnosed) {
            th_sim_send_fax(s, who(w, h), TH_FAX_NO_TREATMENT);
            p->u.p.waiting = 60;
        } else if (a->room_type == th_sim_room_type(s, "gp")) {
            // Sin consulta: esperar a que la construyan (en el hito 4 no pasa).
        } else {
            valid = no_diagnosis_room(w, h);
        }
        if (!valid)
            return;
        a = find_serial(h, serial, NULL);
        if (!a)
            return;
        a->state |= TH_AS_DONE_WALK;
        th_action m = th_action_new(TH_ACTION_MEANDER);
        m.count = 1;
        m.flags |= TH_AF_MUST_HAPPEN;
        th_humanoid_queue_action_at(w, h, m, 0);
    } else {
        idle_anim(w, h, h->last_dir);
        set_tile_position_speed(w, h, h->tile_x, h->tile_y, 0, 0, 0, 0);
    }
}

// ---------------------------------------------------------------------------
// Atender el mostrador (staff_reception.lua)

static void reception_leave(th_world *w, th_humanoid *h, th_action *a, int high)
{
    th_sim *s = w->sim;
    th_person *p = person(w, h);
    th_object *desk = &s->obj[a->obj];
    desk->receptionist = TH_NONE;
    th_sim_desk_check_for_staff(s, a->obj);
    p->u.s.associated_desk = TH_NONE;
    int dx, dy;
    th_sim_use_tile(s, a->obj, 1, &dx, &dy);
    if (high) {
        set_tile_position_speed(w, h, dx, dy, 0, 0, 0, 0);
        th_humanoid_finish_action(w, h);
    } else {
        a->x = (uint8_t)dx;
        a->y = (uint8_t)dy;
        walk_raw(w, h, h->tile_x, h->tile_y, dx, dy, 0, TIMER_RECEPTION_LEFT);
    }
}

static void reception_idle_phase(th_world *w, th_humanoid *h)
{
    th_sim *s = w->sim;
    th_action *a = &h->queue[0];
    th_object *desk = &s->obj[a->obj];
    idle_anim(w, h, h->last_dir);
    set_tile_position_speed(w, h, desk->x, desk->y, 0, 0, 0, 0);
    desk->receptionist = (uint8_t)who(w, h);
    desk->reserved_for = TH_NONE;
    if (a->interrupt)
        a->interrupt = INT_RECEPTION;
    else
        reception_leave(w, h, a, 0);
}

static void reception_start(th_world *w, th_humanoid *h, th_action *a)
{
    th_sim *s = w->sim;
    th_person *p = person(w, h);
    if (a->flags & TH_AF_TODO_INTERRUPT) {
        if (p->u.s.associated_desk != TH_NONE) {
            s->obj[p->u.s.associated_desk].reserved_for = TH_NONE;
            th_sim_desk_check_for_staff(s, p->u.s.associated_desk);
        }
        p->u.s.associated_desk = TH_NONE;
        th_humanoid_finish_action(w, h);
        return;
    }
    const th_object *desk = &s->obj[a->obj];
    walk_raw(w, h, h->tile_x, h->tile_y, desk->x, desk->y, 0, TIMER_RECEPTION_IDLE);
    a->flags |= TH_AF_MUST_HAPPEN;
    a->interrupt = INT_RECEPTION_EARLY;
}

// ---------------------------------------------------------------------------
// Vomitar, orinar, bostezar, dar golpecitos con el pie y mirar el reloj
// (vomit.lua, pee.lua, yawn.lua, tap_foot.lua y check_watch.lua): la
// animación entera, que tiene que acabar. Menos el bostezo, miran al este o
// al sur al azar. Vomitar y orinar dejan basura donde están.

static void need_start(th_world *w, th_humanoid *h, th_action *a)
{
    const th_walk_anims *wa = h->walk;
    const unsigned anim = a->kind == TH_ACTION_VOMIT  ? wa->vomit
                          : a->kind == TH_ACTION_PEE  ? wa->pee
                          : a->kind == TH_ACTION_YAWN ? wa->yawn
                          : a->kind == TH_ACTION_TAP_FOOT ? wa->tap_foot
                                                          : wa->check_watch;
    if (a->kind != TH_ACTION_YAWN)
        h->last_dir = th_rng_range(&w->sim->rng, 0, 1) == 1 ? TH_DIR_EAST : TH_DIR_SOUTH;
    a->flags |= TH_AF_MUST_HAPPEN;
    set_animation(w, h, anim, h->last_dir == TH_DIR_EAST ? 0 : TH_ANIM_FLIP_H);
    th_humanoid_set_timer(h, (unsigned)th_sim_anim_length(w->sim, anim), TIMER_NEED_END);
}

static void need_end(th_world *w, th_humanoid *h)
{
    const th_action *a = &h->queue[0];
    if (a->kind == TH_ACTION_VOMIT || a->kind == TH_ACTION_PEE)
        th_sim_add_litter(w->sim, h->tile_x, h->tile_y, a->kind == TH_ACTION_VOMIT ? TH_LITTER_PUKE : TH_LITTER_PEE,
                          h->last_dir != TH_DIR_SOUTH);
    th_humanoid_finish_action(w, h);
}

void th_humanoid_interrupt_and_requeue(th_world *w, th_humanoid *h, int pos)
{
    const th_action *cur = &h->queue[0];
    th_action r;
    if (cur->kind == TH_ACTION_IDLE) {
        r = th_action_idle();
        r.flags |= cur->flags & TH_AF_MUST_HAPPEN;
        r.dir = cur->dir;
    } else if (cur->kind == TH_ACTION_WALK) {
        r = th_action_walk(cur->x, cur->y);
        // setMustHappen(must_happen) y, si se volvió a empezar, saved_must_happen.
        const int mh = cur->state & TH_AS_RESTARTED ? (cur->state & TH_AS_SAVED_MH) != 0
                                                    : (cur->flags & TH_AF_MUST_HAPPEN) != 0;
        r.flags |= (uint8_t)(mh ? TH_AF_MUST_HAPPEN : 0) | (cur->flags & TH_AF_IS_ENTERING);
        r.aux = cur->aux; // reserve_on_resume
    } else {
        // Buscando sala: se vuelve a empezar, sin must_happen y sin volver a mandar el fax.
        r = th_action_new(TH_ACTION_SEEK_ROOM);
        r.room_type = cur->room_type;
        r.diag_idx = cur->diag_idx;
        r.state |= (uint16_t)((cur->state & (TH_AS_TREATMENT | TH_AS_DIAG_ROOM)) | TH_AS_MSG_SENT);
    }
    th_humanoid_queue_action_at(w, h, r, pos);
    th_action *a = &h->queue[0];
    if (a->interrupt)
        call_interrupt(w, h, a, 0);
    else
        th_humanoid_finish_action(w, h);
}

// ---------------------------------------------------------------------------
// Despacho

static void call_interrupt(th_world *w, th_humanoid *h, th_action *a, int high)
{
    const int handler = a->interrupt;
    a->interrupt = INT_NONE;
    switch (handler) {
    case INT_IDLE: th_humanoid_set_timer(h, 1, TIMER_FINISH); break;
    case INT_WALK: walk_interrupt(w, h, a, high); break;
    case INT_QUEUE: {
        th_sim *s = w->sim;
        const int q = s->obj[a->obj].queue;
        if (a->state & TH_AS_IN_QUEUE)
            th_sim_queue_remove_value(s, q, who(w, h));
        if (a->aux != TH_NONE && s->obj[a->aux].reserved_for == who(w, h))
            s->obj[a->aux].reserved_for = TH_NONE;
        th_humanoid_finish_action(w, h);
        break;
    }
    case INT_USE: use_interrupt(w, h, a, high); break;
    case INT_SEEK_ROOM:
        a->state &= (uint16_t)~TH_AS_DONE_INIT;
        th_humanoid_finish_action(w, h);
        break;
    case INT_RECEPTION: reception_leave(w, h, a, high); break;
    case INT_RECEPTION_EARLY:
        if (high) {
            th_sim *s = w->sim;
            th_person *p = person(w, h);
            s->obj[a->obj].reserved_for = TH_NONE;
            if (p->u.s.associated_desk != TH_NONE)
                th_sim_desk_check_for_staff(s, p->u.s.associated_desk);
            p->u.s.associated_desk = TH_NONE;
            h->timer = 0;
            h->timer_fn = TIMER_NONE;
            int dx, dy;
            th_sim_use_tile(s, a->obj, 1, &dx, &dy);
            set_tile_position_speed(w, h, dx, dy, 0, 0, 0, 0);
            th_humanoid_finish_action(w, h);
        }
        // Sin prioridad alta no hace nada, pero el manejador ya no está: al
        // llegar al mostrador se irá (action_staff_reception_idle_phase).
        break;
    default: break;
    }
}

// Humanoid:_handleEmptyActionQueue
static void empty_queue(th_world *w, th_humanoid *h)
{
    if (!w->sim) {
        th_humanoid_queue_action(h, th_action_idle());
        return;
    }
    const th_person *p = person(w, h);
    if (p->kind == TH_HUM_PATIENT)
        th_humanoid_queue_action(h, p->u.p.going_home ? th_action_idle() : th_action_new(TH_ACTION_SEEK_RECEPTION));
    else
        th_humanoid_queue_action(h, th_action_new(TH_ACTION_MEANDER));
}

static void start_action(th_world *w, th_humanoid *h)
{
    if (h->queue_len == 0)
        empty_queue(w, h);
    th_action *a = &h->queue[0];
    const uint16_t serial = a->serial;
    switch (a->kind) {
    case TH_ACTION_WALK: walk_start(w, h); break;
    case TH_ACTION_SPAWN: spawn_start(w, h, a); break;
    case TH_ACTION_SEEK_RECEPTION: seek_reception_start(w, h, a); break;
    case TH_ACTION_QUEUE: queue_start(w, h, a); break;
    case TH_ACTION_USE_OBJECT: use_start(w, h, a); break;
    case TH_ACTION_KNOCK_DOOR: knock_start(w, h, a); break;
    case TH_ACTION_MEANDER: meander_start(w, h, a); break;
    case TH_ACTION_SEEK_ROOM: seek_room_start(w, h, a); break;
    case TH_ACTION_STAFF_RECEPTION: reception_start(w, h, a); break;
    case TH_ACTION_CALL_CHECKPOINT:
        a->flags |= TH_AF_MUST_HAPPEN;
        th_sim_call_checkpoint(w->sim, a->obj, a->val);
        th_humanoid_finish_action(w, h);
        break;
    case TH_ACTION_ANSWER_CALL:
        a->flags |= TH_AF_MUST_HAPPEN;
        th_sim_answer_call_action(w->sim, who(w, h));
        break;
    case TH_ACTION_VOMIT:
    case TH_ACTION_PEE:
    case TH_ACTION_YAWN:
    case TH_ACTION_TAP_FOOT:
    case TH_ACTION_CHECK_WATCH: need_start(w, h, a); break;
    default: idle_start(w, h, a); break;
    }
    // Si la acción estaba marcada para interrumpirla, se interrumpe ahora.
    a = &h->queue[0];
    if (h->queue_len && a->serial == serial && (a->flags & TH_AF_TODO_INTERRUPT) &&
        !(a->flags & TH_AF_UNINTERRUPTIBLE)) {
        const int high = (a->flags & TH_AF_TODO_HIGH) != 0;
        a->flags &= (uint8_t)~(TH_AF_TODO_INTERRUPT | TH_AF_TODO_HIGH);
        if (a->interrupt)
            call_interrupt(w, h, a, high);
    }
}

void th_humanoid_init(th_world *w, th_humanoid *h, const th_walk_anims *walk)
{
    memset(h, 0, sizeof(*h));
    h->tile_x = h->tile_y = -1;
    h->drawing_layer = TH_LAYER_ENTITY;
    h->walk = walk;
    h->last_dir = TH_DIR_EAST;
    h->speed = 1;
    h->user_of = TH_NONE;
    h->anim = 2;
    h->frame = th_anims_first_frame(w->anims, 2);
    th_humanoid_set_next_action(w, h, th_action_idle());
}

void th_humanoid_set_layer(th_humanoid *h, int layer, int id)
{
    if (layer >= 0 && layer < TH_ANIM_LAYERS)
        h->layers[layer] = (uint8_t)id;
}

void th_humanoid_tick(th_world *w, th_humanoid *h)
{
    // Entity:_tick: con slow_animation, la animación avanza un tick sí y otro no.
    if (!h->slow_animation || !h->skip_next_tick) {
        h->frame = th_anims_next_frame(w->anims, h->frame);
        h->px = (int16_t)(h->px + h->sx);
        h->py = (int16_t)(h->py + h->sy);
    }
    if (h->slow_animation)
        h->skip_next_tick = !h->skip_next_tick;
    if (h->timer && --h->timer == 0) {
        const unsigned fn = h->timer_fn;
        h->timer_fn = TIMER_NONE;
        switch (fn) {
        case TIMER_WALK: walk_tick(w, h); break;
        case TIMER_FINISH: th_humanoid_finish_action(w, h); break;
        case TIMER_WALK_DOOR:
            if (h->user_of != TH_NONE)
                th_sim_object_remove_user(w->sim, h->user_of, who(w, h));
            h->user_of = TH_NONE;
            walk_tick(w, h);
            break;
        case TIMER_IDLE: th_humanoid_finish_action(w, h); break;
        case TIMER_USE: use_tick(w, h); break;
        case TIMER_KNOCK: knock_tick(w, h); break;
        case TIMER_DESPAWN: th_sim_destroy(w->sim, who(w, h)); break;
        case TIMER_RECEPTION_IDLE: reception_idle_phase(w, h); break;
        case TIMER_RECEPTION_LEFT: {
            const th_action *a = &h->queue[0];
            set_tile_position_speed(w, h, a->x, a->y, 0, 0, 0, 0);
            th_humanoid_finish_action(w, h);
            break;
        }
        case TIMER_NEED_END: need_end(w, h); break;
        default: break;
        }
    }
}
