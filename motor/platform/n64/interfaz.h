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
// rom:/fuentes/) y los textos en inglés. Con mandos, empieza a leerlos en
// cada barrido (interfaz_mandos); la ROM de pruebas no los necesita.
bool interfaz_cargar(int ancho, int alto, bool mandos);

// Lo que el juego tiene que hacer después de leer los mandos.
typedef struct {
    int vista_dx, vista_dy; // desplazar la vista (borde de la pantalla)
    int boton;              // botón del panel pulsado y soltado (TH_PI_*), o 0
} interfaz_orden_t;

// Un fotograma de mandos del puerto 1: el stick o el ratón mueven el
// puntero; A y B son los botones izquierdo y derecho; Z acelera el puntero.
// Los mandos se leen en cada barrido (no hay que llamar a joypad_poll); en
// *mando deja la última lectura, para los demás botones.
interfaz_orden_t interfaz_mandos(joypad_inputs_t *mando);

// La lista de la interfaz (panel, descripción y puntero) para info.
void interfaz_lista(const th_bottom_info *info, th_drawlist *out);

// La dibuja con el RDP encima de lo que haya.
void interfaz_dibujar(const th_bottom_info *info, const uint16_t *paleta, mapa_n64_stats_t *st);

// Las hojas de la interfaz por índice TH_SHEET_* (las del mapa a NULL), para
// el rasterizador de referencia de la ROM de pruebas.
const th_sheet *const *interfaz_hojas(void);

// Pone el puntero en (x, y) con los botones como se pidan y la descripción
// a la vista o no (para las comprobaciones de la ROM de pruebas).
void interfaz_estado(int x, int y, bool pulsado, bool descripcion);

#endif
