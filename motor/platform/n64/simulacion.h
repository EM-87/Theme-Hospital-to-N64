// La simulación en una corrutina (libdragon coroutine.h), repartida entre
// fotogramas. El bucle principal pide los ticks que tocan por el reloj
// (simulacion_pedir) y, después de dibujar, le da a la simulación el tiempo
// que sobra hasta un límite (simulacion_avanzar). La simulación se suspende
// en los puntos de pausa del núcleo (th/pausa.h) y sigue en el fotograma
// siguiente: un camino largo o una pasada de temperatura entera ya no se
// comen un fotograma, se reparten entre varios.
//
// El dibujo no lee la simulación, que puede estar a medio tick: lee la
// instantánea que se toma al acabar cada tick (las entidades de th_vista_sim,
// el dinero, la reputación y la fecha). La pasada de temperatura de cada tick se hace
// después de la instantánea (temp_defer), antes del tick siguiente.
//
// Lo que se simula es lo mismo que con th_sim_tick seguido: la huella no
// cambia.
#ifndef TH64_SIMULACION_H
#define TH64_SIMULACION_H

#include <stdint.h>

#include "th/sim.h"
#include "vista.h"

typedef struct {
    const th_view_entity *entidades;
    int n_entidades;
    int32_t saldo;
    int32_t reputacion;
    th_date fecha;
    int personas;
    uint32_t horas; // ticks acabados
} instantanea_t;

typedef struct {
    uint32_t ticks;           // ticks acabados desde el principio
    uint32_t cpu;             // tiempo de CPU dentro de la corrutina (get_ticks)
    uint32_t cpu_temperatura; // de ello, la pasada de temperatura
    uint32_t tick_mas_largo;  // CPU del tick más largo (sin su temperatura), sumando sus trozos
    uint32_t pausas;          // veces que se suspendió con trabajo pendiente
    uint32_t tramo_mas_largo; // el mayor tiempo entre dos puntos de pausa
    int etapa_tramo;          // dónde acabó: 1 tick, 2 instantánea, 3 temperatura
    uint32_t hora_tramo;
} simulacion_stats_t;

// ents: sitio para la instantánea (lo que haría falta para th_vista_sim).
void simulacion_iniciar(th_sim *s, th_view_entity *ents, int max_ents);
void simulacion_pedir(int ticks);
int simulacion_pendientes(void); // ticks pedidos que aún no se han empezado o acabado
// Sigue simulando hasta el instante limite (get_ticks) o hasta que no quede
// nada por hacer.
void simulacion_avanzar(uint32_t limite);
// Acaba todo lo pedido, sin límite de tiempo.
void simulacion_terminar(void);
const instantanea_t *simulacion_instantanea(void);
const simulacion_stats_t *simulacion_stats(void);

#endif
