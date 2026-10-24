// Simulación del hospital (fase 5, hito 4): el bucle mínimo de CorsixTH v0.70.1
// con una recepción, una consulta de GP, pacientes que llegan, hacen cola, se
// diagnostican y se van, y el dinero que entra y sale.
//
// Lo que hace cada parte y de dónde sale:
//   * el reloj y las llegadas: World:onTick, onEndDay, onEndMonth y
//     updateSpawnDates (world.lua), con math.p_random (app.lua);
//   * el hospital: dinero, sueldos, seguros y reputación (hospital.lua);
//   * los pacientes (entities/humanoids/patient.lua) y el personal
//     (staff.lua, receptionist.lua): cansancio, felicidad y subidas de sueldo;
//   * las salas, las puertas, las colas, el mostrador y la consulta (room.lua,
//     objects/door.lua, queue.lua, objects/reception_desk.lua, rooms/gp.lua);
//   * la temperatura del mapa (level_map::update_temperatures, th_map.cpp),
//     que enfría al personal y le quita felicidad;
//   * las acciones de los humanoides, en humanoid.c.
//
// Las cantidades con decimales de CorsixTH (atributos, probabilidades,
// políticas, la población) van en millonésimas (TH_MICRO = 1,0): así las
// constantes de CorsixTH (0,00009 de cansancio por tick...) son exactas. Los
// números aleatorios salen de th_rng, no del generador de Lua, así que la
// comparación con CorsixTH es estadística (tools/oraculo_hito4).
//
// Lo que se deja fuera, por no pesar en el bucle del hito 4 (las cifras están
// en docs/05_prototipo.md): sed, vómitos, ganas de ir al baño, bostezos,
// basura, la salud de los pacientes, la calefacción (no hay radiadores) y los
// sucesos (emergencias, VIP, terremotos, epidemias), que el escenario apaga.
#ifndef TH_SIM_H
#define TH_SIM_H

#include <stdint.h>

#include "th/anim.h"
#include "th/humanoid.h"
#include "th/map.h"
#include "th/path.h"
#include "th/rng.h"
#include "th/rules.h"

#define TH_MICRO 1000000
#define TH_SIM_HUMANOIDS 64
#define TH_SIM_OBJECTS 96
#define TH_SIM_ROOMS 16
#define TH_SIM_QUEUES 24
#define TH_SIM_QUEUE_LEN 30
#define TH_SIM_SPAWN_POINTS 32
#define TH_SIM_DISEASES 40
#define TH_SIM_CASEBOOK 64
#define TH_SIM_ROOM_TYPES 32
#define TH_SIM_DIAG_ROOMS 8
#define TH_SIM_FAXES 16
#define TH_HOURS_PER_DAY 50

// Date de CorsixTH (date.lua): 50 horas por día, sin años bisiestos.
typedef struct th_date {
    uint16_t year; // desde 1
    uint8_t month; // 1–12
    uint8_t day;   // 1–31
    uint8_t hour;  // 0–49
} th_date;

int th_date_last_day(int month);
th_date th_date_plus_hours(th_date d, int hours);
static inline int th_date_month_of_game(th_date d)
{
    return (d.year - 1) * 12 + d.month;
}

// Clase de humanoide para la lógica.
enum { TH_HUM_PATIENT, TH_HUM_DOCTOR, TH_HUM_NURSE, TH_HUM_HANDYMAN, TH_HUM_RECEPTIONIST };

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

typedef struct th_object {
    uint8_t type;      // TH_OBJ_*
    uint8_t thob;      // número de objeto del mapa (object_type.thob)
    uint8_t x, y, dir; // dir: TH_DIR_*
    uint8_t room;      // sala (número) o 0
    uint8_t user;      // humanoide que lo usa (TH_NONE)
    uint8_t reserved_for;
    uint8_t queue;        // cola de las puertas y los mostradores (TH_NONE)
    uint8_t receptionist; // mostrador: quien lo atiende
    uint16_t queue_advance_timer;
    uint16_t times_used;
    th_rnode def; // objetos.<id> en las definiciones
} th_object;

typedef struct th_queue {
    uint8_t member[TH_SIM_QUEUE_LEN];
    uint8_t size, reported, max_size, bench_threshold;
    uint8_t door; // puerta: prioridad para quien sale de su sala (TH_NONE en los mostradores)
    uint8_t expected_count;
    uint16_t visitor_count;
    uint64_t expected; // humanoides que vienen hacia la cola
} th_queue;

typedef struct th_room {
    uint8_t num;  // id de la sala en CorsixTH: lo que guardan las casillas
    uint8_t type; // índice en las definiciones (salas)
    uint8_t x, y, w, h;
    uint8_t door;          // objeto
    uint8_t is_active;
    uint8_t staff_member;  // humanoide (TH_NONE)
    int8_t max_times;      // GP: veces que el médico puede no acabar de diagnosticar
    uint8_t just_dealt_with;
    uint8_t maximum_patients;
    int32_t happiness_factor; // millonésimas
    uint64_t humanoids;       // quién está dentro
} th_room;

// Una enfermedad de las definiciones, con lo que usa la lógica.
typedef struct th_disease {
    const char *id;
    th_rnode def;
    int8_t visuals_id, non_visuals_id; // -1 = no tiene
    uint8_t only_emergency, must_stand;
    uint8_t expertise_id;
    uint8_t n_diag, n_treat;
    uint8_t diag[TH_SIM_DIAG_ROOMS]; // salas de diagnóstico (tipo)
    uint8_t treat[4];
} th_disease;

// Entrada del casebook del hospital (una enfermedad o un diagnóstico).
typedef struct th_casebook {
    const char *id;
    int32_t price;        // multiplicador del precio, en millonésimas
    int32_t cure_price;   // precio base (disease.cure_price)
    int32_t reputation;   // INT32_MIN = sin reputación propia (pseudoenfermedades)
    int32_t effectiveness;
    uint8_t discovered, pseudo;
} th_casebook;

typedef struct th_patient {
    uint8_t disease;      // índice en th_sim.dis
    uint8_t diagnosed;
    uint8_t insurance;    // compañía 1–3; 0 = paga él
    uint8_t going_home;
    uint8_t has_passed_reception;
    uint8_t next_room;    // sala (número) a la que va; 0 = ninguna
    uint8_t n_diag;       // available_diagnosis_rooms
    uint8_t diag[TH_SIM_DIAG_ROOMS];
    uint8_t cure_rooms_visited;
    uint8_t message;      // fax pendiente (TH_FAX_*) o 0
    int16_t waiting;      // días que espera; -1 = no espera
    int32_t diagnosis_progress; // millonésimas
    int32_t pay_amount;
    uint16_t id;          // número de llegada (1, 2, ...): para las trazas
} th_patient;

typedef struct th_staff {
    int32_t skill, attention;     // millonésimas
    int32_t wage;
    uint8_t consultant, junior, surgeon, psychiatrist, researcher;
    int32_t fatigue;              // millonésimas; -1 = no se cansa (recepcionista)
    int16_t timer_until_raise;    // -1 = nil
    int16_t quitting_in;          // -1 = nil
    uint8_t associated_desk;      // mostrador (TH_NONE)
    uint8_t waiting_for_staffroom;
    uint8_t dealing_with_patient;
    uint8_t last_room;            // sala (número)
    uint8_t pay_rise;             // mood pay_rise activo
    int32_t rise_amount;
} th_staff;

typedef struct th_person {
    th_humanoid h;
    uint8_t alive;
    uint8_t kind; // TH_HUM_*
    int32_t happiness, warmth; // millonésimas
    union {
        th_patient p;
        th_staff s;
    } u;
} th_person;

enum { TH_FAX_NO_DIAGNOSIS = 1, TH_FAX_NO_TREATMENT, TH_FAX_RAISE };

typedef struct th_hospital {
    int32_t balance;
    int32_t reputation;
    int32_t population; // millonésimas
    uint8_t opened;
    uint8_t grant_wage_increase;
    int32_t send_home, guess_cure, stop_procedure, goto_staffroom; // políticas, millonésimas
    int32_t under_priced, over_priced;                             // umbrales, millonésimas
    int32_t insurance[3][3]; // insurance_balance: lo que paga cada compañía este mes y los dos siguientes
    uint16_t num_visitors;
    uint16_t num_cured, not_cured;
    int32_t money_in, money_out;
} th_hospital;

// Sucesos para las trazas (th64sim hito4); punteros nulos = sin traza.
enum {
    TH_EV_SPAWN,      // aparece un paciente
    TH_EV_DIAGNOSED,  // Patient:setDiagnosed
    TH_EV_GO_HOME,    // Patient:goHome (arg: TH_HOME_*)
    TH_EV_DESPAWN,    // desaparece por el borde del mapa
};
enum { TH_HOME_KICKED = 1, TH_HOME_OVER_PRICED, TH_HOME_CURED };
enum { TH_MONEY_TREATMENT = 1, TH_MONEY_INSURANCE_PENDING, TH_MONEY_INSURANCE, TH_MONEY_WAGES };

typedef struct th_sim_events {
    void *ctx;
    void (*patient)(void *ctx, const struct th_sim *s, int who, int event, int arg);
    void (*money)(void *ctx, const struct th_sim *s, int32_t amount, int reason, int who);
} th_sim_events;

// Preferencias del jugador para los faxes (lo que hace el arnés de CorsixTH:
// arriesgar un diagnóstico si se puede y, si no, mandar a casa).
enum { TH_ANSWER_SEND_HOME, TH_ANSWER_GUESS_THEN_SEND_HOME };

typedef struct th_sim {
    th_world w;
    const th_rules *defs;  // definiciones (definiciones.trg)
    const th_rules *level; // configuración del nivel (fullNN.trg)
    const th_rules *scen;  // el escenario (tools/oraculo_hito4/escenario.py)
    th_rng rng;
    th_date date;
    uint32_t hours;     // horas simuladas desde el principio del escenario
    uint8_t hours_per_tick;
    // Llegadas.
    int32_t spawn_rate, monthly_spawn_increase; // millonésimas
    uint8_t spawn_dates[32]; // llegadas de cada día del mes
    uint8_t spawn_hours[TH_HOURS_PER_DAY + 1];
    uint8_t n_spawn;
    struct {
        uint8_t x, y, dir;
    } spawn[TH_SIM_SPAWN_POINTS];
    uint8_t n_avail;
    uint8_t avail[TH_SIM_DISEASES]; // available_diseases (índices en dis)
    uint16_t patients_spawned;
    // Definiciones.
    uint8_t n_dis, n_case, n_room_types;
    th_disease dis[TH_SIM_DISEASES];
    th_casebook casebook[TH_SIM_CASEBOOK];
    const char *room_types[TH_SIM_ROOM_TYPES]; // claves de «salas», en orden
    // Configuración del nivel.
    int32_t very_tired, crackup_tired; // en milésimas de cansancio, como gbv
    int32_t alloc_delay, hold_visual_months, hold_visual_peeps, max_salary, salary_ability_div;
    int32_t salary_add[8];
    int32_t min_salary[4];
    th_rnode expertise, visuals_available, non_visuals_available;
    // Estado.
    th_hospital hosp;
    uint8_t n_obj, n_rooms, n_queues;
    th_object obj[TH_SIM_OBJECTS];
    th_room rooms[TH_SIM_ROOMS];
    th_queue queues[TH_SIM_QUEUES];
    th_person hum[TH_SIM_HUMANOIDS];
    uint8_t n_order;
    uint8_t order[TH_SIM_HUMANOIDS]; // humanoides en el orden de world.entities
    uint8_t n_desks;
    uint8_t desks[8]; // mostradores, en orden de creación (ticks)
    int16_t radiator_thob; // -1 si no hay radiadores
    uint8_t temp_idx;
    uint16_t temp[2][TH_MAP_WIDTH * TH_MAP_HEIGHT];
    // Faxes y subidas de sueldo: los que esperan en la cola del panel inferior
    // y los que ya enseña (message_windows), con la puerta de los mensajes
    // (UIBottomPanel:_messageDoorTick).
    uint8_t n_fax, n_shown;
    struct {
        uint8_t who, kind;
    } fax[TH_SIM_FAXES], shown[5];
    uint8_t door_closed, door_delay;
    uint8_t answer; // TH_ANSWER_*
    th_sim_events ev;
} th_sim;

// Prepara la simulación sobre un mapa ya cargado con las casillas del
// escenario aplicadas (th_scenario_apply_cells) y carga el resto del
// escenario: fecha, llegadas, hospital, objetos, salas y personal.
// Devuelve 0 o un número negativo si algo no cuadra.
int th_sim_init(th_sim *s, th_map *m, th_pathfinder *pf, const th_anims *anims, const th_rules *defs,
                const th_rules *level, const th_rules *scen, uint32_t seed);
// Una hora de juego (World:onTick con hours_per_tick 1).
void th_sim_tick(th_sim *s);
// Huella del estado: la misma en el PC y en la N64.
uint32_t th_sim_hash(const th_sim *s);

static inline th_person *th_sim_person(th_sim *s, th_humanoid *h)
{
    (void)s;
    return (th_person *)(void *)h; // th_humanoid es el primer miembro
}
static inline int th_sim_index(const th_sim *s, const th_humanoid *h)
{
    return (int)((const th_person *)(const void *)h - s->hum);
}
static inline th_room *th_sim_room(th_sim *s, int num)
{
    for (int i = 0; i < s->n_rooms; i++)
        if (s->rooms[i].num == num)
            return &s->rooms[i];
    return 0;
}

// Lo que usan las acciones (humanoid.c); who es el índice del humanoide en
// th_sim.hum y obj el del objeto en th_sim.obj.
int th_sim_anim_length(const th_sim *s, unsigned anim); // getAnimLength con las duraciones fijadas
th_room *th_sim_room_at(th_sim *s, int x, int y);
int th_sim_room_type(const th_sim *s, const char *id);
const th_casebook *th_sim_casebook(const th_sim *s, const char *id);
// Objetos.
int th_sim_object_at(const th_sim *s, int x, int y, int type); // TH_NONE si no hay
int th_sim_use_tile(const th_sim *s, int obj, int secondary, int *x, int *y);
void th_sim_object_set_user(th_sim *s, int obj, int who);
void th_sim_object_remove_user(th_sim *s, int obj, int who);
int th_sim_desk_usage_score(const th_sim *s, int desk);
void th_sim_desk_check_for_staff(th_sim *s, int desk);
// Salas.
int th_sim_room_can_enter(th_sim *s, const th_room *r, int who);
int th_sim_room_patient_count(const th_sim *s, const th_room *r);
int th_sim_room_requires_staff(const th_sim *s, const th_room *r);
void th_sim_room_try_advance_queue(th_sim *s, th_room *r);
void th_sim_room_on_enter(th_sim *s, th_room *r, int who);
void th_sim_room_on_leave(th_sim *s, th_room *r, int who);
th_action th_sim_room_enter_action(th_sim *s, th_room *r, int who);
void th_sim_unexpect_from_room(th_sim *s, int who, th_room *r);
int th_sim_find_room_near(th_sim *s, int who, int room_type); // índice en rooms o TH_NONE
void th_sim_gp_loop_callback(th_sim *s, int who, th_action *a);
// Colas.
void th_sim_queue_push(th_sim *s, int q, int who);
void th_sim_queue_remove_value(th_sim *s, int q, int who);
void th_sim_queue_unexpect(th_sim *s, int q, int who);
th_action *th_sim_queue_action_of(th_sim *s, int who, int q);
void th_sim_on_change_queue_position(th_sim *s, int who, th_action *qa); // humanoid.c
void th_sim_on_leave_queue(th_sim *s, int who, th_action *qa);           // humanoid.c
// Búsquedas del pathfinder.
int th_sim_free_bench(th_sim *s, int gx, int gy, int dist, int *bx, int *by, int *found_dist);
int th_sim_find_object_near(th_sim *s, int who, int type, int *ox, int *oy);
int th_sim_idle_tile(th_sim *s, int x, int y, int n, int *ix, int *iy); // 0 si no hay
// Dinero (hospital.lua).
int32_t th_sim_treatment_price(const th_sim *s, const th_casebook *cb);
void th_sim_receive_money(th_sim *s, int32_t amount, int reason, int who);
void th_sim_spend_money(th_sim *s, int32_t amount, int reason);
void th_sim_price_level_impact(th_sim *s, int who, const th_casebook *cb);
// Entre sim.c y sim_rooms.c.
void th_sim_desk_tick(th_sim *s, int desk);
void th_sim_room_make_leave(th_sim *s, th_room *r, int who);
void th_sim_send_staff_to_room(th_sim *s, th_room *r, int who);
int32_t th_sim_random_micro(th_sim *s); // math.random() en millonésimas: [0, 1.000.000)
// Pacientes.
void th_sim_go_home(th_sim *s, int who, int reason);
void th_sim_destroy(th_sim *s, int who);
void th_sim_set_diagnosed(th_sim *s, int who);
int th_sim_agrees_to_pay(th_sim *s, int who, const char *disease_id);
void th_sim_send_fax(th_sim *s, int who, int kind);

#endif
