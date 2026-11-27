// Lo que comparten los ficheros de th64sim (th64sim.c, hito4.c y construir.c).
#ifndef TH64SIM_H
#define TH64SIM_H

#include <stddef.h>
#include <stdint.h>

#include "th/sim.h"

// hito4.c: carga el nivel del escenario con sus casillas, las reglas y las
// animaciones (las de assets/datam si tan_ruta es NULL) y prepara la
// simulación.
th_sim *th64sim_cargar_escenario(const char *assets, const char *trg, const char *tan_ruta, uint32_t semilla);

// construir.c: una línea de una lista de órdenes (TSV), con el vocabulario de
// bench/lib/ordenes.lua: la primera columna es un número (n en las listas de
// la batería, el tick en las de th64sim partida) y la segunda, la orden; sus
// argumentos van detrás. Más «contratar <clase> <índice> <x> <y>» (clase
// doctor, nurse, handyman o receptionist), que la batería aún no tiene.
// Devuelve 0 si la línea no es una orden (vacía, comentario o cabecera), 1 si
// lo es (o->kind TH_ORD_NONE si no se conoce); en texto, la orden y sus
// argumentos separados por espacios. Cambia linea.
int th64sim_leer_orden(const th_sim *s, char *linea, long *n, th_order *o, char *texto, size_t tam);

// construir.c: el id de def dentro de defs.objetos o defs.salas, o «-».
const char *th64sim_id_de(const th_sim *s, th_rnode def);

// construir.c: el nombre de la orden en ese vocabulario, o «?».
const char *th64sim_nombre_orden(int kind);

int th64sim_hito4(char **argv);               // hito4.c
int th64sim_carga(char **argv);               // hito4.c
int th64sim_partida(char **argv);             // hito4.c
int th64sim_construir(int argc, char **argv); // construir.c

#endif
