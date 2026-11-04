// Construir (th/build.h): amueblar el pasillo como las ventanas de CorsixTH.
// Las referencias son de CorsixTH v0.70.1: dialogs/furnish_corridor.lua (fc),
// dialogs/place_objects.lua (po), entities/object.lua, entities/humanoid.lua
// y world.lua.
#include "th/build.h"

#include <string.h>

static const char *const dir_names[4] = {"north", "east", "south", "west"};
static const int8_t dir_dx[4] = {0, 1, 0, -1}, dir_dy[4] = {-1, 0, 1, 0};
static const uint32_t buildable_flag[4] = {TH_TILE_BUILDABLE_N, TH_TILE_BUILDABLE_E, TH_TILE_BUILDABLE_S,
                                           TH_TILE_BUILDABLE_W};
static const uint32_t travel_flag[4] = {TH_TILE_TRAVEL_N, TH_TILE_TRAVEL_E, TH_TILE_TRAVEL_S, TH_TILE_TRAVEL_W};

static int in_map(int x, int y)
{
    return x >= 0 && y >= 0 && x < TH_MAP_WIDTH && y < TH_MAP_HEIGHT;
}

static uint32_t flags_at(const th_sim *s, int x, int y)
{
    return th_map_tile_c(s->w.map, x, y)->flags;
}

static void set_flags(th_sim *s, int x, int y, uint32_t on, uint32_t off)
{
    if (!in_map(x, y))
        return;
    th_tile *t = th_map_tile(s->w.map, x, y);
    t->flags = (t->flags | on) & ~off;
}

static int def_thob(const th_sim *s, th_rnode def)
{
    return th_rules_int(s->defs, th_rules_get(s->defs, def, "thob"), 0);
}

static int def_is_side(const th_sim *s, th_rnode def)
{
    const char *c = th_rules_str(s->defs, th_rules_get(s->defs, def, "class"), NULL);
    return c && !strcmp(c, "SideObject");
}

static int has_orientation(const th_sim *s, th_rnode def, int dir)
{
    return th_rules_get(s->defs, th_rules_get(s->defs, def, "orientations"), dir_names[dir & 3]) != 0;
}

const char *th_build_reason_name(int reason)
{
    static const char *const names[] = {"-",           "no_es_de_pasillo", "sin_dinero",  "maximo",
                                        "sin_ventana", "sin_objeto",       "orientacion", "casillas",
                                        "bloqueo",     "persona",          "no_se_puede", "orden",
                                        "otra_ventana", "lleno",           "no_disponible", "pequena",
                                        "zonas_inaccesibles", "fase",      "posicion",    "puerta_invalida",
                                        "ventana_invalida", "no_es_de_sala", "faltan_objetos"};
    return reason >= 0 && reason < (int)(sizeof names / sizeof names[0]) ? names[reason] : "?";
}

int32_t th_build_object_cost(const th_sim *s, th_rnode def)
{
    // level_config.objects[thob].StartCost; la lista del nivel empieza en el thob 1.
    const th_rules *l = s->level;
    const int thob = def_thob(s, def);
    if (thob < 1)
        return 0;
    const th_rnode objs = th_rules_get(l, l->root, "objects");
    return th_rules_int(l, th_rules_get(l, th_rules_at(l, objs, (uint32_t)(thob - 1)), "StartCost"), 0);
}

// Hospital:spendMoney y receiveMoney con el cambio de valor del hospital.
static void money(th_sim *s, int32_t amount, int reason, th_rnode def)
{
    if (reason == TH_MONEY_BUY_OBJECT || reason == TH_MONEY_BUILD_ROOM) {
        s->hosp.balance -= amount;
        s->hosp.money_out += amount;
        s->hosp.value += amount;
        amount = -amount;
    } else {
        s->hosp.balance += amount;
        s->hosp.money_in += amount;
        s->hosp.value -= amount;
    }
    if (s->ev.money)
        s->ev.money(s->ev.ctx, s, amount, reason, (int)def);
}

// ---------------------------------------------------------------------------
// Comprobar el sitio (po:setBlueprintCell)

// World:willObjectsFootprintTileBeWithinItsAllowedRoomIfLocatedAt. *room_id
// es el parámetro (-1 = nil) y sale con el roomId que sigue.
static int within_allowed_room(const th_sim *s, int x, int y, th_rnode def, int *room_id)
{
    const int room = th_map_tile_c(s->w.map, x, y)->room;
    if (*room_id >= 0)
        return *room_id == room;
    *room_id = room;
    if (room == 0)
        return th_rules_get(s->defs, def, "corridor_object") != 0;
    // Los objetos de la sala (objects_additional y objects_needed) con el mismo thob.
    const th_room *r = NULL;
    for (int i = 0; i < s->n_rooms; i++)
        if (s->rooms[i].num == room)
            r = &s->rooms[i];
    if (!r)
        return 0;
    const th_rules *d = s->defs;
    const th_rnode info = th_rules_get(d, th_rules_get(d, d->root, "salas"), s->room_types[r->type]);
    const th_rnode objs = th_rules_get(d, d->root, "objetos");
    const int thob = def_thob(s, def);
    const th_rnode add = th_rules_get(d, info, "objects_additional");
    for (uint32_t i = 0; i < th_rules_len(d, add); i++) {
        const char *id = th_rules_str(d, th_rules_at(d, add, i), NULL);
        if (id && def_thob(s, th_rules_get(d, objs, id)) == thob)
            return 1;
    }
    const th_rnode need = th_rules_get(d, info, "objects_needed");
    for (uint32_t i = 0; i < th_rules_len(d, need); i++) {
        const char *id = th_rules_str(d, th_rules_entry_key(d, need, i), NULL);
        if (id && def_thob(s, th_rules_get(d, objs, id)) == thob)
            return 1;
    }
    return 0;
}

// World:isFootprintTileBuildableOrPassable, para el jugador 1.
static int buildable_or_passable(const th_sim *s, int x, int y, const th_fp_tile *t, const th_fp_tile *fp, int n,
                                 uint32_t requirement)
{
    const th_tile *tile = th_map_tile_c(s->w.map, x, y);
    if (!(tile->flags & requirement) || th_map_parcel_owner(s->w.map, tile->parcel) != 1)
        return 0;
    for (int d = 0; d < 4; d++) {
        const int x1 = t->dx + dir_dx[d], y1 = t->dy + dir_dy[d];
        int check = (t->f & TH_FP_COMPLETE) || (t->need & (1u << d));
        for (int k = 0; k < n && !check; k++)
            check = fp[k].dx == x1 && fp[k].dy == y1;
        if (check && !(tile->flags & buildable_flag[d]))
            return 0;
    }
    return 1;
}

// World:wouldNonSideObjectBreakPathfindingIfSpawnedAt: con la huella sólida
// bloqueada, las casillas de adjacent_to_solid_footprint de la misma sala y
// pasables tienen que seguir unidas de una en una. Copia la rareza de
// isIsolated, que deja la huella pasable para el resto del bucle.
static int would_break_pathfinding(th_sim *s, int x, int y, th_rnode def, int dir, int room_id,
                                   const th_fp_tile *fp, int n)
{
    th_map *m = s->w.map;
    uint8_t was_passable[TH_FOOTPRINT_MAX];
    for (int k = 0; k < n; k++)
        was_passable[k] = (uint8_t)((flags_at(s, x + fp[k].dx, y + fp[k].dy) & TH_TILE_PASSABLE) != 0);
#define SOLID(on)                                                                            \
    for (int k = 0; k < n; k++)                                                              \
        if (!(fp[k].f & TH_FP_PASSABLE))                                                     \
            set_flags(s, x + fp[k].dx, y + fp[k].dy, (on) ? TH_TILE_PASSABLE : 0, (on) ? 0 : TH_TILE_PASSABLE)
    SOLID(0);
    int ok = 1, have_prev = 0, px = 0, py = 0;
    const th_rules *d = s->defs;
    const th_rnode adj =
        th_rules_get(d, th_rules_get(d, th_rules_get(d, def, "orientations"), dir_names[dir & 3]), "adjacent_to_solid_footprint");
    for (uint32_t i = 0; i < th_rules_len(d, adj) && ok; i++) {
        const th_rnode a = th_rules_at(d, adj, i);
        int ax = x + th_rules_int(d, th_rules_at(d, a, 0), 0), ay = y + th_rules_int(d, th_rules_at(d, a, 1), 0);
        if (!in_map(ax, ay))
            continue;
        const th_tile *t = th_map_tile_c(m, ax, ay);
        if (t->room != room_id || !(t->flags & TH_TILE_PASSABLE))
            continue;
        if (have_prev && th_path_find(s->w.pf, m, ax, ay, px, py) < 0) {
            // isIsolated: bloquea la huella, mira y la deja pasable.
            SOLID(0);
            const int cur_isolated = !th_path_reachable_from_hospital(s->w.pf, m, ax, ay);
            SOLID(1);
            if (!cur_isolated) {
                SOLID(0);
                const int prev_isolated = !th_path_reachable_from_hospital(s->w.pf, m, px, py);
                SOLID(1);
                if (!prev_isolated) {
                    ok = 0;
                    break;
                }
            } else {
                ax = px;
                ay = py;
            }
        }
        have_prev = 1;
        px = ax;
        py = ay;
    }
#undef SOLID
    for (int k = 0; k < n; k++)
        set_flags(s, x + fp[k].dx, y + fp[k].dy, was_passable[k] ? TH_TILE_PASSABLE : 0,
                  was_passable[k] ? 0 : TH_TILE_PASSABLE);
    return !ok;
}

// --- Dentro de una sala en obra: el modo «blando» del bloqueo de zonas 2
// (po:_isNonSideObjectPlacementValid con room.is_active falso).

// Room:getEntranceXY(true): la casilla de dentro de la puerta.
static void room_entrance_inside(const th_sim *s, const th_room *r, int *x, int *y)
{
    const th_object *d = &s->obj[r->door];
    *x = d->x;
    *y = d->y;
    if (th_map_tile_c(s->w.map, d->x, d->y)->room != r->num) {
        if (d->dir == TH_DIR_NORTH)
            (*y)--;
        else if (d->dir == TH_DIR_WEST)
            (*x)--;
    }
}

static int xy_pair(const th_rules *d, th_rnode n, int *x, int *y)
{
    const int t = th_rules_type(d, n);
    if (t == TH_RULES_LIST && th_rules_len(d, n) == 2 && th_rules_type(d, th_rules_at(d, n, 0)) != TH_RULES_LIST &&
        th_rules_type(d, th_rules_at(d, n, 0)) != TH_RULES_MAP) {
        *x = th_rules_int(d, th_rules_at(d, n, 0), 0);
        *y = th_rules_int(d, th_rules_at(d, n, 1), 0);
        return 1;
    }
    if (t == TH_RULES_MAP) {
        *x = th_rules_int(d, th_rules_geti(d, n, 1), 0);
        *y = th_rules_int(d, th_rules_geti(d, n, 2), 0);
        return 1;
    }
    return 0;
}

static th_rnode orient_node(const th_sim *s, th_rnode def, int dir)
{
    return th_rules_get(s->defs, th_rules_get(s->defs, def, "orientations"), dir_names[dir & 3]);
}

// Object:getAllUsageTiles: las casillas de uso de una orientación (pares, o
// listas de pares como handyman_position).
#define USAGE_MAX 24
static int usage_tiles(const th_sim *s, th_rnode orient, int ox, int oy, int *xs, int *ys)
{
    static const char *const names[6] = {"use_position",        "use_position_secondary",
                                         "slave_position",      "finish_use_position",
                                         "finish_use_position_secondary", "handyman_position"};
    const th_rules *d = s->defs;
    int n = 0;
    for (int k = 0; k < 6 && n < USAGE_MAX; k++) {
        const th_rnode v = th_rules_get(d, orient, names[k]);
        int x, y;
        if (!v)
            continue;
        if (xy_pair(d, v, &x, &y)) {
            xs[n] = ox + x;
            ys[n++] = oy + y;
        } else if (th_rules_type(d, v) == TH_RULES_LIST)
            for (uint32_t i = 0; i < th_rules_len(d, v) && n < USAGE_MAX; i++)
                if (xy_pair(d, th_rules_at(d, v, i), &x, &y)) {
                    xs[n] = ox + x;
                    ys[n++] = oy + y;
                }
    }
    return n;
}

#define SOLID_FP(on)                                                                         \
    for (int k = 0; k < n; k++)                                                              \
        if (!(fp[k].f & TH_FP_PASSABLE))                                                     \
            set_flags(s, x + fp[k].dx, y + fp[k].dy, (on) ? TH_TILE_PASSABLE : 0, (on) ? 0 : TH_TILE_PASSABLE)
#define RESTORE_FP()                                                                         \
    for (int k = 0; k < n; k++)                                                              \
        set_flags(s, x + fp[k].dx, y + fp[k].dy, was[k] ? TH_TILE_PASSABLE : 0, was[k] ? 0 : TH_TILE_PASSABLE)

// World:wouldNonSideObjectsNotHaveAccessToRoomDoor: las casillas de uso del
// objeto nuevo llegan a la puerta con la huella cerrada, y también sin ella.
static int no_access_to_door(th_sim *s, int x, int y, th_rnode def, int dir, const th_fp_tile *fp, int n,
                             const th_room *r)
{
    int ex, ey, xs[USAGE_MAX], ys[USAGE_MAX];
    room_entrance_inside(s, r, &ex, &ey);
    const int nu = usage_tiles(s, orient_node(s, def, dir), x, y, xs, ys);
    uint8_t was[TH_FOOTPRINT_MAX];
    for (int k = 0; k < n; k++)
        was[k] = (uint8_t)((flags_at(s, x + fp[k].dx, y + fp[k].dy) & TH_TILE_PASSABLE) != 0);
    int ok = 1;
    for (int i = 0; i < nu && ok; i++) {
        SOLID_FP(0);
        ok = th_path_find(s->w.pf, s->w.map, xs[i], ys[i], ex, ey) >= 0;
        SOLID_FP(1);
        ok = ok && th_path_find(s->w.pf, s->w.map, xs[i], ys[i], ex, ey) >= 0;
    }
    RESTORE_FP();
    return !ok;
}

// World:wouldObjectBreakRoomObjectsAccessToTheRoomDoor: las casillas de uso
// de los objetos que ya hay en la sala (no los de borde) siguen llegando a la
// puerta, y ninguna es el origen del objeto nuevo. Para un objeto de borde,
// el corte del borde lo hace quien llama.
static int breaks_room_access(th_sim *s, int x, int y, const th_fp_tile *fp, int n, const th_room *r, int is_side)
{
    int ex, ey;
    room_entrance_inside(s, r, &ex, &ey);
    uint8_t was[TH_FOOTPRINT_MAX];
    for (int k = 0; k < n; k++)
        was[k] = (uint8_t)((flags_at(s, x + fp[k].dx, y + fp[k].dy) & TH_TILE_PASSABLE) != 0);
    if (!is_side)
        SOLID_FP(0);
    int ok = 1;
    for (int i = 0; i < s->n_obj && ok; i++) {
        const th_object *o = &s->obj[i];
        if (!o->def || !(o->oflags & TH_OBJF_ON_MAP) || o->room != r->num || o->oclass == TH_OCLASS_DOOR ||
            o->oclass == TH_OCLASS_SWING_DOOR || o->oclass == TH_OCLASS_ENTRANCE_DOOR || def_is_side(s, o->def))
            continue;
        int xs[USAGE_MAX], ys[USAGE_MAX];
        const int nu = usage_tiles(s, orient_node(s, o->def, o->dir), o->x, o->y, xs, ys);
        for (int u = 0; u < nu && ok; u++)
            ok = !(xs[u] == x && ys[u] == y) && th_path_find(s->w.pf, s->w.map, xs[u], ys[u], ex, ey) >= 0;
    }
    if (!is_side) {
        SOLID_FP(1);
        RESTORE_FP();
    }
    return !ok;
}
#undef SOLID_FP
#undef RESTORE_FP

// po:_isSideObjectPlacementValid: con el borde cortado por los dos lados, en
// el pasillo (o en una sala activa) la casilla tiene que llegar a la de
// enfrente si esa es pasable; en una sala en obra, los objetos de la sala
// tienen que seguir llegando a la puerta. Al acabar deja el tránsito de las
// dos a 1, no como estaba (rareza 3 de §4.4).
static int side_placement_valid(th_sim *s, int x, int y, int side, const th_room *obra)
{
    if (!(flags_at(s, x, y) & travel_flag[side]))
        return 1; // contra una pared u otro objeto de borde
    const int cx = x + dir_dx[side], cy = y + dir_dy[side];
    set_flags(s, x, y, 0, travel_flag[side]);
    set_flags(s, cx, cy, 0, travel_flag[(side + 2) & 3]);
    int invalid = 0;
    if (obra)
        invalid = breaks_room_access(s, x, y, NULL, 0, obra, 1);
    else if (th_path_find(s->w.pf, s->w.map, x, y, cx, cy) < 0 && in_map(cx, cy) &&
             (flags_at(s, cx, cy) & TH_TILE_PASSABLE))
        invalid = 1;
    set_flags(s, x, y, travel_flag[side], 0);
    set_flags(s, cx, cy, travel_flag[(side + 2) & 3], 0);
    return !invalid;
}

// La fila de la ventana que se está colocando y su objeto cogido, si lo hay
// (el existing_object con el que se abrió la fila).
static int item_existing(const th_place_item *it)
{
    return it->existing ? it->existing - 1 : -1;
}

static int check_cell(th_sim *s, const th_place_item *it, th_rnode def, int x, int y, int dir, int *bad)
{
    th_fp_tile fp[TH_FOOTPRINT_MAX];
    const int n = th_object_footprint(s->defs, def, dir, fp);
    th_objects w = th_sim_objects(s);
    const int side_obj = def_is_side(s, def);
    const int thob = def_thob(s, def);
    int optional_tiles = 0, opt_blocked = 0, valid = 1, side = -1;
    // En una obra (UIEditRoom, self.room), la huella tiene que caer en su sala.
    int room_id = s->obra.phase == TH_OBRA_OBJECTS && s->obra.room != TH_NONE ? s->rooms[s->obra.room].num : -1;
    *bad = 0;
    for (int k = 0; k < n; k++)
        optional_tiles += (fp[k].f & TH_FP_OPTIONAL) != 0;
    const int ex = it ? item_existing(it) : -1;
    const th_object *eo = ex >= 0 ? &s->obj[ex] : NULL;
    const int moving = eo && (eo->oflags & TH_OBJF_PICKED_UP) && (eo->oflags & TH_OBJF_ON_MAP);
    const int same_placement = moving && eo->x == x && eo->y == y && eo->dir == dir;
    const int ex_x = eo ? eo->x : -1, ex_y = eo ? eo->y : -1;
    if (moving && !side_obj)
        th_object_deoccupy(&w, ex); // la huella antigua no cuenta en estas comprobaciones
    int d = dir;
    for (int k = 0; k < n; k++) {
        const int xp = x + fp[k].dx, yp = y + fp[k].dy;
        int allowed = 0;
        if (!in_map(xp, yp)) {
            (*bad)++;
        } else {
            uint32_t flag = (fp[k].f & TH_FP_PASSABLE) ? TH_TILE_PASSABLE : TH_TILE_BUILDABLE;
            if (fp[k].f & TH_FP_SIDE) {
                if (thob == 50 && d == TH_DIR_EAST)
                    d = TH_DIR_WEST; // la papelera al este, como al oeste
                flag = buildable_flag[d];
                side = d;
            }
            allowed = within_allowed_room(s, xp, yp, def, &room_id);
            if (!(fp[k].f & TH_FP_SIDE) && allowed)
                allowed = buildable_or_passable(s, xp, yp, &fp[k], fp, n, flag);
            else if (allowed)
                allowed = ((flags_at(s, xp, yp) & flag) || same_placement) &&
                          th_map_parcel_owner(s->w.map, th_map_tile_c(s->w.map, xp, yp)->parcel) == 1;
            if (!(fp[k].f & TH_FP_SHAREABLE) && allowed) {
                // Una sola cosa por casilla, salvo basura (62) y ratoneras (64).
                const int first = th_map_thob_at(s->w.map, xp, yp, 0);
                const int free_tile = first == 0 || first == 62 || first == 64;
                const int same_place = eo && ex_x == xp && ex_y == yp;
                allowed = th_objects_exclusively_passable(&w, xp, yp, 10) && (same_place || free_tile);
            }
            if (!allowed && !(fp[k].f & TH_FP_INVISIBLE))
                (*bad)++;
        }
        if (!allowed) {
            if (fp[k].f & TH_FP_OPTIONAL) {
                if (++opt_blocked >= optional_tiles)
                    valid = 0;
            } else
                valid = 0;
        }
    }
    if (moving && !side_obj)
        th_object_occupy(&w, ex, ex_x, ex_y);
    if (!valid)
        return 0;
    if (room_id > 0) {
        // En una sala: hace falta que tenga puerta en el mapa. Si está activa,
        // el modo estricto, como en el pasillo; en obra, el blando del modo 2:
        // el acceso a la puerta de este objeto y de los que ya hay en la sala.
        const th_room *r = th_sim_room((th_sim *)s, room_id);
        if (!r || r->door == TH_NONE || !(s->obj[r->door].oflags & TH_OBJF_ON_MAP))
            return 0;
        if (!r->is_active) {
            if (!side_obj)
                return !no_access_to_door(s, x, y, def, dir, fp, n, r) && !breaks_room_access(s, x, y, fp, n, r, 0);
            return side >= 0 ? side_placement_valid(s, x, y, side, r) : 1;
        }
    }
    if (!side_obj)
        return !would_break_pathfinding(s, x, y, def, dir, room_id < 0 ? 0 : room_id, fp, n);
    return side >= 0 ? side_placement_valid(s, x, y, side, NULL) : 1;
}

int th_build_check_cell(th_sim *s, th_rnode def, int x, int y, int dir, int *bad)
{
    const th_place_window *p = &s->place;
    const th_place_item *it = NULL;
    for (int k = 0; k < p->n; k++)
        if (p->item[k].def == def)
            it = &p->item[k];
    return check_cell(s, it, def, x, y, dir, bad);
}

// World:anyHumanoidObscuringArea y Humanoid:isObscuringArea: alguien en la
// casilla, o en una vecina caminando hacia ella. Un caminar sin camino (o a
// su propia casilla) no tiene path_x y tapa: es el que no llegó a empezar
// (sin TH_AS_STARTED, que walk_start pone tras guardar el camino y conserva
// al reiniciar, como path_x); path_len y cur son del humanoide, no de la
// acción, y guardan los del paseo anterior.
static int humanoid_obscuring(const th_sim *s, int x, int y)
{
    for (int i = 0; i < TH_SIM_HUMANOIDS; i++) {
        const th_person *p = &s->hum[i];
        if (!p->alive)
            continue;
        const th_humanoid *h = &p->h;
        const int dx = h->tile_x - x, dy = h->tile_y - y;
        if (h->tile_x < 0 || (dx != 0 && dy != 0) || dx < -1 || dx > 1 || dy < -1 || dy > 1)
            continue;
        if (dx == 0 && dy == 0)
            return 1;
        if (!h->queue_len || h->queue[0].kind != TH_ACTION_WALK)
            continue;
        if (!(h->queue[0].state & TH_AS_STARTED) || !h->path_len || (h->cur_x == x && h->cur_y == y))
            return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// La ventana de colocar (UIPlaceObjects)

static void prepare_footprint(th_sim *s, th_rnode def, int dir, int x, int y)
{
    // World:prepareFootprintTilesForBuild: la basura de las casillas sólidas
    // (y las ratoneras, que el motor aún no tiene).
    th_fp_tile fp[TH_FOOTPRINT_MAX];
    const int n = th_object_footprint(s->defs, def, dir, fp);
    for (int k = 0; k < n; k++)
        if ((fp[k].f & TH_FP_COMPLETE) || !(fp[k].f & TH_FP_PASSABLE))
            th_sim_remove_litter_at(s, x + fp[k].dx, y + fp[k].dy);
}

// po:placeObject: el objeto cogido de la fila activa, si lo hay, o uno nuevo.
// Devuelve su índice.
static int place_object(th_sim *s, int x, int y, int dont_close, int dont_remove);
static void remove_object(th_sim *s, int dont_close, int placed, int move_canceled);

static void close_window(th_sim *s, int move_canceled)
{
    // po:removeAllObjects: tantas pasadas como filas, y en cada una todas las
    // unidades de la primera fila. Una fila agotada (la última colocada, que
    // cierra la ventana) se queda y no hace nada.
    th_place_window *p = &s->place;
    const int rows = p->n;
    for (int e = 0; e < rows && p->n > 0; e++) {
        const int units = p->item[0].qty;
        for (int u = 0; u < units && p->n > 0; u++) {
            p->active = 0;
            remove_object(s, 1, 0, move_canceled);
        }
    }
    memset(p, 0, sizeof *p);
}

static void remove_item(th_place_window *p, int k)
{
    memmove(&p->item[k], &p->item[k + 1], (size_t)(p->n - 1 - k) * sizeof p->item[0]);
    p->n--;
    p->active = 0;
}

// po:removeObject de una unidad de la fila activa.
static void remove_object(th_sim *s, int dont_close, int placed, int move_canceled)
{
    th_place_window *p = &s->place;
    th_place_item *it = &p->item[p->active];
    const int existing = item_existing(it);
    // Lo que decide la devolución es el existing_object de la fila (picked):
    // en una obra, el objeto cogido ya está destruido y existing no lo tiene.
    const int move_cancellation = !placed && existing >= 0 && move_canceled;
    const int purchase_cancellation = !placed && !it->picked;
    const int sale_previously_placed = !placed && it->picked && !move_canceled;
    if (purchase_cancellation || sale_previously_placed)
        money(s, th_build_object_cost(s, it->def), TH_MONEY_SELL_OBJECT, it->def);
    if (move_cancellation) {
        // Vuelve a su casilla y orientación, sin comprobar nada.
        const th_object *o = &s->obj[existing];
        p->orient = o->dir;
        place_object(s, o->x, o->y, 0, 1);
    }
    if (sale_previously_placed && existing >= 0) {
        th_sim_destroy_object(s, existing);
        th_sim_map_changed(s);
    }
    it = &p->item[p->active];
    it->qty--;
    if (it->n_existing > 0 && it->qty < it->n_existing) {
        memmove(&it->existing_list[0], &it->existing_list[1], (size_t)(it->n_existing - 1));
        it->n_existing--;
    }
    if (it->qty == 0) {
        if (p->n == 1 && !dont_close) {
            close_window(s, 0);
            return;
        }
        remove_item(p, p->active);
    }
}

static int place_object(th_sim *s, int x, int y, int dont_close, int dont_remove)
{
    th_place_window *p = &s->place;
    th_place_item *it = &p->item[p->active];
    const th_rnode def = it->def;
    th_objects w = th_sim_objects(s);
    int obj = TH_NONE;
    if (it->n_existing > 0) {
        obj = it->existing_list[0];
        memmove(&it->existing_list[0], &it->existing_list[1], (size_t)(it->n_existing - 1));
        it->n_existing--;
    }
    if (obj != TH_NONE) {
        th_object *o = &s->obj[obj];
        if ((o->oflags & TH_OBJF_PICKED_UP) && (o->oflags & TH_OBJF_ON_MAP)) {
            // Cogido en el pasillo: estaba en el mapa, invisible.
            th_object_set_tile(&w, obj, -1, -1);
            if (o->oclass == TH_OCLASS_SIDE) { // rebuildPassableCellFlags
                th_map_update_pathfinding(s->w.map);
                th_objects_reset_side(&w);
            }
        }
        o->oflags &= (uint8_t)~TH_OBJF_PICKED_UP;
        o->dir = p->orient; // initOrientation si se giró
        prepare_footprint(s, def, o->dir, x, y);
        w = th_sim_objects(s);
        th_object_set_tile(&w, obj, x, y);
        if (o->type == TH_OBJ_RECEPTION_DESK)
            th_sim_desk_check_for_staff(s, obj); // ReceptionDesk:setTile
    } else {
        prepare_footprint(s, def, p->orient, x, y);
        obj = th_sim_new_object(s, def, x, y, p->orient);
        if (obj == TH_NONE)
            return TH_NONE; // no cabe: order_colocar lo mira antes (th_sim_object_fits)
    }
    // La sala del objeto es la de su casilla (Entity:getRoom), también 0 al
    // sacarlo de una sala. room.objects de la sala vieja, que CorsixTH no
    // limpia al mover, es otra cosa (paso 3).
    s->obj[obj].room = (uint8_t)th_map_tile_c(s->w.map, x, y)->room;
    th_sim_map_changed(s);
    if (!dont_remove)
        remove_object(s, dont_close, 1, 0);
    return obj;
}

// po:addObjects en una ventana nueva: una fila. Los objetos cogidos entran en
// existing_objects.
static void open_window(th_sim *s, th_rnode def, int qty, int existing)
{
    th_place_window *p = &s->place;
    memset(p, 0, sizeof *p);
    p->open = 1;
    p->n = 1;
    p->item[0].def = def;
    p->item[0].qty = (uint8_t)qty;
    if (existing >= 0) {
        p->item[0].existing = (uint8_t)(existing + 1);
        p->item[0].picked = 1;
        p->item[0].n_existing = 1;
        p->item[0].existing_list[0] = (uint8_t)existing;
    }
    // setActiveIndex(1): la primera orientación que tenga, desde el norte.
    p->active = 0;
    p->orient = TH_DIR_NORTH;
    for (int d = 0; d < 4; d++)
        if (has_orientation(s, def, d)) {
            p->orient = (uint8_t)d;
            break;
        }
}

// ---------------------------------------------------------------------------
// Las órdenes

static int find_item(const th_place_window *p, th_rnode def)
{
    for (int k = 0; k < p->n; k++)
        if (p->item[k].def == def && p->item[k].qty > 0)
            return k;
    return -1;
}

static void obra_verify_or_abort(th_sim *s);
static int order_recoger_obra(th_sim *s, const th_order *o);

static int order_comprar(th_sim *s, const th_order *o)
{
    // UIFurnishCorridor es una ventana «main»: abrirla cierra la de colocar
    // (UI:addWindow), que devuelve lo que no se colocó. Antes, el botón del
    // panel cierra la obra (GameUI:setEditRoom(false): verifyOrAbortRoom).
    if (s->obra.phase != TH_OBRA_NONE)
        obra_verify_or_abort(s);
    if (s->place.open)
        close_window(s, 0);
    if (!o->def || !th_rules_get(s->defs, o->def, "corridor_object"))
        return TH_REASON_NO_ES_DE_PASILLO;
    // Cada unidad es una pulsación de «+» (purchaseItem(i, 1)), que no cobra:
    // falla si el saldo no llega al total o, en la centésima, porque ya hay 99.
    // Si falla una, se cierra la ventana y no se compra nada. Sin pulsaciones
    // no se mira el saldo: confirm sin nada que comprar solo cierra.
    const int qty = o->qty;
    if (qty <= 0)
        return TH_REASON_OK;
    const int32_t price = th_build_object_cost(s, o->def);
    if (s->hosp.balance < (int64_t)(qty < 99 ? qty : 99) * price)
        return TH_REASON_SIN_DINERO;
    if (qty > 99)
        return TH_REASON_MAXIMO;
    // confirm: cobra y abre la ventana de colocar.
    money(s, qty * price, TH_MONEY_BUY_OBJECT, o->def);
    open_window(s, o->def, qty, -1);
    return TH_REASON_OK;
}

static int order_colocar(th_sim *s, const th_order *o, th_order_result *r)
{
    th_place_window *p = &s->place;
    if (s->obra.phase != TH_OBRA_NONE && s->obra.phase != TH_OBRA_OBJECTS)
        return TH_REASON_FASE; // la obra no está colocando objetos
    if (!p->open)
        return TH_REASON_SIN_VENTANA;
    const int k = find_item(p, o->def);
    if (k < 0)
        return TH_REASON_SIN_OBJETO;
    if (!has_orientation(s, o->def, o->dir))
        return TH_REASON_ORIENTACION;
    p->active = (uint8_t)k;
    p->orient = (uint8_t)(o->dir & 3);
    int bad = 0;
    if (!check_cell(s, &p->item[k], o->def, o->x, o->y, p->orient, &bad)) {
        if (bad > 0) {
            r->bad = (uint8_t)(bad > 255 ? 255 : bad);
            return TH_REASON_CASILLAS;
        }
        return TH_REASON_BLOQUEO;
    }
    if (humanoid_obscuring(s, o->x, o->y))
        return TH_REASON_PERSONA;
    // El tope del motor, donde CorsixTH ya aceptaría: uno nuevo que no cabe
    // se rechaza sin tocar nada (ni la basura ni la unidad pagada).
    if (p->item[k].n_existing == 0 && !th_sim_object_fits(s, o->def))
        return TH_REASON_LLENO;
    // En una obra, UIEditRoom:placeObject no cierra la ventana al colocar el último.
    place_object(s, o->x, o->y, s->obra.phase == TH_OBRA_OBJECTS, 0);
    return TH_REASON_OK;
}

static int order_recoger(th_sim *s, const th_order *o)
{
    if (s->obra.phase != TH_OBRA_NONE)
        return order_recoger_obra(s, o);
    // Con la ventana de colocar abierta no hay prueba del mundo
    // (setWorldHitTest(false)) y el clic derecho gira el plano.
    if (s->place.open)
        return TH_REASON_OTRA_VENTANA;
    // El objeto con ese origen que va el último en world.entities (el de
    // mayor entity_seq: el índice no vale, los huecos se reutilizan).
    int obj = -1;
    for (int i = 0; i < s->n_obj; i++) {
        const th_object *ob = &s->obj[i];
        if (ob->def && (ob->oflags & TH_OBJF_ON_MAP) && ob->x == o->x && ob->y == o->y &&
            (obj < 0 || ob->entity_seq > s->obj[obj].entity_seq))
            obj = i;
    }
    if (obj < 0)
        return TH_REASON_SIN_OBJETO;
    th_object *ob = &s->obj[obj];
    // Object:onClick: las puertas no se cogen (Door:onClick); fuera de una
    // sala, solo los objetos de pasillo.
    const int room = th_map_tile_c(s->w.map, ob->x, ob->y)->room;
    if (ob->oclass == TH_OCLASS_DOOR || ob->oclass == TH_OCLASS_SWING_DOOR ||
        !th_rules_get(s->defs, ob->def, "corridor_object"))
        return TH_REASON_NO_SE_PUEDE;
    (void)room; // con salas (paso 3): solo los de pasillo, o los de la sala en edición
    // picked_up, onPickUp (TODO: handleRemovedObject con gente), invisible y
    // una ventana nueva, que al abrirse cierra la que hubiera.
    ob->oflags |= TH_OBJF_PICKED_UP;
    if (s->place.open)
        close_window(s, 0);
    open_window(s, ob->def, 1, obj);
    s->place.orient = ob->dir; // window:setOrientation(initial_orientation)
    return TH_REASON_OK;
}

// ---------------------------------------------------------------------------
// Las salas rectangulares (UIEditRoom, dialogs/edit_room.lua: er)

static th_rnode room_def(const th_sim *s, int type)
{
    const th_rules *d = s->defs;
    return th_rules_get(d, th_rules_get(d, d->root, "salas"), s->room_types[type]);
}

static th_rnode def_by_id(const th_sim *s, const char *id)
{
    return th_rules_get(s->defs, th_rules_get(s->defs, s->defs->root, "objetos"), id);
}

static const char *def_id(const th_sim *s, th_rnode def)
{
    return th_rules_str(s->defs, th_rules_get(s->defs, def, "id"), NULL);
}

static th_rnode level_object(const th_sim *s, int thob)
{
    const th_rules *l = s->level;
    return th_rules_at(l, th_rules_get(l, l->root, "objects"), (uint32_t)(thob - 1));
}

static int room_type_of(const th_sim *s, th_rnode def)
{
    for (int i = 0; i < s->n_room_types; i++)
        if (def && room_def(s, i) == def)
            return i;
    return -1;
}

// World:getAvailableRooms: el precio de la sala en el nivel (rooms[id].Cost)
// más el de sus objetos obligatorios.
int32_t th_build_room_cost(const th_sim *s, int type)
{
    const th_rules *d = s->defs, *l = s->level;
    const th_rnode rd = room_def(s, type);
    const int cfg = th_rules_int(d, th_rules_get(d, rd, "level_config_id"), 0);
    int32_t cost = th_rules_int(l, th_rules_get(l, th_rules_geti(l, th_rules_get(l, l->root, "rooms"), cfg), "Cost"), 0);
    const th_rnode need = th_rules_get(d, rd, "objects_needed");
    for (uint32_t i = 0; i < th_rules_len(d, need); i++) {
        const char *id = th_rules_str(d, th_rules_entry_key(d, need, i), NULL);
        cost += th_rules_int(d, th_rules_entry_value(d, need, i), 0) * th_build_object_cost(s, def_by_id(s, id ? id : ""));
    }
    return cost;
}

// World:getAvailableRooms: descubierta si todos sus obligatorios están
// disponibles desde el principio del nivel (AvailableForLevel y StartAvail).
static int room_discovered(const th_sim *s, int type)
{
    const th_rules *d = s->defs, *l = s->level;
    const th_rnode need = th_rules_get(d, room_def(s, type), "objects_needed");
    for (uint32_t i = 0; i < th_rules_len(d, need); i++) {
        const char *id = th_rules_str(d, th_rules_entry_key(d, need, i), NULL);
        const th_rnode spec = level_object(s, def_thob(s, def_by_id(s, id ? id : "")));
        if (!th_rules_int(l, th_rules_get(l, spec, "AvailableForLevel"), 1) ||
            !th_rules_int(l, th_rules_get(l, spec, "StartAvail"), 1))
            return 0;
    }
    return 1;
}

static int room_min_size(const th_sim *s, int type)
{
    return th_rules_int(s->defs, th_rules_get(s->defs, room_def(s, type), "minimum_size"), 1);
}

static int room_swing_doors(const th_sim *s, int type)
{
    return th_rules_bool(s->defs, th_rules_get(s->defs, room_def(s, type), "swing_doors"), 0);
}

// Los juegos de paredes (defs.paredes, walls/*.lua): bloques por juego, grupo
// y nombre, y la búsqueda inversa de World:initWallTypes (sin las batientes).
static th_rnode wall_set(const th_sim *s, const char *id)
{
    return th_rules_get(s->defs, th_rules_get(s->defs, s->defs->root, "paredes"), id ? id : "");
}

static int wall_tile(const th_sim *s, th_rnode set, const char *group, const char *name)
{
    return th_rules_int(s->defs, th_rules_get(s->defs, th_rules_get(s->defs, set, group), name), 0);
}

typedef struct wall_info {
    const char *id, *group, *name;
    th_rnode set;
} wall_info;

static int wall_lookup(const th_sim *s, int block, wall_info *wi)
{
    static const char *const groups[3] = {"inside_tiles", "outside_tiles", "window_tiles"};
    const th_rules *d = s->defs;
    const th_rnode sets = th_rules_get(d, d->root, "paredes");
    if (!block)
        return 0;
    for (uint32_t i = 0; i < th_rules_len(d, sets); i++) {
        const th_rnode set = th_rules_entry_value(d, sets, i);
        for (int g = 0; g < 3; g++) {
            const th_rnode grp = th_rules_get(d, set, groups[g]);
            for (uint32_t k = 0; k < th_rules_len(d, grp); k++)
                if (th_rules_int(d, th_rules_entry_value(d, grp, k), -1) == block) {
                    wi->id = th_rules_str(d, th_rules_entry_key(d, sets, i), NULL);
                    wi->group = groups[g];
                    wi->name = th_rules_str(d, th_rules_entry_key(d, grp, k), NULL);
                    wi->set = set;
                    return 1;
                }
        }
    }
    return 0;
}

// Las capas de pared: wall_n es la capa 2 de CorsixTH y wall_w la 3.
static int wall_block(const th_sim *s, int x, int y, int west)
{
    if (!in_map(x, y))
        return 0;
    const th_tile *t = th_map_tile_c(s->w.map, x, y);
    return west ? t->wall_w : t->wall_n;
}

static void set_wall(th_sim *s, int x, int y, int west, int block)
{
    if (!in_map(x, y))
        return;
    th_tile *t = th_map_tile(s->w.map, x, y);
    if (west)
        t->wall_w = (uint8_t)block;
    else
        t->wall_n = (uint8_t)block;
}

// --- El plano

// updateRoomBlueprint (th_lua_map.cpp:232-236): no es de una sala, se puede
// edificar y la parcela es del jugador 1.
static int blueprint_tile_ok(const th_sim *s, int x, int y)
{
    const th_tile *t = th_map_tile_c(s->w.map, x, y);
    return !(t->flags & TH_TILE_ROOM) && (t->flags & TH_TILE_BUILDABLE) && th_map_parcel_owner(s->w.map, t->parcel) == 1;
}

// er:checkReachability con el plano ya cerrado (enterDoorPhase): las casillas
// del anillo, sin las esquinas, que son transitables y tienen paso hacia la
// sala tienen que llegar cada una a la anterior.
static int reachability(th_sim *s, const th_obra *b)
{
    int px = -1, py = -1;
    const int x = b->x, y = b->y, w = b->w, h = b->h;
    for (int k = 0; k < 2 * (w + h); k++) {
        int cx, cy;
        uint32_t flag;
        if (k < w) { // la fila de arriba, de oeste a este
            cx = x + k;
            cy = y - 1;
            flag = TH_TILE_TRAVEL_S;
        } else if (k < w + h) { // la columna del este, de norte a sur
            cx = x + w;
            cy = y + (k - w);
            flag = TH_TILE_TRAVEL_W;
        } else if (k < 2 * w + h) { // la fila de abajo, de este a oeste
            cx = x + w - 1 - (k - w - h);
            cy = y + h;
            flag = TH_TILE_TRAVEL_N;
        } else { // la columna del oeste, de sur a norte
            cx = x - 1;
            cy = y + h - 1 - (k - 2 * w - h);
            flag = TH_TILE_TRAVEL_E;
        }
        if (!in_map(cx, cy))
            continue;
        const uint32_t f = flags_at(s, cx, cy);
        if (!(f & TH_TILE_PASSABLE) || !(f & flag))
            continue;
        if (px >= 0 && th_path_find(s->w.pf, s->w.map, px, py, cx, cy) < 0)
            return 0;
        px = cx;
        py = cy;
    }
    return 1;
}

// --- La puerta y las ventanas

// doorWallOffsetCalculations: de la casilla de dentro y su lado a la casilla
// cuya capa de pared es el tramo (west = 1, la capa oeste).
static void edge_wall(const th_edge *e, int *wx, int *wy, int *west)
{
    *wx = e->x;
    *wy = e->y;
    *west = e->side == TH_DIR_WEST || e->side == TH_DIR_EAST;
    if (e->side == TH_DIR_SOUTH)
        *wy = e->y + 1;
    else if (e->side == TH_DIR_EAST)
        *wx = e->x + 1;
}

// er:screenToWall: los bordes que da la interfaz: sin esquinas; la puerta
// doble, desde rx+3 al norte, rx+2 al sur y ry+2 al oeste y al este.
static int edge_position_ok(const th_obra *b, int x, int y, int side, int swing)
{
    const int rx = b->x, ry = b->y, w = b->w, h = b->h;
    switch (side) {
    case TH_DIR_NORTH: return y == ry && x >= rx + (swing ? 3 : 1) && x <= rx + w - 2;
    case TH_DIR_SOUTH: return y == ry + h - 1 && x >= rx + (swing ? 2 : 1) && x <= rx + w - 2;
    case TH_DIR_WEST: return x == rx && y >= ry + (swing ? 2 : 1) && y <= ry + h - 2;
    case TH_DIR_EAST: return x == rx + w - 1 && y >= ry + (swing ? 2 : 1) && y <= ry + h - 2;
    default: return 0;
    }
}

// La etiqueta del tramo (x, y, capa) en el plano: la puerta (o las tres
// casillas de la doble: la esclava en la coordenada menor, la maestra en el
// centro y «nothing» en la mayor) y las ventanas.
enum { TAG_NONE, TAG_DOOR, TAG_SWING_SLAVE, TAG_SWING_MASTER, TAG_NOTHING, TAG_WINDOW };
static int edge_tag(const th_sim *s, const th_obra *b, int wx, int wy, int west)
{
    if (b->has_door) {
        int dx, dy, dw;
        edge_wall(&b->door, &dx, &dy, &dw);
        if (dw == west) {
            if (room_swing_doors(s, b->type)) {
                for (int i = 1; i <= 3; i++) {
                    const int px = dw ? dx : dx + i - 2, py = dw ? dy + i - 2 : dy;
                    if (px == wx && py == wy)
                        return i == 1 ? TAG_SWING_SLAVE : i == 2 ? TAG_SWING_MASTER : TAG_NOTHING;
                }
            } else if (dx == wx && dy == wy)
                return TAG_DOOR;
        }
    }
    for (int k = 0; k < b->n_windows; k++) {
        int vx, vy, vw;
        edge_wall(&b->windows[k], &vx, &vy, &vw);
        if (vw == west && vx == wx && vy == wy)
            return TAG_WINDOW;
    }
    return TAG_NONE;
}

// validDoorTile: del jugador, sin objeto delante (salvo basura, 62; solo
// cuenta el primero de la lista) y, si es transitable, sin uso exclusivo.
static int valid_door_tile(th_sim *s, int x, int y)
{
    if (!in_map(x, y))
        return 0;
    const th_tile *t = th_map_tile_c(s->w.map, x, y);
    if (th_map_parcel_owner(s->w.map, t->parcel) != 1)
        return 0;
    const int thob = th_map_thob_at(s->w.map, x, y, 0);
    if (thob != 0 && thob != 62)
        return 0;
    if (t->flags & TH_TILE_PASSABLE) {
        th_objects w = th_sim_objects(s);
        return th_objects_exclusively_passable(&w, x, y, 1);
    }
    return 1;
}

// er:setDoorBlueprint: checkDoorWalls (D1 y D3) y validDoorTile (D2).
static int door_valid(th_sim *s, const th_obra *b, const th_edge *e)
{
    int x, y, west;
    edge_wall(e, &x, &y, &west);
    const int swing = room_swing_doors(s, b->type);
    const int dx = west ? 0 : 1, dy = west ? 1 : 0;
    if (wall_block(s, x, y, west))
        return 0;
    if (swing && (wall_block(s, x - dx, y - dy, west) || wall_block(s, x + dx, y + dy, west)))
        return 0;
    const int x2 = west ? x - 1 : x, y2 = west ? y : y - 1;
    if (!valid_door_tile(s, x, y) || !valid_door_tile(s, x2, y2))
        return 0;
    if (swing) {
        if (!valid_door_tile(s, x + dx, y + dy) || !valid_door_tile(s, x2 + dx, y2 + dy))
            return 0;
        if (!valid_door_tile(s, x - dx, y - dy) || !valid_door_tile(s, x2 - dx, y2 - dy))
            return 0;
    }
    return 1;
}

// er:setWindowBlueprint: sin etiqueta en el tramo (W1) y sin un bloque de
// pared conocido en la capa (W2).
static int window_valid(th_sim *s, const th_obra *b, const th_edge *e)
{
    int x, y, west;
    edge_wall(e, &x, &y, &west);
    if (edge_tag(s, b, x, y, west) != TAG_NONE)
        return 0;
    wall_info wi;
    return !wall_lookup(s, wall_block(s, x, y, west), &wi);
}

// --- Terminar la sala (er:finishRoom)

// check_external_window: una pared nueva que toca una ventana de la fachada
// la cambia por pared lisa, o parte la ventana larga.
static void external_window_check(th_sim *s, const th_obra *b, int x, int y, int west)
{
    wall_info wi;
    if (!wall_lookup(s, wall_block(s, x, y, west), &wi) || !wi.name)
        return;
    const th_rnode ext = wall_set(s, "external");
    const th_rnode tiles = th_rules_get(s->defs, ext, wi.group);
#define T(name) th_rules_int(s->defs, th_rules_get(s->defs, tiles, name), 0)
    const int rx = b->x, ry = b->y;
    if (!strcmp(wi.name, "north_window_1")) {
        if (x != rx) {
            set_wall(s, x, y, west, T("north"));
            if (wall_block(s, x + 1, y, west))
                set_wall(s, x + 1, y, west, T("north"));
        }
    } else if (!strcmp(wi.name, "north_window_2")) {
        if (x == rx) {
            if (wall_block(s, x - 1, y, west))
                set_wall(s, x - 1, y, west, T("north"));
            set_wall(s, x, y, west, T("north"));
        }
    } else if (!strcmp(wi.name, "west_window_1")) {
        if (y == ry) {
            set_wall(s, x, y, west, T("west"));
            if (wall_block(s, x, y - 1, west))
                set_wall(s, x, y - 1, west, T("west"));
        }
    } else if (!strcmp(wi.name, "west_window_2")) {
        if (y != ry) {
            if (wall_block(s, x, y + 1, west))
                set_wall(s, x, y + 1, west, T("west"));
            set_wall(s, x, y, west, T("west"));
        }
    } else if (!strcmp(wi.name, "north_window_long")) {
        if (x == rx) {
            set_wall(s, x, y, west, T("north_window_1"));
            const int l = wall_block(s, x - 1, y, west);
            if (l && l != T("north_window_2")) {
                const int ll = wall_block(s, x - 2, y, west);
                set_wall(s, x - 1, y, west,
                         ll == T("north_window_long") || ll == T("north_window_1") ? T("north_window_2") : T("north"));
            }
        } else {
            set_wall(s, x, y, west, T("north_window_2"));
            const int r = wall_block(s, x + 1, y, west);
            if (r && r != T("north_window_1")) {
                const int rr = wall_block(s, x + 2, y, west);
                set_wall(s, x + 1, y, west,
                         rr == T("north_window_long") || rr == T("north_window_2") ? T("north_window_1") : T("north"));
            }
        }
    } else if (!strcmp(wi.name, "west_window_long")) {
        if (y == ry) {
            set_wall(s, x, y, west, T("west_window_2"));
            const int u = wall_block(s, x, y - 1, west);
            if (u && u != T("west_window_1")) {
                const int uu = wall_block(s, x, y - 2, west);
                set_wall(s, x, y - 1, west,
                         uu == T("west_window_long") || uu == T("west_window_2") ? T("west_window_1") : T("west"));
            }
        } else {
            set_wall(s, x, y, west, T("west_window_1"));
            const int d = wall_block(s, x, y + 1, west);
            if (d && d != T("west_window_2")) {
                const int dd = wall_block(s, x, y + 2, west);
                set_wall(s, x, y + 1, west,
                         dd == T("west_window_long") || dd == T("west_window_1") ? T("west_window_2") : T("west"));
            }
        }
    }
#undef T
}

static const char *swing_name(int west, int left)
{
    return west ? (left ? "west_left" : "west_right") : (left ? "north_left" : "north_right");
}

// Un tramo del plano que no es la esquina (x, y): la pared que le toca, o la
// puerta como objeto.
static void wall_piece(th_sim *s, const th_obra *b, th_rnode ws, int x, int y, int west, int *door, int *door2)
{
    const int rx = b->x, ry = b->y, w = b->w, h = b->h;
    const int tag = edge_tag(s, b, x, y, west);
    const char *dir = west ? "west" : "north";
    const int dir_obj = west ? TH_DIR_WEST : TH_DIR_NORTH;
    wall_info wi;
    const int known = wall_lookup(s, wall_block(s, x, y, west), &wi);
    if (known && !strcmp(wi.id, "external")) {
        // Se deja; en las esquinas, se mira si toca una ventana de la fachada.
        if (!west ? (x == rx || x == rx + w - 1) && (y == ry || y == ry + h)
                  : (x == rx || x == rx + w) && (y == ry || y == ry + h - 1))
            external_window_check(s, b, x, y, west);
        return;
    }
    const int swing_tag = tag == TAG_NOTHING || tag == TAG_SWING_SLAVE;
    const char *tiles = swing_tag ? "swing_outside_tiles" : "outside_tiles";
    if (rx <= x && x < rx + w && ry <= y && y < ry + h)
        tiles = swing_tag ? "swing_inside_tiles" : "inside_tiles";
    else if (known)
        return; // fuera: la pared del vecino se deja
    if (tag == TAG_WINDOW || (known && !strcmp(wi.group, "window_tiles")))
        tiles = "window_tiles";
    if (tag == TAG_DOOR)
        *door = th_sim_new_object(s, def_by_id(s, "door"), x, y, dir_obj);
    else if (tag == TAG_SWING_MASTER)
        *door = th_sim_new_object(s, def_by_id(s, "swing_door_right"), x, y, dir_obj);
    else if (tag == TAG_SWING_SLAVE) {
        *door2 = th_sim_new_object(s, def_by_id(s, "swing_door_left"), x, y, dir_obj);
        set_wall(s, x, y, west, wall_tile(s, ws, tiles, swing_name(west, !west))); // norte: _left; oeste: _right
    } else if (tag == TAG_NOTHING)
        set_wall(s, x, y, west, wall_tile(s, ws, tiles, swing_name(west, west))); // norte: _right; oeste: _left
    else
        set_wall(s, x, y, west, wall_tile(s, ws, tiles, dir));
}

static void room_walls(th_sim *s, const th_obra *b, int *door, int *door2)
{
    const int rx = b->x, ry = b->y, w = b->w, h = b->h;
    const th_rnode rd = room_def(s, b->type);
    const th_rnode ws = wall_set(s, th_rules_str(s->defs, th_rules_get(s->defs, rd, "wall_type"), NULL));
    *door = *door2 = TH_NONE;
    // La esquina (rx, ry), con sus dos capas.
    for (int west = 0; west < 2; west++) {
        wall_info wi;
        const int known = wall_lookup(s, wall_block(s, rx, ry, west), &wi);
        if (known && !strcmp(wi.id, "external"))
            external_window_check(s, b, rx, ry, west);
        else if (known && !strcmp(wi.group, "window_tiles"))
            set_wall(s, rx, ry, west, wall_tile(s, ws, "window_tiles", west ? "west" : "north"));
        else
            set_wall(s, rx, ry, west, wall_tile(s, ws, "inside_tiles", west ? "west" : "north"));
    }
    // Norte (rx+1..rx+w-1, ry), sur (rx..rx+w-1, ry+h), oeste (rx, ry+1..ry+h-1) y este (rx+w, ry..ry+h-1).
    for (int i = 1; i < w; i++)
        wall_piece(s, b, ws, rx + i, ry, 0, door, door2);
    for (int i = 0; i < w; i++)
        wall_piece(s, b, ws, rx + i, ry + h, 0, door, door2);
    for (int i = 1; i < h; i++)
        wall_piece(s, b, ws, rx, ry + i, 1, door, door2);
    for (int i = 0; i < h; i++)
        wall_piece(s, b, ws, rx + w, ry + i, 1, door, door2);
}

// --- La lista de objetos de la sala (UIPlaceObjects:addObjects)

static int window_add(th_sim *s, th_rnode def, int qty, int picked, int pay)
{
    th_place_window *p = &s->place;
    int k = 0;
    while (k < p->n && p->item[k].def != def)
        k++;
    if (k == p->n) {
        if (p->n >= TH_SIM_PLACE_ITEMS)
            return -1;
        memset(&p->item[k], 0, sizeof p->item[k]);
        p->item[k].def = def;
        p->item[k].picked = (uint8_t)picked;
        p->n++;
    }
    p->item[k].qty = (uint8_t)(p->item[k].qty + qty);
    if (pay)
        money(s, qty * th_build_object_cost(s, def), TH_MONEY_BUY_OBJECT, def);
    return k;
}

// er:checkEnableConfirm: cada objeto obligatorio está en la sala.
static int check_enable_confirm(const th_sim *s)
{
    const th_obra *b = &s->obra;
    if (b->room == TH_NONE)
        return 0;
    const th_rules *d = s->defs;
    const int num = s->rooms[b->room].num;
    const th_rnode need = th_rules_get(d, room_def(s, b->type), "objects_needed");
    for (uint32_t i = 0; i < th_rules_len(d, need); i++) {
        const char *id = th_rules_str(d, th_rules_entry_key(d, need, i), NULL);
        int left = th_rules_int(d, th_rules_entry_value(d, need, i), 0);
        for (int k = 0; k < s->n_obj && left > 0; k++) {
            const th_object *o = &s->obj[k];
            const char *oid = o->def ? def_id(s, o->def) : NULL;
            if (o->def && (o->oflags & TH_OBJF_ON_MAP) && o->room == num && oid && id && !strcmp(oid, id))
                left--;
        }
        if (left > 0)
            return 0;
    }
    return 1;
}

static void confirm_room(th_sim *s);

// er:enterObjectsPhase: la lista con los obligatorios (o lo que había al
// volver a la puerta), cobrados.
static void enter_objects_phase(th_sim *s)
{
    th_obra *b = &s->obra;
    th_place_window *p = &s->place;
    b->phase = TH_OBRA_OBJECTS;
    memset(p, 0, sizeof *p);
    p->open = 1;
    p->orient = TH_DIR_NORTH;
    if (b->n_backup) {
        for (int k = 0; k < b->n_backup; k++)
            window_add(s, b->backup[k].def, b->backup[k].qty, 0, 1);
        b->n_backup = 0;
    } else {
        const th_rules *d = s->defs;
        const th_rnode need = th_rules_get(d, room_def(s, b->type), "objects_needed");
        for (uint32_t i = 0; i < th_rules_len(d, need); i++) {
            const char *id = th_rules_str(d, th_rules_entry_key(d, need, i), NULL);
            const int num = th_rules_int(d, th_rules_entry_value(d, need, i), 0);
            if (num > 0)
                window_add(s, def_by_id(s, id ? id : ""), num, 0, 1);
        }
    }
    // setActiveIndex(1): la primera fila con la primera orientación que tenga.
    if (p->n)
        for (int d = 0; d < 4; d++)
            if (has_orientation(s, p->item[0].def, d)) {
                p->orient = (uint8_t)d;
                break;
            }
    const th_rnode add = th_rules_get(s->defs, room_def(s, b->type), "objects_additional");
    if (th_rules_len(s->defs, add) == 0 && check_enable_confirm(s))
        confirm_room(s);
}

// er:finishRoom → World:resetSideObjects → enterObjectsPhase: las paredes y
// las puertas, la sala (World:newRoom, Room:initRoom con markRoom y
// prepareRectangleTilesForBuild) y la lista de objetos.
static void finish_room(th_sim *s)
{
    th_obra *b = &s->obra;
    int door, door2;
    room_walls(s, b, &door, &door2);
    const int ri = th_sim_new_room(s, b->type, b->x, b->y, b->w, b->h, door, door2, b->room);
    b->room = (uint8_t)ri;
    const int floor = th_rules_int(s->defs, th_rules_get(s->defs, room_def(s, b->type), "floor_tile"), 0);
    th_map_mark_room(s->w.map, b->x, b->y, b->w, b->h, floor, s->rooms[ri].num);
    for (int y = b->y; y < b->y + b->h; y++)
        for (int x = b->x; x < b->x + b->w; x++)
            th_sim_remove_litter_at(s, x, y);
    th_objects w = th_sim_objects(s);
    th_objects_reset_side(&w);
    th_sim_map_changed(s);
    enter_objects_phase(s);
}

// er:confirm en la fase de objetos: cobrar la sala (sin los obligatorios, ya
// pagados), markRoomAsBuilt y close (devuelve lo no colocado).
static void confirm_room(th_sim *s)
{
    th_obra *b = &s->obra;
    if (!b->paid) {
        const th_rules *d = s->defs;
        const th_rnode rd = room_def(s, b->type);
        int32_t cost = th_build_room_cost(s, b->type);
        const th_rnode need = th_rules_get(d, rd, "objects_needed");
        for (uint32_t i = 0; i < th_rules_len(d, need); i++) {
            const char *id = th_rules_str(d, th_rules_entry_key(d, need, i), NULL);
            cost -= th_rules_int(d, th_rules_entry_value(d, need, i), 0) * th_build_object_cost(s, def_by_id(s, id ? id : ""));
        }
        money(s, cost, TH_MONEY_BUILD_ROOM, rd);
        b->paid = 1;
    }
    th_sim_room_mark_built(s, b->room);
    close_window(s, 0);
    memset(b, 0, sizeof *b);
    b->room = TH_NONE;
}

// --- Deshacer la obra (er:cancel, removeRoom, returnToDoorPhase, abortRoom, close)

// er:_remove_wall_line: cada tramo que no es de la fachada vuelve a ser la
// pared exterior del vecino, se deja si el tramo es del vecino, o se borra.
static void remove_wall_line(th_sim *s, int x, int y, int sx, int sy, int n, int west, int nx, int ny)
{
    for (int i = 0; i < n; i++, x += sx, y += sy) {
        wall_info wi;
        const int known = wall_lookup(s, wall_block(s, x, y, west), &wi);
        if (known && !strcmp(wi.id, "external"))
            continue;
        const th_room *nb = in_map(x + nx, y + ny) ? th_sim_room_at(s, x + nx, y + ny) : NULL;
        if (!nb)
            set_wall(s, x, y, west, 0);
        else if ((nx || ny) && known) {
            const char *group = !strcmp(wi.group, "inside_tiles") ? "outside_tiles" : wi.group;
            const th_rnode ws = wall_set(s, th_rules_str(s->defs, th_rules_get(s->defs, room_def(s, nb->type), "wall_type"), NULL));
            set_wall(s, x, y, west, wall_tile(s, ws, group, wi.name));
        }
    }
}

// er:removeRoom: la basura y los objetos de dentro (con save, vuelven a la
// lista sin cobrar), las puertas, las paredes y unmarkRoom.
static void remove_room(th_sim *s, int save)
{
    th_obra *b = &s->obra;
    th_room *r = &s->rooms[b->room];
    for (int y = r->y; y < r->y + r->h; y++)
        for (int x = r->x; x < r->x + r->w; x++) {
            th_sim_remove_litter_at(s, x, y);
            for (;;) {
                int first = -1;
                for (int k = 0; k < s->n_obj; k++) {
                    const th_object *o = &s->obj[k];
                    if (o->def && (o->oflags & TH_OBJF_ON_MAP) && o->x == x && o->y == y &&
                        (first < 0 || o->seq < s->obj[first].seq))
                        first = k;
                }
                if (first < 0 || first == r->door || s->obj[first].oclass == TH_OCLASS_SWING_DOOR)
                    break;
                const th_rnode def = s->obj[first].def;
                const int slave = s->obj[first].oclass == TH_OCLASS_SWING_DOOR && !(s->obj[first].oflags & TH_OBJF_MASTER);
                th_sim_destroy_object(s, first);
                if (save && !slave)
                    window_add(s, def, 1, 0, 0);
            }
        }
    if (r->door != TH_NONE)
        th_sim_destroy_object(s, r->door);
    if (r->door2 != TH_NONE)
        th_sim_destroy_object(s, r->door2);
    r->door = r->door2 = TH_NONE;
    if (save) {
        const th_place_window *p = &s->place;
        b->n_backup = 0;
        for (int k = 0; k < p->n; k++) {
            b->backup[b->n_backup].def = p->item[k].def;
            b->backup[b->n_backup++].qty = p->item[k].qty;
        }
        close_window(s, 0); // removeAllObjects: todo se devuelve
        s->place.open = 1;  // la ventana de la obra sigue abierta, vacía
    }
    remove_wall_line(s, r->x, r->y, 0, 1, r->h, 1, -1, 0);
    remove_wall_line(s, r->x, r->y, 1, 0, r->w, 0, 0, -1);
    remove_wall_line(s, r->x + r->w, r->y, 0, 1, r->h, 1, 0, 0);
    remove_wall_line(s, r->x, r->y + r->h, 1, 0, r->w, 0, 0, 0);
    th_map_unmark_room(s->w.map, r->x, r->y, r->w, r->h);
    th_sim_map_changed(s);
}

// er:returnToWallPhase: el plano vuelve a ser transitable y se pierden la
// puerta y las ventanas.
static void return_to_wall_phase(th_sim *s)
{
    th_obra *b = &s->obra;
    th_map_blueprint_open(s->w.map, b->x, b->y, b->w, b->h);
    b->has_door = 0;
    b->n_windows = 0;
    b->phase = TH_OBRA_WALLS;
}

// er:enterDoorPhase: cerrar el plano y mirar el anillo; si falla, vuelve a
// las paredes (returnToWallPhase(true)).
static int enter_door_phase(th_sim *s)
{
    th_obra *b = &s->obra;
    th_map_blueprint_close(s->w.map, b->x, b->y, b->w, b->h);
    if (!reachability(s, b)) {
        th_map_blueprint_open(s->w.map, b->x, b->y, b->w, b->h);
        b->phase = TH_OBRA_WALLS;
        return 0;
    }
    b->phase = TH_OBRA_DOOR;
    return 1;
}

// er:returnToDoorPhase: la sala sale del mapa con sus objetos a la lista
// (devueltos) y el plano vuelve a la fase de la puerta.
static void return_to_door_phase(th_sim *s)
{
    th_obra *b = &s->obra;
    s->rooms[b->room].built = 0;
    remove_room(s, 1);
    b->has_door = 0;
    b->n_windows = 0;
    enter_door_phase(s);
}

// er:abortRoom sin pagar: cerrar y quitar la sala de world.rooms.
static void abort_room(th_sim *s)
{
    th_obra *b = &s->obra;
    close_window(s, 0);
    th_map_blueprint_open(s->w.map, b->x, b->y, b->w, b->h);
    if (b->room != TH_NONE && b->room < s->n_rooms) {
        memmove(&s->rooms[b->room], &s->rooms[b->room + 1], (size_t)(s->n_rooms - 1 - b->room) * sizeof s->rooms[0]);
        s->n_rooms--;
    }
    memset(b, 0, sizeof *b);
    b->room = TH_NONE;
}

// er:cancel
static void obra_cancel(th_sim *s)
{
    th_obra *b = &s->obra;
    switch (b->phase) {
    case TH_OBRA_WALLS: abort_room(s); break;
    case TH_OBRA_OBJECTS: {
        return_to_door_phase(s);
        th_objects w = th_sim_objects(s);
        th_objects_reset_side(&w);
        break;
    }
    default: return_to_wall_phase(s); break;
    }
}

// er:close: en la fase de objetos con todo lo obligatorio, confirma; si no,
// cancela hasta las paredes y cierra (la sala, si la hubo, se queda en
// world.rooms fuera del mapa, como en CorsixTH).
static void obra_close(th_sim *s)
{
    th_obra *b = &s->obra;
    if (b->phase == TH_OBRA_OBJECTS && check_enable_confirm(s)) {
        confirm_room(s);
        return;
    }
    while (b->phase != TH_OBRA_WALLS && b->phase != TH_OBRA_NONE)
        obra_cancel(s);
    if (b->phase == TH_OBRA_WALLS) {
        close_window(s, 0);
        th_map_blueprint_open(s->w.map, b->x, b->y, b->w, b->h);
        memset(b, 0, sizeof *b);
        b->room = TH_NONE;
    }
}

// er:verifyOrAbortRoom (GameUI:setEditRoom(false), al abrir otra ventana desde el panel).
static void obra_verify_or_abort(th_sim *s)
{
    th_obra *b = &s->obra;
    if (b->phase == TH_OBRA_OBJECTS && check_enable_confirm(s))
        confirm_room(s);
    else {
        while (b->phase != TH_OBRA_WALLS && b->phase != TH_OBRA_NONE)
            obra_cancel(s);
        if (b->phase == TH_OBRA_WALLS)
            abort_room(s);
    }
}

// er:clearArea: alguien dentro del rectángulo ampliado en una casilla; en su
// borde, solo si camina hacia dentro (Humanoid:isObscuringArea).
static int area_obscured(const th_sim *s, const th_obra *b)
{
    for (int i = 0; i < TH_SIM_HUMANOIDS; i++) {
        const th_person *p = &s->hum[i];
        const th_humanoid *h = &p->h;
        if (!p->alive || h->tile_x < 0)
            continue;
        const int tx = h->tile_x, ty = h->tile_y;
        if (tx < b->x - 1 || tx > b->x + b->w || ty < b->y - 1 || ty > b->y + b->h)
            continue;
        if (tx >= b->x && tx < b->x + b->w && ty >= b->y && ty < b->y + b->h)
            return 1;
        if (!h->queue_len || h->queue[0].kind != TH_ACTION_WALK)
            continue;
        if (!(h->queue[0].state & TH_AS_STARTED) || !h->path_len)
            return 1;
        if (h->cur_x >= b->x && h->cur_x < b->x + b->w && h->cur_y >= b->y && h->cur_y < b->y + b->h)
            return 1;
    }
    return 0;
}

static int free_object_slots(const th_sim *s)
{
    int n = TH_SIM_OBJECTS - s->n_obj;
    for (int i = 0; i < s->n_obj; i++)
        n += !s->obj[i].def;
    return n;
}

void th_build_tick(th_sim *s)
{
    if (s->obra.phase == TH_OBRA_CLEAR && !area_obscured(s, &s->obra))
        finish_room(s);
}

int th_build_rebuild_room(th_sim *s, int type, int x, int y, int w, int h, th_edge door, const th_edge *windows,
                          int n_windows)
{
    th_obra *b = &s->obra;
    if (b->phase != TH_OBRA_NONE || type < 0 || type >= s->n_room_types)
        return -1;
    memset(b, 0, sizeof *b);
    b->phase = TH_OBRA_WINDOWS;
    b->type = (uint8_t)type;
    b->x = (uint8_t)x;
    b->y = (uint8_t)y;
    b->w = (uint8_t)w;
    b->h = (uint8_t)h;
    b->room = TH_NONE;
    b->has_door = 1;
    b->door = door;
    b->n_windows = (uint8_t)(n_windows > TH_SIM_WINDOWS ? TH_SIM_WINDOWS : n_windows);
    memcpy(b->windows, windows, (size_t)b->n_windows * sizeof windows[0]);
    th_map_blueprint_close(s->w.map, x, y, w, h);
    const th_hospital saved = s->hosp;
    finish_room(s);
    const int ri = b->room == TH_NONE ? -1 : b->room;
    if (ri >= 0 && b->phase == TH_OBRA_OBJECTS)
        confirm_room(s);
    s->hosp = saved;
    return ri;
}

// --- Las órdenes de las salas

static int order_sala(th_sim *s, const th_order *o, th_order_result *r)
{
    // UIBuildRoom:buildRoom y UIEditRoom: UI:addWindow cierra la ventana
    // «main» que hubiera (la de colocar del pasillo o la obra anterior).
    if (s->obra.phase != TH_OBRA_NONE)
        obra_close(s);
    else if (s->place.open)
        close_window(s, 0);
    const int type = room_type_of(s, o->def);
    if (type < 0 || !room_discovered(s, type))
        return TH_REASON_NO_DISPONIBLE;
    if (s->hosp.balance < th_build_room_cost(s, type))
        return TH_REASON_SIN_DINERO;
    // setBlueprintRect: el recorte al mapa (la sala nunca llega a la última
    // columna ni a la última fila, donde irían sus paredes este y sur).
    int x = o->x, y = o->y, w = o->w, h = o->h;
    if (x + w > TH_MAP_WIDTH - 1)
        w = TH_MAP_WIDTH - 1 - x;
    if (y + h > TH_MAP_HEIGHT - 1)
        h = TH_MAP_HEIGHT - 1 - y;
    th_obra *b = &s->obra;
    memset(b, 0, sizeof *b);
    b->phase = TH_OBRA_WALLS;
    b->type = (uint8_t)type;
    b->x = (uint8_t)x;
    b->y = (uint8_t)y;
    b->w = (uint8_t)(w < 0 ? 0 : w);
    b->h = (uint8_t)(h < 0 ? 0 : h);
    b->room = TH_NONE;
    const int min = room_min_size(s, type);
    if (w < min || h < min)
        return TH_REASON_PEQUENA;
    int bad = 0;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            bad += !blueprint_tile_ok(s, xx, yy);
    if (bad) {
        r->bad = (uint8_t)(bad > 255 ? 255 : bad);
        return TH_REASON_CASILLAS;
    }
    return enter_door_phase(s) ? TH_REASON_OK : TH_REASON_ZONAS_INACCESIBLES;
}

static int order_puerta(th_sim *s, const th_order *o)
{
    th_obra *b = &s->obra;
    if (b->phase != TH_OBRA_DOOR)
        return TH_REASON_FASE;
    const th_edge e = {o->x, o->y, (uint8_t)(o->dir & 3)};
    if (!edge_position_ok(b, o->x, o->y, e.side, room_swing_doors(s, b->type)))
        return TH_REASON_POSICION;
    if (!door_valid(s, b, &e))
        return TH_REASON_PUERTA_INVALIDA;
    b->door = e;
    b->has_door = 1;
    b->phase = TH_OBRA_WINDOWS; // confirm(true): enterWindowsPhase
    return TH_REASON_OK;
}

static int order_ventana(th_sim *s, const th_order *o, int quitar)
{
    th_obra *b = &s->obra;
    if (b->phase != TH_OBRA_WINDOWS)
        return TH_REASON_FASE;
    const th_edge e = {o->x, o->y, (uint8_t)(o->dir & 3)};
    if (!edge_position_ok(b, o->x, o->y, e.side, 0))
        return TH_REASON_POSICION;
    if (quitar) {
        for (int k = 0; k < b->n_windows; k++)
            if (b->windows[k].x == e.x && b->windows[k].y == e.y && b->windows[k].side == e.side) {
                memmove(&b->windows[k], &b->windows[k + 1], (size_t)(b->n_windows - 1 - k) * sizeof e);
                b->n_windows--;
                return TH_REASON_OK;
            }
        return TH_REASON_SIN_VENTANA;
    }
    if (!window_valid(s, b, &e))
        return TH_REASON_VENTANA_INVALIDA;
    if (b->n_windows >= TH_SIM_WINDOWS)
        return TH_REASON_LLENO;
    b->windows[b->n_windows++] = e;
    return TH_REASON_OK;
}

static int order_fin_ventanas(th_sim *s)
{
    th_obra *b = &s->obra;
    if (b->phase != TH_OBRA_WINDOWS)
        return TH_REASON_FASE;
    // El tope del motor: la sala, la puerta (con cola) y la esclava de una doble.
    if ((b->room == TH_NONE && s->n_rooms >= TH_SIM_ROOMS) || !th_sim_object_fits(s, def_by_id(s, "door")) ||
        free_object_slots(s) < 1 + room_swing_doors(s, b->type))
        return TH_REASON_LLENO;
    b->phase = TH_OBRA_CLEAR;
    if (!area_obscured(s, b))
        finish_room(s);
    return TH_REASON_OK;
}

static int order_comprar_sala(th_sim *s, const th_order *o)
{
    th_obra *b = &s->obra;
    if (b->phase != TH_OBRA_OBJECTS)
        return TH_REASON_FASE;
    // er:purchaseItems: la tienda son los objects_additional disponibles en el nivel.
    const th_rules *d = s->defs;
    const th_rnode add = th_rules_get(d, room_def(s, b->type), "objects_additional");
    const char *id = o->def ? def_id(s, o->def) : NULL;
    int listed = 0;
    for (uint32_t i = 0; i < th_rules_len(d, add) && !listed; i++) {
        const char *a = th_rules_str(d, th_rules_at(d, add, i), NULL);
        listed = a && id && !strcmp(a, id) &&
                 th_rules_int(s->level, th_rules_get(s->level, level_object(s, def_thob(s, o->def)), "AvailableForLevel"), 1);
    }
    if (!listed)
        return TH_REASON_NO_ES_DE_SALA;
    if (o->qty <= 0)
        return TH_REASON_OK; // confirm sin cambios: cierra la tienda
    // Cada unidad es una pulsación de «+» sobre la cantidad que ya hay en la
    // lista (purchaseItem: el tope 99 antes que el dinero, y el saldo tiene
    // que llegar al total de lo nuevo); si falla una, no se compra nada.
    const th_place_window *p = &s->place;
    int cur = 0;
    for (int k = 0; k < p->n; k++)
        if (p->item[k].def == o->def)
            cur = p->item[k].qty;
    const int32_t price = th_build_object_cost(s, o->def);
    int32_t total = 0;
    for (int i = 0; i < o->qty; i++) {
        if (cur >= 99)
            return TH_REASON_MAXIMO;
        total += price;
        if (s->hosp.balance < total)
            return TH_REASON_SIN_DINERO;
        cur++;
    }
    // confirm: cobra lo nuevo y lo añade a la lista sin volver a cobrar.
    money(s, total, TH_MONEY_BUY_OBJECT, o->def);
    window_add(s, o->def, o->qty, 0, 0);
    return TH_REASON_OK;
}

static int order_fin_sala(th_sim *s)
{
    th_obra *b = &s->obra;
    if (b->phase != TH_OBRA_OBJECTS)
        return TH_REASON_FASE;
    if (!check_enable_confirm(s))
        return TH_REASON_FALTAN_OBJETOS;
    confirm_room(s);
    return TH_REASON_OK;
}

// Object:onClick dentro de una obra (room_editing_mode): el objeto se
// destruye y vuelve a la lista sin cobrar, con su orientación.
static int order_recoger_obra(th_sim *s, const th_order *o)
{
    th_obra *b = &s->obra;
    if (b->phase != TH_OBRA_OBJECTS)
        return TH_REASON_FASE;
    int obj = -1;
    for (int i = 0; i < s->n_obj; i++) {
        const th_object *ob = &s->obj[i];
        if (ob->def && (ob->oflags & TH_OBJF_ON_MAP) && ob->x == o->x && ob->y == o->y &&
            (obj < 0 || ob->entity_seq > s->obj[obj].entity_seq))
            obj = i;
    }
    if (obj < 0)
        return TH_REASON_SIN_OBJETO;
    const th_object *ob = &s->obj[obj];
    const int room = th_map_tile_c(s->w.map, ob->x, ob->y)->room;
    const int corridor = th_rules_get(s->defs, ob->def, "corridor_object") != 0;
    if (ob->oclass == TH_OCLASS_DOOR || ob->oclass == TH_OCLASS_SWING_DOOR || ob->oclass == TH_OCLASS_ENTRANCE_DOOR)
        return TH_REASON_NO_SE_PUEDE;
    if (!room || (room != s->rooms[b->room].num && !corridor))
        return TH_REASON_NO_SE_PUEDE;
    const th_rnode def = ob->def;
    const int dir = ob->dir;
    th_sim_destroy_object(s, obj);
    th_sim_map_changed(s);
    const int k = window_add(s, def, 1, 1, 0);
    if (k >= 0)
        s->place.active = (uint8_t)k;
    s->place.orient = (uint8_t)dir;
    return TH_REASON_OK;
}

int th_build_apply(th_sim *s, const th_order *o, th_order_result *r)
{
    th_order_result tmp;
    if (!r)
        r = &tmp;
    r->reason = TH_REASON_OK;
    r->bad = 0;
    int reason;
    const int obra = s->obra.phase != TH_OBRA_NONE;
    switch (o->kind) {
    case TH_ORD_COMPRAR: reason = order_comprar(s, o); break;
    case TH_ORD_COLOCAR: reason = order_colocar(s, o, r); break;
    case TH_ORD_CERRAR: // UIEditRoom:close con una obra; UIPlaceObjects:close sin ella
        if (obra) {
            obra_close(s);
            reason = TH_REASON_OK;
            break;
        }
        /* fallthrough */
    case TH_ORD_VENDER: // sell: close(false) fuera de una sala en edición; dentro no hace nada
        if (obra) {
            reason = TH_REASON_NO_SE_PUEDE;
            break;
        }
        if (!s->place.open) {
            reason = TH_REASON_SIN_VENTANA;
            break;
        }
        close_window(s, 0);
        reason = TH_REASON_OK;
        break;
    case TH_ORD_DESHACER:
        if (obra) {
            reason = TH_REASON_NO_SE_PUEDE;
            break;
        }
        if (!s->place.open) {
            reason = TH_REASON_SIN_VENTANA;
            break;
        }
        close_window(s, 1);
        reason = TH_REASON_OK;
        break;
    case TH_ORD_RECOGER: reason = order_recoger(s, o); break;
    case TH_ORD_SALA: reason = order_sala(s, o, r); break;
    case TH_ORD_PUERTA: reason = order_puerta(s, o); break;
    case TH_ORD_VENTANA: reason = order_ventana(s, o, 0); break;
    case TH_ORD_QUITAR_VENTANA: reason = order_ventana(s, o, 1); break;
    case TH_ORD_FIN_VENTANAS: reason = order_fin_ventanas(s); break;
    case TH_ORD_COMPRAR_SALA: reason = order_comprar_sala(s, o); break;
    case TH_ORD_FIN_SALA: reason = order_fin_sala(s); break;
    case TH_ORD_ATRAS:
        if (!obra) {
            reason = TH_REASON_SIN_VENTANA;
            break;
        }
        obra_cancel(s);
        reason = TH_REASON_OK;
        break;
    default: reason = TH_REASON_ORDEN; break;
    }
    r->reason = (uint8_t)reason;
    return reason;
}

// ---------------------------------------------------------------------------
// La cola

int th_sim_push_order(th_sim *s, const th_order *o)
{
    if (s->n_orders >= TH_SIM_ORDERS)
        return -1;
    s->orders[s->n_orders++] = *o;
    return 0;
}

void th_sim_apply_orders(th_sim *s)
{
    int k = 0;
    while (k < s->n_orders) {
        const th_order o = s->orders[k];
        if (o.tick > s->hours) {
            k++;
            continue;
        }
        memmove(&s->orders[k], &s->orders[k + 1], (size_t)(s->n_orders - 1 - k) * sizeof s->orders[0]);
        s->n_orders--;
        th_order_result r;
        th_build_apply(s, &o, &r);
        if (s->ev.order)
            s->ev.order(s->ev.ctx, s, &o, &r);
    }
}
