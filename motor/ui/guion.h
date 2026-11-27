// El guion de las ventanas de la partida (la demo jugable, docs/06_plan.md):
// el intérprete de los pasos de tools/hito6/ventanas.txt y
// tools/hito6/ventanas_partida.txt, para que th64sim ventanas (el PC) y la ROM
// de pruebas (platform/n64/main.c, comprobar_ventanas) ejecuten EXACTAMENTE los
// mismos fotogramas. Es C portable, sin libdragon ni malloc: lleva el puntero,
// los botones de la pila (th_ui_stack) y del panel inferior, la pausa y los
// ticks pedidos, y deja al anfitrión el fotograma de las ventanas (el PC llama a
// th_game_ui_frame y mete las órdenes en la cola; la ROM pasa por
// ventanas_fotograma y partida_mandar_orden) y la hora de simulación (el PC,
// th_sim_tick; la ROM, la simulación repartida, simulacion.h).
//
// Cada línea del guion:
//   <nombre> <cx> <cy> <giro> <paso>...
// con la casilla del mundo en el centro de la pantalla, el giro de la cámara
// (0–3) y los pasos desde el escenario recién cargado, uno por fotograma salvo
// tick:
//   panel:salas|objetos|contratar   el botón del panel
//   boton:<id>, botond:<id>         clic izquierdo (o derecho: id_right) en el
//                                   botón de la ventana con ese id (TH_GWB_*)
//   clic:x,y, clicd:x,y             clic izquierdo o derecho en la casilla
//   baja:x,y, mueve:x,y, sube:x,y   el botón izquierdo baja, el puntero se
//                                   mueve y el botón sube, en casillas
//   esc                             Escape
//   pausa:0|1                       en pausa desde ese paso (sin fotograma)
//   camara:x,y                      la cámara, con la casilla (x, y) del mundo
//                                   en el centro (sin fotograma; para la
//                                   partida de tools/hito6/ventanas_partida.txt,
//                                   que no cabe en una pantalla)
//   tick:n                          n horas de simulación (con un fotograma cada una)
//   puntero:x,y, px:x,y             el puntero del final (casilla o píxel)
// y, al acabar los pasos, un fotograma más sin nada (el que se dibuja).
//
// Los fotogramas de un paso, en este orden (el de th64sim ventanas desde la
// pieza 5):
//   - con tick:n, n veces: una hora de simulación y un fotograma con el paso;
//     las órdenes de cada uno se aplican en la hora siguiente;
//   - sin tick: un fotograma con el paso; si manda órdenes, una hora (que las
//     aplica y devuelve su resultado a las ventanas) y un fotograma quieto
//     (sin clics: las ventanas como las vería el siguiente, por ejemplo los
//     botones que la fase habilita); si este manda más, otra hora.
// En cada fotograma, antes de llamar al anfitrión: el puntero se mueve sobre
// el panel (th_bottom_panel_mouse_move) y sobre la pila, los botones bajan y
// suben por la pila (lo que ninguna ventana toma va al mundo, world_down y
// world_up; un botón del panel, a panel_button) y th_ui_stack_update(33), como
// interfaz_mandos en la ROM. El fotograma quieto solo hace th_ui_stack_update.
//
// Los límites de este camino (lo que el guion no recorre tal cual lo haría
// un jugador; lo cubren, en parte, los modos de th_guion_host.mando y la hora
// troceada de la ROM de pruebas, platform/n64/main.c,
// comprobar_ventanas_partida):
//   - sin mando, el guion hace él mismo el mouse_down y el mouse_up sobre la
//     pila, y en el mismo fotograma (clic: y boton: bajan y suben a la vez,
//     así que el mundo recibe world_down y world_up juntos); el panel acepta
//     el id 0 (interfaz.c, boton_soltado, exige id > 0) y la cuenta de la
//     descripción no avanza;
//   - el anfitrión del PC acaba la hora entera antes de cada fotograma
//     (sim_idle = 1 siempre): nunca se ve la simulación a medio tick ni una
//     orden retenida por partida_vaciar (platform/n64/partida.c).
#ifndef TH_UI_GUION_H
#define TH_UI_GUION_H

#include "panel_inferior.h"
#include "ui.h"
#include "ventanas.h"
#include "vista.h"

// Lo que pone el anfitrión.
typedef struct th_guion_host {
    void *ctx;
    // Un fotograma: th_game_ui_frame con in (puntero, clics, Escape, pausa;
    // sim_idle = 1) y la cámara del guion, y las órdenes de out a la cola del
    // núcleo con tick = s->hours (las que no quepan, th_game_ui_order_dropped).
    // Devuelve cuántas órdenes mandó la ventana (out->n_orders).
    int (*frame)(void *ctx, const th_game_ui_input *in, const th_view *v, int rot, th_game_ui_output *out);
    // Una hora de simulación entera (th_sim_tick): aplica las órdenes de la cola
    // y su resultado vuelve a las ventanas por el gancho (th_game_ui_order_result).
    void (*tick)(void *ctx);
    // Opcional (NULL: el guion lleva los botones). El botón raton
    // (TH_UI_LEFT o TH_UI_RIGHT) baja (abajo = 1) o sube en el mando del
    // anfitrión, que en el fotograma siguiente lo pasa por la pila como un
    // jugador (en la ROM, interfaz_mandos) y pone él los clics de in (in->win,
    // in->world_down... llegan vacíos; panel_button y escape, los del paso).
    // Entonces un clic son dos fotogramas, el de bajar y el de subir, y la
    // pila (th_ui_stack_update) también la actualiza el anfitrión.
    void (*mando)(void *ctx, int raton, int abajo);
} th_guion_host;

typedef struct th_guion {
    // Del anfitrión (th_guion_init).
    th_ui_stack *stack;     // la pila, con el panel abajo del todo
    th_bottom_panel *panel; // el panel inferior de la pila
    th_game_ui *g;          // las ventanas (th_game_ui_window: los botones por su id)
    int screen_w, screen_h;
    // El estado: la línea en curso.
    char nombre[64];
    th_view v; // la cámara: (cx, cy) en el centro de la pantalla, unidad 16
    int rot;   // el giro de la cámara (0–3)
    int px, py;
    int pausa;
    const char *p;    // los pasos que quedan
    int acabado;      // ya se hizo el fotograma del final
    uint32_t horas;   // horas que el guion pidió (tick:n y las de las órdenes)
    int fotogramas;   // fotogramas que dio
    char error[128];  // por qué falló el último paso
} th_guion;

void th_guion_init(th_guion *gs, th_ui_stack *stack, th_bottom_panel *panel, th_game_ui *g, int screen_w,
                   int screen_h);

// Empieza una línea del guion: el nombre, la cámara y los pasos. 0, o −1 si
// la línea no es un estado (vacía o un comentario, «#»). La línea tiene que
// durar mientras se recorre (los pasos se leen de ella).
int th_guion_linea(th_guion *gs, const char *linea);

// El paso siguiente con sus fotogramas. 1 si queda más; 0 tras el fotograma
// del final (out es lo que se dibuja); −1 si el paso no vale (gs->error).
int th_guion_paso(th_guion *gs, const th_guion_host *h, th_game_ui_output *out);

// El centro del rombo de la casilla (x, y) del mundo en la pantalla, con la
// cámara y el giro del guion.
void th_guion_pixel(const th_guion *gs, int x, int y, int *px, int *py);

#endif
