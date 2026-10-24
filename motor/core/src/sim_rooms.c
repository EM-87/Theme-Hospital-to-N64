// Objetos, colas, salas, el mostrador de recepción y la consulta de GP de la
// simulación (th/sim.h): entities/object.lua, queue.lua, room.lua,
// objects/reception_desk.lua y rooms/gp.lua de CorsixTH.
#include <string.h>

#include "th/sim.h"

static const char *const dir_names[] = {"north", "east", "south", "west"};

// ---------------------------------------------------------------------------
// Objetos

int th_sim_object_at(const th_sim *s, int x, int y, int type)
{
    for (int i = 0; i < s->n_obj; i++)
        if (s->obj[i].type == type && s->obj[i].x == x && s->obj[i].y == y)
            return i;
    return TH_NONE;
}

static th_rnode orientation(const th_sim *s, const th_object *o)
{
    return th_rules_get(s->defs, th_rules_get(s->defs, o->def, "orientations"), dir_names[o->dir]);
}

static int pair(const th_rules *r, th_rnode n, int *x, int *y)
{
    if (th_rules_type(r, n) == TH_RULES_LIST) {
        *x = th_rules_int(r, th_rules_at(r, n, 0), 0);
        *y = th_rules_int(r, th_rules_at(r, n, 1), 0);
        return 1;
    }
    if (th_rules_type(r, n) == TH_RULES_MAP) {
        *x = th_rules_int(r, th_rules_geti(r, n, 1), 0);
        *y = th_rules_int(r, th_rules_geti(r, n, 2), 0);
        return 1;
    }
    return 0;
}

// Object:_getUsageTile: casilla de uso (o la secundaria) del objeto.
int th_sim_use_tile(const th_sim *s, int obj, int secondary, int *x, int *y)
{
    const th_object *o = &s->obj[obj];
    int dx, dy;
    if (!pair(s->defs, th_rules_get(s->defs, orientation(s, o), secondary ? "use_position_secondary" : "use_position"),
              &dx, &dy)) {
        *x = o->x;
        *y = o->y;
        return 0;
    }
    *x = o->x + dx;
    *y = o->y + dy;
    return 1;
}

// pathfind_allowed_dirs[d] de la orientación del objeto.
static int allowed_dir(const th_sim *s, const th_object *o, int d)
{
    th_rnode n = th_rules_get(s->defs, orientation(s, o), "pathfind_allowed_dirs");
    if (th_rules_type(s->defs, n) == TH_RULES_LIST)
        return d >= 1 && th_rules_bool(s->defs, th_rules_at(s->defs, n, (uint32_t)(d - 1)), 1);
    return th_rules_bool(s->defs, th_rules_geti(s->defs, n, d), 1);
}

// Object:setUser y removeUser (objetos de un solo usuario): poner un usuario
// quita la reserva, sea de quien sea.
void th_sim_object_set_user(th_sim *s, int obj, int who)
{
    s->obj[obj].user = (uint8_t)who;
    s->obj[obj].reserved_for = TH_NONE;
}

void th_sim_object_remove_user(th_sim *s, int obj, int who)
{
    (void)who;
    s->obj[obj].user = TH_NONE;
}

// ---------------------------------------------------------------------------
// Colas (queue.lua)

static int patient_size(const th_queue *q)
{
    return q->reported + q->expected_count;
}

// _getHumanoidQueuePriority
static int queue_priority(th_sim *s, const th_queue *q, int who)
{
    const th_person *p = &s->hum[who];
    if (q->door != TH_NONE) {
        th_room *r = th_sim_room(s, s->obj[q->door].room);
        th_humanoid *h = (th_humanoid *)&p->h;
        const th_room *hr = h->in_room ? th_sim_room(s, h->in_room)
                                       : (h->tile_x >= 0 ? th_sim_room_at(s, h->tile_x, h->tile_y) : NULL);
        if (r && hr == r)
            return 1;
    }
    return p->kind == TH_HUM_PATIENT ? 6 : 2;
}

th_action *th_sim_queue_action_of(th_sim *s, int who, int q)
{
    th_humanoid *h = &s->hum[who].h;
    for (int i = 0; i < h->queue_len; i++)
        if (h->queue[i].kind == TH_ACTION_QUEUE && h->queue[i].obj != TH_NONE && s->obj[h->queue[i].obj].queue == q)
            return &h->queue[i];
    return NULL;
}

static void change_position(th_sim *s, int q, int who)
{
    th_action *qa = th_sim_queue_action_of(s, who, q);
    if (qa)
        th_sim_on_change_queue_position(s, who, qa);
}

void th_sim_queue_unexpect(th_sim *s, int q, int who)
{
    if (q == TH_NONE)
        return;
    th_queue *qu = &s->queues[q];
    const uint64_t bit = 1ull << who;
    if (qu->expected & bit) {
        qu->expected &= ~bit;
        if (s->hum[who].kind == TH_HUM_PATIENT)
            qu->expected_count--;
    }
}

static void queue_expect(th_sim *s, int q, int who)
{
    th_queue *qu = &s->queues[q];
    const uint64_t bit = 1ull << who;
    if (!(qu->expected & bit)) {
        qu->expected |= bit;
        if (s->hum[who].kind == TH_HUM_PATIENT)
            qu->expected_count++;
    }
}

// Queue:push: por prioridad, y avisa a los que quedan detrás.
void th_sim_queue_push(th_sim *s, int q, int who)
{
    th_queue *qu = &s->queues[q];
    if (qu->size >= TH_SIM_QUEUE_LEN)
        return;
    const int pr = queue_priority(s, qu, who);
    int index = qu->size;
    while (index > 0 && queue_priority(s, qu, qu->member[index - 1]) > pr)
        index--;
    if (pr > 3)
        qu->reported++;
    memmove(&qu->member[index + 1], &qu->member[index], (unsigned)(qu->size - index));
    qu->member[index] = (uint8_t)who;
    qu->size++;
    for (int i = index + 1; i < qu->size; i++)
        change_position(s, q, qu->member[i]);
}

// Queue:pop: el primero sale (onLeaveQueue) y los demás se recolocan.
static int queue_pop(th_sim *s, int q)
{
    th_queue *qu = &s->queues[q];
    if (qu->reported == qu->size)
        qu->reported--;
    const int front = qu->member[0];
    memmove(&qu->member[0], &qu->member[1], (unsigned)(qu->size - 1));
    qu->size--;
    th_action *qa = th_sim_queue_action_of(s, front, q);
    if (qa)
        th_sim_on_leave_queue(s, front, qa);
    for (int i = 0; i < qu->size; i++)
        change_position(s, q, qu->member[i]);
    return front;
}

// Queue:removeValue (sin recolocar a nadie, como CorsixTH).
void th_sim_queue_remove_value(th_sim *s, int q, int who)
{
    th_queue *qu = &s->queues[q];
    for (int i = 0; i < qu->size; i++) {
        if (qu->member[i] != who)
            continue;
        if (i >= qu->size - qu->reported)
            qu->reported--;
        memmove(&qu->member[i], &qu->member[i + 1], (unsigned)(qu->size - i - 1));
        qu->size--;
        return;
    }
}

// ---------------------------------------------------------------------------
// Salas (room.lua)

th_room *th_sim_room_at(th_sim *s, int x, int y)
{
    const int num = th_map_tile_c(s->w.map, x, y)->room;
    return num ? th_sim_room(s, num) : NULL;
}

static th_room *humanoid_room(th_sim *s, int who)
{
    th_humanoid *h = &s->hum[who].h;
    if (h->in_room)
        return th_sim_room(s, h->in_room);
    return h->tile_x >= 0 ? th_sim_room_at(s, h->tile_x, h->tile_y) : NULL;
}

static int in_room(const th_room *r, int who)
{
    return (r->humanoids >> who) & 1u;
}

int th_sim_room_patient_count(const th_sim *s, const th_room *r)
{
    int n = 0;
    for (int i = 0; i < TH_SIM_HUMANOIDS; i++)
        if (in_room(r, i) && s->hum[i].kind == TH_HUM_PATIENT)
            n++;
    return n;
}

static int room_patient(const th_sim *s, const th_room *r)
{
    for (int i = 0; i < TH_SIM_HUMANOIDS; i++)
        if (in_room(r, i) && s->hum[i].kind == TH_HUM_PATIENT)
            return i;
    return TH_NONE;
}

static th_rnode required_staff(const th_sim *s, const th_room *r)
{
    return th_rules_get(s->defs, th_rules_get(s->defs, th_rules_get(s->defs, s->defs->root, "salas"),
                                              s->room_types[r->type]),
                        "required_staff");
}

int th_sim_room_requires_staff(const th_sim *s, const th_room *r)
{
    return th_rules_len(s->defs, required_staff(s, r)) > 0;
}

static int fulfills(const th_person *p, const char *criterion)
{
    if (!strcmp(criterion, "Doctor"))
        return p->kind == TH_HUM_DOCTOR;
    if (!strcmp(criterion, "Nurse"))
        return p->kind == TH_HUM_NURSE;
    if (p->kind != TH_HUM_DOCTOR)
        return 0;
    if (!strcmp(criterion, "Psychiatrist"))
        return p->u.s.psychiatrist;
    if (!strcmp(criterion, "Surgeon"))
        return p->u.s.surgeon;
    if (!strcmp(criterion, "Researcher"))
        return p->u.s.researcher;
    return 0;
}

// Room:testStaffCriteria(getRequiredStaffCriteria())
static int staff_criteria_met(const th_sim *s, const th_room *r)
{
    th_rnode req = required_staff(s, r);
    const uint32_t n = th_rules_len(s->defs, req);
    for (uint32_t k = 0; k < n; k++) {
        const char *crit = th_rules_str(s->defs, th_rules_entry_key(s->defs, req, k), NULL);
        int count = th_rules_int(s->defs, th_rules_entry_value(s->defs, req, k), 0);
        for (int i = 0; i < TH_SIM_HUMANOIDS && crit; i++) {
            const th_person *p = &s->hum[i];
            if (!in_room(r, i) || p->kind == TH_HUM_PATIENT || !fulfills(p, crit))
                continue;
            if (p->h.queue_len && (p->h.queue[0].flags & TH_AF_IS_LEAVING))
                continue;
            if (r->staff_member == i)
                count--;
        }
        if (count > 0)
            return 0;
    }
    return 1;
}

int th_sim_room_can_enter(th_sim *s, const th_room *r, int who)
{
    if (!r->is_active)
        return 0;
    if (s->hum[who].kind != TH_HUM_PATIENT)
        return 1;
    return staff_criteria_met(s, r) && th_sim_room_patient_count(s, r) < r->maximum_patients;
}

// Room:getEntranceXY
static void entrance(const th_sim *s, const th_room *r, int inside, int *x, int *y)
{
    const th_object *d = &s->obj[r->door];
    int dx = d->x, dy = d->y;
    const int door_in_room = th_map_tile_c(s->w.map, dx, dy)->room == r->num;
    if ((inside && !door_in_room) || (!inside && door_in_room)) {
        if (d->dir == TH_DIR_NORTH)
            dy--;
        else if (d->dir == TH_DIR_WEST)
            dx--;
    }
    *x = dx;
    *y = dy;
}

static th_action leave_action(th_sim *s, th_room *r)
{
    int x, y;
    entrance(s, r, 0, &x, &y);
    th_action a = th_action_walk(x, y);
    a.flags |= TH_AF_IS_LEAVING | TH_AF_TRUNCATE_HIGH;
    return a;
}

th_action th_sim_room_enter_action(th_sim *s, th_room *r, int who)
{
    int x, y;
    entrance(s, r, 1, &x, &y);
    if (r->is_active)
        queue_expect(s, s->obj[r->door].queue, who);
    th_action a = th_action_walk(x, y);
    a.flags |= TH_AF_IS_ENTERING;
    return a;
}

// Humanoid:unexpectFromRoom
void th_sim_unexpect_from_room(th_sim *s, int who, th_room *r)
{
    if (!r)
        return;
    const th_person *p = &s->hum[who];
    const int next = p->kind == TH_HUM_PATIENT ? p->u.p.next_room : 0;
    if (next == r->num || (!humanoid_room(s, who) || p->kind != TH_HUM_PATIENT))
        th_sim_queue_unexpect(s, s->obj[r->door].queue, who);
}

void th_sim_room_try_advance_queue(th_sim *s, th_room *r)
{
    if (!r || r->door == TH_NONE)
        return;
    const th_object *d = &s->obj[r->door];
    const th_queue *q = &s->queues[d->queue];
    if (q->size == 0 || d->user != TH_NONE || d->reserved_for != TH_NONE)
        return;
    const int front = q->member[0];
    if (th_sim_room_can_enter(s, r, front) || in_room(r, front))
        queue_pop(s, d->queue);
}

static int usage_score(th_sim *s, const th_room *r)
{
    const th_queue *q = &s->queues[s->obj[r->door].queue];
    int score = (patient_size(q) + th_sim_room_patient_count(s, r) - r->maximum_patients) * 10;
    if (staff_criteria_met(s, r))
        score -= 50;
    if (q->size >= q->max_size)
        score += 1000;
    return score;
}

// World:findRoomNear(humanoid, tipo, nil, "advanced")
int th_sim_find_room_near(th_sim *s, int who, int room_type)
{
    const th_humanoid *h = &s->hum[who].h;
    int best = TH_NONE, score = 0;
    for (int i = 0; i < s->n_rooms; i++) {
        th_room *r = &s->rooms[i];
        if (r->type != room_type || !r->is_active)
            continue;
        int x, y;
        entrance(s, r, 0, &x, &y);
        const int d = th_path_find(s->w.pf, s->w.map, h->tile_x, h->tile_y, x, y);
        if (d < 0)
            continue;
        const int sc = d + usage_score(s, r);
        if (best == TH_NONE || sc < score) {
            score = sc;
            best = i;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Búsquedas del pathfinder (World:getFreeBench, findObjectNear, getIdleTile)

typedef struct {
    th_sim *s;
    int type;
    int free_only;
    int obj, x, y, dist;
} visit_ctx;

static int visit(void *ctx, int x, int y, int dir, int dist)
{
    visit_ctx *c = ctx;
    const int o = th_sim_object_at(c->s, x, y, c->type);
    if (o == TH_NONE)
        return 0;
    const th_object *ob = &c->s->obj[o];
    if (c->free_only && (ob->user != TH_NONE || ob->reserved_for != TH_NONE))
        return 0;
    if (!allowed_dir(c->s, ob, dir))
        return 0;
    th_sim_use_tile(c->s, o, 0, &c->x, &c->y);
    c->obj = o;
    c->dist = dist;
    return 1;
}

static int type_thob(const th_sim *s, int type)
{
    for (int i = 0; i < s->n_obj; i++)
        if (s->obj[i].type == type)
            return s->obj[i].thob;
    return -1;
}

int th_sim_free_bench(th_sim *s, int gx, int gy, int dist, int *bx, int *by, int *found_dist)
{
    visit_ctx c = {s, TH_OBJ_BENCH, 1, TH_NONE, 0, 0, 0};
    const int thob = type_thob(s, TH_OBJ_BENCH);
    if (thob < 0 || !th_path_visit_objects(s->w.pf, s->w.map, gx, gy, thob, dist, visit, &c))
        return TH_NONE;
    *bx = c.x;
    *by = c.y;
    *found_dist = c.dist;
    return c.obj;
}

int th_sim_find_object_near(th_sim *s, int who, int type, int *ox, int *oy)
{
    const th_humanoid *h = &s->hum[who].h;
    visit_ctx c = {s, type, 0, TH_NONE, 0, 0, 0};
    const int thob = type_thob(s, type);
    if (thob < 0 || !th_path_visit_objects(s->w.pf, s->w.map, h->tile_x, h->tile_y, thob, 1 << 30, visit, &c))
        return TH_NONE;
    *ox = c.x;
    *oy = c.y;
    return c.obj;
}

int th_sim_idle_tile(th_sim *s, int x, int y, int n, int *ix, int *iy)
{
    return th_path_idle_tile(s->w.pf, s->w.map, x, y, n, ix, iy);
}

// ---------------------------------------------------------------------------
// Mostrador de recepción (objects/reception_desk.lua)

int th_sim_desk_usage_score(const th_sim *s, int desk)
{
    const th_queue *q = &s->queues[s->obj[desk].queue];
    int score = patient_size(q) * 10;
    if (q->size >= q->max_size)
        score += 1000;
    return score;
}

// ReceptionDesk:occupy
static int desk_occupy(th_sim *s, int desk, int who)
{
    th_object *d = &s->obj[desk];
    if (d->receptionist != TH_NONE || d->reserved_for != TH_NONE)
        return 0;
    d->reserved_for = (uint8_t)who;
    s->hum[who].u.s.associated_desk = (uint8_t)desk;
    int ux, uy;
    th_sim_use_tile(s, desk, 1, &ux, &uy);
    th_action wk = th_action_walk(ux, uy);
    wk.flags |= TH_AF_MUST_HAPPEN;
    th_humanoid_set_next_action(&s->w, &s->hum[who].h, wk);
    th_action r = th_action_new(TH_ACTION_STAFF_RECEPTION);
    r.obj = (uint8_t)desk;
    r.flags |= TH_AF_MUST_HAPPEN;
    th_humanoid_queue_action(&s->hum[who].h, r);
    return 1;
}

// ReceptionDesk:checkForNearbyStaff
void th_sim_desk_check_for_staff(th_sim *s, int desk)
{
    const th_object *d = &s->obj[desk];
    if (d->receptionist != TH_NONE || d->reserved_for != TH_NONE)
        return;
    int ux, uy, best = TH_NONE, best_d = 0;
    th_sim_use_tile(s, desk, 1, &ux, &uy);
    for (int k = 0; k < s->n_order; k++) {
        const int i = s->order[k];
        const th_person *p = &s->hum[i];
        if (p->kind != TH_HUM_RECEPTIONIST || p->u.s.associated_desk != TH_NONE)
            continue;
        const int dist = th_path_find(s->w.pf, s->w.map, p->h.tile_x, p->h.tile_y, ux, uy);
        if (best == TH_NONE || dist < best_d) {
            best = i;
            best_d = dist;
        }
    }
    if (best != TH_NONE)
        desk_occupy(s, desk, best);
}

// ReceptionDesk:tick
void th_sim_desk_tick(th_sim *s, int desk)
{
    th_object *d = &s->obj[desk];
    th_queue *q = &s->queues[d->queue];
    int reset = 1;
    if (d->receptionist != TH_NONE && q->size > 0) {
        const int front = q->member[0];
        th_person *p = &s->hum[front];
        if (p->h.queue_len && p->h.queue[0].kind == TH_ACTION_IDLE)
            d->queue_advance_timer++;
        // 4 + horas por día × (1 − habilidad)
        const int64_t need = 4ll * TH_MICRO + (int64_t)TH_HOURS_PER_DAY * (TH_MICRO - s->hum[d->receptionist].u.s.skill);
        if ((int64_t)d->queue_advance_timer * TH_MICRO < need) {
            reset = 0;
        } else if (p->kind == TH_HUM_PATIENT) {
            th_action sr = th_action_new(TH_ACTION_SEEK_ROOM);
            if (p->u.p.next_room) {
                sr.room_type = th_sim_room(s, p->u.p.next_room)->type;
                th_humanoid_queue_action(&p->h, sr);
            } else if (th_sim_agrees_to_pay(s, front, "diag_gp")) {
                sr.room_type = (uint8_t)th_sim_room_type(s, "gp");
                th_humanoid_queue_action(&p->h, sr);
            } else {
                th_sim_go_home(s, front, TH_HOME_OVER_PRICED);
            }
        }
        if (reset) {
            queue_pop(s, d->queue);
            q->visitor_count++;
            p->u.p.has_passed_reception = 1;
        }
    }
    if (reset)
        d->queue_advance_timer = 0;
}

// ---------------------------------------------------------------------------
// Consulta de GP (rooms/gp.lua) y lo general de Room

static void dealt_with_patient(th_sim *s, th_room *r, int patient);
static void drop_call(th_sim *s, th_room *r);
static void checkpoint_completed(th_sim *s, int room, int serial);

// GPRoom:doStaffUseCycle
static void gp_staff_cycle(th_sim *s, th_room *r, int doctor)
{
    th_humanoid *h = &s->hum[doctor].h;
    int ox, oy;
    const int cabinet = th_sim_find_object_near(s, doctor, TH_OBJ_CABINET, &ox, &oy);
    th_humanoid_set_next_action(&s->w, h, th_action_walk(ox, oy)); // walkTo
    th_action use = th_action_new(TH_ACTION_USE_OBJECT);
    use.obj = (uint8_t)cabinet;
    th_humanoid_queue_action(h, use);
    const int desk = th_sim_find_object_near(s, doctor, TH_OBJ_DESK, &ox, &oy);
    th_humanoid_queue_action(h, th_action_walk(ox, oy));
    // desk_use_time = random(floor(3 + 5 · (1 − habilidad)), ceil(8 + 10 · (1 − habilidad)))
    const int32_t inv = TH_MICRO - s->hum[doctor].u.s.skill;
    const int lo = (int)((3ll * TH_MICRO + 5ll * inv) / TH_MICRO);
    const int hi = (int)((8ll * TH_MICRO + 10ll * inv + TH_MICRO - 1) / TH_MICRO);
    th_action du = th_action_new(TH_ACTION_USE_OBJECT);
    du.obj = (uint8_t)desk;
    du.cb = 1; // CB_GP_DESK
    du.val = (int16_t)th_rng_range(&s->rng, lo, hi);
    th_humanoid_queue_action(h, du);
    (void)r;
}

// gp_loop_callback: cada vuelta del médico en la mesa.
void th_sim_gp_loop_callback(th_sim *s, int doctor, th_action *a)
{
    th_room *r = th_sim_room(s, s->hum[doctor].h.in_room);
    if (!r)
        return;
    if (--a->val != 0)
        return;
    const th_person *dp = &s->hum[doctor];
    if (dp->u.s.consultant && th_sim_random_micro(s) >= dp->u.s.fatigue)
        a->val = (int16_t)th_rng_range(&s->rng, 7, 14);
    else
        gp_staff_cycle(s, r, doctor);
    const int patient = room_patient(s, r);
    if (patient == TH_NONE)
        return;
    // math.random() <= 0,7 + 0,3 · habilidad
    if (th_sim_random_micro(s) <= 700000 + (int32_t)(3ll * dp->u.s.skill / 10) || r->max_times <= 0) {
        const th_humanoid *ph = &s->hum[patient].h;
        if (ph->user_of != TH_NONE && s->obj[ph->user_of].type != TH_OBJ_DOOR)
            dealt_with_patient(s, r, patient);
    } else {
        r->max_times--;
    }
}

// Room:commandEnteringStaff: en la consulta, el ciclo del médico (GPRoom); en
// las demás salas (aún no las hay), deambular.
static void command_entering_staff(th_sim *s, th_room *r, int staff)
{
    r->staff_member = (uint8_t)staff;
    if (r->type == th_sim_room_type(s, "gp"))
        gp_staff_cycle(s, r, staff);
    else
        th_humanoid_set_next_action(&s->w, &s->hum[staff].h, th_action_new(TH_ACTION_MEANDER));
    if (staff_criteria_met(s, r))
        drop_call(s, r);
}

// GPRoom:commandEnteringPatient
static void command_entering_patient(th_sim *s, th_room *r, int patient)
{
    th_humanoid *h = &s->hum[patient].h;
    int ox, oy;
    const int chair = th_sim_find_object_near(s, patient, TH_OBJ_CHAIR, &ox, &oy);
    th_humanoid_set_next_action(&s->w, h, th_action_walk(ox, oy));
    th_action use = th_action_new(TH_ACTION_USE_OBJECT);
    use.obj = (uint8_t)chair;
    th_humanoid_queue_action(h, use);
    r->max_times = 3;
    s->queues[s->obj[r->door].queue].visitor_count++;
}

// Patient:completeDiagnosticStep
static void complete_diagnostic_step(th_sim *s, th_room *r, int patient)
{
    th_person *p = &s->hum[patient];
    const th_disease *d = &s->dis[p->u.p.disease];
    th_rnode ex = th_rules_at(s->level, s->expertise, (uint32_t)(d->expertise_id - 1));
    const int32_t max_diag = th_rules_int(s->level, th_rules_get(s->level, ex, "MaxDiagDiff"), 0);
    // base = 0,4 · (1 − MaxDiagDiff / 1000)
    const int64_t base = 400000ll * (1000 - max_diag) / 1000;
    int64_t bonus = 400000, mult = TH_MICRO;
    if (r->staff_member != TH_NONE) {
        const th_person *doc = &s->hum[r->staff_member];
        const int64_t fatigue = doc->u.s.fatigue < 0 ? 0 : doc->u.s.fatigue;
        const int64_t tired = TH_MICRO - (fatigue - 500000); // 1 − (cansancio − 0,5)
        if (doc->u.s.skill >= 900000)
            mult = th_rng_range(&s->rng, 1, 5) * tired;
        else
            mult = tired;
        const int div = th_rng_range(&s->rng, 1, 3);
        // (atención / div + 0,4) · habilidad / div
        bonus = ((int64_t)doc->u.s.attention / div + 400000) * ((int64_t)doc->u.s.skill / div) / TH_MICRO;
    }
    int64_t prog = p->u.p.diagnosis_progress + base + bonus * mult / TH_MICRO;
    if (prog > s->hosp.stop_procedure)
        prog = s->hosp.stop_procedure;
    if (prog < 0)
        prog = 0;
    p->u.p.diagnosis_progress = (int32_t)prog;
}

// Hospital:receiveMoneyForTreatment
static void receive_money_for_treatment(th_sim *s, int patient, th_room *r)
{
    th_person *p = &s->hum[patient];
    char id[32];
    const char *disease_id;
    if (p->u.p.diagnosed) {
        disease_id = s->dis[p->u.p.disease].id;
    } else {
        if (!r)
            return;
        id[0] = 0;
        strcpy(id, "diag_");
        strncat(id, s->room_types[r->type], sizeof id - 6);
        disease_id = id;
    }
    const th_casebook *cb = th_sim_casebook(s, disease_id);
    if (!cb)
        return;
    int32_t amount = p->u.p.pay_amount > 0 ? p->u.p.pay_amount : th_sim_treatment_price(s, cb);
    if (p->u.p.insurance) {
        s->hosp.insurance[p->u.p.insurance - 1][0] += amount;
        if (s->ev.money)
            s->ev.money(s->ev.ctx, s, amount, TH_MONEY_INSURANCE_PENDING, patient);
    } else {
        th_sim_price_level_impact(s, patient, cb);
        th_sim_receive_money(s, amount, TH_MONEY_TREATMENT, patient);
    }
    p->u.p.pay_amount = 0;
}

// GPRoom:sendPatientToNextDiagnosisRoom
static void send_to_next_diagnosis_room(th_sim *s, int patient)
{
    th_person *p = &s->hum[patient];
    if (p->u.p.n_diag == 0) {
        th_sim_go_home(s, patient, TH_HOME_KICKED);
        return;
    }
    const int k = th_rng_range(&s->rng, 0, p->u.p.n_diag - 1);
    const int type = p->u.p.diag[k];
    char id[32] = "diag_";
    strncat(id, s->room_types[type], sizeof id - 6);
    if (th_sim_agrees_to_pay(s, patient, id)) {
        th_action sr = th_action_new(TH_ACTION_SEEK_ROOM);
        sr.room_type = (uint8_t)type;
        sr.diag_idx = (uint8_t)k;
        sr.state |= TH_AS_DIAG_ROOM;
        th_humanoid_queue_action(&p->h, sr);
    } else {
        th_sim_go_home(s, patient, TH_HOME_OVER_PRICED);
    }
}

// GPRoom:dealtWithPatient
static void dealt_with_patient(th_sim *s, th_room *r, int patient)
{
    if (r->just_dealt_with == patient)
        return;
    r->just_dealt_with = (uint8_t)patient;
    if (s->ev.patient)
        s->ev.patient(s->ev.ctx, s, patient, TH_EV_DEALT, 0);
    th_person *p = &s->hum[patient];
    th_humanoid_set_next_action(&s->w, &p->h, leave_action(s, r));
    if (!p->u.p.diagnosed) {
        receive_money_for_treatment(s, patient, r);
        complete_diagnostic_step(s, r, patient);
        const int done = p->u.p.diagnosis_progress >= s->hosp.stop_procedure ||
                         (p->u.p.diagnosis_progress >= TH_MICRO && p->u.p.n_diag == 0);
        if (done) {
            th_sim_set_diagnosed(s, patient);
            const th_disease *d = &s->dis[p->u.p.disease];
            if (th_sim_agrees_to_pay(s, patient, d->id)) {
                th_action sr = th_action_new(TH_ACTION_SEEK_ROOM);
                sr.room_type = d->treat[0];
                sr.state |= TH_AS_TREATMENT;
                th_humanoid_queue_action(&p->h, sr);
            } else {
                th_sim_go_home(s, patient, TH_HOME_OVER_PRICED);
            }
        } else {
            send_to_next_diagnosis_room(s, patient);
        }
    }
    if (r->staff_member != TH_NONE)
        s->hum[r->staff_member].u.s.dealing_with_patient = 0;
}

// Room:onHumanoidEnter
void th_sim_room_on_enter(th_sim *s, th_room *r, int who)
{
    th_person *p = &s->hum[who];
    p->h.in_room = r->num;
    if (p->kind != TH_HUM_PATIENT)
        p->u.s.last_room = r->num;
    r->humanoids |= 1ull << who;
    if (p->kind != TH_HUM_PATIENT) {
        if (p->u.s.on_call) // Staff:setCallCompleted
            checkpoint_completed(s, p->u.s.on_call, th_sim_room(s, p->u.s.on_call)->call_serial);
        command_entering_staff(s, r, who);
        th_sim_room_try_advance_queue(s, r);
        return;
    }
    th_sim_room_try_advance_queue(s, r);
    if (staff_criteria_met(s, r)) {
        if (r->staff_member != TH_NONE)
            s->hum[r->staff_member].u.s.dealing_with_patient = 1;
        command_entering_patient(s, r, who);
    } else {
        th_humanoid_set_next_action(&s->w, &p->h, leave_action(s, r));
        th_humanoid_queue_action(&p->h, th_sim_room_enter_action(s, r, who));
    }
}

// Room:onHumanoidLeave (con lo de GPRoom)
void th_sim_room_on_leave(th_sim *s, th_room *r, int who)
{
    th_person *p = &s->hum[who];
    if (r->staff_member == who)
        r->staff_member = TH_NONE;
    if (r->just_dealt_with == who)
        r->just_dealt_with = TH_NONE;
    p->h.in_room = 0;
    if (!in_room(r, who))
        return;
    r->humanoids &= ~(1ull << who);
    th_sim_room_try_advance_queue(s, r);
}

// Lo que hace un paciente que se va de casa estando en una sala:
// Room:makeHumanoidLeave.
void th_sim_room_make_leave(th_sim *s, th_room *r, int who)
{
    th_humanoid_set_next_action(&s->w, &s->hum[who].h, leave_action(s, r));
}

// ---------------------------------------------------------------------------
// Llamadas de personal (calls_dispatcher.lua). Una sala sin el personal que
// necesita lo llama cuando un paciente va hacia ella: acude el más adecuado de
// los que están libres o, si no hay ninguno, el primero que se quede libre (al
// empezar a deambular). Solo las llamadas a salas, y una por sala (las del
// hito 4 piden un solo miembro del personal); las de reparar, regar y vacunar
// no hacen falta aún.

// Room:isRoomInDemand, simplificado: algún paciente dentro o en la cola.
static int room_in_demand(const th_sim *s, const th_room *r)
{
    return th_sim_room_patient_count(s, r) > 0 || patient_size(&s->queues[s->obj[r->door].queue]) > 0;
}

// Staff:isIdle. Sin sala de personal, nadie va a descansar (waiting_for_staffroom
// no cuenta), así que going_to_staffroom y staffroom_needed no se ponen nunca.
static int staff_is_idle(const th_sim *s, int who)
{
    const th_person *p = &s->hum[who];
    if (!p->alive || p->kind == TH_HUM_PATIENT || p->u.s.on_call)
        return 0;
    if (p->h.user_of != TH_NONE && s->obj[p->h.user_of].type == TH_OBJ_DOOR)
        return 0;
    const th_room *r = humanoid_room((th_sim *)s, who);
    if (!r)
        return 1;
    if (p->h.queue_len && (p->h.queue[0].flags & TH_AF_IS_LEAVING))
        return 0;
    return !room_in_demand(s, r);
}

// Lo que pide la sala (getRequiredStaffCriteria): el primer criterio.
static const char *call_criterion(const th_sim *s, const th_room *r)
{
    th_rnode req = required_staff(s, r);
    return th_rules_len(s->defs, req) ? th_rules_str(s->defs, th_rules_entry_key(s->defs, req, 0), NULL) : NULL;
}

// CallsDispatcher.verifyStaffForRoom (staff_allowed_to_move es true por defecto)
static int verify_staff_for_room(const th_sim *s, const th_room *r, int who)
{
    const char *crit = call_criterion(s, r);
    return crit && staff_is_idle(s, who) && fulfills(&s->hum[who], crit);
}

// CallsDispatcher.getPriorityForRoom, en millonésimas (cuanto menos, mejor).
static int64_t priority_for_room(th_sim *s, const th_room *r, int who)
{
    const th_person *p = &s->hum[who];
    int x, y;
    entrance(s, r, 1, &x, &y);
    const int d = th_path_find(s->w.pf, s->w.map, p->h.tile_x, p->h.tile_y, x, y);
    int64_t score = d >= 0 ? (int64_t)d * TH_MICRO : 0;
    score -= (int64_t)s->queues[s->obj[r->door].queue].reported * 5 * TH_MICRO;
    score -= (int64_t)(p->u.s.fatigue > 0 ? p->u.s.fatigue : 0) * 40;
    if (!humanoid_room(s, who))
        score -= 50 * TH_MICRO;
    const char *crit = call_criterion(s, r);
    if (crit && (!strcmp(crit, "Researcher") || !strcmp(crit, "Psychiatrist") || !strcmp(crit, "Surgeon")))
        score -= 100000ll * TH_MICRO;
    return score;
}

// CallsDispatcher.sendStaffToRoom (desde fuera de la sala)
static void execute_call(th_sim *s, th_room *r, int who)
{
    r->call_staff = (uint8_t)who;
    s->hum[who].u.s.on_call = r->num;
    th_humanoid *h = &s->hum[who].h;
    th_humanoid_set_next_action(&s->w, h, th_sim_room_enter_action(s, r, who));
    th_action cp = th_action_new(TH_ACTION_CALL_CHECKPOINT);
    cp.obj = r->num;
    cp.val = r->call_serial;
    th_humanoid_queue_action(h, cp);
}

// CallsDispatcher:findSuitableStaff: el de menor puntuación entre los que
// valen. Con uno solo no hace falta puntuar (la puntuación no gasta azar).
static void find_suitable_staff(th_sim *s, th_room *r)
{
    int best = TH_NONE, n = 0;
    int64_t best_score = 0;
    uint8_t cand[TH_SIM_HUMANOIDS];
    for (int k = 0; k < s->n_order; k++) {
        const int who = s->order[k];
        const th_person *p = &s->hum[who];
        if (p->kind != TH_HUM_PATIENT && p->kind != TH_HUM_HANDYMAN && verify_staff_for_room(s, r, who))
            cand[n++] = (uint8_t)who;
    }
    for (int i = 0; i < n; i++) {
        const int64_t score = n > 1 ? priority_for_room(s, r, cand[i]) : 0;
        if (best == TH_NONE || score < best_score) {
            best = cand[i];
            best_score = score;
        }
    }
    if (best != TH_NONE)
        execute_call(s, r, best);
}

// CallsDispatcher:callForStaff
void th_sim_call_for_staff(th_sim *s, th_room *r)
{
    if (!r || staff_criteria_met(s, r) || !call_criterion(s, r) || r->call_open)
        return;
    r->call_open = 1;
    r->call_staff = TH_NONE;
    r->call_serial++;
    find_suitable_staff(s, r);
}

// CallsDispatcher.unassignCall
static void unassign_call(th_sim *s, th_room *r, int answer_next)
{
    const int who = r->call_staff;
    r->call_staff = TH_NONE;
    s->hum[who].u.s.on_call = 0;
    if (answer_next)
        th_humanoid_set_next_action(&s->w, &s->hum[who].h, th_action_new(TH_ACTION_ANSWER_CALL));
}

// CallsDispatcher:dropFromQueue
static void drop_call(th_sim *s, th_room *r)
{
    if (!r->call_open)
        return;
    r->call_open = 0;
    if (r->call_staff != TH_NONE)
        unassign_call(s, r, 1);
}

// CallsDispatcher.onCheckpointCompleted (CallCheckPointAction al empezar y
// Staff:setCallCompleted al entrar en la sala).
static void checkpoint_completed(th_sim *s, int room, int serial)
{
    th_room *r = th_sim_room(s, room);
    if (!r || !r->call_open || r->call_serial != serial || r->call_staff == TH_NONE)
        return;
    s->hum[r->call_staff].u.s.on_call = 0;
    r->call_staff = TH_NONE;
    r->call_open = 0;
}

void th_sim_call_checkpoint(th_sim *s, int room, int serial)
{
    checkpoint_completed(s, room, serial);
}

// CallsDispatcher.staffActionInterruptHandler: la CallCheckPointAction se
// quita de la cola sin llegar a empezar. La llamada se queda sin nadie (el
// callForStaff que sigue la encuentra en la cola y no busca a otro): la
// atenderá el primero que se quede libre.
void th_sim_call_checkpoint_removed(th_sim *s, int who, int room, int serial)
{
    th_room *r = th_sim_room(s, room);
    if (!r || !r->call_open || r->call_serial != serial || r->call_staff != who)
        return;
    r->call_staff = TH_NONE;
    s->hum[who].u.s.on_call = 0;
}

// CallsDispatcher:answerCall: la mejor llamada para este miembro del
// personal, quitándosela a otro si la atiende peor.
int th_sim_answer_call(th_sim *s, int who)
{
    const th_person *p = &s->hum[who];
    if (p->kind == TH_HUM_HANDYMAN || p->kind == TH_HUM_PATIENT)
        return 0;
    th_room *best = NULL;
    int64_t best_score = 0;
    for (int i = 0; i < s->n_rooms; i++) {
        th_room *r = &s->rooms[i];
        if (!r->call_open || !verify_staff_for_room(s, r, who))
            continue;
        const int64_t score = priority_for_room(s, r, who);
        if (r->call_staff != TH_NONE && priority_for_room(s, r, r->call_staff) <= score)
            continue;
        if (!best || score < best_score) {
            best = r;
            best_score = score;
        }
    }
    if (!best)
        return 0;
    if (best->call_staff != TH_NONE)
        unassign_call(s, best, 1);
    execute_call(s, best, who);
    return 1;
}

// El principio de MeanderAction para el personal libre en el pasillo: atender
// una llamada o volver a su última sala si aún le hace falta. Devuelve 1 si
// ya no deambula.
int th_sim_staff_meander(th_sim *s, int who, int must_happen)
{
    th_person *p = &s->hum[who];
    if (p->kind == TH_HUM_PATIENT || humanoid_room(s, who) || !staff_is_idle(s, who))
        return 0;
    th_humanoid *h = &p->h;
    if (p->kind != TH_HUM_HANDYMAN && th_sim_answer_call(s, who)) {
        if (must_happen)
            th_humanoid_finish_action(&s->w, h);
        return 1;
    }
    // testStaffCriteria(getMaximumStaffCriteria(), humanoid): en las salas del
    // hito 4 el máximo es lo requerido.
    th_room *r = p->u.s.last_room ? th_sim_room(s, p->u.s.last_room) : NULL;
    const char *crit = r ? call_criterion(s, r) : NULL;
    if (r && r->is_active && crit && !staff_criteria_met(s, r) && fulfills(p, crit)) {
        th_humanoid_queue_action(h, th_sim_room_enter_action(s, r, who));
        th_humanoid_finish_action(&s->w, h);
        return 1;
    }
    p->u.s.last_room = 0;
    return 0;
}

// AnswerCallAction
void th_sim_answer_call_action(th_sim *s, int who)
{
    th_humanoid *h = &s->hum[who].h;
    if (!th_sim_answer_call(s, who)) {
        th_room *r = humanoid_room(s, who);
        if (r)
            th_humanoid_queue_action(h, leave_action(s, r));
        th_humanoid_queue_action(h, th_action_new(TH_ACTION_MEANDER));
    }
    th_humanoid_finish_action(&s->w, h);
}
