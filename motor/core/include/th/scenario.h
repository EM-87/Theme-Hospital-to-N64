// Escenarios: una partida guardada de CorsixTH llevada al motor (fase 5,
// hito 4). bench/scenarios/exportar_escenario.lua vuelca lo que el jugador
// ha construido y el estado de la simulación, y
// tools/oraculo_hito4/escenario.py lo pasa a TRG1 (th/rules.h) con estas
// partes:
//   celdas     las casillas que difieren del mapa que deja th_map_load, como
//              [x, y, suelo, norte, oeste, banderas, sala, objeto], con las
//              banderas en el formato TH_TILE_* (sin las de sombra, que se
//              recalculan);
//   objetos    [{id, x, y, direccion, sala}], con el id del objeto de las
//              definiciones;
//   salas      [{num, id, x, y, ancho, alto, puerta = {x, y, direccion}}];
//   personal   [{clase, x, y, perfil = {habilidad, atencion, sueldo, ...}}];
//   fecha, mundo y hospital: el reloj, las llegadas y el dinero.
#ifndef TH_SCENARIO_H
#define TH_SCENARIO_H

#include "th/map.h"
#include "th/rules.h"

// Aplica las casillas al mapa y recalcula las sombras. Devuelve cuántas
// casillas cambió, o -1 si la lista no vale.
int th_scenario_apply_cells(th_map *m, const th_rules *r, th_rnode cells);

#endif
