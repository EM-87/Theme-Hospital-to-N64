#include "simulacion.h"

#include <libdragon.h>

#include "objetos.h"
#include "th/pausa.h"

#define PILA (32 * 1024)

static th_sim *sim;
static coroutine_t *corrutina;
static th_view_entity *ents;
static int max_ents;
static instantanea_t foto;
static simulacion_stats_t stats;
static int pedidos;      // ticks pedidos y no empezados
static int en_curso;     // 1 mientras hay un tick o su pasada de temperatura a medias
static uint32_t limite;  // hasta cuándo puede seguir la corrutina en esta vuelta
static uint32_t entrada; // cuándo se reanudó por última vez
static int en_temperatura;
static uint32_t ultima_pausa; // para el mayor tramo sin pausas
static int etapa;             // 1 tick, 2 instantánea, 3 temperatura
static int giro;              // la cámara de las instantáneas
static void (*al_acabar)(th_sim *); // simulacion_al_acabar_tick

// Suma a la cuenta el tiempo desde la última vez.
static void contar(void)
{
    const uint32_t ahora = get_ticks(), d = ahora - entrada;
    stats.cpu += d;
    if (en_temperatura)
        stats.cpu_temperatura += d;
    entrada = ahora;
}

// Suspende la corrutina y apunta el tiempo que llevaba.
static void ceder(void)
{
    contar();
    coro_yield();
    entrada = ultima_pausa = get_ticks();
}

// El gancho de th/pausa.h: si se ha pasado el límite, hasta el fotograma siguiente.
static void pausa(void)
{
    const uint32_t ahora = get_ticks();
    if (ahora - ultima_pausa > stats.tramo_mas_largo) {
        stats.tramo_mas_largo = ahora - ultima_pausa;
        stats.etapa_tramo = etapa;
        stats.hora_tramo = sim->hours;
    }
    ultima_pausa = ahora;
    if ((int32_t)(ahora - limite) >= 0) {
        stats.pausas++;
        ceder();
    }
}

static void hacer_instantanea(void)
{
    foto.entidades = ents;
    foto.n_entidades = th_vista_sim_rot(sim, giro, ents, max_ents);
    foto.giro = giro;
    foto.saldo = sim->hosp.balance;
    foto.reputacion = sim->hosp.reputation;
    foto.fecha = sim->date;
    foto.personas = sim->n_order;
    foto.horas = sim->hours;
}

#ifdef TH_PRUEBAS
// La pila de la corrutina, pintada al empezar (bucle): lo más bajo que se ha
// escrito da lo más que se ha usado, interrupciones incluidas (en libdragon
// van por la pila en curso). Se deja sin pintar el último KiB, el de abajo, y
// lo que ya usa pintar.
#define MARCA 0x5A5AA5A5u
static uint32_t *pila_arriba, *pila_abajo;

static void pintar(void)
{
    volatile uint32_t aqui = 0;
    pila_arriba = (uint32_t *)((uintptr_t)&aqui & ~(uintptr_t)7);
    pila_abajo = pila_arriba - (PILA - 1024) / 4;
    for (uint32_t *q = pila_abajo; q < pila_arriba - 64; q++)
        *q = MARCA;
}

int simulacion_pila_usada(void)
{
    if (!pila_abajo)
        return -1;
    const uint32_t *q = pila_abajo;
    while (q < pila_arriba && *q == MARCA)
        q++;
    return (int)((const char *)pila_arriba - (const char *)q);
}

int simulacion_pila(void)
{
    return PILA;
}
#endif

static void bucle(void *arg)
{
    (void)arg;
#ifdef TH_PRUEBAS
    pintar();
#endif
    entrada = ultima_pausa = get_ticks();
    for (;;) {
        while (pedidos == 0)
            ceder();
        pedidos--;
        en_curso = 1;
        // Lo que tarda el tick, sumando los trozos en que se reparte.
        contar();
        const uint32_t cpu0 = stats.cpu;
        sim->temp_defer = 1;
        etapa = 1;
        const uint32_t hora = sim->hours;
        th_sim_tick(sim);
        contar();
        if (stats.cpu - cpu0 > stats.tick_mas_largo)
            stats.tick_mas_largo = stats.cpu - cpu0;
        const uint32_t temp0 = stats.cpu_temperatura;
        stats.ultima_temperatura = 0;
        stats.ultimo_tick = stats.cpu - cpu0;
        stats.hora_ultimo_tick = hora;
        etapa = 2;
        hacer_instantanea();
        pausa();
        contar();
        etapa = 3;
        en_temperatura = 1;
        while (th_sim_temperature_step(sim, 16))
            pausa();
        contar();
        en_temperatura = 0;
        stats.ultima_temperatura = stats.cpu_temperatura - temp0;
        etapa = 0;
        stats.ticks++;
        en_curso = 0;
        if (al_acabar)
            al_acabar(sim);
    }
}

void simulacion_iniciar(th_sim *s, th_view_entity *e, int max_e)
{
    sim = s;
    ents = e;
    max_ents = max_e;
    pedidos = en_curso = 0;
    stats = (simulacion_stats_t){0};
    hacer_instantanea();
    if (!corrutina)
        corrutina = coro_create(bucle, NULL, PILA);
}

void simulacion_pedir(int ticks)
{
    pedidos += ticks;
}

void simulacion_soltar(void)
{
    pedidos = 0;
}

int simulacion_pendientes(void)
{
    return pedidos + en_curso;
}

int simulacion_a_medias(void)
{
    return en_curso;
}

void simulacion_al_acabar_tick(void (*fn)(th_sim *s))
{
    al_acabar = fn;
}

int simulacion_en_tick(void)
{
    return en_curso && etapa == 1;
}

void simulacion_avanzar(uint32_t hasta)
{
    if (!simulacion_pendientes())
        return;
    limite = hasta;
    th_pausa = pausa;
    coro_resume(corrutina);
    th_pausa = NULL;
}

void simulacion_terminar(void)
{
    while (simulacion_pendientes())
        simulacion_avanzar(get_ticks() + TICKS_FROM_MS(1000));
}

const instantanea_t *simulacion_instantanea(void)
{
    return &foto;
}

void simulacion_girar(int g)
{
    giro = g & 3;
    if (sim && etapa != 1) // a medio tick las entidades no se pueden leer
        hacer_instantanea();
}

const simulacion_stats_t *simulacion_stats(void)
{
    return &stats;
}
