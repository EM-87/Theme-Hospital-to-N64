// Ventanas de la interfaz como las de CorsixTH (window.lua): paneles, que son
// sprites de una hoja colocados en la ventana, y botones sobre los paneles,
// que cambian el sprite del panel mientras se pulsan. Es C portable: la
// ventana se dibuja en una lista de dibujo (vista.h) y recibe el puntero y
// los botones del ratón, sea el de la N64, el stick o una prueba en el PC.
// No sabe qué hace cada botón: al soltarlo devuelve su número.
#ifndef TH_UI_H
#define TH_UI_H

#include <stdint.h>

#include "vista.h"

#define TH_UI_PANELS 48
#define TH_UI_BUTTONS 24
#define TH_UI_REGIONS 8

typedef struct th_ui_panel {
    int16_t x, y; // en la ventana
    uint16_t sprite;
    uint8_t sheet; // TH_SHEET_*
    uint8_t visible;
} th_ui_panel;

typedef struct th_ui_button {
    int16_t x, y, r, b; // rectángulo en la ventana: [x, r) × [y, b)
    uint8_t panel;      // el panel cuyo sprite cambia
    uint8_t enabled, is_toggle, toggled, active;
    uint16_t sprite_normal, sprite_active, sprite_disabled;
    uint16_t id;           // lo que devuelve al pulsarlo
    const char *tooltip;   // ruta en los textos (NULL = sin descripción)
    int16_t tip_x, tip_y;  // dónde va la descripción, en la ventana
} th_ui_button;

// makeTooltip: una zona con descripción que no es un botón.
typedef struct th_ui_region {
    int16_t x, y, r, b;
    int16_t tip_x, tip_y;
    const char *tooltip;
} th_ui_region;

typedef struct th_ui_window {
    int16_t x, y, w, h; // en la pantalla
    uint8_t n_panels, n_buttons, n_regions;
    int8_t active_button; // el que se está pulsando (−1: ninguno)
    th_ui_panel panels[TH_UI_PANELS];
    th_ui_button buttons[TH_UI_BUTTONS];
    th_ui_region regions[TH_UI_REGIONS];
} th_ui_window;

enum { TH_UI_LEFT = 0, TH_UI_RIGHT = 1 }; // botones del ratón

// Ventana de w × h; (x, y) como Window:setPosition: un valor en [0, 1) es la
// fracción del hueco libre en la pantalla y uno negativo cuenta desde el
// borde derecho o inferior.
void th_ui_window_init(th_ui_window *win, int w, int h);
void th_ui_window_place(th_ui_window *win, double x, double y, int screen_w, int screen_h);

// addPanel: devuelve su índice, o −1 si no caben más.
int th_ui_add_panel(th_ui_window *win, int sheet, int sprite, int x, int y);
// makeButtonOnPanel: el rectángulo (x, y, w, h) es relativo al panel y
// sprite_active es el del panel mientras se pulsa. Devuelve el índice o −1.
int th_ui_make_button(th_ui_window *win, int panel, int x, int y, int w, int h, int sprite_active, int id);
void th_ui_set_toggle(th_ui_window *win, int button);
// Button:toggle: cambia el estado y el sprite de reposo.
void th_ui_toggle(th_ui_window *win, int button);
// Button:setTooltip: la descripción va centrada encima del botón.
void th_ui_button_tooltip(th_ui_window *win, int button, const char *tooltip);
// Window:makeTooltip: zona [x, r) × [y, b) con descripción encima, en el centro.
void th_ui_region_tooltip(th_ui_window *win, const char *tooltip, int x, int y, int r, int b);

// Window:draw: los paneles visibles, en orden.
int th_ui_window_draw(const th_ui_window *win, th_drawlist *out);

// El ratón, con (x, y) en la pantalla. mouse_down y mouse_move devuelven si
// la ventana se ha dado por aludida; mouse_up devuelve el id del botón
// pulsado y soltado encima, o −1.
int th_ui_mouse_down(th_ui_window *win, int button, int x, int y);
int th_ui_mouse_up(th_ui_window *win, int button, int x, int y);
int th_ui_mouse_move(th_ui_window *win, int x, int y);

// getTooltipAt: la descripción bajo (x, y) de la pantalla y dónde va (su
// esquina inferior izquierda, en la pantalla), o NULL.
const char *th_ui_tooltip_at(const th_ui_window *win, int x, int y, int *tx, int *ty);

#endif
