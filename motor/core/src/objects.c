// Objetos sobre el mapa (th/objects.h): entities/object.lua,
// objects/door.lua, objects/doors/*.lua y world.lua de CorsixTH.
#include "th/objects.h"

#include <string.h>

static const char *const dir_names[4] = {"north", "east", "south", "west"};

// Object.directionParameters: norte, este, sur y oeste.
static const int8_t dir_dx[4] = {0, 1, 0, -1}, dir_dy[4] = {-1, 0, 1, 0};
static const uint32_t buildable_flag[4] = {TH_TILE_BUILDABLE_N, TH_TILE_BUILDABLE_E, TH_TILE_BUILDABLE_S,
                                           TH_TILE_BUILDABLE_W};
static const uint32_t travel_flag[4] = {TH_TILE_TRAVEL_N, TH_TILE_TRAVEL_E, TH_TILE_TRAVEL_S, TH_TILE_TRAVEL_W};

static int in_map(int x, int y)
{
    return x >= 0 && y >= 0 && x < TH_MAP_WIDTH && y < TH_MAP_HEIGHT;
}

static void set_flags(th_map *m, int x, int y, uint32_t on, uint32_t off)
{
    if (!in_map(x, y))
        return;
    th_tile *t = th_map_tile(m, x, y);
    t->flags = (t->flags | on) & ~off;
}

static uint32_t flags_at(const th_map *m, int x, int y)
{
    return in_map(x, y) ? th_map_tile_c(m, x, y)->flags : 0;
}

int th_object_footprint(const th_rules *r, th_rnode def, int dir, th_fp_tile *fp)
{
    const th_rnode ori = th_rules_get(r, th_rules_get(r, def, "orientations"), dir_names[dir & 3]);
    const th_rnode list = th_rules_get(r, ori, "footprint");
    uint32_t n = th_rules_len(r, list);
    if (th_rules_type(r, list) != TH_RULES_LIST)
        return 0;
    if (n > TH_FOOTPRINT_MAX)
        n = TH_FOOTPRINT_MAX;
    static const char *const need[4] = {"need_north_side", "need_east_side", "need_south_side", "need_west_side"};
    for (uint32_t i = 0; i < n; i++) {
        const th_rnode t = th_rules_at(r, list, i);
        th_fp_tile *p = &fp[i];
        p->f = p->need = 0;
        if (th_rules_type(r, t) == TH_RULES_LIST) {
            p->dx = (int8_t)th_rules_int(r, th_rules_at(r, t, 0), 0);
            p->dy = (int8_t)th_rules_int(r, th_rules_at(r, t, 1), 0);
            continue;
        }
        p->dx = (int8_t)th_rules_int(r, th_rules_geti(r, t, 1), 0);
        p->dy = (int8_t)th_rules_int(r, th_rules_geti(r, t, 2), 0);
        p->f |= th_rules_bool(r, th_rules_get(r, t, "complete_cell"), 0) ? TH_FP_COMPLETE : 0;
        p->f |= th_rules_bool(r, th_rules_get(r, t, "only_passable"), 0) ? TH_FP_PASSABLE : 0;
        p->f |= th_rules_bool(r, th_rules_get(r, t, "only_side"), 0) ? TH_FP_SIDE : 0;
        p->f |= th_rules_bool(r, th_rules_get(r, t, "optional"), 0) ? TH_FP_OPTIONAL : 0;
        p->f |= th_rules_bool(r, th_rules_get(r, t, "shareable"), 0) ? TH_FP_SHAREABLE : 0;
        p->f |= th_rules_bool(r, th_rules_get(r, t, "invisible"), 0) ? TH_FP_INVISIBLE : 0;
        for (int d = 0; d < 4; d++)
            p->need |= th_rules_bool(r, th_rules_get(r, t, need[d]), 0) ? (uint8_t)(1u << d) : 0;
    }
    return (int)n;
}

// La huella de la orientación del objeto (self.footprint), o 0 casillas.
static int footprint(const th_objects *w, const th_object *o, th_fp_tile *fp)
{
    return th_object_footprint(w->defs, o->def, o->dir, fp);
}

static int in_footprint(const th_fp_tile *fp, int n, int dx, int dy)
{
    for (int i = 0; i < n; i++)
        if (fp[i].dx == dx && fp[i].dy == dy)
            return 1;
    return 0;
}

// Los bordes de la casilla k que la huella ocupa: los que dan a otra casilla
// de la huella, todos con complete_cell y los need_<dir>_side.
static uint32_t footprint_sides(const th_fp_tile *fp, int n, int k)
{
    uint32_t sides = 0;
    for (int d = 0; d < 4; d++)
        if (in_footprint(fp, n, fp[k].dx + dir_dx[d], fp[k].dy + dir_dy[d]) || (fp[k].f & TH_FP_COMPLETE) ||
            (fp[k].need & (1u << d)))
            sides |= buildable_flag[d];
    return sides;
}

// La dirección con la que un objeto de borde ocupa: la suya, salvo la
// papelera (thob 50) mirando al este, que ocupa el borde oeste.
static int side_dir(const th_object *o)
{
    return o->thob == 50 && o->dir == TH_DIR_EAST ? TH_DIR_WEST : o->dir & 3;
}

int th_object_init(th_object *o, const th_rules *defs, th_rnode def, int dir)
{
    if (!def || th_rules_type(defs, def) != TH_RULES_MAP)
        return -1;
    const char *cls = th_rules_str(defs, th_rules_get(defs, def, "class"), NULL);
    const char *id = th_rules_str(defs, th_rules_get(defs, def, "id"), NULL);
    o->oclass = TH_OCLASS_OBJECT;
    if (cls && !strcmp(cls, "SideObject"))
        o->oclass = TH_OCLASS_SIDE;
    else if (cls && !strcmp(cls, "Door"))
        o->oclass = TH_OCLASS_DOOR;
    else if (cls && !strcmp(cls, "SwingDoor"))
        o->oclass = TH_OCLASS_SWING_DOOR;
    else if (cls && !strcmp(cls, "EntranceDoor"))
        o->oclass = TH_OCLASS_ENTRANCE_DOOR;
    else if (cls && !strcmp(cls, "Litter"))
        return -1; // la basura no es un Object en CorsixTH
    // is_master = object_type == object: la hoja derecha.
    o->oflags = id && (!strcmp(id, "swing_door_right") || !strcmp(id, "entrance_right_door")) ? TH_OBJF_MASTER : 0;
    o->thob = (uint8_t)th_rules_int(defs, th_rules_get(defs, def, "thob"), 0);
    o->def = def;
    o->dir = (uint8_t)(dir & 3);
    o->seq = 0;
    return 0;
}

int th_objects_first_at(const th_objects *w, int x, int y)
{
    int best = -1;
    uint32_t best_seq = 0;
    for (int i = 0; i < w->n_obj; i++) {
        const th_object *o = &w->obj[i];
        if ((o->oflags & TH_OBJF_ON_MAP) && o->x == x && o->y == y && (best < 0 || o->seq < best_seq)) {
            best = i;
            best_seq = o->seq;
        }
    }
    for (int k = 0; k < w->n_litter; k++) {
        const th_litter *l = &w->litter[k];
        if (l->x == x && l->y == y && (best == -1 || l->seq < best_seq)) {
            best = TH_OBJECTS_FIRST_LITTER;
            best_seq = l->seq;
        }
    }
    return best;
}

int th_object_walkable_tiles(const th_objects *w, int i, int *xs, int *ys)
{
    const th_object *o = &w->obj[i];
    const int x = o->x, y = o->y, west = o->dir == TH_DIR_WEST;
    int n = 0;
    switch (o->oclass) {
    case TH_OCLASS_DOOR:
        xs[0] = x;
        ys[0] = y;
        xs[1] = west ? x - 1 : x;
        ys[1] = west ? y : y - 1;
        return 2;
    case TH_OCLASS_SWING_DOOR:
        if (!(o->oflags & TH_OBJF_MASTER))
            return 0;
        // {x−1..x} × {y−1..y+1} al oeste y {x−1..x+1} × {y−1, y} al norte, en ese orden.
        for (int dy = -1; dy <= (west ? 1 : 0); dy++)
            for (int dx = -1; dx <= (west ? 0 : 1); dx++) {
                xs[n] = x + dx;
                ys[n++] = y + dy;
            }
        return n;
    case TH_OCLASS_ENTRANCE_DOOR: {
        int8_t dx[3], dy[3];
        const int k = th_map_entrance_door_tiles(o->thob, west, dx, dy);
        xs[n] = x;
        ys[n++] = y;
        for (int j = 0; j < k; j++) {
            xs[n] = x + dx[j];
            ys[n++] = y + dy[j];
        }
        return n;
    }
    default: {
        th_fp_tile fp[TH_FOOTPRINT_MAX];
        const int nf = footprint(w, o, fp);
        for (int j = 0; j < nf && n < TH_OBJECT_MAX_WALKABLE; j++)
            if (fp[j].f & TH_FP_PASSABLE) {
                xs[n] = x + fp[j].dx;
                ys[n++] = y + fp[j].dy;
            }
        return n;
    }
    }
}

typedef struct near_ctx {
    const th_objects *w;
    int x, y;
} near_ctx;

// El visitante de findAllObjectsNear: el primer objeto de la casilla
// (getObject sin tipo) y, si una de sus casillas de paso es la buscada, se acaba.
// La basura no tiene casillas de paso para ipairs (Litter:getWalkableTiles las
// pone desde el índice 0): si va primera, tapa a los objetos de su casilla.
static int near_visit(void *ctx, int tx, int ty, int dir, int dist)
{
    (void)dir;
    (void)dist;
    const near_ctx *c = ctx;
    const int o = th_objects_first_at(c->w, tx, ty);
    if (o < 0)
        return 0;
    int xs[TH_OBJECT_MAX_WALKABLE], ys[TH_OBJECT_MAX_WALKABLE];
    const int n = th_object_walkable_tiles(c->w, o, xs, ys);
    for (int k = 0; k < n; k++)
        if (xs[k] == c->x && ys[k] == c->y)
            return 1;
    return 0;
}

int th_objects_part_of_nearby(th_objects *w, int x, int y, int distance)
{
    if (!in_map(x, y))
        return 0;
    near_ctx c = {w, x, y};
    return th_path_visit_objects(w->pf, w->map, x, y, 0, distance, near_visit, &c) != 0;
}

// isTileExclusivelyPassable, sobre lo que da findAllObjectsNear: el primer
// objeto de cada casilla vista. Los objetos con huella (los que tienen
// orientaciones) cuentan por sus casillas only_passable sin shareable; los
// demás (las puertas), por sus casillas de paso. La basura y lo cogido, no.
static int exclusive_visit(void *ctx, int tx, int ty, int dir, int dist)
{
    (void)dir;
    (void)dist;
    const near_ctx *c = ctx;
    const th_objects *w = c->w;
    const int i = th_objects_first_at(w, tx, ty);
    if (i < 0)
        return 0;
    const th_object *o = &w->obj[i];
    if (o->oflags & TH_OBJF_PICKED_UP)
        return 0;
    th_fp_tile fp[TH_FOOTPRINT_MAX];
    const int nf = footprint(w, o, fp);
    if (o->oclass != TH_OCLASS_DOOR && o->oclass != TH_OCLASS_SWING_DOOR && o->oclass != TH_OCLASS_ENTRANCE_DOOR) {
        for (int k = 0; k < nf; k++)
            if (o->x + fp[k].dx == c->x && o->y + fp[k].dy == c->y && (fp[k].f & TH_FP_PASSABLE) &&
                !(fp[k].f & TH_FP_SHAREABLE))
                return 1;
        return 0;
    }
    int xs[TH_OBJECT_MAX_WALKABLE], ys[TH_OBJECT_MAX_WALKABLE];
    const int n = th_object_walkable_tiles(w, i, xs, ys);
    for (int k = 0; k < n; k++)
        if (xs[k] == c->x && ys[k] == c->y)
            return 1;
    return 0;
}

int th_objects_exclusively_passable(th_objects *w, int x, int y, int distance)
{
    if (!in_map(x, y))
        return 1;
    near_ctx c = {w, x, y};
    return th_path_visit_objects(w->pf, w->map, x, y, 0, distance, exclusive_visit, &c) == 0;
}

// La casilla del objeto y su sitio, el último, entre los objetos con ese
// origen (world.objects, cuya lista en la casilla queda abierta para siempre).
static void record_tile(th_objects *w, th_object *o, int x, int y)
{
    o->x = (uint8_t)x;
    o->y = (uint8_t)y;
    o->oflags |= TH_OBJF_ON_MAP;
    o->seq = ++*w->seq;
    th_map_open_object_list(w->map, x, y);
}

// World:addObjectToTile: lo anterior y la lista de tipos de la casilla.
static void add_to_tile(th_objects *w, th_object *o, int x, int y)
{
    record_tile(w, o, x, y);
    th_map_add_thob(w->map, x, y, o->thob);
}

// Object:occupyTilesByObjectFootprintAt.
static void occupy(th_objects *w, th_object *o, int x, int y)
{
    th_map *m = w->map;
    const int dir = side_dir(o);
    add_to_tile(w, o, x, y);
    th_fp_tile fp[TH_FOOTPRINT_MAX];
    const int n = footprint(w, o, fp);
    const int room = in_map(x, y) ? th_map_tile_c(m, x, y)->room : 0; // world:getRoom(x, y)
    int optional_found = 0;
    for (int k = 0; k < n; k++) {
        const int lx = x + fp[k].dx, ly = y + fp[k].dy;
        int change = 1;
        if (fp[k].f & TH_FP_OPTIONAL) {
            if (optional_found)
                change = 0;
            else {
                int allowed = 1;
                if (room && (!in_map(lx, ly) || th_map_tile_c(m, lx, ly)->room != room))
                    allowed = 0;
                else if ((fp[k].f & TH_FP_PASSABLE) && !th_path_reachable_from_hospital(w->pf, m, lx, ly))
                    allowed = 0;
                change = allowed;
                optional_found = allowed;
            }
        }
        if (!change)
            continue;
        if (fp[k].f & TH_FP_SIDE) {
            set_flags(m, lx, ly, 0, buildable_flag[dir]);
            // setPassableFlags: la vecina es el origen más la dirección.
            if (flags_at(m, lx, ly) & travel_flag[dir]) {
                o->oflags |= TH_OBJF_SIDE_PASSABLE;
                set_flags(m, lx, ly, 0, travel_flag[dir]);
                set_flags(m, x + dir_dx[dir], y + dir_dy[dir], 0, travel_flag[(dir + 2) & 3]);
            }
        } else {
            const uint32_t passable = (fp[k].f & TH_FP_PASSABLE) ? TH_TILE_PASSABLE : 0;
            set_flags(m, lx, ly, passable, TH_TILE_BUILDABLE | (TH_TILE_PASSABLE & ~passable));
            set_flags(m, lx, ly, 0, footprint_sides(fp, n, k));
        }
    }
}

// Object:deoccupyTilesByObjectFootprintAt: con la casilla en la que estaba.
static void deoccupy(th_objects *w, th_object *o)
{
    th_map *m = w->map;
    const int dir = side_dir(o), x = o->x, y = o->y;
    th_map_remove_thob(m, x, y, o->thob);
    o->oflags &= (uint8_t)~TH_OBJF_ON_MAP;
    th_fp_tile fp[TH_FOOTPRINT_MAX];
    const int n = footprint(w, o, fp);
    for (int k = 0; k < n; k++) {
        const int xp = x + fp[k].dx, yp = y + fp[k].dy;
        if (fp[k].f & TH_FP_SIDE) {
            if (o->oflags & TH_OBJF_SIDE_PASSABLE) {
                o->oflags &= (uint8_t)~TH_OBJF_SIDE_PASSABLE;
                set_flags(m, xp, yp, travel_flag[dir], 0);
                set_flags(m, xp + dir_dx[dir], yp + dir_dy[dir], travel_flag[(dir + 2) & 3], 0);
            }
            set_flags(m, xp, yp, buildable_flag[dir], 0);
            continue;
        }
        set_flags(m, xp, yp, footprint_sides(fp, n, k), 0);
        if (!(flags_at(m, xp, yp) & TH_TILE_PASSABLE))
            set_flags(m, xp, yp, TH_TILE_BUILDABLE | TH_TILE_PASSABLE, 0);
        else if (!th_objects_part_of_nearby(w, xp, yp, 10))
            // Las casillas de paso pueden ser de varios objetos.
            set_flags(m, xp, yp, TH_TILE_BUILDABLE, 0);
    }
}

// Object.setTile.
static void object_set_tile(th_objects *w, th_object *o, int x, int y)
{
    if (o->oflags & TH_OBJF_ON_MAP)
        deoccupy(w, o);
    if (x >= 0)
        occupy(w, o, x, y);
}

void th_object_occupy(th_objects *w, int i, int x, int y)
{
    occupy(w, &w->obj[i], x, y);
}

void th_object_deoccupy(th_objects *w, int i)
{
    if (w->obj[i].oflags & TH_OBJF_ON_MAP)
        deoccupy(w, &w->obj[i]);
}

// Door:setTile, también para las dos hojas de la puerta doble.
static void door_set_tile(th_objects *w, int i, int x, int y)
{
    th_object *o = &w->obj[i];
    th_map *m = w->map;
    const uint32_t door = o->dir == TH_DIR_WEST ? TH_TILE_DOOR_W | TH_TILE_TALL_W : TH_TILE_DOOR_N | TH_TILE_TALL_N;
    int cx[TH_OBJECT_MAX_WALKABLE], cy[TH_OBJECT_MAX_WALKABLE], cleanup = -1;
    if (o->oflags & TH_OBJF_ON_MAP) {
        cleanup = th_object_walkable_tiles(w, i, cx, cy);
        set_flags(m, o->x, o->y, 0, door);
        for (int k = 0; k < cleanup; k++)
            set_flags(m, cx[k], cy[k], 0, TH_TILE_DO_NOT_IDLE);
    }
    object_set_tile(w, o, x, y);
    for (int k = 0; k < cleanup; k++)
        if (!th_objects_part_of_nearby(w, cx[k], cy[k], 10))
            set_flags(m, cx[k], cy[k], TH_TILE_BUILDABLE, 0);
    if (x >= 0) {
        set_flags(m, x, y, door, 0);
        int nx[TH_OBJECT_MAX_WALKABLE], ny[TH_OBJECT_MAX_WALKABLE];
        const int n = th_object_walkable_tiles(w, i, nx, ny);
        for (int k = 0; k < n; k++)
            set_flags(m, nx[k], ny[k], TH_TILE_DO_NOT_IDLE, TH_TILE_BUILDABLE);
    }
    th_map_update_shadows(m);
}

// EntranceDoor:setTile: las banderas y la lista de la casilla las cambia
// th_map_entrance_door.
static void entrance_set_tile(th_objects *w, th_object *o, int x, int y)
{
    const int west = o->dir == TH_DIR_WEST;
    if (o->oflags & TH_OBJF_ON_MAP) {
        th_map_entrance_door(w->map, o->x, o->y, o->thob, west, 0);
        o->oflags &= (uint8_t)~TH_OBJF_ON_MAP;
    }
    if (x >= 0) {
        record_tile(w, o, x, y); // el tipo lo pone th_map_entrance_door
        th_map_entrance_door(w->map, x, y, o->thob, west, 1);
    }
    th_map_update_shadows(w->map);
}

void th_object_set_tile(th_objects *w, int i, int x, int y)
{
    th_object *o = &w->obj[i];
    if (x >= 0 && !in_map(x, y))
        return;
    switch (o->oclass) {
    case TH_OCLASS_DOOR:
    case TH_OCLASS_SWING_DOOR: door_set_tile(w, i, x, y); break;
    case TH_OCLASS_ENTRANCE_DOOR: entrance_set_tile(w, o, x, y); break;
    default: object_set_tile(w, o, x, y); break;
    }
}

void th_object_adopt(th_objects *w, int i, int x, int y)
{
    record_tile(w, &w->obj[i], x, y);
}

int th_object_init_map_object(th_object *o, const th_rules *defs, const th_map_object *mo)
{
    // object_id_by_thob y supports_creation_for_map.
    const th_rnode objs = th_rules_get(defs, defs->root, "objetos");
    const uint32_t n = th_rules_type(defs, objs) == TH_RULES_MAP ? th_rules_len(defs, objs) : 0;
    for (uint32_t i = 0; i < n; i++) {
        const th_rnode def = th_rules_entry_value(defs, objs, i);
        if (th_rules_int(defs, th_rules_get(defs, def, "thob"), -1) != mo->thob)
            continue;
        if (!th_rules_bool(defs, th_rules_get(defs, def, "supports_creation_for_map"), 0))
            return -1;
        return th_object_init(o, defs, def, mo->flags & 1 ? TH_DIR_WEST : TH_DIR_NORTH);
    }
    return -1;
}

void th_objects_reset_side(th_objects *w)
{
    // pairs(world.objects): solo las casillas con objetos de borde hacen algo.
    // Van por índice, de la menor a la mayor; al volver a poner un objeto en
    // la misma casilla no cambia ninguna.
    for (int last = -1;;) {
        int tile = TH_MAP_WIDTH * TH_MAP_HEIGHT;
        for (int i = 0; i < w->n_obj; i++) {
            const th_object *o = &w->obj[i];
            const int t = o->y * TH_MAP_WIDTH + o->x;
            if (o->oclass == TH_OCLASS_SIDE && (o->oflags & TH_OBJF_ON_MAP) && t > last && t < tile)
                tile = t;
        }
        if (tile == TH_MAP_WIDTH * TH_MAP_HEIGHT)
            break;
        last = tile;
        const int x = tile % TH_MAP_WIDTH, y = tile / TH_MAP_WIDTH;
        // world.objects de la casilla, en orden: los objetos (índice >= 0) y
        // la basura (-1 - índice), que no hace nada pero ocupa su sitio.
        int list[TH_OBJECT_MAX_WALKABLE], n = 0;
        uint32_t seqs[TH_OBJECT_MAX_WALKABLE];
        for (int j = 0; j < w->n_obj + w->n_litter && n < TH_OBJECT_MAX_WALKABLE; j++) {
            uint32_t sq;
            if (j < w->n_obj) {
                const th_object *o = &w->obj[j];
                if (!(o->oflags & TH_OBJF_ON_MAP) || o->x != x || o->y != y)
                    continue;
                sq = o->seq;
            } else {
                const th_litter *l = &w->litter[j - w->n_obj];
                if (l->x != x || l->y != y)
                    continue;
                sq = l->seq;
            }
            int k = n++;
            while (k > 0 && seqs[k - 1] > sq) {
                list[k] = list[k - 1];
                seqs[k] = seqs[k - 1];
                k--;
            }
            list[k] = j < w->n_obj ? j : -1 - (j - w->n_obj);
            seqs[k] = sq;
        }
        // for _, obj in ipairs(objects): setTile saca al objeto de la lista y
        // lo pone al final, así que se salta el que venía detrás y el último
        // objeto de borde puede pasar dos veces.
        for (int k = 0; k < n; k++) {
            const int i = list[k];
            if (i < 0 || w->obj[i].oclass != TH_OCLASS_SIDE)
                continue;
            object_set_tile(w, &w->obj[i], x, y);
            memmove(&list[k], &list[k + 1], (size_t)(n - 1 - k) * sizeof list[0]);
            list[n - 1] = i;
        }
    }
}
