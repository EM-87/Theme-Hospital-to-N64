// El panel inferior de la partida (UIBottomPanel, dialogs/bottom_panel.lua):
// el banco, el dinero, la reputación y la fecha, los botones de construir,
// amueblar, editar y contratar, y la barra de información, que deja ver los
// botones de gestión mientras el puntero está sobre el panel.
//
// Hay dos disposiciones:
// - 640 × 48, la de CorsixTH con los gráficos de alta resolución (PANEL02V y
//   las fuentes de QDATA). Sirve para comparar píxel a píxel con CorsixTH.
// - 320 × 25, para la N64, con los de baja resolución (PANEL02M y las fuentes
//   de QDATAM). CorsixTH no la tiene: sale de encajar las piezas una detrás
//   de otra, como en la de 640, y de medir en los gráficos dónde caen las
//   casillas del dinero, el termómetro y la libreta de la fecha.
#ifndef TH_UI_PANEL_INFERIOR_H
#define TH_UI_PANEL_INFERIOR_H

#include <stdint.h>

#include "texto.h"
#include "ui.h"

// Lo que hace cada botón (el id que devuelve th_ui_mouse_up).
enum {
    TH_PI_BANCO = 1,
    TH_PI_SALAS,
    TH_PI_OBJETOS,
    TH_PI_EDITAR,
    TH_PI_CONTRATAR,
    TH_PI_PERSONAL,
    TH_PI_CIUDAD,
    TH_PI_CASOS,
    TH_PI_INVESTIGACION,
    TH_PI_ESTADO,
    TH_PI_GRAFICAS,
    TH_PI_POLITICA,
};

// Las fuentes de la interfaz (UIBottomPanel:_initFonts y la de UI).
typedef struct th_ui_fonts {
    th_font money, date, white, pause, tooltip;
} th_ui_fonts;

typedef struct th_panel_layout th_panel_layout;

typedef struct th_bottom_panel {
    th_ui_window win;
    const th_panel_layout *lay;
    uint8_t extra[7];    // los paneles de los botones de gestión
    int8_t door_closed;  // message_door.closed_amount: cuánto tapa la puerta de los faxes
} th_bottom_panel;

// alta: 1 para la disposición de 640 (CorsixTH), 0 para la de 320 (N64).
void th_bottom_panel_init(th_bottom_panel *p, int alta, int screen_w, int screen_h);

// Lo que enseña el panel.
typedef struct th_bottom_info {
    int32_t balance;
    int day, month;               // día del mes (1–31) y mes (1–12)
    int reputation, rep_min, rep_max;
    int paused; // en pausa, la barra de información dice «PAUSA»
} th_bottom_info;

// UIBottomPanel:draw: los paneles, el dinero, la fecha, la pausa, la puerta
// de los faxes y el termómetro de la reputación.
int th_bottom_panel_draw(const th_bottom_panel *p, const th_ui_fonts *f, const th_bottom_info *info,
                         const th_rules *textos, th_drawlist *out);

// UIBottomPanel:onMouseMove: los botones de gestión se ven mientras el
// puntero está sobre el panel. Devuelve si cambió algo.
int th_bottom_panel_mouse_move(th_bottom_panel *p, int x, int y);

// La descripción bajo el puntero (getTooltipAt), ya formateada en buf.
// Devuelve 0 si no hay ninguna.
int th_bottom_panel_tooltip(const th_bottom_panel *p, int x, int y, const th_bottom_info *info,
                            const th_rules *textos, char *buf, int cap, int *tx, int *ty);

// Ancho de las descripciones (200 a 640 × 480) y alto del panel.
int th_bottom_panel_tooltip_width(const th_bottom_panel *p);

#endif
