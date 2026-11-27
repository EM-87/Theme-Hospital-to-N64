// th64sim hito4: corre el escenario del hito 4 con la simulación del núcleo
// (th/sim.h) y escribe la misma traza que bench/scenarios/traza_hito4.lua en
// CorsixTH, para compararlas con tools/oraculo_hito4/resumen.py:
//   pacientes.tsv  una línea por paciente: horas de cada paso (-1 = no llegó)
//   dinero.tsv     cada movimiento de dinero: hora, importe (+/-), motivo
//   dias.tsv       cada 50 horas: saldo y cuántos pacientes hay en cada fase
//   acciones.tsv   horas-paciente pasadas en cada acción, por fase
// Las horas, como en la traza de CorsixTH: un suceso dentro de la hora k
// (desde 0) lleva k; lo que se mira al acabar la hora, k + 1.
// th64sim partida escribe lo mismo repitiendo una lista de órdenes con tick
// (abajo).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "th/build.h"
#include "th/map.h"
#include "th/rules.h"
#include "th/scenario.h"
#include "th/sim.h"
#include "th64sim.h"

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
                                       "staff_reception", "call_checkpoint", "answer_call", "vomit",
                                       "pee",         "yawn",       "tap_foot",   "check_watch"};
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
    // Con los movimientos de construir, who es la definición (th_rnode) del
    // objeto o de la sala, no una persona (build.c: money_build, llamada
    // desde comprar, vender y levantar la sala).
    const int de_obra =
        motivo == TH_MONEY_BUILD_ROOM || motivo == TH_MONEY_BUY_OBJECT || motivo == TH_MONEY_SELL_OBJECT;
    const int de_persona = !de_obra;
    const registro *r = de_persona && who != TH_NONE && who >= 0 && who < TH_SIM_HUMANOIDS &&
                                s->hum[who].kind == TH_HUM_PATIENT
                            ? de(s, who)
                            : NULL;
    const int id = r ? (int)(r - reg) : 0;
    if (motivo == TH_MONEY_TREATMENT && r)
        reg[id].pagos++;
    const char *texto = motivo == TH_MONEY_TREATMENT           ? "Treat: General Practice"
                        : motivo == TH_MONEY_INSURANCE_PENDING ? "seguro (pendiente)"
                        : motivo == TH_MONEY_INSURANCE         ? "Insurance"
                        // Las órdenes de construir (th64sim partida), con los
                        // nombres del dinero.tsv de la batería de construir.
                        : motivo == TH_MONEY_BUY_OBJECT  ? "buy_object"
                        : motivo == TH_MONEY_SELL_OBJECT ? "sell_object"
                        : motivo == TH_MONEY_BUILD_ROOM  ? "build_room"
                        : motivo == TH_MONEY_HIRE_STAFF  ? "hire_staff"
                                                         : "Wages";
    if (motivo == TH_MONEY_INSURANCE_PENDING && r)
        reg[id].pagos++;
    // Los de construir, «<motivo>:<id>» como en el dinero.tsv de la batería
    // (construir.c, al_mover_dinero), y 0 en la columna del paciente.
    if (de_obra)
        fprintf(f_dinero, "%u\t%ld\t%s:%s\t0\n", (unsigned)s->hours, (long)importe, texto,
                th64sim_id_de(s, (th_rnode)who));
    else if (motivo == TH_MONEY_HIRE_STAFF) // who es la persona contratada: su clase, como en la batería
        fprintf(f_dinero, "%u\t%ld\t%s:%s\t0\n", (unsigned)s->hours, (long)importe, texto,
                th64sim_clase_nombre(s, who));
    else
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
// Carga el nivel del escenario esc (ya leído) con sus casillas, las reglas y
// las animaciones, y prepara la simulación.
static th_sim *cargar_con(const char *assets, const th_rules *esc_leido, const char *tan_ruta, uint32_t semilla)
{
    static th_rules defs, nivel, esc;
    char ruta[1024];
    esc = *esc_leido;
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
    // El escenario vacío de th64sim mercado no trae casillas.
    const th_rnode celdas = th_rules_get(&esc, esc.root, "celdas");
    if (celdas && th_scenario_apply_cells(&mapa, &esc, celdas) < 0) {
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

th_sim *th64sim_cargar_escenario(const char *assets, const char *trg, const char *tan_ruta, uint32_t semilla)
{
    th_rules esc;
    if (cargar_reglas(trg, &esc))
        return NULL;
    return cargar_con(assets, &esc, tan_ruta, semilla);
}

// La traza de hito4 y partida: abre dias.tsv y dinero.tsv y engancha los
// sucesos de la simulación.
static FILE *f_dias;
static int32_t saldo0;

static int traza_empezar(th_sim *s, const char *dir)
{
    s->ev.patient = suceso;
    s->ev.money = dinero;
    f_dias = abrir(dir, "dias.tsv");
    f_dinero = abrir(dir, "dinero.tsv");
    if (!f_dias || !f_dinero)
        return 1;
    fputs("hora\timporte\tmotivo\tpaciente\n", f_dinero);
    fputs("hora\tfecha\tsaldo\tpacientes\tllegando\tcola_recepcion\ttras_recepcion\tconsulta\tdiagnosticado\ta_casa\n",
          f_dias);
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
    saldo0 = s->hosp.balance;
    volcar_dia(f_dias, s);
    return 0;
}

// Lo que se mira al acabar la hora h (desde 0).
static void traza_hora(th_sim *s, int h)
{
    muestrear(s);
    if (f_medico)
        sondear_gp(s);
    if ((h + 1) % 50 == 0)
        volcar_dia(f_dias, s);
}

// Cierra la traza: pacientes.tsv, acciones.tsv y la línea del resumen.
static int traza_terminar(th_sim *s, const char *dir, int horas, const char *nombre)
{
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
    for (int i = 1; i <= s->patients_spawned && i <= MAX_PACIENTES; i++) {
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
    // Las casillas con más de una cosa (la basura se amontona) y lo que no cupo.
    int pilas = 0, mayor = 0;
    for (int i = 0; i < TH_MAP_MAX_STACKS; i++) {
        pilas += s->w.map->stacks[i].n > 0;
        if (s->w.map->stacks[i].n > mayor)
            mayor = s->w.map->stacks[i].n;
    }

    printf("%s: %u pacientes en %d horas; saldo %ld (%+ld); reputación %ld; huella %08lx; %lu vómitos, %lu "
           "orinadas, %u basuras en el suelo, %u con la salud a 0; %d casillas con varias cosas (hasta %d), %u "
           "sin sitio\n",
           nombre, (unsigned)s->patients_spawned, horas, (long)s->hosp.balance, (long)(s->hosp.balance - saldo0),
           (long)s->hosp.reputation, (unsigned long)th_sim_hash(s), (unsigned long)s->n_vomit,
           (unsigned long)s->n_pee, (unsigned)s->n_litter, (unsigned)s->would_die, pilas, pilas ? mayor + 1 : 1,
           (unsigned)s->w.map->thob_lost);
    return 0;
}

// th64sim hito4 <assets> <escenario.trg> <semilla> <horas> <dir_salida>
int th64sim_hito4(char **argv)
{
    const char *dir = argv[6];
    if (!th64sim_cargar_escenario(argv[2], argv[3], NULL, (uint32_t)strtoul(argv[4], NULL, 0)))
        return 1;
    if (traza_empezar(&sim, dir))
        return 1;
    const int horas = atoi(argv[5]);
    for (int h = 0; h < horas; h++) {
        th_sim_tick(&sim);
        traza_hora(&sim, h);
    }
    return traza_terminar(&sim, dir, horas, "hito4");
}

// ---------------------------------------------------------------------------
// th64sim partida <assets> <escenario.trg> <ordenes.tsv> <semilla> <horas> <dir_salida>
//
// Repite una partida: las órdenes del jugador con el tick en el que se
// aplican (TSV con cabecera: tick, orden y sus argumentos, con el vocabulario
// de bench/lib/ordenes.lua más «contratar»; th64sim.h), sobre el escenario y
// la semilla de la partida. Las órdenes entran en la cola del núcleo
// (th_sim_push_order) antes del tick de su hora y se aplican al empezarlo
// (th_sim_tick: th_build_tick y th_sim_apply_orders), como en la ROM, que las
// empuja cuando el jugador las da. La cola tiene TH_SIM_ORDERS huecos: si un
// mismo tick tiene más órdenes que huecos, las que no caben se aplican en el
// tick siguiente y se avisa (la ROM tampoco puede meter más por tick). Una
// orden de contratar sin resumen del perfil (la lista escrita a mano) lo toma
// al entrar en la cola del candidato de ese índice en el mercado de ese
// momento; con él (el registro de la ROM), se repite tal cual y se rechaza
// igual que allí si el mercado era otro.
//
// Escribe la traza de hito4 (dias.tsv, pacientes.tsv, dinero.tsv y
// acciones.tsv) más:
//   ordenes_resultado.tsv  tick, orden, aceptada/rechazada, motivo: del gancho
//                          th_sim_events.order, con el tick en el que se aplicó
//   ordenes_rom.log        las mismas órdenes con las líneas que escribe la
//                          ROM («TH64 orden», tools/hito6/README.md), para
//                          compararlas con su registro (tools/hito6/repetir.sh)
// y al final la línea «partida: N órdenes, A aceptadas; H horas; huella …;
// saldo …; P pacientes», con N las órdenes aplicadas.
#define PARTIDA_ORDENES 4096

typedef struct {
    th_order o;
    char texto[96];
} orden_leida;

static orden_leida partida_ordenes[PARTIDA_ORDENES];
static int partida_n, partida_aplicadas, partida_aceptadas;
static FILE *f_resultado, *f_rom;

// El gancho de la cola. Las órdenes entran con los ticks de menor a mayor y
// th_sim_apply_orders aplica las de la hora en el orden en que entraron, así
// que la aplicada es siempre la siguiente de la lista.
static void al_aplicar(void *ctx, const th_sim *s, const th_order *o, const th_order_result *r)
{
    (void)ctx;
    const orden_leida *l = partida_aplicadas < partida_n ? &partida_ordenes[partida_aplicadas] : NULL;
    partida_aplicadas++;
    const int ok = r->reason == TH_REASON_OK;
    partida_aceptadas += ok;
    char motivo[48];
    if (r->reason == TH_REASON_CASILLAS)
        snprintf(motivo, sizeof motivo, "casillas:%d", r->bad);
    else
        snprintf(motivo, sizeof motivo, "%s", th_build_reason_name(r->reason));
    if (l && s->hours != l->o.tick)
        fprintf(stderr, "partida: la orden «%s» del tick %u se aplica en el %u (la cola estaba llena)\n", l->texto,
                (unsigned)l->o.tick, (unsigned)s->hours);
    fprintf(f_resultado, "%u\t%s\t%s\t%s\n", (unsigned)s->hours, l ? l->texto : th64sim_nombre_orden(o->kind),
            ok ? "aceptada" : "rechazada", ok ? "-" : motivo);
    // En contratar, def es el resumen del perfil (th_staff_profile_digest), no
    // un th_rnode: va en hexadecimal, y así lo vuelve a leer th64sim_leer_orden.
    fprintf(f_rom, "TH64 orden\t%u\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%s\t%s\n", (unsigned)s->hours,
            th64sim_nombre_orden(o->kind), o->x, o->y, o->dir, o->qty, o->w, o->h,
            o->kind == TH_ORD_CONTRATAR || o->kind == TH_ORD_DEVOLVER ? th64sim_resumen_de(o->def)
                                                                       : th64sim_id_de(s, o->def),
            ok ? "-" : th_build_reason_name(r->reason));
}

int th64sim_partida(char **argv)
{
    const char *dir = argv[7];
    th_sim *s = th64sim_cargar_escenario(argv[2], argv[3], NULL, (uint32_t)strtoul(argv[5], NULL, 0));
    if (!s)
        return 1;
    FILE *f = fopen(argv[4], "r");
    if (!f) {
        fprintf(stderr, "no se puede leer %s\n", argv[4]);
        return 1;
    }
    static char linea[4096];
    int num_linea = 0;
    partida_n = 0;
    while (fgets(linea, sizeof linea, f)) {
        num_linea++;
        if (!strchr(linea, '\n') && !feof(f)) {
            fprintf(stderr, "%s:%d: línea de más de %zu B\n", argv[4], num_linea, sizeof linea - 2);
            fclose(f);
            return 1;
        }
        // Se lee en un hueco aparte: el tope solo cuenta las órdenes, no los
        // comentarios ni las líneas vacías.
        long tick;
        orden_leida leida;
        if (!th64sim_leer_orden(s, linea, &tick, &leida.o, leida.texto, sizeof leida.texto))
            continue;
        if (partida_n >= PARTIDA_ORDENES) {
            fprintf(stderr, "%s:%d: más de %d órdenes\n", argv[4], num_linea, PARTIDA_ORDENES);
            fclose(f);
            return 1;
        }
        if (tick < 0 || (unsigned long)tick > UINT32_MAX) {
            fprintf(stderr, "%s:%d: tick fuera de rango (%ld)\n", argv[4], num_linea, tick);
            fclose(f);
            return 1;
        }
        orden_leida *l = &partida_ordenes[partida_n];
        *l = leida;
        if (partida_n > 0 && (uint32_t)tick < partida_ordenes[partida_n - 1].o.tick) {
            fprintf(stderr, "%s:%d: los ticks tienen que ir de menor a mayor (%ld)\n", argv[4], num_linea, tick);
            fclose(f);
            return 1;
        }
        if (l->o.kind == TH_ORD_NONE)
            fprintf(stderr, "%s:%d: orden desconocida «%s» (el núcleo la rechazará)\n", argv[4], num_linea, l->texto);
        l->o.tick = (uint32_t)tick;
        partida_n++;
    }
    fclose(f);
    f_resultado = abrir(dir, "ordenes_resultado.tsv");
    f_rom = abrir(dir, "ordenes_rom.log");
    if (!f_resultado || !f_rom || traza_empezar(s, dir))
        return 1;
    fputs("tick\torden\tresultado\tmotivo\n", f_resultado);
    s->ev.order = al_aplicar;
    partida_aplicadas = partida_aceptadas = 0;
    const int horas = atoi(argv[6]);
    int metidas = 0;
    for (int h = 0; h < horas; h++) {
        while (metidas < partida_n && partida_ordenes[metidas].o.tick <= s->hours) {
            th_order *o = &partida_ordenes[metidas].o;
            th64sim_completar_resumen(s, o);
            if (th_sim_push_order(s, o) != 0)
                break;
            metidas++;
        }
        th_sim_tick(s);
        traza_hora(s, h);
    }
    fclose(f_resultado);
    fclose(f_rom);
    if (partida_aplicadas < partida_n)
        fprintf(stderr, "partida: %d órdenes sin aplicar (su tick no llega en %d horas)\n",
                partida_n - partida_aplicadas, horas);
    if (traza_terminar(s, dir, horas, "traza"))
        return 1;
    printf("partida: %d órdenes, %d aceptadas; %d horas; huella %08lx; saldo %ld; %u pacientes\n", partida_aplicadas,
           partida_aceptadas, horas, (unsigned long)th_sim_hash(s), (long)s->hosp.balance,
           (unsigned)s->patients_spawned);
    return 0;
}

// th64sim carga <assets> <escenario.trg> <semilla> <horas>: la carga de cada
// tick (th_sim_load_count antes del tick), las casillas de temperatura que
// recalcula, las búsquedas de camino y los nodos que recorren, y lo que tarda
// el tick en el PC. Es lo mismo que escribe la
// ROM de medida del hito 5 (make MEDIR=1), para juntar las dos por la hora.
int th64sim_carga(char **argv)
{
    th_sim *s = th64sim_cargar_escenario(argv[2], argv[3], NULL, (uint32_t)strtoul(argv[4], NULL, 0));
    if (!s)
        return 1;
    const int horas = atoi(argv[5]);
    printf("hora\tpersonas\tpacientes\tandando\tcasillas_temp\tbusquedas\tnodos\tns_pc\n");
    for (int h = 1; h <= horas; h++) {
        th_sim_load l;
        th_sim_load_count(s, &l);
        const uint32_t b0 = s->w.pf->total_searches, n0 = s->w.pf->total_expanded, c0 = s->temp_cells_total;
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        th_sim_tick(s);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        const long ns = (t1.tv_sec - t0.tv_sec) * 1000000000l + (t1.tv_nsec - t0.tv_nsec);
        printf("%d\t%u\t%u\t%u\t%u\t%u\t%u\t%ld\n", h, l.people, l.patients, l.walking,
               (unsigned)(s->temp_cells_total - c0),
               (unsigned)(s->w.pf->total_searches - b0), (unsigned)(s->w.pf->total_expanded - n0), ns);
    }
    fprintf(stderr, "carga: %d horas; %u pacientes; huella %08lx\n", horas, (unsigned)s->patients_spawned,
            (unsigned long)th_sim_hash(s));
    return 0;
}

// ---------------------------------------------------------------------------
// th64sim mercado <assets> <nivel> <semilla> <mes>
//
// El mercado de candidatos del núcleo (th_sim.available: World:makeAvailableStaff,
// world.lua:510-532, y StaffProfile:randomise, staff_profile.lua:89-209) en
// JSON, para que la batería de construir meta los mismos perfiles en
// world.available_staff de CorsixTH (bench/scenarios/construir.lua,
// --th64-mercado) y «contratar <clase> <índice>» sea la misma persona en los
// dos lados. <nivel> es un escenario TRG1 (inicio.trg de la batería, demo.trg)
// o el número de un nivel, que se carga recién empezado en dificultad full
// con un escenario vacío (solo nivel y dificultad: sin parcelas, sin personal
// y sin salas; el mercado no depende de nada más que del nivel, la semilla y
// el mes). El mercado se regenera con <mes> (th_sim_make_available_staff: el
// monthOfGame del mes que acaba, 0 en una partida nueva).

// TRG1 mínimo: {dificultad = "full", nivel = n} (tools/conv/reglas.py, escribir).
static size_t trg_vacio(uint8_t *d, int n)
{
    size_t k = 16;
    uint32_t pos_cad[3];
    static const char *const cad[3] = {"dificultad", "full", "nivel"};
    for (int i = 0; i < 3; i++) {
        const uint32_t len = (uint32_t)strlen(cad[i]);
        pos_cad[i] = (uint32_t)k;
        d[k++] = 0, d[k++] = 0, d[k++] = 0, d[k++] = (uint8_t)len;
        memcpy(d + k, cad[i], len + 1);
        k += len + 1;
        while (k % 4)
            d[k++] = 0;
    }
    const uint32_t pos_mapa = (uint32_t)k;
#define TRG_U32(v)                                                                                                     \
    (d[k++] = (uint8_t)((v) >> 24), d[k++] = (uint8_t)((v) >> 16), d[k++] = (uint8_t)((v) >> 8), d[k++] = (uint8_t)(v))
#define TRG_VAL(t, v) (d[k++] = (t), d[k++] = 0, d[k++] = 0, d[k++] = 0, TRG_U32(v))
    TRG_U32(2u);
    TRG_VAL(TH_RULES_STRING, pos_cad[0]);
    TRG_VAL(TH_RULES_STRING, pos_cad[1]);
    TRG_VAL(TH_RULES_STRING, pos_cad[2]);
    TRG_VAL(TH_RULES_INT, (uint32_t)n);
    const uint32_t pos_raiz = (uint32_t)k;
    TRG_VAL(TH_RULES_MAP, pos_mapa);
    const uint32_t tam = (uint32_t)k;
    k = 0;
    d[k++] = 'T', d[k++] = 'R', d[k++] = 'G', d[k++] = '1';
    TRG_U32(tam);
    TRG_U32(pos_raiz);
    TRG_U32(0u);
#undef TRG_VAL
#undef TRG_U32
    return tam;
}

int th64sim_mercado(char **argv)
{
    const char *assets = argv[2], *nivel = argv[3];
    const uint32_t semilla = (uint32_t)strtoul(argv[4], NULL, 0);
    const int mes = atoi(argv[5]);
    const size_t ln = strlen(nivel);
    th_sim *s;
    if (ln > 4 && !strcmp(nivel + ln - 4, ".trg"))
        s = th64sim_cargar_escenario(assets, nivel, NULL, semilla);
    else {
        char *fin;
        const long n = strtol(nivel, &fin, 10);
        if (fin == nivel || *fin || n < 1 || n > 99) {
            fprintf(stderr, "mercado: «%s» no es un escenario .trg ni un número de nivel\n", nivel);
            return 1;
        }
        static uint8_t datos[128];
        static th_rules esc;
        const size_t tam = trg_vacio(datos, (int)n);
        if (th_rules_init(&esc, datos, tam) != 0) {
            fprintf(stderr, "mercado: el escenario vacío no vale\n");
            return 1;
        }
        s = cargar_con(assets, &esc, NULL, semilla);
    }
    if (!s)
        return 1;
    th_sim_make_available_staff(s, mes);
    static const char *const clases[TH_SIM_STAFF_CLASSES] = {"doctor", "nurse", "handyman", "receptionist"};
    static const char *const clases_lua[TH_SIM_STAFF_CLASSES] = {"Doctor", "Nurse", "Handyman", "Receptionist"};
    const th_rules *e = s->scen;
    const char *dif = th_rules_str(e, th_rules_get(e, e->root, "dificultad"), NULL);
    printf("{\n  \"nivel\": %ld,\n  \"dificultad\": \"%s\",\n  \"semilla\": %lu,\n  \"mes\": %d,\n  \"clases\": {",
           (long)th_rules_int(e, th_rules_get(e, e->root, "nivel"), 0), dif ? dif : "full", (unsigned long)semilla,
           mes);
    for (int k = 0; k < TH_SIM_STAFF_CLASSES; k++) {
        const th_staff_profile *l;
        const int n = th_sim_candidates(s, TH_HUM_DOCTOR + k, &l);
        printf("%s\n    \"%s\": [", k ? "," : "", clases[k]);
        for (int i = 0; i < n; i++) {
            const th_staff_profile *p = &l[i];
            printf("%s\n      {\"indice\": %d, \"resumen\": \"%08lx\", \"clase\": \"%s\", \"kind\": %u, \"skill\": %ld, "
                   "\"attention\": %ld, \"wage\": %ld, \"titles\": %u, \"junior\": %d, \"consultant\": %d, "
                   "\"surgeon\": %d, \"psychiatrist\": %d, \"researcher\": %d, \"layer5\": %u, \"hair\": %u, "
                   "\"face\": %u, \"chin\": %u, \"desc_swap\": %u, \"name_seed\": %u, \"name_rnd\": [%u, %u], "
                   "\"desc_rnd\": [%u, %u]}",
                   i ? "," : "", i, (unsigned long)th_staff_profile_digest(p), clases_lua[k], p->kind, (long)p->skill,
                   (long)p->attention, (long)p->wage, p->titles, (p->titles & TH_TITLE_JUNIOR) != 0,
                   (p->titles & TH_TITLE_CONSULTANT) != 0, (p->titles & TH_TITLE_SURGEON) != 0,
                   (p->titles & TH_TITLE_PSYCHIATRIST) != 0, (p->titles & TH_TITLE_RESEARCHER) != 0, p->layer5,
                   p->hair, p->face, p->chin, p->desc_swap, p->name_seed, p->name_rnd[0], p->name_rnd[1],
                   p->desc_rnd[0], p->desc_rnd[1]);
        }
        printf("%s]", n ? "\n    " : "");
    }
    printf("\n  }\n}\n");
    return 0;
}
