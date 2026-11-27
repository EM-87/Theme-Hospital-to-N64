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
#include "th/objects.h"
#include "th/path.h"
#include "th/rng.h"
#include "th/rules.h"

#define TH_MICRO 1000000
#define TH_SIM_HUMANOIDS 128 // el tope de la demo (docs/04_construccion.md)
// Objetos: CorsixTH no tiene tope (el hospital «lleno» de docs/01_mediciones.md
// tiene 305). Con índices de 8 bits y TH_NONE = 255 caben 254; más exige
// índices de 16 bits (las acciones, las colas, las salas y la ventana de colocar).
#define TH_SIM_OBJECTS 254
// Salas: el hospital «lleno» tiene 36 (docs/01_mediciones.md); los ids de sala van en un uint8_t.
#define TH_SIM_ROOMS 32
#define TH_SIM_QUEUES 40 // las de las puertas y los mostradores: lo que no tiene cola no cabe
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

// Conjunto de humanoides (índices en th_sim.hum), un bit por humanoide.
typedef struct th_hset {
    uint32_t w[TH_SIM_HUMANOIDS / 32];
} th_hset;
static inline int th_hset_has(const th_hset *c, int i)
{
    return c->w[i >> 5] >> (i & 31) & 1u;
}
static inline void th_hset_add(th_hset *c, int i)
{
    c->w[i >> 5] |= 1u << (i & 31);
}
static inline void th_hset_del(th_hset *c, int i)
{
    c->w[i >> 5] &= ~(1u << (i & 31));
}

typedef struct th_queue {
    uint8_t member[TH_SIM_QUEUE_LEN];
    uint8_t size, reported, max_size, bench_threshold;
    uint8_t door; // puerta: prioridad para quien sale de su sala (TH_NONE en los mostradores)
    uint8_t expected_count;
    uint16_t visitor_count;
    th_hset expected; // humanoides que vienen hacia la cola
} th_queue;

typedef struct th_room {
    uint8_t num;  // id de la sala en CorsixTH: lo que guardan las casillas
    uint8_t type; // índice en las definiciones (salas)
    uint8_t x, y, w, h;
    uint8_t door;          // objeto (la hoja maestra de una puerta doble)
    uint8_t door2;         // la esclava de una puerta doble, o TH_NONE
    uint8_t built;         // Room.built: terminada (roomFinished); 0 mientras se edita desde la puerta
    uint8_t is_active;
    uint8_t staff_member;  // humanoide (TH_NONE)
    int8_t max_times;      // GP: veces que el médico puede no acabar de diagnosticar
    uint8_t just_dealt_with;
    uint8_t maximum_patients;
    // La llamada de personal de la sala en CallsDispatcher (una sola: las
    // salas del hito 4 piden un único miembro del personal).
    uint8_t call_open;   // hay llamada en la cola
    uint8_t call_staff;  // quién la atiende (TH_NONE = nadie aún)
    uint8_t call_serial; // identidad de la llamada (la de CallCheckPointAction)
    int32_t happiness_factor; // millonésimas
    th_hset humanoids;        // quién está dentro
} th_room;

// Una enfermedad de las definiciones, con lo que usa la lógica.
typedef struct th_disease {
    const char *id;
    th_rnode def;
    int8_t visuals_id, non_visuals_id; // -1 = no tiene
    uint8_t only_emergency, must_stand;
    uint8_t more_loo_use, yawn; // van más al baño (gut_rot, the_squits); bostezan (sleeping_illness)
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
    // Necesidades (Patient:tickDay).
    int32_t thirst, toilet_need; // millonésimas
    uint8_t has_vomitted;
    uint8_t going_to_toilet;  // TH_TOILET_*
    uint8_t dying_mood;       // el mood dying1..5 activo (0 = ninguno)
    int8_t thirst_timeout;    // Patient.timeout: días hasta volver a buscar una máquina de bebidas
    int16_t litter_countdown; // casillas hasta tirar basura; -1 = nil (aún no ha pisado el hospital)
} th_patient;

enum { TH_TOILET_NO, TH_TOILET_YES, TH_TOILET_NO_TOILETS }; // going_to_toilet

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
    uint8_t on_call;              // sala cuya llamada atiende (0 = ninguna)
    uint8_t pay_rise;             // mood pay_rise activo
    int32_t rise_amount;
} th_staff;

typedef struct th_person {
    th_humanoid h;
    uint8_t alive;
    uint8_t kind;        // TH_HUM_*
    uint32_t entity_seq; // orden de alta en world.entities (th_sim.entity_seq)
    int32_t happiness, warmth, health; // millonésimas
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
    int32_t value; // valor del hospital: lo pagado por lo construido (Hospital.value)
} th_hospital;

// Basura en el suelo (th_litter, en objects.h). No la recoge nadie, porque
// aún no hay bedeles: solo la quita colocar un objeto encima.
#define TH_SIM_LITTER 512

// Sucesos para las trazas (th64sim hito4); punteros nulos = sin traza.
enum {
    TH_EV_SPAWN,      // aparece un paciente
    TH_EV_DIAGNOSED,  // Patient:setDiagnosed
    TH_EV_GO_HOME,    // Patient:goHome (arg: TH_HOME_*)
    TH_EV_DESPAWN,    // desaparece por el borde del mapa
    TH_EV_DEALT,      // GPRoom:dealtWithPatient (el médico lo despacha)
    TH_EV_FAX,        // th_sim_send_fax con un fax del paciente (arg: TH_FAX_*)
    TH_EV_FAX_ANSWER, // el jugador contesta su fax (arg: 1 arriesga el diagnóstico, 0 lo manda a casa)
};
enum { TH_HOME_KICKED = 1, TH_HOME_OVER_PRICED, TH_HOME_CURED };
// Motivos de los movimientos de dinero. Con BUY_OBJECT y SELL_OBJECT, who es
// la definición del objeto (th_rnode) y el valor del hospital cambia en lo
// mismo que el importe.
enum {
    TH_MONEY_TREATMENT = 1,
    TH_MONEY_INSURANCE_PENDING,
    TH_MONEY_INSURANCE,
    TH_MONEY_WAGES,
    TH_MONEY_BUY_OBJECT,
    TH_MONEY_SELL_OBJECT,
    TH_MONEY_BUILD_ROOM, // who es la definición de la sala (salas.<id>)
    // Hospital:addStaff (hospital.lua:1400-1406): el sueldo de un mes al
    // contratar, con spendMoney sin changeValue (el valor no cambia); who es
    // la persona contratada (índice en hum), no un th_rnode: quien lea el
    // motivo (el gancho de dinero de la batería, platform/pc/construir.c)
    // solo puede mirar el id del nodo con BUY_OBJECT, SELL_OBJECT y
    // BUILD_ROOM; este es «hire_staff» (hospital.lua:1405).
    TH_MONEY_HIRE_STAFF,
};

// El mercado de candidatos (World:makeAvailableStaff, world.lua:510-532, y
// StaffProfile, staff_profile.lua). Lo genera un th_rng propio sembrado con la
// semilla del escenario, la Seed de staff_levels y el mes: nunca s->rng, para
// que generarlo no cambie la simulación ni la huella de los escenarios que no
// contratan. Se regenera entero al empezar (el mes 0 en una partida nueva) y
// al acabar cada mes, como CorsixTH.
// Hasta 9 por clase en los 36 niveles (staff_levels de full/easy/hard).
#define TH_SIM_CANDIDATES 12
#define TH_SIM_STAFF_CLASSES 4 // médicos, enfermeras, bedeles, recepcionistas: TH_HUM_DOCTOR..RECEPTIONIST
// Títulos del perfil (th_staff_profile.titles): is_junior, is_consultant
// (parseSkillLevel), is_surgeon, is_psychiatrist, is_researcher (== 1.0).
enum {
    TH_TITLE_JUNIOR = 1u << 0,
    TH_TITLE_CONSULTANT = 1u << 1,
    TH_TITLE_SURGEON = 1u << 2,
    TH_TITLE_PSYCHIATRIST = 1u << 3,
    TH_TITLE_RESEARCHER = 1u << 4,
};
typedef struct th_staff_profile {
    int32_t skill;     // millonésimas, [0, 1.000.000)
    int32_t attention; // attention_to_detail, millonésimas
    int32_t wage;      // getFairWage
    uint8_t kind;      // TH_HUM_*
    uint8_t titles;    // TH_TITLE_*
    uint8_t layer5;    // la capa 5 del aspecto (Staff:setProfile): 2, o 4 los médicos negros
    // El retrato (drawFace): franjas de FACE01 (pelo, cara y barbilla, 0-17).
    uint8_t hair, face, chin;
    uint8_t desc_swap; // shuffle de las dos descripciones: 1 = la segunda va delante
    uint16_t name_seed; // math.random(1, 1009): la inicial es initials[name_seed % #initials]
    // Los __random de las tablas de textos, que el núcleo no carga: la
    // interfaz toma el índice como valor % tamaño de la tabla.
    uint16_t name_rnd[2]; // humanoid_name_starts y humanoid_name_ends
    uint16_t desc_rnd[2]; // staff_descriptions.bad (habilidad < 0,55) o .good, y .misc
} th_staff_profile;

// La ventana de colocar objetos (UIPlaceObjects de CorsixTH): lo que se ha
// comprado o cogido y queda por colocar, una fila por tipo de objeto.
// Filas (tipos de objeto) de la lista: los tipos distintos de una sala (9
// como mucho, psiquiatría e investigación) más los 7 objetos de pasillo que
// una sala nueva puede tapar y removeRoom devuelve a la lista: window_add no
// se queda sin sitio con las reglas del juego.
#define TH_SIM_PLACE_ITEMS 16
#define TH_SIM_PLACE_EXISTING 8
typedef struct th_place_item {
    th_rnode def;
    uint8_t qty;
    uint8_t existing; // existing_object de la fila: índice + 1 del objeto cogido con el que se abrió, o 0
    uint8_t picked;   // la fila se abrió con un objeto cogido (existing_object): al cerrar se vende, no se devuelve
    uint8_t n_existing;
    uint8_t existing_list[TH_SIM_PLACE_EXISTING]; // existing_objects: el primero es el que se coloca
} th_place_item;
typedef struct th_place_window {
    uint8_t open;
    uint8_t n;      // filas
    uint8_t active; // active_index, desde 0
    uint8_t orient; // object_orientation (TH_DIR_*)
    th_place_item item[TH_SIM_PLACE_ITEMS];
} th_place_window;

// Una sala en obra (UIEditRoom de CorsixTH, dialogs/edit_room.lua): el plano,
// la fase, la puerta y las ventanas elegidas. Es también una ventana «main»:
// con ella abierta, th_sim.place es su lista de objetos (en la fase de
// objetos). Un borde es la casilla de dentro de la sala y su lado (TH_DIR_*).
enum { TH_OBRA_NONE, TH_OBRA_WALLS, TH_OBRA_DOOR, TH_OBRA_WINDOWS, TH_OBRA_CLEAR, TH_OBRA_OBJECTS };
typedef struct th_edge {
    uint8_t x, y, side;
} th_edge;
#define TH_SIM_WINDOWS 32
typedef struct th_obra {
    uint8_t phase;      // TH_OBRA_*
    uint8_t type;       // índice en room_types
    uint8_t x, y, w, h; // el plano (blueprint_rect), desde 0
    uint8_t paid;
    uint8_t room;     // índice en rooms desde finishRoom, o TH_NONE
    uint8_t has_door; // la puerta elegida (blueprint_door)
    th_edge door;
    uint8_t n_windows;
    th_edge windows[TH_SIM_WINDOWS];
    // objects_backup: lo que había en la lista al volver de la fase de objetos
    // a la de la puerta (se devolvió), para cobrarlo otra vez al volver a ella.
    uint8_t n_backup;
    struct {
        th_rnode def;
        uint8_t qty;
    } backup[TH_SIM_PLACE_ITEMS];
} th_obra;

// Una orden del jugador (th/build.h): kind es TH_ORD_* y reason, TH_REASON_*.
typedef struct th_order {
    uint32_t tick; // th_sim.hours en el que se aplica (la cola)
    uint8_t kind;
    uint8_t x, y, dir; // dir: orientación del objeto, o el lado de una puerta o ventana
    uint8_t qty;
    uint8_t w, h;  // el plano de una sala
    th_rnode def;  // objetos.<id>, salas.<id>, o 0
} th_order;
typedef struct th_order_result {
    uint8_t reason;
    uint8_t bad; // con TH_REASON_CASILLAS: cuántas casillas de la huella
} th_order_result;
#define TH_SIM_ORDERS 16

// Un paso de diagnóstico (Patient:completeDiagnosticStep), para las sondas:
// las entradas de la fórmula y el progreso antes y después, en millonésimas.
// staff es TH_NONE si no había médico en la sala; r5 es el factor 1..5 del
// consultor (0 si no se sacó) y div, el divisor 1..3 (0 sin médico).
typedef struct th_diag_step {
    int staff;
    int32_t max_diag;
    int32_t skill, attention, fatigue;
    int8_t r5, div;
    int32_t before, after;
} th_diag_step;

typedef struct th_sim_events {
    void *ctx;
    void (*patient)(void *ctx, const struct th_sim *s, int who, int event, int arg);
    void (*money)(void *ctx, const struct th_sim *s, int32_t amount, int reason, int who);
    // Cada orden de la cola, al aplicarla (para grabarla y repetirla).
    void (*order)(void *ctx, const struct th_sim *s, const th_order *o, const th_order_result *r);
    // Cada paso de diagnóstico de un paciente (solo para observar).
    void (*diagnosis)(void *ctx, const struct th_sim *s, int who, const th_diag_step *d);
    // Una entidad de world.entities que se queda sin tick esta hora porque
    // otra de delante salió de la lista (th_sim.tick_seq; solo para observar).
    void (*skipped)(void *ctx, const struct th_sim *s, uint32_t entity_seq);
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
    int32_t doctor_threshold, consultant_threshold; // habilidad, en millonésimas (gbv, en milésimas)
    int32_t alloc_delay, hold_visual_months, hold_visual_peeps, max_salary, salary_ability_div;
    int32_t salary_add[8];
    int32_t min_salary[4];
    th_rnode expertise, visuals_available, non_visuals_available;
    // Estado.
    th_hospital hosp;
    uint8_t n_obj, n_rooms, n_queues;
    th_object obj[TH_SIM_OBJECTS];
    uint32_t obj_seq; // llegadas de objetos a una casilla (th_objects.seq)
    th_room rooms[TH_SIM_ROOMS];
    th_queue queues[TH_SIM_QUEUES];
    th_person hum[TH_SIM_HUMANOIDS];
    uint8_t n_order;
    uint8_t order[TH_SIM_HUMANOIDS]; // humanoides en el orden de world.entities
    uint8_t n_desks;
    uint8_t desks[TH_SIM_QUEUES]; // mostradores con cola, en orden de alta (ticks)
    // El orden de alta en world.entities, común a objetos y personas
    // (th_object.entity_seq y th_person.entity_seq): el último creado tiene el
    // mayor. Los huecos de obj se reutilizan, así que el índice no lo da.
    // La basura también está en world.entities (Litter es un Object).
    uint32_t entity_seq;
    // El recorrido de world.entities en th_sim_tick. World:onTick lo hace con
    // ipairs (world.lua:923-928) y World:destroyEntity quita con table.remove
    // (world.lua:1792-1800): si sale una entidad que está en la casilla de la
    // que hace tick o antes (casi siempre ella misma, al desaparecer por el
    // borde del mapa, spawn.lua:67-72), todo se corre una casilla y la
    // siguiente se queda sin tick esa hora. tick_seq es el entity_seq de lo
    // que hace tick (0 fuera del recorrido), removed_behind cuántas salieron
    // así en su tick y skip_through hasta qué entity_seq se salta.
    uint32_t tick_seq, skip_through;
    uint8_t removed_behind;
    int16_t radiator_thob; // -1 si no hay radiadores
    // Temperatura de cada casilla, con dos memorias que se alternan (las de
    // level_map), y lo que hace falta para recalcular solo lo que puede
    // cambiar: por casilla, los pesos de sus vecinos y el tipo de mezcla
    // (th_sim_temperature_prepare), qué casillas cambiaron en la última pasada
    // y con qué aire y radiador se hizo.
    // Lo de cada casilla va junto: en tres tablas separadas por 32 KiB, las
    // tres lecturas de una casilla caen en la misma línea de la caché de datos
    // de la VR4300 (8 KiB, de correspondencia directa) y se echan unas a otras.
    // La pasada se puede dejar a medias (temp_defer) y seguir entre
    // fotogramas: nadie lee la temperatura hasta el tick siguiente, que la
    // acaba antes de nada.
    uint8_t temp_idx, temp_all;
    uint8_t temp_defer;   // th_sim_tick deja la pasada por hacer
    uint8_t temp_pending; // hay una pasada a medias
    uint16_t temp_word;   // siguiente palabra de temp_need
    uint16_t temp_air, temp_radiator;
    uint32_t temp_cells_total; // casillas recalculadas desde el principio, para medir
    struct {
        uint16_t t[2]; // las dos memorias
        uint16_t cell; // pesos de los vecinos, tipo de mezcla y suma de pesos
    } temp[TH_MAP_WIDTH * TH_MAP_HEIGHT];
    uint32_t temp_changed[TH_MAP_WIDTH * TH_MAP_HEIGHT / 32];
    uint32_t temp_need[TH_MAP_WIDTH * TH_MAP_HEIGHT / 32]; // lo que recalcula la pasada en curso
    // Las casillas que se mezclan con el aire y con el radiador: las que hay
    // que recalcular si cambia uno de los dos (th_sim_temperature_prepare).
    uint32_t temp_air_mask[TH_MAP_WIDTH * TH_MAP_HEIGHT / 32];
    uint32_t temp_rad_mask[TH_MAP_WIDTH * TH_MAP_HEIGHT / 32];
    // th_sim_map_changed en tres tiempos (el riesgo 1 de la demo): al
    // cambiar el mapa solo se apunta que ha cambiado (temp_map_dirty); al
    // acabar la orden (th_sim_temperature_flush, desde th_sim_apply_orders y
    // th_build_tick) se lee, casilla a casilla, lo que leen los pesos (el
    // paso a cada lado, si es del hospital o de una sala y si tiene
    // radiador: temp_key); los pesos y las máscaras salen de ahí en la pasada
    // siguiente (th_sim_temperature_step), por partes, antes de recalcular
    // ninguna casilla. temp_prep_word es la siguiente palabra por hacer.
    uint8_t temp_key[TH_MAP_WIDTH * TH_MAP_HEIGHT];
    uint8_t temp_map_dirty;
    uint8_t temp_prep_pending;
    uint16_t temp_prep_word;
    // Faxes y subidas de sueldo: los que esperan en la cola del panel inferior
    // y los que ya enseña (message_windows), con la puerta de los mensajes
    // (UIBottomPanel:_messageDoorTick).
    // guess: el fax de sin sala de diagnóstico ofrece arriesgar; se decide al
    // mandarlo, con la enfermedad descubierta o no en ese momento
    // (PlayerHospital:makeNoDiagnosisRoomFax, player_hospital.lua:632-650).
    uint8_t n_fax, n_shown;
    struct {
        uint8_t who, kind, guess;
    } fax[TH_SIM_FAXES], shown[5];
    uint8_t door_closed, door_delay;
    uint8_t answer; // TH_ANSWER_*
    // Basura, y cuántas veces se ha vomitado y orinado (para las trazas).
    uint16_t n_litter;
    th_litter litter[TH_SIM_LITTER];
    uint16_t litter_lost;      // basura que no cupo (no debería pasar)
    uint16_t would_die;        // pacientes con la salud a 0: aún no se mueren (no hay DieAction)
    uint32_t n_vomit, n_pee;
    // Construir (th/build.h): la ventana de colocar, la sala en obra y la cola de órdenes.
    th_place_window place;
    th_obra obra;
    uint8_t n_orders;
    th_order orders[TH_SIM_ORDERS];
    // Contratar: la semilla de th_sim_init (para el generador del mercado), el
    // mes con que se generó el mercado y los candidatos de cada clase
    // (available[kind - TH_HUM_DOCTOR], en el orden de available_staff).
    // th_sim_hash NO los mete: son función de la semilla, el nivel y el mes,
    // y lo que se contrata entra en la huella como persona.
    uint32_t seed;
    int16_t market_month;
    uint8_t n_available[TH_SIM_STAFF_CLASSES];
    th_staff_profile available[TH_SIM_STAFF_CLASSES][TH_SIM_CANDIDATES];
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
// Huella del estado: la misma en el PC y en la N64. Con la pasada de
// temperatura acabada (th_sim_temperature_step). Incluye el mapa
// (th_map_hash), los objetos y las salas.
uint32_t th_sim_hash(const th_sim *s);

// La carga de la simulación antes de un tick, para medir (th64sim carga y la
// ROM de medida del hito 5): personas en el mapa, pacientes y cuántas caminan.
// Las casillas de temperatura recalculadas van en temp_cells_total y las
// búsquedas de camino, en el buscador (total_searches, total_expanded).
typedef struct th_sim_load {
    uint16_t people, patients, walking;
} th_sim_load;
void th_sim_load_count(const th_sim *s, th_sim_load *l);
// level_map::update_temperatures: una pasada con el aire y el radiador dados
// (de 0 a 65.535). Da lo mismo que recorrer el mapa entero, pero solo
// recalcula las casillas que pueden cambiar. th_sim_temperature_prepare
// recalcula los pesos de las casillas: hay que llamarla si cambia el mapa.
// th_sim_map_changed (abajo) hace lo mismo en dos veces: lee el mapa en
// el momento y deja el resto para el principio de la pasada siguiente.
void th_sim_temperature_prepare(th_sim *s);
void th_sim_update_temperatures(th_sim *s, uint16_t air, uint16_t radiator);
// La misma pasada por partes: begin la prepara y step recalcula como mucho
// «palabras» × 32 casillas; devuelve 1 si aún queda. Si el mapa cambió
// (th_sim_map_changed), cada step calcula antes los pesos de otras tantas
// palabras, hasta tenerlos todos, y solo entonces sigue con las casillas.
#define TH_SIM_TEMP_WORDS (TH_MAP_WIDTH * TH_MAP_HEIGHT / 32)
void th_sim_temperature_begin(th_sim *s, uint16_t air, uint16_t radiator);
int th_sim_temperature_step(th_sim *s, int palabras);

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
// Necesidades de los pacientes (sim.c): Patient:setTile (la basura del
// camino) y la basura que dejan vomitar y orinar.
void th_sim_patient_set_tile(th_sim *s, int who, int x, int y);
void th_sim_add_litter(th_sim *s, int x, int y, int type, int mirror);
// World:removeAllLitter: quita la basura de la casilla (toda la del juego se
// puede limpiar). Devuelve cuántas.
int th_sim_remove_litter_at(th_sim *s, int x, int y);
// Objetos.
int th_sim_object_at(const th_sim *s, int x, int y, int type); // TH_NONE si no hay
// Los objetos de la simulación para th/objects.h.
th_objects th_sim_objects(th_sim *s);
// World:newObject: un objeto nuevo con su casilla y lo que la simulación
// lleva de él (tipo, cola y lista de los mostradores; un mostrador busca
// recepcionista, ReceptionDesk:setTile). Devuelve su índice o TH_NONE si no
// cabe: quien llama lo mira antes con th_sim_object_fits.
int th_sim_new_object(th_sim *s, th_rnode def, int x, int y, int dir);
// Si cabe un objeto nuevo de esa definición: un hueco en obj y, si lleva cola
// (mostrador o puerta), una cola libre. Es el tope del motor; CorsixTH no lo tiene.
int th_sim_object_fits(const th_sim *s, th_rnode def);
// World:destroyEntity de un objeto: sale del mapa y de la simulación, y su
// hueco queda libre.
void th_sim_destroy_object(th_sim *s, int obj);
// World:newRoom y Room:initRoom sin markRoom: una sala nueva con el id de
// CorsixTH (#rooms + 1), inactiva y sin terminar, con su puerta (y la
// esclava de una doble, o TH_NONE); con existing (índice en rooms), se
// reutiliza esa sala, como al volver a la fase de la puerta. Devuelve su
// índice en rooms, o TH_NONE si no caben más.
int th_sim_new_room(th_sim *s, int type, int x, int y, int w, int h, int door, int door2, int existing);
// World:markRoomAsBuilt: roomFinished (built, is_active, el ánimo por
// ventanas y espacio), el libro de casos descubre diag_<id> y notifyNewRoom.
void th_sim_room_mark_built(th_sim *s, int room);
// Después de cambiar banderas o radiadores: la temperatura vuelve a mirar el
// mapa entero. Solo lo apunta: el mapa se lee en th_sim_temperature_flush
// (con puntos de pausa), que llaman th_sim_apply_orders al acabar cada orden
// y th_build_tick al acabar la obra, y si no, th_sim_temperature_begin.
void th_sim_map_changed(th_sim *s);
void th_sim_temperature_flush(th_sim *s);
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
// Llamadas de personal (CallsDispatcher).
void th_sim_call_for_staff(th_sim *s, th_room *r);
int th_sim_answer_call(th_sim *s, int who);
void th_sim_answer_call_action(th_sim *s, int who);
int th_sim_staff_meander(th_sim *s, int who, int must_happen);
void th_sim_call_checkpoint(th_sim *s, int room, int serial);
void th_sim_call_checkpoint_removed(th_sim *s, int who, int room, int serial);
int32_t th_sim_random_micro(th_sim *s); // math.random() en millonésimas: [0, 1.000.000)
// Personal y contratar (sim.c).
// StaffProfile:getFairWage (staff_profile.lua:236-255): MinSalary de la clase
// + habilidad · 1000 / SalaryAbilityDivisor + SalaryAdd por título (médicos),
// con floor y nunca menos que MinSalary. skill en millonésimas, titles TH_TITLE_*.
int32_t th_sim_fair_wage(const th_sim *s, int kind, int32_t skill, unsigned titles);
// World:makeAvailableStaff(month): regenera el mercado entero (month es
// monthOfGame del mes que acaba; 0 al crear el mundo).
void th_sim_make_available_staff(th_sim *s, int month);
// Los candidatos de una clase (TH_HUM_DOCTOR..TH_HUM_RECEPTIONIST): cuántos
// hay y, en *list, el primero. 0 y NULL con otra clase.
static inline int th_sim_candidates(const th_sim *s, int kind, const th_staff_profile **list)
{
    const int k = kind - TH_HUM_DOCTOR;
    if (k < 0 || k >= TH_SIM_STAFF_CLASSES) {
        if (list)
            *list = 0;
        return 0;
    }
    if (list)
        *list = s->available[k];
    return s->n_available[k];
}
// El resumen de un perfil (th_order.def de TH_ORD_CONTRATAR): un hash de
// todos sus campos, para que la orden contrate a la persona que vio quien la
// dio y no a la que ocupe ese índice si el mercado cambió (decisión 5 de
// docs/06_plan.md).
uint32_t th_staff_profile_digest(const th_staff_profile *pf);
// UIPlaceStaff:onMouseUp con un perfil nuevo (place_staff.lua:156-175), sin
// validar nada (lo valida TH_ORD_CONTRATAR, build.c): newEntity, setProfile,
// setTile, Hospital:addStaff (cobra el sueldo y avisa a los pacientes que
// esperan: staff_change_callback) y, según la casilla, Room:onHumanoidEnter o
// onPlaceInCorridor. Devuelve el índice en hum, o TH_NONE si no cabe.
int th_sim_hire(th_sim *s, const th_staff_profile *pf, int x, int y);
// Receptionist:onPlaceInCorridor (receptionist.lua:70-79): el mostrador
// libre más cercano por el camino (findObjectNear con occupy). 1 si ocupa uno.
int th_sim_receptionist_find_desk(th_sim *s, int who);
// Avisos a los pacientes que esperan sala con SeekRoom (seek_room.lua,
// humanoid.c): build_callback al terminar una sala (th_sim_room_mark_built) y
// staff_change_callback al contratar a alguien que no es recepcionista ni bedel.
// Con los datos reales, staff_change_callback aún no tiene efecto: ninguna
// enfermedad se trata en la consulta, así que checkDiseaseRequirements
// siempre falla (disease_requirements_missing, humanoid.c) hasta el 6.5.
// Los dos quitan el fax del paciente que va a la sala (removeMessage).
void th_sim_notify_new_room(th_sim *s, int room);
void th_sim_notify_staff_change(th_sim *s, int staff);
// Pacientes.
void th_sim_go_home(th_sim *s, int who, int reason);
void th_sim_destroy(th_sim *s, int who);
// Una entidad sale de world.entities (para el recorrido de th_sim_tick: th_sim.tick_seq).
void th_sim_entity_removed(th_sim *s, uint32_t entity_seq);
void th_sim_set_diagnosed(th_sim *s, int who);
int th_sim_agrees_to_pay(th_sim *s, int who, const char *disease_id);
void th_sim_send_fax(th_sim *s, int who, int kind);
// PlayerHospital:removeMessage: quita el fax pendiente del paciente (si lo
// tiene) y pone message a 0.
void th_sim_remove_message(th_sim *s, int who);

#endif
