// La partida de la demo jugable (pieza 6 de docs/06_plan.md): las órdenes del
// jugador hacia la cola del núcleo (th_sim_push_order), el registro que las
// graba por debugf para repetirlas en el PC (tools/hito6/README.md, «La
// partida grabada en la ROM»; tools/hito6/repetir.sh), el aviso de las
// rechazadas (texto en la barra y un sonido) y el cierre de la partida con
// su huella. Las ventanas (ui/ventanas.h) reciben por aquí el resultado de
// cada orden aplicada.
#ifndef TH64_PARTIDA_H
#define TH64_PARTIDA_H

#include <stdbool.h>
#include <stdint.h>

#include "th/build.h"
#include "ventanas.h"

// Empieza a grabar la partida de s: engancha th_sim_events.order (el registro
// y th_game_ui_order_result de g, que puede ser NULL) y, si escenario no es
// NULL, escribe «TH64 partida nueva: escenario <escenario>, semilla <semilla>».
// Con el escenario a NULL (un hospital adelantado con make HORAS=N, que el PC
// no puede repetir desde la hora 0) graba las órdenes sin esa línea.
void partida_empezar(th_sim *s, th_game_ui *g, const char *escenario, uint32_t semilla);

// Manda una orden: tick = s->hours y a la cola del núcleo, que la aplica al
// empezar el tick siguiente (o en este, si la corrutina aún no ha llegado a
// th_sim_apply_orders: el registro apunta el tick en que se aplicó). Si la
// simulación está a medio tick (simulacion_en_tick) o la cola está llena,
// espera en un hueco propio y entra con partida_vaciar. 0, o −1 si no cupo ni
// ahí (se descarta y se dice).
int partida_mandar_orden(th_sim *s, const th_order *o);
// Mete en la cola las que esperan, si la simulación no está a medio tick.
void partida_vaciar(th_sim *s);
// Órdenes que esperan, en el hueco propio y en la cola del núcleo.
int partida_pendientes(const th_sim *s);

// La última orden rechazada desde la llamada anterior: 1 y su motivo
// (TH_REASON_*), o 0.
int partida_rechazo(int *motivo);
// Las órdenes aplicadas desde que empezó la partida (y, si no es NULL, en
// *aceptadas las que el núcleo aceptó).
int partida_aplicadas(int *aceptadas);
// El último tick que aplicó alguna orden: su hora, cuántas y sus nombres
// separados por comas.
const char *partida_ultimo_tick(uint32_t *hora, int *cuantas);

// El aviso de un rechazo que da la ventana (th_game_ui_output.reject_sound):
// el texto (copiado; NULL, ninguno) en la barra del panel durante unos 3 s, y
// el sonido (si partida_sonido_iniciar lo cargó). Un rechazo del núcleo no
// avisa por sí solo (place_objects.lua:513-522): partida_rechazo lo dice.
void partida_aviso(const char *texto);
// Un texto de la ventana (th_game_ui_output.message): sale mientras la
// ventana lo dé y unos 3 s más, sin sonido.
void partida_texto(const char *texto);
// El texto que va en th_bottom_info.message este fotograma, o NULL.
const char *partida_mensaje(void);
// El sonido de los rechazos: wrong2.wav, el que CorsixTH pone al no poder
// contratar, construir o comprar (hire_staff.lua:135, edit_room.lua:935,
// furnish_corridor.lua:106), de rom:/sonido/comun. Inicia el audio;
// partida_sonido_cerrar lo para. En cada fotograma, partida_sonido_fotograma
// (mixer_try_play).
bool partida_sonido_iniciar(void);
void partida_sonido_fotograma(void);
void partida_sonido_cerrar(void);

// Cierra la partida: sin órdenes nuevas, y con la simulación ya terminada
// (simulacion_terminar: th_sim_tick acabado), descarta las que esperan (y lo
// dice), suelta el gancho y escribe «TH64 partida: <horas> horas, huella
// <hash>, saldo <saldo>».
void partida_terminar(th_sim *s);

// Los nombres del registro (los de th64sim: platform/pc/construir.c).
const char *partida_nombre_orden(int kind);
const char *partida_id_de(const th_sim *s, th_rnode def);

#endif
