#include "th/humanoid.h"

#include <string.h>

// walk_anims de entities/humanoid.lua (CorsixTH): caminar al norte y al este,
// y quieto mirando al norte y al este. El sur y el oeste son los mismos en espejo.
static const th_walk_anims walk_table[] = {
    {"Standard Male Patient", 16, 18, 24, 26},
    {"Gowned Male Patient", 406, 408, 414, 416},
    {"Stripped Male Patient", 818, 820, 826, 828},
    {"Stripped Male Patient 2", 818, 820, 826, 828},
    {"Stripped Male Patient 3", 818, 820, 826, 828},
    {"Alternate Male Patient", 2704, 2706, 2712, 2714},
    {"Slack Male Patient", 1484, 1486, 1492, 1494},
    {"Slack Female Patient", 0, 2, 8, 10},
    {"Transparent Male Patient", 1064, 1066, 1072, 1074},
    {"Standard Female Patient", 0, 2, 8, 10},
    {"Gowned Female Patient", 2876, 2878, 2884, 2886},
    {"Stripped Female Patient", 834, 836, 842, 844},
    {"Stripped Female Patient 2", 834, 836, 842, 844},
    {"Stripped Female Patient 3", 834, 836, 842, 844},
    {"Transparent Female Patient", 3012, 3014, 3020, 3022},
    {"Chewbacca Patient", 858, 860, 866, 868},
    {"Elvis Patient", 978, 980, 986, 988},
    {"Invisible Patient", 1642, 1644, 1840, 1842},
    {"Alien Male Patient", 3598, 3600, 3606, 3608},
    {"Alien Female Patient", 3598, 3600, 3606, 3608},
    {"Doctor", 32, 34, 40, 42},
    {"Surgeon", 2288, 2290, 2296, 2298},
    {"Nurse", 1206, 1208, 1650, 1652},
    {"Handyman", 1858, 1860, 1866, 1868},
    {"Receptionist", 3668, 3670, 3676, 3678},
    {"VIP", 266, 268, 274, 276},
    {"Inspector", 266, 268, 274, 276},
    {"Grim Reaper", 994, 996, 1002, 1004},
};

enum { TIMER_NONE, TIMER_WALK, TIMER_FINISH };

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

static void set_timer(th_humanoid *h, unsigned ticks, unsigned fn)
{
    h->timer = (uint16_t)ticks;
    h->timer_fn = (uint8_t)fn;
}

static void start_action(th_world *w, th_humanoid *h);

static void finish_action(th_world *w, th_humanoid *h)
{
    if (h->queue_len) {
        memmove(&h->queue[0], &h->queue[1], (h->queue_len - 1u) * sizeof(th_action));
        h->queue_len--;
    }
    start_action(w, h);
}

// idle.lua: la animación de quieto según la última dirección, sin velocidad.
static void idle_start(th_world *w, th_humanoid *h)
{
    const th_walk_anims *a = h->walk;
    switch (h->last_dir) {
    case TH_DIR_NORTH: set_animation(w, h, a->idle_north, 0); break;
    case TH_DIR_EAST: set_animation(w, h, a->idle_east, 0); break;
    case TH_DIR_SOUTH: set_animation(w, h, a->idle_east, TH_ANIM_FLIP_H); break;
    default: set_animation(w, h, a->idle_north, TH_ANIM_FLIP_H); break;
    }
    // humanoid.th:setTile(map, x, y, layer): vuelve a entrar en su casilla.
    if (h->tile_x >= 0)
        h->attach_seq = ++w->attach_seq;
    h->sx = h->sy = 0;
}

// action_walk_raw sin puertas.
static void walk_raw(th_world *w, th_humanoid *h, int x1, int y1, int x2, int y2)
{
    const th_walk_anims *a = h->walk;
    if (x1 != x2) {
        if (x1 < x2) {
            h->last_dir = TH_DIR_EAST;
            set_animation(w, h, a->walk_east, 0);
            set_tile_position_speed(w, h, x2, y2, -32, -16, WALK_DX, WALK_DY);
        } else {
            h->last_dir = TH_DIR_WEST;
            set_animation(w, h, a->walk_north, TH_ANIM_FLIP_H);
            set_tile_position_speed(w, h, x1, y1, 0, 0, -WALK_DX, -WALK_DY);
        }
    } else if (y1 < y2) {
        h->last_dir = TH_DIR_SOUTH;
        set_animation(w, h, a->walk_east, TH_ANIM_FLIP_H);
        set_tile_position_speed(w, h, x2, y2, 32, -16, -WALK_DX, WALK_DY);
    } else {
        h->last_dir = TH_DIR_NORTH;
        set_animation(w, h, a->walk_north, 0);
        set_tile_position_speed(w, h, x1, y1, 0, 0, WALK_DX, -WALK_DY);
    }
    set_timer(h, WALK_TICKS, TIMER_WALK);
}

static int step_dir(const th_humanoid *h, unsigned i)
{
    return (h->path_dirs[i >> 2] >> ((i & 3) * 2)) & 3;
}

static int passable(const th_world *w, int x, int y)
{
    return (th_map_tile_c(w->map, x, y)->flags & TH_TILE_PASSABLE) != 0;
}

static void walk_start(th_world *w, th_humanoid *h);

// action_walk_tick: arranca el paso siguiente o termina la acción.
static void walk_tick(th_world *w, th_humanoid *h)
{
    const int x1 = h->cur_x, y1 = h->cur_y;
    if (h->path_index >= h->path_len) {
        set_tile_position_speed(w, h, x1, y1, 0, 0, 0, 0);
        if (h->path_truncated) {
            walk_start(w, h);
            return;
        }
        finish_action(w, h);
        return;
    }
    static const int8_t dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    const int d = step_dir(h, h->path_index);
    const int x2 = x1 + dx[d], y2 = y1 + dy[d];
    // Si la casilla siguiente ha dejado de ser transitable, se busca otro camino.
    if (!passable(w, x2, y2) && passable(w, x1, y1)) {
        set_tile_position_speed(w, h, x1, y1, 0, 0, 0, 0);
        walk_start(w, h);
        return;
    }
    walk_raw(w, h, x1, y1, x2, y2);
    h->path_index++;
    h->cur_x = (uint8_t)x2;
    h->cur_y = (uint8_t)y2;
}

// action_walk_start
static void walk_start(th_world *w, th_humanoid *h)
{
    th_action *a = &h->queue[0];
    int len = h->tile_x < 0 ? -1 : th_path_find(w->pf, w->map, h->tile_x, h->tile_y, a->x, a->y);
    if (len <= 0) {
        // Sin camino, o ya está allí: quieto un tick y se acaba la acción.
        idle_start(w, h);
        set_timer(h, 1, TIMER_FINISH);
        return;
    }
    // El camino sale de los nodos del A*: de la meta hacia atrás por «prev».
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
    len = kept;
    h->path_len = (uint16_t)len;
    h->path_index = 0;
    h->cur_x = (uint8_t)h->tile_x;
    h->cur_y = (uint8_t)h->tile_y;
    h->walk_interruptible = 1;
    a->must_happen = 1;
    walk_tick(w, h);
}

static void start_action(th_world *w, th_humanoid *h)
{
    if (h->queue_len == 0)
        h->queue[h->queue_len++] = th_action_idle(); // _handleEmptyActionQueue
    switch (h->queue[0].kind) {
    case TH_ACTION_WALK: walk_start(w, h); break;
    default: idle_start(w, h); break;
    }
}

void th_humanoid_init(th_world *w, th_humanoid *h, const th_walk_anims *walk)
{
    memset(h, 0, sizeof(*h));
    h->tile_x = h->tile_y = -1;
    h->drawing_layer = TH_LAYER_ENTITY;
    h->walk = walk;
    h->last_dir = TH_DIR_EAST;
    h->anim = 2;
    h->frame = th_anims_first_frame(w->anims, 2);
    th_humanoid_set_next_action(w, h, th_action_idle());
}

void th_humanoid_set_layer(th_humanoid *h, int layer, int id)
{
    if (layer >= 0 && layer < TH_ANIM_LAYERS)
        h->layers[layer] = (uint8_t)id;
}

void th_humanoid_queue_action(th_humanoid *h, th_action a)
{
    if (h->queue_len < TH_HUMANOID_QUEUE)
        h->queue[h->queue_len++] = a;
}

void th_humanoid_set_next_action(th_world *w, th_humanoid *h, th_action a)
{
    // Las acciones que tienen que acabar (un paso de caminar) se quedan; el
    // resto se descarta y la nueva va detrás.
    unsigned i = 0;
    while (i < h->queue_len && h->queue[i].must_happen)
        i++;
    if (i >= TH_HUMANOID_QUEUE)
        i = TH_HUMANOID_QUEUE - 1;
    h->queue[i] = a;
    h->queue_len = (uint8_t)(i + 1);
    if (i == 0) {
        start_action(w, h);
        return;
    }
    // action_walk_interrupt: el camino se corta tras la casilla a la que va.
    if (h->queue[0].kind == TH_ACTION_WALK && h->walk_interruptible) {
        h->walk_interruptible = 0;
        h->path_len = h->path_index;
        h->path_truncated = 0;
    }
}

void th_humanoid_tick(th_world *w, th_humanoid *h)
{
    h->frame = th_anims_next_frame(w->anims, h->frame);
    h->px = (int16_t)(h->px + h->sx);
    h->py = (int16_t)(h->py + h->sy);
    if (h->timer && --h->timer == 0) {
        unsigned fn = h->timer_fn;
        h->timer_fn = TIMER_NONE;
        if (fn == TIMER_WALK)
            walk_tick(w, h);
        else if (fn == TIMER_FINISH)
            finish_action(w, h);
    }
}
