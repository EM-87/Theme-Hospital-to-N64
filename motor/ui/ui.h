// Ventanas de la interfaz como las de CorsixTH (window.lua): paneles, que son
// sprites de una hoja colocados en la ventana, y botones sobre los paneles,
// que cambian el sprite del panel mientras se pulsan. Es C portable: la
// ventana se dibuja en una lista de dibujo (vista.h) y recibe el puntero y
// los botones del ratón, sea el de la N64, el stick o una prueba en el PC.
// No sabe qué hace cada botón: al soltarlo devuelve su número.
//
// Encima, una pila de ventanas (th_ui_stack) como la de UI (ui.lua): la de
// arriba recibe el ratón primero, y lo que ninguna toma llega al mundo.
#ifndef TH_UI_H
#define TH_UI_H

#include <stdint.h>

#include "vista.h"

// Amueblar el pasillo con 10 filas (UIFurnishCorridor, furnish_corridor.lua:
// 79-123) pide 59 paneles y 32 botones; queda margen para un fondo de color.
#define TH_UI_PANELS 64
#define TH_UI_BUTTONS 40
#define TH_UI_REGIONS 8
#define TH_UI_CLICKS 8 // pulsaciones de botones repetidos sin recoger

typedef struct th_ui_panel {
    int16_t x, y;    // en la ventana
    uint16_t sprite; // con sheet = TH_SHEET_FILL, el color RGBA5551
    uint8_t sheet;   // TH_SHEET_* o TH_SHEET_FILL
    uint8_t visible;
    int16_t w, h; // solo para TH_SHEET_FILL: el tamaño del rectángulo
} th_ui_panel;

typedef struct th_ui_button {
    int16_t x, y, r, b; // rectángulo en la ventana: [x, r) × [y, b)
    uint8_t panel;      // el panel cuyo sprite cambia
    uint8_t enabled, is_toggle, toggled, active;
    uint8_t is_repeat; // Button:makeRepeat
    uint16_t sprite_normal, sprite_active, sprite_disabled;
    uint16_t id;          // lo que devuelve al pulsarlo con el izquierdo
    int16_t id_right;     // on_rightclick: el del derecho (−1: no responde al derecho)
    const char *tooltip;  // ruta en los textos (NULL = sin descripción)
    int16_t tip_x, tip_y; // dónde va la descripción, en la ventana
} th_ui_button;

// makeTooltip: una zona con descripción que no es un botón.
typedef struct th_ui_region {
    int16_t x, y, r, b;
    int16_t tip_x, tip_y;
    const char *tooltip;
} th_ui_region;

// Una pulsación de un botón: su id (id o id_right) y con qué botón del ratón.
typedef struct th_ui_click {
    int16_t id;
    uint8_t mouse; // TH_UI_LEFT o TH_UI_RIGHT
} th_ui_click;

struct th_ui_window;
// Window:draw de cada clase: lo que pinta encima de los paneles (textos,
// vistas previas). Si no es NULL, th_ui_stack_draw la llama en lugar de
// th_ui_window_draw (que tiene que llamar ella, normalmente primero).
typedef int (*th_ui_draw_fn)(const struct th_ui_window *win, th_drawlist *out, void *ctx);

typedef struct th_ui_window {
    th_ui_draw_fn draw; // NULL: solo los paneles
    void *draw_ctx;
    int16_t x, y, w, h; // en la pantalla
    uint8_t n_panels, n_buttons, n_regions;
    int8_t active_button; // el que se está pulsando (−1: ninguno)
    uint8_t modal_class;  // Window.modal_class: 0 ninguna, TH_UI_MAIN…
    uint8_t visible;      // Window.visible (th_ui_window_init la pone a 1)
    int8_t repeat_delay;  // Window.btn_repeat_delay, en ticks de CorsixTH
    uint8_t n_clicks;
    th_ui_click clicks[TH_UI_CLICKS]; // las de los botones repetidos, por orden
    th_ui_panel panels[TH_UI_PANELS];
    th_ui_button buttons[TH_UI_BUTTONS];
    th_ui_region regions[TH_UI_REGIONS];
} th_ui_window;

enum { TH_UI_LEFT = 0, TH_UI_RIGHT = 1 }; // botones del ratón

// modal_class: abrir una ventana de una clase cierra la que hubiera de esa
// clase (UI:addWindow, ui.lua:1044-1060). «main» son construir, amueblar,
// colocar y contratar.
enum { TH_UI_MODAL_NONE = 0, TH_UI_MAIN = 1 };

// Botones repetidos (window.lua:1650-1654, 1904-1917): al bajar el botón se
// pulsa una vez y Window.btn_repeat_delay = 10; Window:onTick lo descuenta en
// cada tick y, cuando llega a 0, pulsa otra vez y lo pone a 2. Los ticks de la
// interfaz de CorsixTH son de 18 ms (usertick_period_ms, Src/lua_sdl.h:43),
// así que repite a los 11 ticks (198 ms) y luego cada 3 (54 ms). La N64 dibuja
// a 30 fotogramas por segundo (33,3 ms): th_ui_stack_update cuenta los ticks
// de 18 ms por el tiempo pasado (1 o 2 por fotograma, 1,85 de media), así que
// la primera repetición llega en el 6.º fotograma (200 ms) y después hay una
// cada 1,6 fotogramas (unas 18,5 por segundo, como CorsixTH).
enum { TH_UI_TICK_MS = 18, TH_UI_REPEAT_FIRST = 10, TH_UI_REPEAT_NEXT = 2 };

// Ventana de w × h; (x, y) como Window:setPosition: un valor en [0, 1) es la
// fracción del hueco libre en la pantalla y uno negativo cuenta desde el
// borde derecho o inferior.
void th_ui_window_init(th_ui_window *win, int w, int h);
void th_ui_window_place(th_ui_window *win, double x, double y, int screen_w, int screen_h);

// addPanel: devuelve su índice, o −1 si no caben más.
int th_ui_add_panel(th_ui_window *win, int sheet, int sprite, int x, int y);
// addColourPanel: un rectángulo de color (RGBA5551) de w × h, para fondos.
int th_ui_add_fill(th_ui_window *win, int x, int y, int w, int h, uint16_t color);
// makeButtonOnPanel: el rectángulo (x, y, w, h) es relativo al panel y
// sprite_active es el del panel mientras se pulsa. Devuelve el índice o −1.
int th_ui_make_button(th_ui_window *win, int panel, int x, int y, int w, int h, int sprite_active, int id);
// on_rightclick de makeButtonOnPanel: el botón también responde al derecho y
// devuelve id_right (−1 lo quita).
void th_ui_set_rightclick(th_ui_window *win, int button, int id_right);
void th_ui_set_toggle(th_ui_window *win, int button);
// Button:makeRepeat: se pulsa al bajar el botón del ratón y se repite
// mientras siga abajo y encima (ver TH_UI_REPEAT_*); al soltarlo no se pulsa.
void th_ui_set_repeat(th_ui_window *win, int button);
// Button:toggle: cambia el estado y el sprite de reposo.
void th_ui_toggle(th_ui_window *win, int button);
// Button:setDisabledSprite y Button:enable: deshabilitado, el panel enseña
// sprite_disabled (por defecto, el normal) y el botón no responde.
void th_ui_set_disabled_sprite(th_ui_window *win, int button, int sprite);
void th_ui_enable(th_ui_window *win, int button, int enable);
// Button:setTooltip: la descripción va centrada encima del botón.
void th_ui_button_tooltip(th_ui_window *win, int button, const char *tooltip);
// Window:makeTooltip: zona [x, r) × [y, b) con descripción encima, en el
// centro.
void th_ui_region_tooltip(th_ui_window *win, const char *tooltip, int x, int y, int r, int b);

// Window:draw: los paneles visibles, en orden.
int th_ui_window_draw(const th_ui_window *win, th_drawlist *out);

// El ratón, con (x, y) en la pantalla. mouse_down y mouse_move devuelven si
// la ventana se ha dado por aludida; mouse_up devuelve el id del botón
// pulsado y soltado encima (id con el izquierdo, id_right con el derecho), o
// −1. Los botones repetidos no se pulsan al soltar: sus pulsaciones (la de
// mouse_down y las de th_ui_tick) se recogen con th_ui_take_click.
int th_ui_mouse_down(th_ui_window *win, int button, int x, int y);
int th_ui_mouse_up(th_ui_window *win, int button, int x, int y);
int th_ui_mouse_move(th_ui_window *win, int x, int y);
// Window:onTick: un tick de 18 ms de la interfaz. mouse es el botón del ratón
// que está abajo (TH_UI_LEFT, TH_UI_RIGHT) o −1.
void th_ui_tick(th_ui_window *win, int mouse);
// La pulsación más antigua de un botón repetido: 1 si había, 0 si no.
int th_ui_take_click(th_ui_window *win, th_ui_click *out);
// hitTest: si (x, y) de la pantalla cae en la ventana (visible).
int th_ui_hit(const th_ui_window *win, int x, int y);

// getTooltipAt: la descripción bajo (x, y) de la pantalla y dónde va (su
// esquina inferior izquierda, en la pantalla), o NULL.
const char *th_ui_tooltip_at(const th_ui_window *win, int x, int y, int *tx, int *ty);

// La pila de ventanas (UI.windows y UI.modal_windows de ui.lua): win[0] es
// la de arriba. No es dueña de las ventanas: guarda punteros.
#define TH_UI_STACK 4

typedef struct th_ui_stack {
    th_ui_window *win[TH_UI_STACK];
    uint8_t n;
    uint8_t down; // botones del ratón abajo: bit TH_UI_LEFT y bit TH_UI_RIGHT
                  // (UI.buttons_down)
    int16_t ms;   // tiempo aún sin convertir en ticks de 18 ms
    int16_t screen_w, screen_h; // th_ui_stack_screen (0: sin margen)
} th_ui_stack;

// Lo que pasó con un evento del ratón en la pila.
typedef struct th_ui_event {
    th_ui_window *win; // la ventana que lo tomó; NULL si llega al mundo
    int id;            // mouse_up: el id del botón pulsado y soltado (−1: ninguno)
} th_ui_event;

void th_ui_stack_init(th_ui_stack *s);
// El tamaño de la pantalla, para el margen de 3 píxeles de UI:onMouseDown
// (ui.lua:888-890). Sin llamarla (0 × 0) no hay margen.
void th_ui_stack_screen(th_ui_stack *s, int screen_w, int screen_h);
// UI:addWindow: la pone arriba; si tiene modal_class, cierra antes la ventana
// de esa clase (y la devuelve en *closed si no es NULL, para que quien la abrió
// haga lo suyo, como verifyOrAbortRoom). Si ya estaba, solo la sube. Devuelve
// 0, o −1 si la pila está llena.
int th_ui_stack_open(th_ui_stack *s, th_ui_window *win, th_ui_window **closed);
// Window:close: la quita (y suelta su botón pulsado). 1 si estaba.
int th_ui_stack_close(th_ui_stack *s, th_ui_window *win);
// La ventana abierta de esa clase, o NULL (UI.modal_windows[clase]).
th_ui_window *th_ui_stack_modal(const th_ui_stack *s, int modal_class);
// UI:onMouseDown: de arriba abajo, la primera que lo toma (un botón o su
// rectángulo), y esa sube arriba de la pila (Window:bringToTop); si ninguna,
// llega al mundo (win = NULL). El botón del ratón cuenta como abajo para los
// repetidos solo a más de 3 píxeles del borde (th_ui_stack_screen).
th_ui_event th_ui_stack_mouse_down(th_ui_stack *s, int button, int x, int y);
// UI:onMouseUp: la ventana con un botón pulsado lo suelta (id); si no hay,
// la de arriba bajo el puntero se lo queda (id = −1); si tampoco, llega al
// mundo (cursor_entity:onClick).
th_ui_event th_ui_stack_mouse_up(th_ui_stack *s, int button, int x, int y);
// UI:onMouseMove: a todas; devuelve la de arriba bajo el puntero (NULL: el
// puntero está sobre el mundo, GameUI:onCursorWorldPositionChange).
th_ui_window *th_ui_stack_mouse_move(th_ui_stack *s, int x, int y);
// Pasa ms milisegundos: los ticks de 18 ms que tocan (UI:onTick →
// Window:onTick).
void th_ui_stack_update(th_ui_stack *s, int ms);
// La pulsación más antigua de un botón repetido en cualquier ventana.
int th_ui_stack_take_click(th_ui_stack *s, th_ui_window **win, th_ui_click *out);
// Dibuja de abajo arriba (la de arriba, la última).
int th_ui_stack_draw(const th_ui_stack *s, th_drawlist *out);
// La descripción bajo el puntero en la ventana de arriba que lo tiene.
const char *th_ui_stack_tooltip_at(const th_ui_stack *s, int x, int y, int *tx, int *ty);

#endif
