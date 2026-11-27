// Construir (UIBuildRoom, dialogs/build_room.lua) a 320 × 240 con las piezas
// 20–36 de MUTTON01. Medidas (mutton01.h y el volcado de cada pieza):
//   - columna izquierda, x = 0, de 79: las cuatro categorías (20, 22, 24 y 26,
//     de 23, 18, 18 y 22 de alto) en y = 0, 23, 41 y 59, y debajo la rejilla
//     de la vista previa: 28 (19) en y = 81, 29 (7) ocho veces desde y = 100
//     y 30 (19) en y = 156: 175 en total;
//   - columna derecha, x = 79, de 116: la cabecera 31 (19) en y = 0, diez
//     filas 32 (11) desde y = 19 y la caja del coste con el cerrar, 34 (42),
//     en y = 129: 171;
//   - el recuadro hundido de las categorías: filas 8–19 de la 20 y 3–14 de las
//     otras (columnas 7–71); el texto (9 de alto) va en x = 10 y 1 fila más
//     abajo del borde del hundido;
//   - el de la cabecera 31: filas 8–16, columnas 8–107; el de las filas 32:
//     filas 1–9, columnas 8–108; el del coste en la 34: filas 2–10,
//     columnas 8–108;
//   - el botón cerrar: la pieza 35 (108 × 19) es idéntica a la 34 desde
//     (4, 19) (comparadas fila a fila), así que va ahí como panel con el botón
//     y la 36 de pulsado, como el aux_ui de CorsixTH (build_room.lua:84-91).
// La ventana, 195 × 175, centrada (setDefaultPosition(0.5, 0.5)).
#include <stdio.h>
#include <string.h>

#include "ventanas_int.h"

#define LISTA_X 79
#define FILA_Y0 19
#define FILA_H 11
#define COSTE_Y 129

static const char *const categorias[4] = {"diagnosis", "treatment", "clinics", "facilities"};
static const char *const titulos_cat[4] = {"room_classes.diagnosis", "room_classes.treatment",
                                          "room_classes.clinics", "room_classes.facilities"};
static const int16_t cat_y[4] = {0, 23, 41, 59};
static const uint8_t cat_texto_y[4] = {9, 4, 4, 4}; // una fila bajo el hundido de cada pieza

static th_rnode sala_def(const th_sim *s, int tipo)
{
    const th_rules *d = s->defs;
    return th_rules_get(d, th_rules_get(d, d->root, "salas"), s->room_types[tipo]);
}

// UIBuildRoom:updateBuildableRooms: las salas descubiertas (y con clase: las
// no implementadas solo salen en depuración) de la categoría, ordenadas por
// su número en categories.
static void lista_salas(th_game_ui *g)
{
    const th_sim *s = g->s;
    g->n_salas = 0;
    if (!s || g->cat < 0)
        return;
    const th_rules *d = s->defs;
    int orden[GW_FILAS];
    for (int t = 0; t < s->n_room_types && g->n_salas < GW_FILAS; t++) {
        const th_rnode rd = sala_def(s, t);
        const th_rnode n = th_rules_get(d, th_rules_get(d, rd, "categories"), categorias[g->cat]);
        if (!n || !th_rules_get(d, rd, "class") || !th_build_room_available(s, t))
            continue;
        const int k = th_rules_int(d, n, 0);
        int i = g->n_salas++;
        while (i > 0 && orden[i - 1] > k) { // table.sort: inserción, estable con el tipo
            orden[i] = orden[i - 1];
            g->salas[i] = g->salas[i - 1];
            i--;
        }
        orden[i] = k;
        g->salas[i] = (uint8_t)t;
    }
}

static int dibujar(const th_ui_window *w, th_drawlist *out, void *ctx)
{
    const th_game_ui *g = ctx;
    const th_ui_fonts *f = g->env.fonts;
    if (th_ui_window_draw(w, out) < 0)
        return -1;
    const int x = w->x, y = w->y;
    const char *titulo = th_text(g->env.texts, g->cat < 0 ? "build_room_window.pick_department"
                                                          : "build_room_window.pick_room_type");
    th_font_draw(&f->white, titulo, x + LISTA_X + 9, y + 8, 0, 0, TH_ALIGN_LEFT, out, NULL, NULL);
    for (int i = 0; i < 4; i++)
        th_font_draw(i == g->cat ? &f->pause : &f->white, th_text(g->env.texts, titulos_cat[i]), x + 10,
                     y + cat_y[i] + cat_texto_y[i], 0, 0, TH_ALIGN_LEFT, out, NULL, NULL);
    for (int i = 0; i < g->n_salas && g->s; i++)
        th_font_draw(i == g->hover ? &f->pause : &f->white, gw_nombre(g, g->s->defs, sala_def(g->s, g->salas[i])),
                     x + LISTA_X + 9, y + FILA_Y0 + i * FILA_H + 1, 0, 0, TH_ALIGN_LEFT, out, NULL, NULL);
    th_font_draw(&f->white, g->coste, x + LISTA_X + 9, y + COSTE_Y + 2, 0, 0, TH_ALIGN_LEFT, out, NULL, NULL);
    return out->count < out->capacity ? 0 : -1;
}

static void habilitar(th_game_ui *g)
{
    // UIBuildRoom:setCategory: los botones de las filas con sala, habilitados.
    for (int i = 0; i < GW_FILAS; i++)
        th_ui_enable(&g->w, 4 + i, i < g->n_salas);
}

void gw_construir_abrir(th_game_ui *g)
{
    th_ui_window *w = &g->w;
    th_ui_window_init(w, 195, 175);
    th_ui_window_place(w, 0.5, 0.5, g->env.screen_w, g->env.screen_h);
    w->draw = dibujar;
    // Las categorías (build_room.lua:62-66), botones 0–3: el hundido de cada pieza.
    gw_boton(w, TH_MT_CON_DIAGNOSTICO, 0, cat_y[0], 6, 7, 67, 14, TH_MT_CON_DIAGNOSTICO_P, TH_GWB_CATEGORIA + 0);
    gw_boton(w, TH_MT_CON_TRATAMIENTO, 0, cat_y[1], 6, 2, 67, 14, TH_MT_CON_TRATAMIENTO_P, TH_GWB_CATEGORIA + 1);
    gw_boton(w, TH_MT_CON_CLINICAS, 0, cat_y[2], 6, 2, 67, 14, TH_MT_CON_CLINICAS_P, TH_GWB_CATEGORIA + 2);
    gw_boton(w, TH_MT_CON_INSTALACIONES, 0, cat_y[3], 6, 2, 67, 14, TH_MT_CON_INSTALACIONES_P,
             TH_GWB_CATEGORIA + 3);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_CON_REJILLA_ARRIBA, 0, 81);
    for (int i = 0; i < 8; i++)
        th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_CON_REJILLA, 0, 100 + 7 * i);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_CON_REJILLA_ABAJO, 0, 156);
    // La lista (build_room.lua:73-83): botones 4–13, deshabilitados hasta elegir categoría.
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_CON_LISTA_CABECERA, LISTA_X, 0);
    for (int i = 0; i < GW_FILAS; i++)
        gw_boton(w, TH_MT_CON_FILA, LISTA_X, FILA_Y0 + i * FILA_H, 7, 0, 102, FILA_H, TH_MT_CON_FILA_P,
                 TH_GWB_SALA + i);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_CON_CERRAR_FONDO, LISTA_X, COSTE_Y);
    // El cerrar (botón 14), en el sitio exacto donde la 34 lo trae pintado.
    const int b = gw_boton(w, TH_MT_CON_CERRAR, LISTA_X + 4, COSTE_Y + 19, 0, 0, 108, 19, TH_MT_CON_CERRAR_P,
                           TH_GWB_CON_CERRAR);
    th_ui_button_tooltip(w, b, "tooltip.build_room_window.close");
    th_ui_region_tooltip(w, "tooltip.build_room_window.cost", LISTA_X + 8, COSTE_Y + 2, LISTA_X + 109,
                         COSTE_Y + 11);
    g->cat = -1;
    g->hover = -1;
    g->n_salas = 0;
    snprintf(g->coste, sizeof g->coste, "%s0", th_text(g->env.texts, "build_room_window.cost"));
    habilitar(g);
    gw_abrir_ventana(g, TH_GW_CONSTRUIR);
}

// UIBuildRoom:buildRoom: con dinero, a la obra (UIEditRoom); sin él, el aviso.
static void elegir(th_game_ui *g, int i)
{
    const th_sim *s = g->s;
    if (!s || i >= g->n_salas)
        return;
    if (s->hosp.balance < th_build_room_cost(s, g->salas[i])) {
        gw_aviso(g, "adviser.warnings.money_very_low_take_loan");
        return;
    }
    gw_obra_empezar(g, g->salas[i]);
}

void gw_construir_boton(th_game_ui *g, int id, int mouse)
{
    (void)mouse;
    if (id >= TH_GWB_CATEGORIA && id < TH_GWB_CATEGORIA + 4) {
        g->cat = id - TH_GWB_CATEGORIA; // UIBuildRoom:setCategory
        lista_salas(g);
        habilitar(g);
    } else if (id >= TH_GWB_SALA && id < TH_GWB_SALA + GW_FILAS)
        elegir(g, id - TH_GWB_SALA);
    else if (id == TH_GWB_CON_CERRAR)
        gw_cerrar(g);
}

void gw_construir_frame(th_game_ui *g)
{
    // UIBuildRoom:onMouseMove (build_room.lua:206-235): la fila habilitada bajo
    // el puntero y su coste.
    const int x = g->in->px - g->w.x, y = g->in->py - g->w.y;
    int hover = -1;
    if (x >= LISTA_X + 7 && x < LISTA_X + 109 && y >= FILA_Y0 && y < FILA_Y0 + GW_FILAS * FILA_H) {
        const int i = (y - FILA_Y0) / FILA_H;
        if (i < g->n_salas)
            hover = i;
    }
    if (g->cat >= 0) { // la lista se rehace (una sala puede descubrirse con la partida en marcha)
        lista_salas(g);
        habilitar(g);
    }
    g->hover = hover;
    const int32_t coste = hover < 0 ? 0 : th_build_room_cost(g->s, g->salas[hover]);
    snprintf(g->coste, sizeof g->coste, "%s%ld", th_text(g->env.texts, "build_room_window.cost"), (long)coste);
}
