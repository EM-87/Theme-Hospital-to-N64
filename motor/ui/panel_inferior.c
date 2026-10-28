#include "panel_inferior.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// Una pieza del panel: su sprite y su x; si lleva botón, el rectángulo
// relativo a la pieza, el sprite hundido, el id y la descripción.
typedef struct {
    uint8_t sprite;
    int16_t x;
    int8_t bx, by, bw, bh;
    uint8_t active, id, toggle;
    const char *tip;
} pieza;

typedef struct {
    int16_t x, y, r, b;
} zona;

struct th_panel_layout {
    int16_t w, h;
    pieza piezas[6]; // banco, fondo, salas, objetos, editar, contratar
    int16_t barra_x, relleno_x0, relleno_x1, relleno_paso, fin_x; // la barra de información
    pieza extra[7];  // botones de gestión, en lugar de la barra
    int16_t dinero_x, dinero_dx, dinero_y;
    int16_t fecha_x, fecha_y, fecha_w;
    int16_t rep_x, rep_y, rep_w;
    int16_t pausa_x, pausa_y, pausa_w; // el texto de la pausa en la barra
    int16_t puerta_x0, puerta_x1, puerta_x2, puerta_y;
    int8_t puerta_cerrada; // MESSAGE_DOOR_FULLY_SHUT
    zona zona_dinero, zona_fecha, zona_rep;
    int16_t tooltip_w;
};

#define T(n) "tooltip.toolbar." n

// drawPanels de CorsixTH, sin el botón del menú de máquinas (solo cabe con
// más de 676 píxeles de ancho).
static const th_panel_layout alta = {
    .w = 640, .h = 48,
    .piezas = {
        {1, 0, 6, 6, 35, 36, 2, TH_PI_BANCO, 1, T("bank_button")},
        {3, 40, 0, 0, 0, 0, 0, 0, 0, NULL},
        {4, 206, 6, 6, 35, 36, 5, TH_PI_SALAS, 0, T("rooms")},
        {6, 248, 1, 6, 35, 36, 7, TH_PI_OBJETOS, 0, T("objects")},
        {8, 285, 1, 6, 35, 36, 9, TH_PI_EDITAR, 0, T("edit")},
        {10, 322, 1, 6, 35, 36, 11, TH_PI_CONTRATAR, 0, T("hire")},
    },
    .barra_x = 364, .relleno_x0 = 377, .relleno_x1 = 630, .relleno_paso = 10, .fin_x = 627,
    .extra = {
        {15, 364, 6, 6, 35, 36, 16, TH_PI_PERSONAL, 1, T("staff_list")},
        {17, 407, 1, 6, 35, 36, 18, TH_PI_CIUDAD, 1, T("town_map")},
        {19, 445, 1, 6, 35, 36, 20, TH_PI_CASOS, 1, T("casebook")},
        {21, 483, 1, 6, 35, 36, 22, TH_PI_INVESTIGACION, 1, T("research")},
        {23, 521, 1, 6, 35, 36, 24, TH_PI_ESTADO, 1, T("status")},
        {25, 559, 1, 6, 35, 36, 26, TH_PI_GRAFICAS, 1, T("charts")},
        {27, 597, 1, 6, 35, 36, 28, TH_PI_POLITICA, 1, T("policy")},
    },
    .dinero_x = 44, .dinero_dx = 13, .dinero_y = 9,
    .fecha_x = 140, .fecha_y = 20, .fecha_w = 60,
    .rep_x = 55, .rep_y = 35, .rep_w = 65,
    .pausa_x = 374, .pausa_y = 14, .pausa_w = 255,
    .puerta_x0 = 177, .puerta_x1 = 179, .puerta_x2 = 201, .puerta_y = 1, .puerta_cerrada = 22,
    .zona_dinero = {41, 5, 137, 28}, .zona_fecha = {140, 5, 200, 42}, .zona_rep = {41, 30, 137, 42},
    .tooltip_w = 200,
};

// La de 320: las piezas de PANEL02M encajadas como las de 640 (21 + 89 +
// 21 + 18 + 18 + 21 = 188 y la barra hasta 320). Los botones ocupan la
// pieza como en la de 640, a escala. En el fondo (pieza 3, en x = 21), las
// casillas de las cifras están en x = 1 + 7·i, y = 4; el termómetro, en
// x = 10–41, y = 18–19; la libreta, en x = 57–83, y = 6–19; y la ranura de la
// puerta de los faxes, en x = 75–86 de la fila 1.
static const th_panel_layout baja = {
    .w = 320, .h = 25,
    .piezas = {
        {1, 0, 3, 3, 18, 19, 2, TH_PI_BANCO, 1, T("bank_button")},
        {3, 21, 0, 0, 0, 0, 0, 0, 0, NULL},
        {4, 110, 3, 3, 18, 19, 5, TH_PI_SALAS, 0, T("rooms")},
        {6, 131, 1, 3, 17, 19, 7, TH_PI_OBJETOS, 0, T("objects")},
        {8, 149, 1, 3, 17, 19, 9, TH_PI_EDITAR, 0, T("edit")},
        {10, 167, 3, 3, 18, 19, 11, TH_PI_CONTRATAR, 0, T("hire")},
    },
    .barra_x = 188, .relleno_x0 = 191, .relleno_x1 = 315, .relleno_paso = 5, .fin_x = 316,
    .extra = {
        {15, 188, 3, 3, 18, 19, 16, TH_PI_PERSONAL, 1, T("staff_list")},
        {17, 209, 1, 3, 17, 19, 18, TH_PI_CIUDAD, 1, T("town_map")},
        {19, 227, 1, 3, 17, 19, 20, TH_PI_CASOS, 1, T("casebook")},
        {21, 245, 1, 3, 17, 19, 22, TH_PI_INVESTIGACION, 1, T("research")},
        {23, 263, 1, 3, 17, 19, 24, TH_PI_ESTADO, 1, T("status")},
        {25, 281, 1, 3, 17, 19, 26, TH_PI_GRAFICAS, 1, T("charts")},
        {27, 299, 3, 3, 18, 19, 28, TH_PI_POLITICA, 1, T("policy")},
    },
    .dinero_x = 22, .dinero_dx = 7, .dinero_y = 4,
    .fecha_x = 78, .fecha_y = 8, .fecha_w = 27,
    .rep_x = 30, .rep_y = 18, .rep_w = 33,
    .pausa_x = 193, .pausa_y = 7, .pausa_w = 127,
    .puerta_x0 = 96, .puerta_x1 = 97, .puerta_x2 = 105, .puerta_y = 1, .puerta_cerrada = 8,
    .zona_dinero = {22, 3, 72, 14}, .zona_fecha = {73, 3, 107, 22}, .zona_rep = {21, 15, 72, 23},
    .tooltip_w = 100,
};

#define REPUTACION "#reputacion" // la descripción dinámica de la reputación

static int poner_pieza(th_ui_window *w, const pieza *z)
{
    int p = th_ui_add_panel(w, TH_SHEET_PANEL, z->sprite, z->x, 0);
    if (z->bw) {
        int b = th_ui_make_button(w, p, z->bx, z->by, z->bw, z->bh, z->active, z->id);
        if (z->toggle)
            th_ui_set_toggle(w, b);
        th_ui_button_tooltip(w, b, z->tip);
    }
    return p;
}

void th_bottom_panel_init(th_bottom_panel *p, int alta_res, int screen_w, int screen_h)
{
    const th_panel_layout *l = alta_res ? &alta : &baja;
    p->lay = l;
    th_ui_window_init(&p->win, l->w, l->h);
    th_ui_window_place(&p->win, 0.5, -0.1, screen_w, screen_h); // setDefaultPosition(0.5, -0.1)
    for (int i = 0; i < 6; i++)
        poner_pieza(&p->win, &l->piezas[i]);
    th_ui_add_panel(&p->win, TH_SHEET_PANEL, 12, l->barra_x, 0);
    for (int x = l->relleno_x0; x <= l->relleno_x1; x += l->relleno_paso)
        th_ui_add_panel(&p->win, TH_SHEET_PANEL, 13, x, 0);
    th_ui_add_panel(&p->win, TH_SHEET_PANEL, 14, l->fin_x, 0);
    for (int i = 0; i < 7; i++) {
        p->extra[i] = (uint8_t)poner_pieza(&p->win, &l->extra[i]);
        p->win.panels[p->extra[i]].visible = 0;
    }
    const zona *z = &l->zona_dinero;
    th_ui_region_tooltip(&p->win, T("balance"), z->x, z->y, z->r, z->b);
    z = &l->zona_fecha;
    th_ui_region_tooltip(&p->win, T("date"), z->x, z->y, z->r, z->b);
    z = &l->zona_rep;
    th_ui_region_tooltip(&p->win, REPUTACION, z->x, z->y, z->r, z->b);
    p->door_closed = l->puerta_cerrada;
}

static int sprite(th_drawlist *out, int sheet, int i, int x, int y)
{
    if (out->count >= out->capacity)
        return -1;
    out->items[out->count++] = (th_draw){.sprite = (uint16_t)i, .sheet = (uint8_t)sheet, .x = (int16_t)x,
                                         .y = (int16_t)y};
    return 0;
}

int th_bottom_panel_draw(const th_bottom_panel *p, const th_ui_fonts *f, const th_bottom_info *info,
                         const th_rules *textos, th_drawlist *out)
{
    const th_panel_layout *l = p->lay;
    const int x = p->win.x, y = p->win.y;
    if (th_ui_window_draw(&p->win, out) < 0)
        return -1;
    // El dinero: cada cifra en su casilla, alineado a la derecha en 7.
    char num[16], cifra[2] = {0, 0};
    int i = 7 - snprintf(num, sizeof num, "%ld", (long)info->balance);
    for (const char *c = num; *c; c++, i++) {
        cifra[0] = *c;
        th_font_text(&f->money, cifra, 1, x + l->dinero_x + i * l->dinero_dx, y + l->dinero_y, out);
    }
    // La fecha: date_format.daymonth, centrada en la libreta.
    char fecha[32];
    const th_text_arg args[2] = {th_text_num(info->day), th_text_num(info->month)};
    th_text_format(fecha, sizeof fecha, th_text(textos, "date_format.daymonth"), args, 2, textos);
    th_font_draw(&f->date, fecha, x + l->fecha_x, y + l->fecha_y, l->fecha_w, 0, TH_ALIGN_CENTER, out, NULL, NULL);
    // drawDynamicInfo: en pausa, «PAUSA» donde no están los botones de gestión.
    if (info->paused && !p->win.panels[p->extra[0]].visible) {
        const char *t = th_text(textos, "misc.pause");
        th_font_wrapped(&f->pause, t, strlen(t), x + l->pausa_x, y + l->pausa_y, l->pausa_w, 1 << 30, 0,
                        TH_ALIGN_CENTER, out);
    }
    // La puerta de los faxes.
    if (p->door_closed != 0) {
        sprite(out, TH_SHEET_PANEL, 40, x + l->puerta_x0, y + l->puerta_y);
        for (int dx = 0; dx <= p->door_closed; dx++)
            sprite(out, TH_SHEET_PANEL, 41, x + l->puerta_x1 + dx, y + l->puerta_y);
        if (p->door_closed == l->puerta_cerrada)
            sprite(out, TH_SHEET_PANEL, 42, x + l->puerta_x2, y + l->puerta_y);
    }
    // El termómetro de la reputación (drawReputationMeter), con la misma
    // aritmética de coma flotante que Lua.
    const double paso = (double)l->rep_w / (info->rep_max - info->rep_min);
    sprite(out, TH_SHEET_PANEL, 36, x + l->rep_x + (int)floor(paso * (info->reputation - info->rep_min)),
           y + l->rep_y);
    return out->count < out->capacity ? 0 : -1;
}

int th_bottom_panel_mouse_move(th_bottom_panel *p, int x, int y)
{
    int cambio = th_ui_mouse_move(&p->win, x, y);
    x -= p->win.x;
    y -= p->win.y;
    const int dentro = x >= 0 && y >= 0 && x < p->win.w && y < p->win.h;
    if (p->win.panels[p->extra[0]].visible != dentro) {
        for (int i = 0; i < 7; i++)
            p->win.panels[p->extra[i]].visible = (uint8_t)dentro;
        cambio = 1;
    }
    return cambio;
}

int th_bottom_panel_tooltip(const th_bottom_panel *p, int x, int y, const th_bottom_info *info,
                            const th_rules *textos, char *buf, int cap, int *tx, int *ty)
{
    const char *t = th_ui_tooltip_at(&p->win, x, y, tx, ty);
    if (!t)
        return 0;
    if (!strcmp(t, REPUTACION))
        snprintf(buf, (size_t)cap, "%s (%d)", th_text(textos, T("reputation")), info->reputation);
    else
        snprintf(buf, (size_t)cap, "%s", th_text(textos, t));
    return 1;
}

int th_bottom_panel_tooltip_width(const th_bottom_panel *p)
{
    return p->lay->tooltip_w;
}
