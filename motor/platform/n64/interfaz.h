// La interfaz de la partida en la N64 (hito 6.1): el panel inferior, las
// descripciones de sus botones y el puntero, que mueven el stick o el ratón
// de N64. La lógica es la de ui/ (portable); aquí se cargan las hojas de baja
// resolución, se leen los mandos y se dibuja con el RDP por el mismo camino
// que el mapa (mapa_n64.c).
#ifndef TH64_INTERFAZ_H
#define TH64_INTERFAZ_H

#include <libdragon.h>
#include <stdbool.h>

#include "mapa_n64.h"
#include "panel_inferior.h"

// Carga las hojas (rom:/datam/panel02m.tsh, mpointer.tsh y las fuentes de
// rom:/fuentes/) y los textos en inglés, la primera vez. Con mandos, empieza
// a leerlos en cada barrido (interfaz_mandos_iniciar); la ROM de pruebas no
// los necesita.
bool interfaz_cargar(int ancho, int alto, bool mandos);

// Empieza a leer los mandos en cada barrido. Desde entonces no hay que
// llamar a joypad_poll: interfaz_mandos o interfaz_leer dan la última lectura.
void interfaz_mandos_iniciar(void);
// La última lectura del puerto 1, sin mover el puntero (el menú, el visor).
void interfaz_leer(joypad_inputs_t *mando);
// Dónde está el puntero, en píxeles de la pantalla.
void interfaz_puntero(int *x, int *y);

// Lo que el juego tiene que hacer después de leer los mandos.
typedef struct {
    int vista_dx, vista_dy; // desplazar la vista (borde de la pantalla)
    int boton;              // botón del panel pulsado y soltado (TH_PI_*), o 0
    // Un botón de otra ventana de la pila pulsado y soltado: la ventana, su id
    // (id o id_right) y con qué botón del ratón (TH_UI_LEFT o TH_UI_RIGHT). Las
    // pulsaciones de los botones repetidos se recogen con th_ui_stack_take_click.
    th_ui_window *ventana; // NULL: ninguno
    int id, raton;
    // Lo que ninguna ventana tomó y llega al mundo: bit TH_UI_LEFT o TH_UI_RIGHT
    // de los botones que bajaron o subieron en este fotograma.
    int mundo_abajo, mundo_arriba;
    th_ui_window *sobre; // la ventana de arriba bajo el puntero (NULL: el mundo)
} interfaz_orden_t;

// Un fotograma de mandos del puerto 1: el stick o el ratón mueven el
// puntero; A y B son los botones izquierdo y derecho; Z acelera el puntero.
// Los mandos se leen en cada barrido (no hay que llamar a joypad_poll); en
// *mando deja la última lectura, para los demás botones.
interfaz_orden_t interfaz_mandos(joypad_inputs_t *mando);

// La lista de la interfaz (la pila de ventanas con el panel, la descripción
// y el puntero) para info. 0, o −1 si algo no cupo en la lista.
int interfaz_lista(const th_bottom_info *info, th_drawlist *out);

// La dibuja con el RDP encima de lo que haya.
void interfaz_dibujar(const th_bottom_info *info, const uint16_t *paleta, mapa_n64_stats_t *st);

// Las hojas de la interfaz por índice TH_SHEET_* (las del mapa a NULL), para
// el rasterizador de referencia de la ROM de pruebas; con TH_SHEET_MUTTON.
const th_sheet *const *interfaz_hojas(void);

// La pila de ventanas, con el panel inferior abajo del todo: las ventanas de
// la partida se abren con th_ui_stack_open (las TH_UI_MAIN cierran la
// anterior) y se dibujan encima del panel con su draw. Las fuentes y los
// textos, para dibujarlas.
th_ui_stack *interfaz_pila(void);
const th_ui_fonts *interfaz_fuentes(void);
const th_rules *interfaz_textos(void);

// Pone el puntero en (x, y) con los botones como se pidan y la descripción
// a la vista o no (para las comprobaciones de la ROM de pruebas).
void interfaz_estado(int x, int y, bool pulsado, bool descripcion);

// La ROM de pruebas con las ventanas (comprobar_ventanas, el guion de
// ui/guion.h): el panel inferior de la pila (su hover y sus botones, para
// th_guion_init); el puntero en (x, y) sin tocar los botones ni la pila, y sin
// descripción (la cuenta de quieto a cero, como en th64sim ventanas, que no las
// dibuja); y una lista de la interfaz (interfaz_lista) dibujada con el RDP como
// interfaz_dibujar, para medir aparte la lista y el envío.
th_bottom_panel *interfaz_panel(void);
// La pila y el panel como recién cargados (cada estado del guion empieza así,
// como en th64sim ventanas): sin ventanas encima, sin botones abajo ni hover.
void interfaz_reiniciar(void);
void interfaz_poner_puntero(int x, int y);
// La ROM de pruebas (el guion con mando, comprobar_ventanas_partida): el
// botón A (TH_UI_LEFT) o B (TH_UI_RIGHT) del mando abajo o arriba, como si lo
// hubiese leído el gancho de cada barrido; interfaz_mandos lo verá en la
// llamada siguiente. Solo antes de interfaz_mandos_iniciar (que lo pisaría).
void interfaz_inyectar_boton(int boton, bool abajo);
void interfaz_dibujar_lista(const th_drawlist *l, const uint16_t *paleta, mapa_n64_stats_t *st);

#endif
