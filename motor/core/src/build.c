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
                                        "otra_ventana"};
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
    if (reason == TH_MONEY_BUY_OBJECT) {
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

// po:_isSideObjectPlacementValid en el pasillo: con el borde cortado por los
// dos lados, la casilla tiene que llegar a la de enfrente si esa es pasable.
// Al acabar deja el tránsito de las dos a 1, no como estaba (rareza 3 de §4.4).
static int side_placement_valid(th_sim *s, int x, int y, int side)
{
    if (!(flags_at(s, x, y) & travel_flag[side]))
        return 1; // contra una pared u otro objeto de borde
    const int cx = x + dir_dx[side], cy = y + dir_dy[side];
    set_flags(s, x, y, 0, travel_flag[side]);
    set_flags(s, cx, cy, 0, travel_flag[(side + 2) & 3]);
    int invalid = 0;
    if (th_path_find(s->w.pf, s->w.map, x, y, cx, cy) < 0 && in_map(cx, cy) &&
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
    int optional_tiles = 0, opt_blocked = 0, valid = 1, room_id = -1, side = -1;
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
        // En una sala: hace falta que tenga puerta. Si está activa, el modo
        // estricto, como en el pasillo; si está en obras, el del modo 2 con la
        // puerta de la sala, que llega con las salas (paso 3).
        const th_room *r = th_sim_room((th_sim *)s, room_id);
        if (!r || r->door == TH_NONE || !r->is_active)
            return 0;
    }
    if (!side_obj)
        return !would_break_pathfinding(s, x, y, def, dir, room_id < 0 ? 0 : room_id, fp, n);
    return side >= 0 ? side_placement_valid(s, x, y, side) : 1;
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
// casilla, o en una vecina caminando hacia ella.
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
        if (!h->path_len || (h->cur_x == x && h->cur_y == y))
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
    const int move_cancellation = !placed && existing >= 0 && move_canceled;
    const int purchase_cancellation = !placed && existing < 0;
    const int sale_previously_placed = !placed && existing >= 0 && !move_canceled;
    if (purchase_cancellation || sale_previously_placed)
        money(s, th_build_object_cost(s, it->def), TH_MONEY_SELL_OBJECT, it->def);
    if (move_cancellation) {
        // Vuelve a su casilla y orientación, sin comprobar nada.
        const th_object *o = &s->obj[existing];
        p->orient = o->dir;
        place_object(s, o->x, o->y, 0, 1);
    }
    if (sale_previously_placed) {
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
    } else {
        prepare_footprint(s, def, p->orient, x, y);
        obj = th_sim_new_object(s, def, x, y, p->orient);
    }
    if (obj != TH_NONE) {
        const int room = th_map_tile_c(s->w.map, x, y)->room;
        if (room)
            s->obj[obj].room = (uint8_t)room;
    }
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

static int order_comprar(th_sim *s, const th_order *o)
{
    // UIFurnishCorridor es una ventana «main»: abrirla cierra la de colocar
    // (UI:addWindow), que devuelve lo que no se colocó.
    if (s->place.open)
        close_window(s, 0);
    if (!o->def || !th_rules_get(s->defs, o->def, "corridor_object"))
        return TH_REASON_NO_ES_DE_PASILLO;
    // Cada unidad es una pulsación de «+» (purchaseItem(i, 1)), que no cobra:
    // falla si el saldo no llega al total o, en la centésima, porque ya hay 99.
    // Si falla una, se cierra la ventana y no se compra nada.
    const int qty = o->qty;
    const int32_t price = th_build_object_cost(s, o->def);
    if (s->hosp.balance < (int64_t)(qty < 99 ? qty : 99) * price)
        return TH_REASON_SIN_DINERO;
    if (qty > 99)
        return TH_REASON_MAXIMO;
    if (qty <= 0)
        return TH_REASON_OK; // confirm sin nada que comprar: cierra
    // confirm: cobra y abre la ventana de colocar.
    money(s, qty * price, TH_MONEY_BUY_OBJECT, o->def);
    open_window(s, o->def, qty, -1);
    return TH_REASON_OK;
}

static int order_colocar(th_sim *s, const th_order *o, th_order_result *r)
{
    th_place_window *p = &s->place;
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
    place_object(s, o->x, o->y, 0, 0);
    return TH_REASON_OK;
}

static int order_recoger(th_sim *s, const th_order *o)
{
    // Con la ventana de colocar abierta no hay prueba del mundo
    // (setWorldHitTest(false)) y el clic derecho gira el plano.
    if (s->place.open)
        return TH_REASON_OTRA_VENTANA;
    // El objeto con ese origen que va el último en world.entities.
    int obj = -1;
    for (int i = 0; i < s->n_obj; i++) {
        const th_object *ob = &s->obj[i];
        if (ob->def && (ob->oflags & TH_OBJF_ON_MAP) && ob->x == o->x && ob->y == o->y)
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

int th_build_apply(th_sim *s, const th_order *o, th_order_result *r)
{
    th_order_result tmp;
    if (!r)
        r = &tmp;
    r->reason = TH_REASON_OK;
    r->bad = 0;
    int reason;
    switch (o->kind) {
    case TH_ORD_COMPRAR: reason = order_comprar(s, o); break;
    case TH_ORD_COLOCAR: reason = order_colocar(s, o, r); break;
    case TH_ORD_CERRAR:
    case TH_ORD_VENDER: // sell: close(false) fuera de una sala en edición
        if (!s->place.open) {
            reason = TH_REASON_SIN_VENTANA;
            break;
        }
        close_window(s, 0);
        reason = TH_REASON_OK;
        break;
    case TH_ORD_DESHACER:
        if (!s->place.open) {
            reason = TH_REASON_SIN_VENTANA;
            break;
        }
        close_window(s, 1);
        reason = TH_REASON_OK;
        break;
    case TH_ORD_RECOGER: reason = order_recoger(s, o); break;
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
