// Simulación del hospital (th/sim.h): el reloj, las llegadas, el hospital, el
// personal, la temperatura y la carga del escenario. Las salas, los objetos y
// las colas están en sim_rooms.c; las acciones, en humanoid.c.
#include "th/sim.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Fecha (date.lua)

static const uint8_t month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

int th_date_last_day(int month)
{
    return month_days[(month - 1) % 12];
}

th_date th_date_plus_hours(th_date d, int hours)
{
    int h = d.hour + hours;
    while (h >= TH_HOURS_PER_DAY) {
        h -= TH_HOURS_PER_DAY;
        if (++d.day > th_date_last_day(d.month)) {
            d.day = 1;
            if (++d.month > 12) {
                d.month = 1;
                d.year++;
            }
        }
    }
    d.hour = (uint8_t)h;
    return d;
}

// ---------------------------------------------------------------------------
// Números aleatorios

int32_t th_sim_random_micro(th_sim *s)
{
    return (int32_t)(((uint64_t)th_rng_u32(&s->rng) * TH_MICRO) >> 32);
}

// e^(−x) en Q32, con x en millonésimas (x >= 0): e^(−k) · e^(−f), la parte
// fraccionaria por su serie de Taylor.
static uint64_t exp_neg_q32(int64_t x)
{
    static const uint64_t e_inv = 1580030169u; // e^(−1) · 2^32
    uint64_t r = 1ull << 32;
    while (x >= TH_MICRO && r) {
        r = (r * e_inv) >> 32;
        x -= TH_MICRO;
    }
    const uint64_t f = ((uint64_t)x << 32) / TH_MICRO; // Q32, < 1
    uint64_t t = 1ull << 32;
    int64_t sum = (int64_t)t;
    for (int n = 1; n < 16 && t; n++) {
        t = ((t * f) >> 32) / (uint64_t)n;
        sum += (n & 1) ? -(int64_t)t : (int64_t)t;
    }
    return (r * (uint64_t)sum) >> 32;
}

// p × u >> 32 sin desbordar (p < 2^48, u < 2^32).
static uint64_t mul_q32(uint64_t p, uint32_t u)
{
    return (p >> 32) * u + (((p & 0xFFFFFFFFu) * u) >> 32);
}

// math.p_random(mean) de app.lua (Knuth, con el exceso de la media aplicado
// por tramos para no salirse de rango); la media en millonésimas.
static int p_random(th_sim *s, int64_t mean)
{
    const int64_t step = 10ll * TH_MICRO;
    const uint64_t one = 1ull << 32;
    int64_t left = mean;
    uint64_t p = one;
    int k = 0;
    for (;;) {
        k++;
        p = mul_q32(p, th_rng_u32(&s->rng)); // xorshift nunca da 0
        while (p < one && left > 0) {
            const int64_t chunk = left > step ? step : left;
            const uint64_t e = (1ull << 48) / exp_neg_q32(chunk); // e^chunk en Q16
            p = (p * e) >> 16;
            left -= chunk;
        }
        if (p <= one)
            break;
        if (k > 100000)
            break;
    }
    return k - 1;
}

// ---------------------------------------------------------------------------
// Utilidades de las reglas

static th_rnode get(const th_rules *r, th_rnode n, const char *k)
{
    return th_rules_get(r, n, k);
}

static int dir_from(const th_rules *r, th_rnode n)
{
    const char *d = th_rules_str(r, n, NULL);
    if (!d)
        return TH_DIR_NORTH;
    if (!strcmp(d, "east"))
        return TH_DIR_EAST;
    if (!strcmp(d, "south"))
        return TH_DIR_SOUTH;
    if (!strcmp(d, "west"))
        return TH_DIR_WEST;
    return TH_DIR_NORTH;
}

int th_sim_room_type(const th_sim *s, const char *id)
{
    for (int i = 0; i < s->n_room_types; i++)
        if (!strcmp(s->room_types[i], id))
            return i;
    return TH_NONE;
}

const th_casebook *th_sim_casebook(const th_sim *s, const char *id)
{
    for (int i = 0; i < s->n_case; i++)
        if (!strcmp(s->casebook[i].id, id))
            return &s->casebook[i];
    return NULL;
}

static th_casebook *casebook_mut(th_sim *s, const char *id)
{
    return (th_casebook *)th_sim_casebook(s, id);
}

static int disease_index(const th_sim *s, const char *id)
{
    for (int i = 0; i < s->n_dis; i++)
        if (!strcmp(s->dis[i].id, id))
            return i;
    return TH_NONE;
}

int th_sim_anim_length(const th_sim *s, unsigned anim)
{
    // Duraciones fijadas por el Lua de CorsixTH (setAnimLength).
    th_rnode list = get(s->defs, s->defs->root, "duraciones");
    const uint32_t n = th_rules_len(s->defs, list);
    for (uint32_t i = 0; i < n; i++) {
        th_rnode e = th_rules_at(s->defs, list, i);
        if ((unsigned)th_rules_int(s->defs, get(s->defs, e, "animacion"), -1) == anim)
            return th_rules_int(s->defs, get(s->defs, e, "duracion"), 1);
    }
    return (int)th_anims_length(s->w.anims, anim);
}

static void change_attr(int32_t *v, int64_t amount)
{
    int64_t x = *v + amount;
    *v = (int32_t)(x > TH_MICRO ? TH_MICRO : x < 0 ? 0 : x);
}

// Humanoid:changeAttribute("happiness"): la recepcionista y quien cobra el
// máximo nunca están descontentos.
static void change_happiness(th_sim *s, th_person *p, int64_t amount)
{
    if (p->kind == TH_HUM_RECEPTIONIST || (p->kind != TH_HUM_PATIENT && p->u.s.wage >= s->max_salary)) {
        p->happiness = TH_MICRO;
        return;
    }
    change_attr(&p->happiness, amount);
}

// ---------------------------------------------------------------------------
// Dinero y reputación (hospital.lua)

static void event_money(th_sim *s, int32_t amount, int reason, int who)
{
    if (s->ev.money)
        s->ev.money(s->ev.ctx, s, amount, reason, who);
}

void th_sim_receive_money(th_sim *s, int32_t amount, int reason, int who)
{
    s->hosp.balance += amount;
    s->hosp.money_in += amount;
    event_money(s, amount, reason, who);
}

void th_sim_spend_money(th_sim *s, int32_t amount, int reason)
{
    s->hosp.balance -= amount;
    s->hosp.money_out += amount;
    event_money(s, -amount, reason, TH_NONE);
}

// Hospital:getTreatmentPrice
int32_t th_sim_treatment_price(const th_sim *s, const th_casebook *cb)
{
    const int64_t rep = cb->reputation != INT32_MIN ? cb->reputation : s->hosp.reputation;
    int64_t num = (int64_t)cb->cure_price * cb->price; // × 10^6
    int64_t den = TH_MICRO;
    if (rep >= 500) {
        num *= rep;
        den *= 500;
    }
    return (int32_t)((num + den - 1) / den); // math.ceil
}

// Hospital:isReputationChangeAllowed y unconditionalChangeReputation
static void change_reputation(th_sim *s, int amount, const char *disease)
{
    const int32_t rep = s->hosp.reputation;
    int allowed = (amount > 0 && rep <= 500) || (amount < 0 && rep >= 500) || amount == 0;
    if (!allowed) {
        // 1 − (a·x² − b·x + c), con a = 0,000004008, b = 0,004008, c = 1
        const int64_t x = rep;
        const int64_t refused = 4008ll * x * x / 1000 - 4008ll * x + TH_MICRO; // millonésimas
        allowed = th_sim_random_micro(s) <= TH_MICRO - refused;
    }
    if (allowed) {
        int32_t r = rep + amount;
        s->hosp.reputation = r < 0 ? 0 : r > 1000 ? 1000 : r;
    }
    th_casebook *cb = disease ? casebook_mut(s, disease) : NULL;
    if (cb && cb->reputation != INT32_MIN)
        cb->reputation += amount;
}

// Hospital:computePriceLevelImpact con Patient:getPriceDistortion
void th_sim_price_level_impact(th_sim *s, int who, const th_casebook *cb)
{
    th_person *p = &s->hum[who];
    const int64_t rep = cb->reputation != INT32_MIN ? cb->reputation : s->hosp.reputation;
    // esperado = 0,1 · felicidad + 0,6 · reputación / 1000 + 0,3 · eficacia / 100
    const int64_t expected = p->happiness / 10 + 600ll * rep + 3000ll * cb->effectiveness;
    // nivel = (precio − 0,5) / 3 · 2
    const int64_t level = ((int64_t)cb->price - 500000) * 2 / 3;
    const int64_t distortion = level - expected;
    change_happiness(s, p, -distortion / 2);
    if (distortion < s->hosp.under_priced) {
        if (th_rng_range(&s->rng, 1, 100) == 1)
            change_reputation(s, 1, NULL);
    } else if (distortion > s->hosp.over_priced) {
        if (th_rng_range(&s->rng, 1, 100) == 1)
            change_reputation(s, -2, NULL);
    }
}

// ---------------------------------------------------------------------------
// Pacientes (patient.lua)

static void event_patient(th_sim *s, int who, int event, int arg)
{
    if (s->ev.patient)
        s->ev.patient(s->ev.ctx, s, who, event, arg);
}

int th_sim_agrees_to_pay(th_sim *s, int who, const char *disease_id)
{
    const th_casebook *cb = th_sim_casebook(s, disease_id);
    if (!cb)
        return 1;
    int agrees = 1;
    if (cb->price > TH_MICRO) {
        // exp(−4 · (precio − 1))
        const uint64_t chance = exp_neg_q32(4ll * (cb->price - TH_MICRO));
        agrees = ((uint64_t)th_sim_random_micro(s) << 32) / TH_MICRO <= chance;
    }
    if (agrees)
        s->hum[who].u.p.pay_amount = th_sim_treatment_price(s, cb);
    return agrees;
}

void th_sim_set_diagnosed(th_sim *s, int who)
{
    s->hum[who].u.p.diagnosed = 1;
    event_patient(s, who, TH_EV_DIAGNOSED, 0);
}

// Humanoid.despawn: hacia un punto de aparición al azar, y fuera.
static void despawn(th_sim *s, int who)
{
    th_action a = th_action_new(TH_ACTION_SPAWN);
    a.dir = 1;
    a.aux = (uint8_t)th_rng_range(&s->rng, 0, s->n_spawn - 1);
    a.flags |= TH_AF_MUST_HAPPEN;
    th_humanoid_set_next_action(&s->w, &s->hum[who].h, a);
}

// UIBottomPanel:deleteMessage: los mensajes de quien se va desaparecen.
static void drop_faxes(th_sim *s, int who)
{
    int k = 0;
    for (int i = 0; i < s->n_fax; i++)
        if (s->fax[i].who != who)
            s->fax[k++] = s->fax[i];
    s->n_fax = (uint8_t)k;
    k = 0;
    for (int i = 0; i < s->n_shown; i++)
        if (s->shown[i].who != who)
            s->shown[k++] = s->shown[i];
    s->n_shown = (uint8_t)k;
}

// Patient:goHome
void th_sim_go_home(th_sim *s, int who, int reason)
{
    th_person *p = &s->hum[who];
    if (p->u.p.going_home) {
        despawn(s, who);
        return;
    }
    if (reason == TH_HOME_KICKED || reason == TH_HOME_OVER_PRICED) {
        if (reason == TH_HOME_OVER_PRICED)
            change_happiness(s, p, -500000);
        change_reputation(s, reason == TH_HOME_KICKED ? -3 : -2, s->dis[p->u.p.disease].id);
        s->hosp.not_cured++;
    } else if (reason == TH_HOME_CURED) {
        change_happiness(s, p, 800000);
        change_reputation(s, 1, s->dis[p->u.p.disease].id);
        s->hosp.num_cured++;
    }
    event_patient(s, who, TH_EV_GO_HOME, reason);
    drop_faxes(s, who);
    p->u.p.going_home = 1;
    p->u.p.waiting = -1;
    th_room *r = p->h.in_room ? th_sim_room(s, p->h.in_room)
                              : (p->h.tile_x >= 0 ? th_sim_room_at(s, p->h.tile_x, p->h.tile_y) : NULL);
    if (r)
        th_sim_room_make_leave(s, r, who);
    despawn(s, who);
}

void th_sim_send_fax(th_sim *s, int who, int kind)
{
    if (s->n_fax >= TH_SIM_FAXES)
        return;
    s->fax[s->n_fax].who = (uint8_t)who;
    s->fax[s->n_fax].kind = (uint8_t)kind;
    s->n_fax++;
    if (kind != TH_FAX_RAISE)
        s->hum[who].u.p.message = (uint8_t)kind;
}

// Quita a alguien del mundo (World:destroyEntity) y borra lo que lo nombra.
void th_sim_destroy(th_sim *s, int who)
{
    th_person *p = &s->hum[who];
    if (p->kind == TH_HUM_PATIENT)
        event_patient(s, who, TH_EV_DESPAWN, 0);
    drop_faxes(s, who);
    for (int i = 0; i < s->n_obj; i++) {
        if (s->obj[i].user == who)
            s->obj[i].user = TH_NONE;
        if (s->obj[i].reserved_for == who)
            s->obj[i].reserved_for = TH_NONE;
        if (s->obj[i].receptionist == who)
            s->obj[i].receptionist = TH_NONE;
    }
    for (int i = 0; i < s->n_rooms; i++) {
        th_room *r = &s->rooms[i];
        r->humanoids &= ~(1ull << who);
        if (r->staff_member == who)
            r->staff_member = TH_NONE;
        if (r->just_dealt_with == who)
            r->just_dealt_with = TH_NONE;
    }
    for (int q = 0; q < s->n_queues; q++) {
        th_sim_queue_remove_value(s, q, who);
        th_sim_queue_unexpect(s, q, who);
    }
    int k = 0;
    for (int i = 0; i < s->n_order; i++)
        if (s->order[i] != who)
            s->order[k++] = s->order[i];
    s->n_order = (uint8_t)k;
    p->alive = 0;
}

static int new_person(th_sim *s, int kind, const th_walk_anims *walk)
{
    int who = TH_NONE;
    for (int i = 0; i < TH_SIM_HUMANOIDS; i++)
        if (!s->hum[i].alive) {
            who = i;
            break;
        }
    if (who == TH_NONE || !walk)
        return TH_NONE;
    th_person *p = &s->hum[who];
    memset(p, 0, sizeof *p);
    p->alive = 1;
    p->kind = (uint8_t)kind;
    p->happiness = TH_MICRO;
    p->warmth = 290000;
    if (kind == TH_HUM_PATIENT) {
        p->u.p.waiting = -1;
    } else {
        p->u.s.timer_until_raise = -1;
        p->u.s.quitting_in = -1;
        p->u.s.associated_desk = TH_NONE;
    }
    s->order[s->n_order++] = (uint8_t)who;
    th_humanoid_init(&s->w, &p->h, walk);
    return who;
}

// disease.initPatient: la clase y las capas del paciente, con la distribución
// exacta que sacó el conversor (aspecto: tipos con su peso y bloques de capas).
static const th_walk_anims *init_patient_look(th_sim *s, th_person *p, const th_disease *d)
{
    const th_rules *r = s->defs;
    th_rnode asp = get(r, d->def, "aspecto");
    th_rnode tipos = get(r, asp, "tipos");
    int32_t pick = th_rng_range(&s->rng, 0, th_rules_int(r, get(r, asp, "total"), 1) - 1);
    th_rnode tipo = 0;
    for (uint32_t i = 0; i < th_rules_len(r, tipos); i++) {
        tipo = th_rules_at(r, tipos, i);
        const int32_t w = th_rules_int(r, get(r, tipo, "peso"), 0);
        if (pick < w)
            break;
        pick -= w;
    }
    const int32_t peso = th_rules_int(r, get(r, tipo, "peso"), 1);
    th_rnode bloques = get(r, tipo, "bloques");
    for (uint32_t b = 0; b < th_rules_len(r, bloques); b++) {
        th_rnode bl = th_rules_at(r, bloques, b);
        th_rnode capas = get(r, bl, "capas"), valores = get(r, bl, "valores");
        int32_t v = th_rng_range(&s->rng, 0, peso - 1);
        th_rnode elegido = 0;
        for (uint32_t i = 0; i < th_rules_len(r, valores); i++) {
            elegido = th_rules_at(r, valores, i);
            const int32_t w = th_rules_int(r, th_rules_at(r, elegido, 1), 0);
            if (v < w)
                break;
            v -= w;
        }
        th_rnode vals = th_rules_at(r, elegido, 0);
        for (uint32_t c = 0; c < th_rules_len(r, capas); c++)
            th_humanoid_set_layer(&p->h, th_rules_int(r, th_rules_at(r, capas, c), 0),
                                  th_rules_int(r, th_rules_at(r, vals, c), 0));
    }
    return th_walk_anims_find(th_rules_str(r, get(r, tipo, "tipo"), NULL));
}

// isDiseaseUsableForNewPatient
static int disease_usable(const th_sim *s, const th_disease *d)
{
    if (d->only_emergency)
        return 0;
    const int month = th_date_month_of_game(s->date);
    if (d->visuals_id >= 0) {
        if ((s->hold_visual_months >= 0 && s->hold_visual_months > month) ||
            (s->hold_visual_peeps >= 0 && s->hold_visual_peeps > s->hosp.num_visitors))
            return 0;
        return th_rules_int(s->level, get(s->level, th_rules_geti(s->level, s->visuals_available, d->visuals_id), "Value"),
                            0) < month;
    }
    if (d->non_visuals_id >= 0)
        return th_rules_int(s->level,
                            get(s->level, th_rules_geti(s->level, s->non_visuals_available, d->non_visuals_id), "Value"),
                            0) < month;
    return 0;
}

static int staffed_desk(const th_sim *s)
{
    for (int i = 0; i < s->n_desks; i++)
        if (s->obj[s->desks[i]].receptionist != TH_NONE || s->obj[s->desks[i]].reserved_for != TH_NONE)
            return 1;
    return 0;
}

// World:spawnPatient
static void spawn_patient(th_sim *s)
{
    if (s->n_avail == 0 || s->n_spawn == 0 || !staffed_desk(s))
        return;
    int dis = s->avail[th_rng_range(&s->rng, 0, s->n_avail - 1)];
    if (!disease_usable(s, &s->dis[dis])) {
        uint8_t usable[TH_SIM_DISEASES];
        int n = 0;
        for (int i = 0; i < s->n_avail; i++)
            if (disease_usable(s, &s->dis[s->avail[i]]))
                usable[n++] = s->avail[i];
        if (n == 0)
            return;
        dis = usable[th_rng_range(&s->rng, 0, n - 1)];
    }
    int point = TH_NONE;
    if (s->hosp.num_visitors == 0) {
        // El primer paciente llega por el punto más cercano a un mostrador atendido.
        int best = -1;
        for (int i = 0; i < s->n_spawn; i++)
            for (int k = 0; k < s->n_desks; k++) {
                const th_object *d = &s->obj[s->desks[k]];
                if (d->receptionist == TH_NONE && d->reserved_for == TH_NONE)
                    continue;
                const int dist = th_path_find(s->w.pf, s->w.map, d->x, d->y, s->spawn[i].x, s->spawn[i].y);
                if (dist >= 0 && (best < 0 || dist < best)) {
                    best = dist;
                    point = i;
                }
            }
    }
    if (point == TH_NONE)
        point = th_rng_range(&s->rng, 0, s->n_spawn - 1);
    // newEntity("Patient", 2, 1), con el aspecto de la enfermedad.
    th_person tmp;
    memset(&tmp, 0, sizeof tmp);
    const th_walk_anims *walk = init_patient_look(s, &tmp, &s->dis[dis]);
    const int who = new_person(s, TH_HUM_PATIENT, walk);
    if (who == TH_NONE)
        return;
    th_person *p = &s->hum[who];
    memcpy(p->h.layers, tmp.h.layers, sizeof p->h.layers);
    p->u.p.disease = (uint8_t)dis;
    const th_disease *d = &s->dis[dis];
    p->u.p.n_diag = d->n_diag;
    memcpy(p->u.p.diag, d->diag, sizeof p->u.p.diag);
    if (th_rng_range(&s->rng, 1, 4) == 1)
        p->u.p.insurance = (uint8_t)th_rng_range(&s->rng, 1, 3);
    p->u.p.id = ++s->patients_spawned;
    th_action sp = th_action_new(TH_ACTION_SPAWN);
    sp.dir = 0;
    sp.aux = (uint8_t)point;
    th_humanoid_set_next_action(&s->w, &p->h, sp);
    // Patient:setHospital: a buscar la recepción.
    th_humanoid_set_next_action(&s->w, &p->h, th_action_new(TH_ACTION_SEEK_RECEPTION));
    s->hosp.num_visitors++;
    event_patient(s, who, TH_EV_SPAWN, 0);
}

// ---------------------------------------------------------------------------
// Personal (staff.lua, receptionist.lua, staff_profile.lua)

// StaffProfile:getFairWage
static int32_t fair_wage(const th_sim *s, const th_person *p)
{
    const int conf = p->kind == TH_HUM_NURSE ? 0 : p->kind == TH_HUM_DOCTOR ? 1 : p->kind == TH_HUM_HANDYMAN ? 2 : 3;
    int64_t wage = (int64_t)s->min_salary[conf] * TH_MICRO + (int64_t)p->u.s.skill * 1000 / s->salary_ability_div;
    if (p->kind == TH_HUM_DOCTOR) {
        if (p->u.s.surgeon)
            wage += (int64_t)s->salary_add[5 - 1] * TH_MICRO;
        if (p->u.s.psychiatrist)
            wage += (int64_t)s->salary_add[6 - 1] * TH_MICRO;
        if (p->u.s.researcher)
            wage += (int64_t)s->salary_add[8 - 1] * TH_MICRO;
        if (!p->u.s.junior && !p->u.s.consultant)
            wage += (int64_t)s->salary_add[4 - 1] * TH_MICRO;
    }
    const int32_t w = (int32_t)(wage / TH_MICRO);
    return w > s->min_salary[conf] ? w : s->min_salary[conf];
}

static int very_tired(const th_sim *s, const th_person *p)
{
    return p->u.s.fatigue >= 0 && (int64_t)p->u.s.fatigue * 1000 >= (int64_t)s->very_tired * TH_MICRO;
}

static int crackup_tired(const th_sim *s, const th_person *p)
{
    return p->u.s.fatigue >= 0 && (int64_t)p->u.s.fatigue * 1000 >= (int64_t)s->crackup_tired * TH_MICRO;
}

static th_room *person_room(th_sim *s, th_person *p)
{
    if (p->h.in_room)
        return th_sim_room(s, p->h.in_room);
    return p->h.tile_x >= 0 ? th_sim_room_at(s, p->h.tile_x, p->h.tile_y) : NULL;
}

// Staff:requestRaise
static void request_raise(th_sim *s, int who)
{
    th_person *p = &s->hum[who];
    if (p->u.s.wage >= s->max_salary) {
        p->u.s.timer_until_raise = -1;
        return;
    }
    if (p->u.s.pay_rise)
        return;
    // floor(max(1,1 · sueldo, (justo + sueldo) / 2) − sueldo), con los dos términos × 10
    const int64_t w = p->u.s.wage;
    const int64_t a10 = w * 11, b10 = (fair_wage(s, p) + w) * 5;
    int32_t amount = (int32_t)((a10 > b10 ? a10 : b10) / 10 - w);
    if (p->u.s.wage + amount > s->max_salary)
        amount = s->max_salary - p->u.s.wage;
    if (th_rng_range(&s->rng, 1, 5) != 1 || amount <= 0) {
        p->u.s.timer_until_raise = -1;
        return;
    }
    p->u.s.rise_amount = amount;
    th_sim_send_fax(s, who, TH_FAX_RAISE);
    p->u.s.quitting_in = (int16_t)(25 * 30 + th_rng_range(&s->rng, 0, 50));
    p->u.s.pay_rise = 1;
}

// Staff:increaseWage
static void increase_wage(th_sim *s, th_person *p, int32_t amount)
{
    if (p->u.s.wage < s->max_salary) {
        int32_t w = p->u.s.wage + amount;
        p->u.s.wage = w > s->max_salary ? s->max_salary : w;
    }
    p->u.s.pay_rise = 0;
    change_happiness(s, p, 990000);
}

// Staff:tick, después de Entity.tick
static void staff_tick(th_sim *s, int who)
{
    th_person *p = &s->hum[who];
    th_room *room = person_room(s, p);
    if (p->kind != TH_HUM_RECEPTIONIST) {
        // checkIfNeedRest (sin sala de personal en el hito 4: se queda esperándola)
        if (very_tired(s, p))
            change_happiness(s, p, -200);
        if (p->u.s.fatigue >= s->hosp.goto_staffroom) {
            if (p->u.s.waiting_for_staffroom)
                change_happiness(s, p, -1000);
            else
                p->u.s.waiting_for_staffroom = 1;
        }
    }
    // checkIfWaitedTooLongForRaise
    if (p->u.s.quitting_in >= 0 && --p->u.s.quitting_in < 0) {
        p->u.s.quitting_in = -1;
        if (s->hosp.grant_wage_increase)
            increase_wage(s, p, p->u.s.rise_amount);
    }
    // isTiring: dentro de una sala (que no sea la de personal) y, los bedeles,
    // también fuera; la recepcionista, nunca.
    if (p->kind != TH_HUM_RECEPTIONIST && (room || p->kind == TH_HUM_HANDYMAN)) {
        change_attr(&p->u.s.fatigue, 90);
        change_happiness(s, p, -20);
    }
    if (p->happiness < 100000) {
        if (p->u.s.timer_until_raise < 0)
            p->u.s.timer_until_raise = 200;
        if (p->u.s.timer_until_raise == 0)
            request_raise(s, who);
        else
            p->u.s.timer_until_raise--;
    } else {
        p->u.s.timer_until_raise = -1;
    }
    // updateSpeed
    int level = p->u.s.junior ? 1 : p->u.s.consultant ? 3 : 2;
    if (p->kind != TH_HUM_RECEPTIONIST) {
        if (crackup_tired(s, p))
            level -= 2;
        else if (very_tired(s, p))
            level -= 1;
    }
    p->h.speed = (uint8_t)(level >= 3 ? 2 : level <= 1 ? 0 : 1);
    p->h.slow_animation = level <= 1;
}

static uint16_t temperature_at(const th_sim *s, int x, int y)
{
    return s->temp[s->temp_idx][y * TH_MAP_WIDTH + x];
}

// Humanoid:tickDay: el calor y la felicidad. Devuelve 0 si no sigue.
static int humanoid_tick_day(th_sim *s, th_person *p)
{
    if (p->kind == TH_HUM_PATIENT && p->u.p.going_home)
        return 0;
    if (p->h.tile_x < 0)
        return 1;
    const int64_t t = (int64_t)temperature_at(s, p->h.tile_x, p->h.tile_y) * TH_MICRO / 65535;
    p->warmth = (int32_t)(((int64_t)p->warmth * 3 + t) / 4);
    if (p->warmth < 220000)
        change_happiness(s, p, -(220000 - (int64_t)p->warmth) / 10);
    else if (p->warmth > 360000)
        change_happiness(s, p, -((int64_t)p->warmth - 360000) / 10);
    else
        change_happiness(s, p, 5000);
    return 1;
}

static void staff_tick_day(th_sim *s, int who)
{
    th_person *p = &s->hum[who];
    if (!humanoid_tick_day(s, p) || p->kind == TH_HUM_RECEPTIONIST)
        return;
    const int32_t fair = fair_wage(s, p);
    change_happiness(s, p, 50000ll * (p->u.s.wage - fair) / (fair ? fair : 1));
    // Nunca descansa (no hay sala de personal).
    if (p->u.s.fatigue >= s->hosp.goto_staffroom)
        change_happiness(s, p, -20000);
    if (p->u.s.fatigue >= 500000)
        change_happiness(s, p, -10000);
    th_room *r = person_room(s, p);
    if (r)
        change_happiness(s, p, r->happiness_factor);
}

static void patient_tick_day(th_sim *s, int who)
{
    th_person *p = &s->hum[who];
    if (p->u.p.waiting >= 0) {
        if (--p->u.p.waiting == 0) {
            th_sim_go_home(s, who, TH_HOME_KICKED);
            return;
        }
    }
    humanoid_tick_day(s, p);
}

// ---------------------------------------------------------------------------
// Faxes: el jugador del arnés de CorsixTH los abre en cuanto llegan

static void answer_fax(th_sim *s, int who, int kind)
{
    th_person *p = &s->hum[who];
    if (!p->alive)
        return;
    if (kind == TH_FAX_RAISE) {
        increase_wage(s, p, p->u.s.rise_amount);
        p->u.s.quitting_in = -1;
        return;
    }
    if (p->u.p.going_home)
        return;
    p->u.p.message = 0;
    const th_disease *d = &s->dis[p->u.p.disease];
    const th_casebook *cb = th_sim_casebook(s, d->id);
    const int guess = kind == TH_FAX_NO_DIAGNOSIS && cb && cb->discovered && s->answer == TH_ANSWER_GUESS_THEN_SEND_HOME;
    if (!guess) {
        th_sim_go_home(s, who, TH_HOME_KICKED);
        return;
    }
    th_sim_set_diagnosed(s, who);
    if (th_sim_agrees_to_pay(s, who, d->id)) {
        th_action sr = th_action_new(TH_ACTION_SEEK_ROOM);
        sr.room_type = d->treat[0];
        sr.state |= TH_AS_TREATMENT;
        th_humanoid_set_next_action(&s->w, &p->h, sr);
    } else {
        th_sim_go_home(s, who, TH_HOME_OVER_PRICED);
    }
}

// Tipo de mensaje del panel: los faxes de pacientes son "information" y las
// subidas de sueldo, "strike". No se enseñan dos del mismo tipo a la vez.
static int fax_type(int kind)
{
    return kind == TH_FAX_RAISE;
}

// UIBottomPanel:_messageDoorTick, una vez por tick de la interfaz: la puerta
// se abre de 22 a 0 mientras haya algo que enseñar, enseña un mensaje y
// espera 9 ticks; sin nada, se cierra.
static void message_door_tick(th_sim *s)
{
    if (s->door_delay > 0) {
        s->door_delay--;
        return;
    }
    int show = -1;
    if (s->n_shown < 5)
        for (int i = 0; i < s->n_fax && show < 0; i++) {
            int same = 0;
            for (int k = 0; k < s->n_shown; k++)
                same |= fax_type(s->shown[k].kind) == fax_type(s->fax[i].kind);
            if (!same)
                show = i;
        }
    if (show >= 0 && s->door_closed == 0) {
        s->shown[s->n_shown++] = s->fax[show];
        memmove(&s->fax[show], &s->fax[show + 1], (unsigned)(s->n_fax - show - 1) * sizeof s->fax[0]);
        s->n_fax--;
        s->door_delay = 9;
        return;
    }
    if (show >= 0 && s->door_closed > 0)
        s->door_closed--;
    else if (show < 0 && s->door_closed < 22)
        s->door_closed++;
}

// El jugador del arnés de CorsixTH (bench/lib/th64.lua). A velocidad Normal
// hay 3 ticks de la aplicación por cada hora de juego, y en cada uno corre la
// interfaz después del mundo. Tras el tick con hora, el arnés abre el primer
// mensaje que enseña el panel; el fax pausa el juego y el arnés lo contesta
// al empezar su siguiente tanda de 50 ticks, así que mientras tanto la
// interfaz sigue (de 0 a 49 ticks).
static void answer_faxes(th_sim *s)
{
    message_door_tick(s);
    if (s->n_shown) {
        const int pause = th_rng_range(&s->rng, 0, 49);
        for (int i = 0; i < pause; i++)
            message_door_tick(s);
        const int who = s->shown[0].who, kind = s->shown[0].kind;
        memmove(&s->shown[0], &s->shown[1], (unsigned)(s->n_shown - 1) * sizeof s->shown[0]);
        s->n_shown--;
        answer_fax(s, who, kind);
    }
    message_door_tick(s);
    message_door_tick(s);
}

// ---------------------------------------------------------------------------
// Temperatura (level_map::update_temperatures)

static uint16_t merge(uint32_t node, uint32_t other, uint32_t ratio)
{
    return (uint16_t)((node * (ratio - 1) + other) / ratio);
}

static uint32_t thermal(const th_sim *s, uint32_t *sum, int travel, int i, int nb, int prev)
{
    if (nb < 0 || nb >= TH_MAP_WIDTH * TH_MAP_HEIGHT)
        return 0;
    const th_tile *a = &s->w.map->tiles[i], *b = &s->w.map->tiles[nb];
    const uint32_t t = s->temp[prev][nb];
    if (travel || (((a->flags ^ b->flags) & (TH_TILE_HOSPITAL | TH_TILE_ROOM)) == 0)) {
        *sum += t * 4;
        return 4;
    }
    *sum += t;
    return 1;
}

static void update_temperatures(th_sim *s, uint16_t air, uint16_t radiator)
{
    if (radiator < air)
        radiator = air;
    const int prev = s->temp_idx, cur = prev ^ 1;
    s->temp_idx = (uint8_t)cur;
    const th_tile *tiles = s->w.map->tiles;
    for (int i = 0; i < TH_MAP_WIDTH * TH_MAP_HEIGHT; i++) {
        const uint32_t f = tiles[i].flags;
        uint32_t sum = 0, count = 0;
        count += thermal(s, &sum, (f & TH_TILE_TRAVEL_N) != 0, i, i - TH_MAP_WIDTH, prev);
        count += thermal(s, &sum, (f & TH_TILE_TRAVEL_S) != 0, i, i + TH_MAP_WIDTH, prev);
        count += thermal(s, &sum, (f & TH_TILE_TRAVEL_E) != 0, i, i + 1, prev);
        count += thermal(s, &sum, (f & TH_TILE_TRAVEL_W) != 0, i, i - 1, prev);
        uint32_t other, ratio;
        if (f & TH_TILE_HOSPITAL) {
            const int radiator_here = s->radiator_thob >= 0 && tiles[i].object == s->radiator_thob;
            other = radiator_here ? radiator : 0;
            ratio = radiator_here ? 2 : 1000;
        } else {
            other = air;
            ratio = 100;
        }
        uint16_t t = s->temp[prev][i];
        if (count)
            t = merge(t, sum / count, 4);
        s->temp[cur][i] = merge(t, other, ratio);
    }
}

// outside_temperatures de world.lua (en grados / 50), × 65535.
static const uint16_t outside[12] = {5373, 5767, 8257, 11337, 15662, 19660, 22216, 21692, 18546, 13762, 8912, 6225};

// ---------------------------------------------------------------------------
// Llegadas (World:onEndDay, onEndMonth, updateSpawnDates)

static void update_spawn_dates(th_sim *s)
{
    memset(s->spawn_dates, 0, sizeof s->spawn_dates);
    if (s->hosp.population <= 0)
        return;
    const int last = th_date_last_day(s->date.month);
    const int64_t spawns = (int64_t)s->spawn_rate * s->hosp.population / TH_MICRO; // millonésimas
    int64_t interval = spawns > 0 ? (int64_t)last * TH_MICRO * TH_MICRO / spawns : INT64_MAX / 4;
    if (interval > 1000ll * TH_MICRO)
        interval = 1000ll * TH_MICRO;
    int day = 1, forced = 1;
    while (day <= last) {
        day += p_random(s, interval);
        if (day <= last) {
            s->spawn_dates[day]++;
            forced = 0;
        }
    }
    if (forced)
        s->spawn_dates[1 + (int)(((int64_t)th_sim_random_micro(s) * last) / TH_MICRO)] = 1;
}

static void end_day(th_sim *s)
{
    // Hospital:onEndDay no hace nada que pese en el hito 4. World:onEndDay:
    for (int k = 0; k < s->n_order; k++) {
        const int who = s->order[k];
        if (!s->hum[who].alive)
            continue;
        if (s->hum[who].kind == TH_HUM_PATIENT)
            patient_tick_day(s, who);
        else
            staff_tick_day(s, who);
    }
    memset(s->spawn_hours, 0, sizeof s->spawn_hours);
    const int n = s->spawn_dates[s->date.day];
    for (int i = 0; i < n; i++)
        s->spawn_hours[th_rng_range(&s->rng, 1, TH_HOURS_PER_DAY)]++;
}

static void end_month(th_sim *s)
{
    // Hospital:onEndMonth: sueldos y seguros.
    int32_t wages = 0;
    for (int k = 0; k < s->n_order; k++) {
        const th_person *p = &s->hum[s->order[k]];
        if (p->alive && p->kind != TH_HUM_PATIENT)
            wages += p->u.s.wage;
    }
    if (wages)
        th_sim_spend_money(s, wages, TH_MONEY_WAGES);
    for (int i = 0; i < 3; i++) {
        if (s->hosp.insurance[i][2] > 0)
            th_sim_receive_money(s, s->hosp.insurance[i][2], TH_MONEY_INSURANCE, TH_NONE);
        s->hosp.insurance[i][2] = s->hosp.insurance[i][1];
        s->hosp.insurance[i][1] = s->hosp.insurance[i][0];
        s->hosp.insurance[i][0] = 0;
    }
    s->hosp.money_in = s->hosp.money_out = 0;
    // World:onEndMonth: población, ritmo de llegadas y días de llegada.
    const int month = th_date_month_of_game(s->date);
    s->hosp.population = 250000;
    if (month >= s->alloc_delay) {
        int64_t impact = TH_MICRO + ((int64_t)s->hosp.reputation - 500) * TH_MICRO / 250;
        if (impact <= 0)
            impact = 10000;
        s->hosp.population = (int32_t)(250000 * impact / TH_MICRO);
    }
    th_rnode popn = get(s->scen, get(s->scen, s->scen->root, "mundo"), "popn");
    for (int i = 0;; i++) {
        th_rnode e = th_rules_geti(s->scen, popn, i);
        if (!e)
            break;
        if (th_rules_int(s->scen, get(s->scen, e, "Month"), -1) == month) {
            s->monthly_spawn_increase = th_rules_micro(s->scen, get(s->scen, e, "Change"), 0);
            break;
        }
    }
    s->spawn_rate += s->monthly_spawn_increase;
    update_spawn_dates(s);
}

void th_sim_tick(th_sim *s)
{
    const th_date next = th_date_plus_hours(s->date, s->hours_per_tick);
    if (next.day != s->date.day) {
        end_day(s);
        if (s->date.day == th_date_last_day(s->date.month))
            end_month(s);
    }
    s->date = next;
    const int count = s->spawn_hours[s->date.hour];
    if (count && s->hosp.opened)
        for (int i = 0; i < count; i++)
            spawn_patient(s);
    // Entidades en el orden de world.entities: los mostradores (objetos) y
    // después las personas en el orden en que se crearon.
    for (int k = 0; k < s->n_desks; k++)
        th_sim_desk_tick(s, s->desks[k]);
    uint8_t order[TH_SIM_HUMANOIDS];
    const int n = s->n_order;
    memcpy(order, s->order, (unsigned)n);
    for (int k = 0; k < n; k++) {
        const int who = order[k];
        th_person *p = &s->hum[who];
        if (!p->alive)
            continue;
        th_humanoid_tick(&s->w, &p->h);
        if (p->alive && p->kind != TH_HUM_PATIENT)
            staff_tick(s, who);
    }
    update_temperatures(s, outside[s->date.month - 1], (uint16_t)(400000ll * 65535 / TH_MICRO));
    s->hours++;
    answer_faxes(s);
}

// ---------------------------------------------------------------------------
// Carga del escenario

static int load_definitions(th_sim *s)
{
    const th_rules *r = s->defs;
    th_rnode salas = get(r, r->root, "salas");
    s->n_room_types = 0;
    for (uint32_t i = 0; i < th_rules_len(r, salas) && i < TH_SIM_ROOM_TYPES; i++)
        s->room_types[s->n_room_types++] = th_rules_str(r, th_rules_entry_key(r, salas, i), NULL);
    th_rnode enf = get(r, r->root, "enfermedades");
    s->n_dis = 0;
    for (uint32_t i = 0; i < th_rules_len(r, enf) && i < TH_SIM_DISEASES; i++) {
        th_disease *d = &s->dis[s->n_dis++];
        th_rnode n = th_rules_entry_value(r, enf, i);
        d->id = th_rules_str(r, th_rules_entry_key(r, enf, i), NULL);
        d->def = n;
        d->visuals_id = (int8_t)th_rules_int(r, get(r, n, "visuals_id"), -1);
        d->non_visuals_id = (int8_t)th_rules_int(r, get(r, n, "non_visuals_id"), -1);
        d->only_emergency = (uint8_t)th_rules_bool(r, get(r, n, "only_emergency"), 0);
        d->must_stand = (uint8_t)th_rules_bool(r, get(r, n, "must_stand"), 0);
        d->expertise_id = (uint8_t)th_rules_int(r, get(r, n, "expertise_id"), 1);
        th_rnode dr = get(r, n, "diagnosis_rooms"), tr = get(r, n, "treatment_rooms");
        d->n_diag = d->n_treat = 0;
        for (uint32_t k = 0; k < th_rules_len(r, dr) && d->n_diag < TH_SIM_DIAG_ROOMS; k++)
            d->diag[d->n_diag++] = (uint8_t)th_sim_room_type(s, th_rules_str(r, th_rules_at(r, dr, k), NULL));
        for (uint32_t k = 0; k < th_rules_len(r, tr) && d->n_treat < 4; k++)
            d->treat[d->n_treat++] = (uint8_t)th_sim_room_type(s, th_rules_str(r, th_rules_at(r, tr, k), NULL));
    }
    // Las animaciones de caminar y de las puertas de humanoid.c, contra las reglas.
    th_rnode cam = get(r, r->root, "caminar"), pue = get(r, r->root, "puertas");
    for (unsigned i = 0; th_walk_anims_at(i); i++) {
        const th_walk_anims *a = th_walk_anims_at(i);
        th_rnode c = get(r, cam, a->name), p = get(r, pue, a->name);
        if (!c || th_rules_int(r, get(r, c, "walk_north"), -1) != a->walk_north ||
            th_rules_int(r, get(r, c, "walk_east"), -1) != a->walk_east ||
            th_rules_int(r, get(r, c, "idle_north"), -1) != a->idle_north ||
            th_rules_int(r, get(r, c, "idle_east"), -1) != a->idle_east ||
            th_rules_int(r, get(r, p, "leaving"), 0) != a->door_leaving ||
            th_rules_int(r, get(r, p, "entering"), 0) != a->door_entering ||
            th_rules_int(r, get(r, p, "knock_north"), 0) != a->knock_north ||
            th_rules_int(r, get(r, p, "knock_east"), 0) != a->knock_east ||
            th_rules_int(r, get(r, p, "leaving_swing"), 0) != a->swing_leaving ||
            th_rules_int(r, get(r, p, "entering_swing"), 0) != a->swing_entering)
            return -10;
    }
    return 0;
}

static int load_level(th_sim *s)
{
    const th_rules *l = s->level;
    th_rnode gbv = get(l, l->root, "gbv");
    s->very_tired = th_rules_int(l, get(l, gbv, "VeryTired"), 700);
    s->crackup_tired = th_rules_int(l, get(l, gbv, "CrackUpTired"), 800);
    s->alloc_delay = th_rules_int(l, get(l, gbv, "AllocDelay"), 3);
    s->hold_visual_months = th_rules_int(l, get(l, gbv, "HoldVisualMonths"), -1);
    s->hold_visual_peeps = th_rules_int(l, get(l, gbv, "HoldVisualPeepCount"), -1);
    s->salary_ability_div = th_rules_int(l, get(l, gbv, "SalaryAbilityDivisor"), 10);
    th_rnode add = get(l, gbv, "SalaryAdd");
    for (int i = 0; i < 8; i++)
        s->salary_add[i] = th_rules_int(l, th_rules_at(l, add, (uint32_t)i), 0);
    s->max_salary = th_rules_int(l, get(l, get(l, l->root, "payroll"), "MaxSalary"), 2000);
    th_rnode staff = get(l, l->root, "staff");
    for (int i = 0; i < 4; i++)
        s->min_salary[i] = th_rules_int(l, get(l, th_rules_geti(l, staff, i), "MinSalary"), 0);
    s->expertise = get(l, l->root, "expertise");
    s->visuals_available = get(l, l->root, "visuals_available");
    s->non_visuals_available = get(l, l->root, "non_visuals_available");
    return s->expertise && s->visuals_available && s->non_visuals_available ? 0 : -11;
}

static int object_type(const char *id)
{
    static const struct {
        const char *id;
        uint8_t type;
    } t[] = {{"door", TH_OBJ_DOOR},  {"bench", TH_OBJ_BENCH},   {"reception_desk", TH_OBJ_RECEPTION_DESK},
             {"desk", TH_OBJ_DESK},  {"cabinet", TH_OBJ_CABINET}, {"chair", TH_OBJ_CHAIR},
             {"radiator", TH_OBJ_RADIATOR}};
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
        if (!strcmp(t[i].id, id))
            return t[i].type;
    return TH_OBJ_OTHER;
}

static int new_queue(th_sim *s, int max_size, int bench_threshold, int door)
{
    if (s->n_queues >= TH_SIM_QUEUES)
        return TH_NONE;
    th_queue *q = &s->queues[s->n_queues];
    memset(q, 0, sizeof *q);
    q->max_size = (uint8_t)max_size;
    q->bench_threshold = (uint8_t)bench_threshold;
    q->door = (uint8_t)door;
    return s->n_queues++;
}

// Room:calculateHappinessFactor: ventanas y espacio de más (round(log n) / 1000).
static int32_t happiness_factor(const th_sim *s, const th_room *r, int minimum_size)
{
    int windows = 0;
    for (int x = r->x; x <= r->x + r->w && x < TH_MAP_WIDTH; x++)
        for (int y = r->y; y <= r->y + r->h && y < TH_MAP_HEIGHT; y++) {
            const th_tile *t = th_map_tile_c(s->w.map, x, y);
            const int n = t->wall_n, w = t->wall_w;
            if ((n >= 116 && n <= 121) || (n >= 124 && n <= 127) || (w >= 116 && w <= 121) || (w >= 124 && w <= 127))
                windows++;
        }
    // round(ln n): 1 → 0, 2–4 → 1, 5–12 → 2, 13–33 → 3, 34–90 → 4
    static const int thresholds[] = {2, 5, 13, 34, 91, 245};
    int32_t f = 0;
    for (int k = 0; k < 6 && windows >= thresholds[k]; k++)
        f += 1000;
    // espacio: (ancho · alto) / mínimo² > 1 → round(ln(espacio)) / 1000
    const int area = r->w * r->h, min2 = minimum_size * minimum_size;
    if (min2 > 0 && area > min2) {
        // round(ln(a/m)) = k  ⇔  e^(k−0,5) <= a/m < e^(k+0,5)
        static const int64_t e_half[] = {1648721, 4481689, 12182494, 33115452, 90017131}; // e^(k+0,5) · 10^6
        for (int k = 0; k < 5 && (int64_t)area * TH_MICRO >= e_half[k] * min2; k++)
            f += 1000;
    }
    return f;
}

static int load_scenario(th_sim *s)
{
    const th_rules *r = s->scen;
    th_rnode root = r->root;
    th_rnode fecha = get(r, root, "fecha");
    s->date.year = (uint16_t)th_rules_int(r, get(r, fecha, "anio"), 1);
    s->date.month = (uint8_t)th_rules_int(r, get(r, fecha, "mes"), 1);
    s->date.day = (uint8_t)th_rules_int(r, get(r, fecha, "dia"), 1);
    s->date.hour = (uint8_t)th_rules_int(r, get(r, fecha, "hora"), 0);
    th_rnode mundo = get(r, root, "mundo");
    s->hours_per_tick = (uint8_t)th_rules_int(r, get(r, mundo, "hours_per_tick"), 1);
    s->spawn_rate = th_rules_micro(r, get(r, mundo, "spawn_rate"), 0);
    s->monthly_spawn_increase = th_rules_micro(r, get(r, mundo, "monthly_spawn_increase"), 0);
    th_rnode sd = get(r, mundo, "spawn_dates"), sh = get(r, mundo, "spawn_hours");
    for (int d = 1; d <= 31; d++)
        s->spawn_dates[d] = (uint8_t)th_rules_int(r, th_rules_geti(r, sd, d), 0);
    for (int h = 1; h <= TH_HOURS_PER_DAY; h++)
        s->spawn_hours[h] = (uint8_t)th_rules_int(r, th_rules_geti(r, sh, h), 0);
    th_rnode pts = get(r, mundo, "puntos_aparicion");
    s->n_spawn = 0;
    for (uint32_t i = 0; i < th_rules_len(r, pts) && s->n_spawn < TH_SIM_SPAWN_POINTS; i++) {
        th_rnode p = th_rules_at(r, pts, i);
        s->spawn[s->n_spawn].x = (uint8_t)th_rules_int(r, get(r, p, "x"), 0);
        s->spawn[s->n_spawn].y = (uint8_t)th_rules_int(r, get(r, p, "y"), 0);
        s->spawn[s->n_spawn].dir = (uint8_t)dir_from(r, get(r, p, "direccion"));
        s->n_spawn++;
    }
    th_rnode av = get(r, mundo, "enfermedades");
    s->n_avail = 0;
    for (uint32_t i = 0; i < th_rules_len(r, av); i++) {
        const int d = disease_index(s, th_rules_str(r, th_rules_at(r, av, i), NULL));
        if (d == TH_NONE)
            return -20;
        s->avail[s->n_avail++] = (uint8_t)d;
    }
    // Hospital.
    th_rnode hosp = get(r, root, "hospital");
    th_hospital *h = &s->hosp;
    h->balance = th_rules_int(r, get(r, hosp, "saldo"), 0);
    h->reputation = th_rules_int(r, get(r, hosp, "reputacion"), 500);
    h->population = th_rules_micro(r, get(r, hosp, "poblacion"), 250000);
    h->opened = (uint8_t)th_rules_bool(r, get(r, hosp, "abierto"), 0);
    th_rnode pol = get(r, hosp, "politicas");
    h->send_home = th_rules_micro(r, get(r, pol, "send_home"), 100000);
    h->guess_cure = th_rules_micro(r, get(r, pol, "guess_cure"), 900000);
    h->stop_procedure = th_rules_micro(r, get(r, pol, "stop_procedure"), TH_MICRO);
    h->goto_staffroom = th_rules_micro(r, get(r, pol, "goto_staffroom"), 600000);
    h->grant_wage_increase = (uint8_t)th_rules_bool(r, get(r, pol, "grant_wage_increase"), 0);
    // Umbrales de precio según la dificultad (easy, full, hard).
    const char *dif = th_rules_str(r, get(r, root, "dificultad"), NULL);
    const int di = dif && !strcmp(dif, "easy") ? 0 : dif && !strcmp(dif, "hard") ? 2 : 1;
    static const int32_t under[3] = {-300000, -400000, -500000}, over[3] = {400000, 300000, 200000};
    h->under_priced = under[di];
    h->over_priced = over[di];
    th_rnode ins = get(r, hosp, "saldo_seguros");
    for (int i = 0; i < 3; i++)
        for (int k = 0; k < 3; k++)
            h->insurance[i][k] = th_rules_int(r, th_rules_at(r, th_rules_at(r, ins, (uint32_t)i), (uint32_t)k), 0);
    th_rnode cbk = get(r, hosp, "casebook");
    s->n_case = 0;
    for (uint32_t i = 0; i < th_rules_len(r, cbk) && s->n_case < TH_SIM_CASEBOOK; i++) {
        th_casebook *c = &s->casebook[s->n_case++];
        th_rnode e = th_rules_entry_value(r, cbk, i);
        c->id = th_rules_str(r, th_rules_entry_key(r, cbk, i), NULL);
        c->price = th_rules_micro(r, get(r, e, "precio"), TH_MICRO);
        c->discovered = (uint8_t)th_rules_bool(r, get(r, e, "descubierta"), 0);
        c->effectiveness = th_rules_int(r, get(r, e, "eficacia"), 100);
        c->pseudo = (uint8_t)th_rules_bool(r, get(r, e, "pseudo"), 0);
        th_rnode rep = get(r, e, "reputacion");
        c->reputation = rep ? th_rules_int(r, rep, 500) : INT32_MIN;
        // cure_price de la enfermedad o del diagnóstico
        th_rnode def = get(s->defs, get(s->defs, s->defs->root, "enfermedades"), c->id);
        if (!def)
            def = get(s->defs, get(s->defs, s->defs->root, "diagnosticos"), c->id);
        c->cure_price = th_rules_int(s->defs, get(s->defs, def, "cure_price"), 0);
    }
    // Objetos.
    th_rnode objs = get(r, root, "objetos"), defs_obj = get(s->defs, s->defs->root, "objetos");
    s->n_obj = 0;
    s->n_desks = 0;
    s->radiator_thob = -1;
    for (uint32_t i = 0; i < th_rules_len(r, objs) && s->n_obj < TH_SIM_OBJECTS; i++) {
        th_rnode o = th_rules_at(r, objs, i);
        const char *id = th_rules_str(r, get(r, o, "id"), NULL);
        th_object *ob = &s->obj[s->n_obj];
        memset(ob, 0, sizeof *ob);
        ob->def = get(s->defs, defs_obj, id ? id : "");
        if (!ob->def)
            return -21;
        ob->type = (uint8_t)object_type(id);
        ob->thob = (uint8_t)th_rules_int(s->defs, get(s->defs, ob->def, "thob"), 0);
        ob->x = (uint8_t)th_rules_int(r, get(r, o, "x"), 0);
        ob->y = (uint8_t)th_rules_int(r, get(r, o, "y"), 0);
        ob->dir = (uint8_t)dir_from(r, get(r, o, "direccion"));
        ob->room = (uint8_t)th_rules_int(r, get(r, o, "sala"), 0);
        ob->user = ob->reserved_for = ob->queue = ob->receptionist = TH_NONE;
        if (ob->type == TH_OBJ_RECEPTION_DESK) {
            ob->queue = (uint8_t)new_queue(s, 20, 3, TH_NONE);
            s->desks[s->n_desks++] = s->n_obj;
        } else if (ob->type == TH_OBJ_DOOR) {
            ob->queue = (uint8_t)new_queue(s, 6, 0, s->n_obj);
        } else if (ob->type == TH_OBJ_RADIATOR) {
            s->radiator_thob = ob->thob;
        }
        s->n_obj++;
    }
    // Salas.
    th_rnode salas = get(r, root, "salas"), defs_salas = get(s->defs, s->defs->root, "salas");
    s->n_rooms = 0;
    for (uint32_t i = 0; i < th_rules_len(r, salas) && s->n_rooms < TH_SIM_ROOMS; i++) {
        th_rnode sa = th_rules_at(r, salas, i);
        th_room *rm = &s->rooms[s->n_rooms];
        memset(rm, 0, sizeof *rm);
        rm->num = (uint8_t)th_rules_int(r, get(r, sa, "num"), 0);
        const char *id = th_rules_str(r, get(r, sa, "id"), NULL);
        rm->type = (uint8_t)th_sim_room_type(s, id ? id : "");
        if (rm->type == TH_NONE)
            return -22;
        rm->x = (uint8_t)th_rules_int(r, get(r, sa, "x"), 0);
        rm->y = (uint8_t)th_rules_int(r, get(r, sa, "y"), 0);
        rm->w = (uint8_t)th_rules_int(r, get(r, sa, "ancho"), 0);
        rm->h = (uint8_t)th_rules_int(r, get(r, sa, "alto"), 0);
        rm->is_active = (uint8_t)th_rules_bool(r, get(r, sa, "activa"), 1);
        rm->staff_member = rm->just_dealt_with = TH_NONE;
        rm->maximum_patients = 1;
        th_rnode pu = get(r, sa, "puerta");
        rm->door = (uint8_t)th_sim_object_at(s, th_rules_int(r, get(r, pu, "x"), 0), th_rules_int(r, get(r, pu, "y"), 0),
                                             TH_OBJ_DOOR);
        if (rm->door == TH_NONE)
            return -23;
        s->obj[rm->door].room = rm->num;
        const int min_size = th_rules_int(s->defs, get(s->defs, get(s->defs, defs_salas, id), "minimum_size"), 0);
        rm->happiness_factor = happiness_factor(s, rm, min_size);
        s->n_rooms++;
    }
    // Personal.
    th_rnode staff = get(r, root, "personal");
    for (uint32_t i = 0; i < th_rules_len(r, staff); i++) {
        th_rnode st = th_rules_at(r, staff, i);
        const char *cls = th_rules_str(r, get(r, st, "clase"), NULL);
        if (!cls)
            return -24;
        const int kind = !strcmp(cls, "Doctor")         ? TH_HUM_DOCTOR
                         : !strcmp(cls, "Nurse")        ? TH_HUM_NURSE
                         : !strcmp(cls, "Handyman")     ? TH_HUM_HANDYMAN
                         : !strcmp(cls, "Receptionist") ? TH_HUM_RECEPTIONIST
                                                        : -1;
        if (kind < 0)
            return -24;
        const int who = new_person(s, kind, th_walk_anims_find(cls));
        if (who == TH_NONE)
            return -25;
        th_person *p = &s->hum[who];
        th_rnode pf = get(r, st, "perfil"), at = get(r, st, "atributos");
        p->u.s.skill = th_rules_micro(r, get(r, pf, "habilidad"), 0);
        p->u.s.attention = th_rules_micro(r, get(r, pf, "atencion"), 0);
        p->u.s.wage = th_rules_int(r, get(r, pf, "sueldo"), 0);
        p->u.s.consultant = (uint8_t)th_rules_bool(r, get(r, pf, "consultor"), 0);
        p->u.s.junior = (uint8_t)th_rules_bool(r, get(r, pf, "junior"), 0);
        p->u.s.surgeon = th_rules_int(r, get(r, pf, "cirujano"), 0) == 1;
        p->u.s.psychiatrist = th_rules_int(r, get(r, pf, "psiquiatra"), 0) == 1;
        p->u.s.researcher = th_rules_int(r, get(r, pf, "investigador"), 0) == 1;
        p->u.s.fatigue = kind == TH_HUM_RECEPTIONIST ? -1 : th_rules_micro(r, get(r, at, "fatiga"), 0);
        p->happiness = th_rules_micro(r, get(r, at, "felicidad"), TH_MICRO);
        th_rnode capas = get(r, st, "capas");
        for (uint32_t k = 0; k < th_rules_len(r, capas); k++)
            th_humanoid_set_layer(&p->h, th_rules_int(r, th_rules_entry_key(r, capas, k), 0),
                                  th_rules_int(r, th_rules_entry_value(r, capas, k), 0));
        th_humanoid_set_tile(&s->w, &p->h, th_rules_int(r, get(r, st, "x"), 0), th_rules_int(r, get(r, st, "y"), 0));
    }
    return 0;
}

int th_sim_init(th_sim *s, th_map *m, th_pathfinder *pf, const th_anims *anims, const th_rules *defs,
                const th_rules *level, const th_rules *scen, uint32_t seed)
{
    memset(s, 0, sizeof *s);
    s->w.map = m;
    s->w.pf = pf;
    s->w.anims = anims;
    s->w.sim = s;
    s->w.attach_seq = TH_SIM_OBJECTS; // los objetos se anclaron antes que nadie
    s->defs = defs;
    s->level = level;
    s->scen = scen;
    s->answer = TH_ANSWER_GUESS_THEN_SEND_HOME;
    s->door_closed = 22;
    th_rng_seed(&s->rng, seed);
    for (int i = 0; i < TH_MAP_WIDTH * TH_MAP_HEIGHT; i++)
        s->temp[0][i] = s->temp[1][i] = 8192;
    int e = load_definitions(s);
    if (!e)
        e = load_level(s);
    if (!e)
        e = load_scenario(s);
    if (e)
        return e;
    // El personal va a su sitio: la recepcionista al mostrador más cercano
    // (Receptionist:onPlaceInCorridor) y el médico a su consulta (el despachador).
    for (int k = 0; k < s->n_desks; k++)
        th_sim_desk_check_for_staff(s, s->desks[k]);
    for (int k = 0; k < s->n_order; k++) {
        const int who = s->order[k];
        if (s->hum[who].kind != TH_HUM_DOCTOR)
            continue;
        for (int i = 0; i < s->n_rooms; i++)
            if (s->rooms[i].type == th_sim_room_type(s, "gp") && s->rooms[i].staff_member == TH_NONE) {
                th_sim_send_staff_to_room(s, &s->rooms[i], who);
                s->rooms[i].staff_member = (uint8_t)who; // reservado mientras llega
                break;
            }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Huella del estado (FNV-1a de 32 bits): para comprobar que el PC y la N64
// simulan exactamente lo mismo.

static uint32_t fnv(uint32_t h, uint32_t v)
{
    for (int i = 0; i < 4; i++) {
        h ^= (v >> (8 * i)) & 0xFF;
        h *= 16777619u;
    }
    return h;
}

uint32_t th_sim_hash(const th_sim *s)
{
    uint32_t h = 2166136261u;
    h = fnv(h, s->hours);
    h = fnv(h, (uint32_t)s->date.year << 16 | (uint32_t)s->date.month << 8 | s->date.day);
    h = fnv(h, s->date.hour);
    h = fnv(h, s->rng.s);
    h = fnv(h, (uint32_t)s->hosp.balance);
    h = fnv(h, (uint32_t)s->hosp.reputation);
    h = fnv(h, (uint32_t)s->hosp.population);
    h = fnv(h, s->hosp.num_visitors);
    h = fnv(h, (uint32_t)s->spawn_rate);
    for (int i = 0; i < 3; i++)
        for (int k = 0; k < 3; k++)
            h = fnv(h, (uint32_t)s->hosp.insurance[i][k]);
    for (int k = 0; k < s->n_order; k++) {
        const th_person *p = &s->hum[s->order[k]];
        const th_humanoid *u = &p->h;
        h = fnv(h, s->order[k]);
        h = fnv(h, (uint32_t)(uint16_t)u->tile_x << 16 | (uint16_t)u->tile_y);
        h = fnv(h, (uint32_t)(uint16_t)u->px << 16 | (uint16_t)u->py);
        h = fnv(h, (uint32_t)u->anim << 16 | u->anim_flags);
        h = fnv(h, u->frame);
        h = fnv(h, (uint32_t)u->timer << 8 | u->timer_fn);
        h = fnv(h, (uint32_t)p->happiness);
        for (int i = 0; i < u->queue_len; i++)
            h = fnv(h, (uint32_t)u->queue[i].kind << 24 | (uint32_t)u->queue[i].flags << 16 | u->queue[i].state);
        if (p->kind == TH_HUM_PATIENT)
            h = fnv(h, (uint32_t)p->u.p.diagnosis_progress);
        else
            h = fnv(h, (uint32_t)p->u.s.fatigue ^ (uint32_t)p->u.s.wage << 20);
    }
    for (int i = 0; i < s->n_obj; i++)
        h = fnv(h, (uint32_t)s->obj[i].user << 8 | s->obj[i].reserved_for);
    for (int q = 0; q < s->n_queues; q++)
        for (int i = 0; i < s->queues[q].size; i++)
            h = fnv(h, (uint32_t)q << 8 | s->queues[q].member[i]);
    uint32_t t = 0;
    for (int i = 0; i < TH_MAP_WIDTH * TH_MAP_HEIGHT; i++)
        t = t * 31u + s->temp[s->temp_idx][i];
    return fnv(h, t);
}
