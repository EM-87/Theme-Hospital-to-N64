// th64sim hito4: corre el escenario del hito 4 con la simulación del núcleo
// (th/sim.h) y escribe la misma traza que bench/scenarios/traza_hito4.lua en
// CorsixTH, para compararlas con tools/oraculo_hito4/resumen.py:
//   pacientes.tsv  una línea por paciente: horas de cada paso (-1 = no llegó)
//   dinero.tsv     cada movimiento de dinero: hora, importe (+/-), motivo
//   dias.tsv       cada 50 horas: saldo y cuántos pacientes hay en cada fase
//   acciones.tsv   horas-paciente pasadas en cada acción, por fase
// Las horas, como en la traza de CorsixTH: un suceso dentro de la hora k
// (desde 0) lleva k; lo que se mira al acabar la hora, k + 1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "th/map.h"
#include "th/rules.h"
#include "th/scenario.h"
#include "th/sim.h"

static th_map mapa;
static th_pathfinder buscador;
static th_sim sim;

#define MAX_PACIENTES 4096

typedef struct {
    int usado;
    char enfermedad[32];
    int seguro, llegada, cola_recepcion, recepcion, entra_gp, sale_gp, visitas_gp, diagnostico, pagos, a_casa;
    int muerte, desaparece, dentro;
    double progreso;
    const char *motivo;
} registro;

static registro reg[MAX_PACIENTES + 1];
static FILE *f_dinero;
// TH64_SONDA_GP=<dir>: lo mismo que bench/scenarios/sonda_gp.lua (medico.tsv y visitas.tsv).
static FILE *f_medico, *f_visitas;

enum { F_LLEGANDO, F_COLA, F_TRAS_RECEPCION, F_CONSULTA, F_DIAGNOSTICADO, F_A_CASA, N_FASES };
static const char *const fases[N_FASES] = {"llegando", "cola_recepcion", "tras_recepcion",
                                           "consulta", "diagnosticado",  "a_casa"};
static const char *const acciones[] = {"-",           "idle",  "walk",       "spawn",      "seek_reception",
                                       "queue",       "use_object", "knock_door", "meander", "seek_room",
                                       "staff_reception", "call_checkpoint", "answer_call"};
static long horas_accion[N_FASES][sizeof acciones / sizeof acciones[0]];

static uint8_t *leer(const char *ruta, size_t *len)
{
    FILE *f = fopen(ruta, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)n + 1);
    if (d && fread(d, 1, (size_t)n, f) != (size_t)n) {
        free(d);
        d = NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return d;
}

static int cargar_reglas(const char *ruta, th_rules *r)
{
    size_t len;
    uint8_t *d = leer(ruta, &len);
    if (!d || th_rules_init(r, d, len) != 0) {
        fprintf(stderr, "no se pueden leer las reglas %s\n", ruta);
        return 1;
    }
    return 0;
}

static registro *de(const th_sim *s, int who)
{
    const int id = s->hum[who].u.p.id;
    return id > 0 && id <= MAX_PACIENTES ? &reg[id] : NULL;
}

static void suceso(void *ctx, const th_sim *s, int who, int ev, int arg)
{
    (void)ctx;
    registro *r = de(s, who);
    if (!r)
        return;
    const int hora = (int)s->hours;
    switch (ev) {
    case TH_EV_SPAWN: {
        memset(r, 0, sizeof *r);
        r->usado = 1;
        snprintf(r->enfermedad, sizeof r->enfermedad, "%s", s->dis[s->hum[who].u.p.disease].id);
        r->seguro = s->hum[who].u.p.insurance != 0;
        r->llegada = hora;
        r->cola_recepcion = r->recepcion = r->entra_gp = r->sale_gp = r->diagnostico = r->a_casa = -1;
        r->muerte = r->desaparece = -1;
        r->motivo = "-";
        break;
    }
    case TH_EV_DIAGNOSED: r->diagnostico = hora; break;
    case TH_EV_GO_HOME:
        if (r->a_casa < 0) {
            r->a_casa = hora;
            r->motivo = arg == TH_HOME_KICKED ? "kicked" : arg == TH_HOME_OVER_PRICED ? "over_priced" : "cured";
        }
        break;
    case TH_EV_DESPAWN: r->desaparece = hora; break;
    case TH_EV_DEALT:
        if (f_visitas)
            fprintf(f_visitas, "%d\tdespacha\n", hora);
        break;
    default: break;
    }
}

static void dinero(void *ctx, const th_sim *s, int32_t importe, int motivo, int who)
{
    (void)ctx;
    const registro *r = who != TH_NONE && who >= 0 && who < TH_SIM_HUMANOIDS && s->hum[who].kind == TH_HUM_PATIENT
                            ? de(s, who)
                            : NULL;
    const int id = r ? (int)(r - reg) : 0;
    if (motivo == TH_MONEY_TREATMENT && r)
        reg[id].pagos++;
    const char *texto = motivo == TH_MONEY_TREATMENT           ? "Treat: General Practice"
                        : motivo == TH_MONEY_INSURANCE_PENDING ? "seguro (pendiente)"
                        : motivo == TH_MONEY_INSURANCE         ? "Insurance"
                                                               : "Wages";
    if (motivo == TH_MONEY_INSURANCE_PENDING && r)
        reg[id].pagos++;
    fprintf(f_dinero, "%u\t%ld\t%s\t%d\n", (unsigned)s->hours, (long)importe, texto, id);
}

static int fase(const registro *r)
{
    if (r->a_casa >= 0)
        return F_A_CASA;
    if (r->diagnostico >= 0)
        return F_DIAGNOSTICADO;
    if (r->entra_gp >= 0)
        return F_CONSULTA;
    if (r->recepcion >= 0)
        return F_TRAS_RECEPCION;
    if (r->cola_recepcion >= 0)
        return F_COLA;
    return F_LLEGANDO;
}

// TH64_DEPURAR=<id>: la cola de acciones de ese paciente tras cada hora.
static int depurar = -1;

static void depurar_paciente(th_sim *s, int who)
{
    const th_person *p = &s->hum[who];
    fprintf(stderr, "%5u p%d (%d,%d) sala %d:", (unsigned)s->hours, p->u.p.id, p->h.tile_x, p->h.tile_y, p->h.in_room);
    for (int i = 0; i < p->h.queue_len; i++) {
        const th_action *a = &p->h.queue[i];
        fprintf(stderr, " %s%s%s", acciones[a->kind], (a->flags & TH_AF_MUST_HAPPEN) ? "!" : "",
                (a->flags & TH_AF_TODO_INTERRUPT) ? "*" : "");
    }
    fprintf(stderr, " | t=%u fn=%u fax=%d\n", p->h.timer, p->h.timer_fn, s->n_fax);
}

static void sondear_gp(const th_sim *s)
{
    static const char *const objetos[] = {"other", "door", "bench", "reception_desk", "desk", "cabinet", "chair",
                                          "radiator"};
    static uint8_t sentado[TH_SIM_HUMANOIDS];
    for (int k = 0; k < s->n_order; k++) {
        const int who = s->order[k];
        const th_person *p = &s->hum[who];
        if (!p->alive)
            continue;
        const th_action *a = p->h.queue_len ? &p->h.queue[0] : NULL;
        const char *obj = a && a->kind == TH_ACTION_USE_OBJECT ? objetos[s->obj[a->obj].type] : "-";
        if (p->kind == TH_HUM_DOCTOR) {
            const th_room *r = p->h.in_room ? th_sim_room((th_sim *)s, p->h.in_room) : NULL;
            fprintf(f_medico, "%u\t%s\t%s\t%u\t%d\t%.4f\t%s\n", (unsigned)s->hours, a ? acciones[a->kind] : "-", obj,
                    (unsigned)p->h.anim, p->h.slow_animation ? 1 : 0, p->u.s.fatigue / 1e6,
                    r ? s->room_types[r->type] : "-");
        } else if (p->kind == TH_HUM_PATIENT) {
            const int en_silla = a && a->kind == TH_ACTION_USE_OBJECT && s->obj[a->obj].type == TH_OBJ_CHAIR;
            if (en_silla && !sentado[who])
                fprintf(f_visitas, "%u\tsienta\n", (unsigned)s->hours);
            sentado[who] = (uint8_t)en_silla;
        }
    }
}

// Lo que traza_hito4.lua mira al acabar cada hora.
static void muestrear(th_sim *s)
{
    const int t = (int)s->hours;
    const int gp = th_sim_room_type(s, "gp");
    for (int k = 0; k < s->n_order; k++) {
        const int who = s->order[k];
        th_person *p = &s->hum[who];
        if (p->kind != TH_HUM_PATIENT)
            continue;
        registro *r = de(s, who);
        if (!r || !r->usado)
            continue;
        if (r->cola_recepcion < 0)
            for (int d = 0; d < s->n_desks; d++)
                if (th_sim_queue_action_of(s, who, s->obj[s->desks[d]].queue))
                    r->cola_recepcion = t;
        if (r->recepcion < 0 && p->u.p.has_passed_reception)
            r->recepcion = t;
        th_room *room = p->h.in_room ? th_sim_room(s, p->h.in_room)
                                     : (p->h.tile_x >= 0 ? th_sim_room_at(s, p->h.tile_x, p->h.tile_y) : NULL);
        const int en_gp = room && room->type == gp;
        if (en_gp && !r->dentro) {
            r->dentro = 1;
            r->visitas_gp++;
            if (r->entra_gp < 0)
                r->entra_gp = t;
        } else if (!en_gp && r->dentro) {
            r->dentro = 0;
            r->sale_gp = t;
        }
        r->progreso = p->u.p.diagnosis_progress / 1e6;
        if (p->u.p.id == depurar)
            depurar_paciente(s, who);
        const unsigned a = p->h.queue_len ? p->h.queue[0].kind : 0;
        horas_accion[fase(r)][a < sizeof acciones / sizeof acciones[0] ? a : 0]++;
    }
}

static void volcar_dia(FILE *f, const th_sim *s)
{
    int n[N_FASES] = {0}, total = 0;
    for (int k = 0; k < s->n_order; k++) {
        const int who = s->order[k];
        if (s->hum[who].kind != TH_HUM_PATIENT)
            continue;
        const registro *r = de(s, who);
        if (!r)
            continue;
        total++;
        n[fase(r)]++;
    }
    fprintf(f, "%u\t%u-%02u-%02uT%02u\t%ld\t%d", (unsigned)s->hours, (unsigned)s->date.year, (unsigned)s->date.month,
            (unsigned)s->date.day, (unsigned)s->date.hour, (long)s->hosp.balance, total);
    for (int i = 0; i < N_FASES; i++)
        fprintf(f, "\t%d", n[i]);
    fputc('\n', f);
}

static FILE *abrir(const char *dir, const char *nombre)
{
    char ruta[1024];
    snprintf(ruta, sizeof ruta, "%s/%s", dir, nombre);
    FILE *f = fopen(ruta, "w");
    if (!f)
        fprintf(stderr, "no se puede escribir %s\n", ruta);
    return f;
}

// Carga el nivel del escenario con sus casillas, las reglas y las
// animaciones (las de assets/datam si tan es NULL) y prepara la simulación.
th_sim *th64sim_cargar_escenario(const char *assets, const char *trg, const char *tan_ruta, uint32_t semilla)
{
    static th_rules defs, nivel, esc;
    char ruta[1024];
    if (cargar_reglas(trg, &esc))
        return NULL;
    const int n = th_rules_int(&esc, th_rules_get(&esc, esc.root, "nivel"), 0);
    const char *dif = th_rules_str(&esc, th_rules_get(&esc, esc.root, "dificultad"), NULL);
    snprintf(ruta, sizeof ruta, "%s/reglas/definiciones.trg", assets);
    if (cargar_reglas(ruta, &defs))
        return NULL;
    snprintf(ruta, sizeof ruta, "%s/reglas/%s%02d.trg", assets, dif ? dif : "full", n);
    if (cargar_reglas(ruta, &nivel))
        return NULL;
    size_t len;
    snprintf(ruta, sizeof ruta, "%s/niveles/nivel%02d.tmp", assets, n);
    uint8_t *tmp = leer(ruta, &len);
    const int cargado = tmp && th_map_load(&mapa, tmp, len, n) == TH_MAP_OK;
    free(tmp);
    if (!cargado) {
        fprintf(stderr, "no se puede cargar %s\n", ruta);
        return NULL;
    }
    if (th_scenario_apply_cells(&mapa, &esc, th_rules_get(&esc, esc.root, "celdas")) < 0) {
        fprintf(stderr, "las casillas del escenario no valen\n");
        return NULL;
    }
    snprintf(ruta, sizeof ruta, "%s/datam/animaciones.tan", assets);
    if (tan_ruta)
        snprintf(ruta, sizeof ruta, "%s", tan_ruta);
    uint8_t *tan = leer(ruta, &len);
    static th_anims anims;
    if (!tan || th_anims_init(&anims, tan, len) != 0) {
        fprintf(stderr, "no se pueden leer las animaciones %s\n", ruta);
        return NULL;
    }
    const int e = th_sim_init(&sim, &mapa, &buscador, &anims, &defs, &nivel, &esc, semilla);
    if (e) {
        fprintf(stderr, "el escenario no se puede cargar (%d)\n", e);
        return NULL;
    }
    return &sim;
}

// th64sim hito4 <assets> <escenario.trg> <semilla> <horas> <dir_salida>
int th64sim_hito4(char **argv)
{
    const char *dir = argv[6];
    if (!th64sim_cargar_escenario(argv[2], argv[3], NULL, (uint32_t)strtoul(argv[4], NULL, 0)))
        return 1;
    sim.ev.patient = suceso;
    sim.ev.money = dinero;
    FILE *f_dias = abrir(dir, "dias.tsv");
    f_dinero = abrir(dir, "dinero.tsv");
    if (!f_dias || !f_dinero)
        return 1;
    fputs("hora\timporte\tmotivo\tpaciente\n", f_dinero);
    fputs("hora\tfecha\tsaldo\tpacientes\tllegando\tcola_recepcion\ttras_recepcion\tconsulta\tdiagnosticado\ta_casa\n",
          f_dias);
    const int horas = atoi(argv[5]);
    if (getenv("TH64_DEPURAR"))
        depurar = atoi(getenv("TH64_DEPURAR"));
    if (getenv("TH64_SONDA_GP")) {
        f_medico = abrir(getenv("TH64_SONDA_GP"), "medico.tsv");
        f_visitas = abrir(getenv("TH64_SONDA_GP"), "visitas.tsv");
        if (!f_medico || !f_visitas)
            return 1;
        fputs("hora\taccion\tobjeto\tanimacion\tlento\tcansancio\tsala\n", f_medico);
        fputs("hora\tsuceso\n", f_visitas);
    }
    const int32_t saldo0 = sim.hosp.balance;
    volcar_dia(f_dias, &sim);
    for (int h = 0; h < horas; h++) {
        th_sim_tick(&sim);
        muestrear(&sim);
        if (f_medico)
            sondear_gp(&sim);
        if ((h + 1) % 50 == 0)
            volcar_dia(f_dias, &sim);
    }
    fclose(f_dias);
    fclose(f_dinero);
    if (f_medico) {
        fclose(f_medico);
        fclose(f_visitas);
    }
    FILE *f = abrir(dir, "pacientes.tsv");
    if (!f)
        return 1;
    fputs("id\tenfermedad\tseguro\tllegada\tcola_recepcion\trecepcion\tentra_gp\tsale_gp\tvisitas_gp\tprogreso\t"
          "diagnostico\tpagos\ta_casa\tmotivo\tmuerte\tdesaparece\n",
          f);
    for (int i = 1; i <= sim.patients_spawned && i <= MAX_PACIENTES; i++) {
        const registro *r = &reg[i];
        fprintf(f, "%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%.3f\t%d\t%d\t%d\t%s\t%d\t%d\n", i, r->enfermedad, r->seguro,
                r->llegada, r->cola_recepcion, r->recepcion, r->entra_gp, r->sale_gp, r->visitas_gp, r->progreso,
                r->diagnostico, r->pagos, r->a_casa, r->motivo, r->muerte, r->desaparece);
    }
    fclose(f);
    f = abrir(dir, "acciones.tsv");
    if (!f)
        return 1;
    fputs("fase\taccion\thoras_paciente\n", f);
    for (int i = 0; i < N_FASES; i++)
        for (unsigned a = 0; a < sizeof acciones / sizeof acciones[0]; a++)
            if (horas_accion[i][a])
                fprintf(f, "%s\t%s\t%ld\n", fases[i], acciones[a], horas_accion[i][a]);
    fclose(f);
    printf("hito4: %u pacientes en %d horas; saldo %ld (%+ld); reputación %ld; huella %08lx\n",
           (unsigned)sim.patients_spawned, horas, (long)sim.hosp.balance, (long)(sim.hosp.balance - saldo0),
           (long)sim.hosp.reputation, (unsigned long)th_sim_hash(&sim));
    return 0;
}
