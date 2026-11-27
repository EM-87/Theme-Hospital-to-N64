// Amueblar (UIFurnishCorridor, dialogs/furnish_corridor.lua) a 320 × 240 con
// las piezas 39–56 de MUTTON01. Medidas:
//   - columna izquierda, x = 0, de 79: la rejilla de la vista previa, 39 (19)
//     en y = 0, 40 (7) ocho veces desde y = 19 y 41 (19) en y = 75; la caja
//     del precio y el total, 42 (22) en y = 94, 43 (15) en y = 116 y 44 (21)
//     en y = 131; y el cerrar, 45 (23, con el fondo; 46 pulsado) en y = 152:
//     175 en total;
//   - columna derecha, x = 79, de 136: la cabecera 47 (19) en y = 0, diez
//     filas 48 (11) desde y = 19, el pie 50 (19) en y = 129 y el confirmar,
//     51 (23; 52 pulsado), en y = 148: 171;
//   - el hundido de la cabecera 47: filas 7–17, columnas 7–129; el de cada
//     fila 48: el nombre en las columnas 7–94 (filas 0–10), el «−» en las
//     96–104 (filas 1–9), la caja de la cantidad en las 107–117 y el «+» en
//     las 120–128: los botones 53/54 y 55/56 (9 × 9) van en (96, 1) y (120, 1)
//     de la fila;
//   - la caja del precio: el hundido va de la fila 8 de la 42 a la 4 de la 43
//     (y = 102–120 de la ventana); la del total, de la fila 8 de la 43 a la 11
//     de la 44 (y = 124–142); los textos, en x = 9 y a 2 filas del borde;
//   - el cerrar: el hundido del botón, columnas 4–73, filas 1–17 de la 45.
// Como en CorsixTH (furnish_corridor.lua:225-231), el nombre va a la
// izquierda sin caja y la cantidad, centrada en su caja.
// La ventana, 215 × 175, en (0.5, 0.4).
#include <stdio.h>
#include <string.h>

#include "ventanas_int.h"

#define LISTA_X 79
#define FILA_Y0 19
#define FILA_H 11

static int dibujar(const th_ui_window *w, th_drawlist *out, void *ctx)
{
    const th_game_ui *g = ctx;
    const th_ui_fonts *f = g->env.fonts;
    if (th_ui_window_draw(w, out) < 0)
        return -1;
    const int x = w->x, y = w->y;
    th_font_draw(&f->white, th_text(g->env.texts, "buy_objects_window.choose_items"), x + LISTA_X + 9, y + 8, 0, 0,
                 TH_ALIGN_LEFT, out, NULL, NULL);
    th_font_draw(&f->white, g->precio_txt, x + 9, y + 104, 0, 0, TH_ALIGN_LEFT, out, NULL, NULL);
    th_font_draw(&f->white, g->total_txt, x + 9, y + 128, 0, 0, TH_ALIGN_LEFT, out, NULL, NULL);
    for (int i = 0; i < g->n_obj && g->s; i++) {
        char qty[8];
        snprintf(qty, sizeof qty, "%d", g->obj_qty[i]);
        if (th_text_row(i == g->hover ? &f->pause : &f->white, gw_nombre(g, g->s->defs, g->obj_def[i]),
                        x + LISTA_X + 9, qty, x + LISTA_X + 107, 11, y + FILA_Y0 + i * FILA_H + 1, out) < 0)
            return -1;
    }
    return out->count < out->capacity ? 0 : -1;
}

// La tienda de la sala (er:purchaseItems): los objects_additional disponibles
// en el nivel, como los acepta TH_ORD_COMPRAR_SALA (build.c, order_comprar_sala).
static int en_tienda(const th_sim *s, th_rnode def)
{
    const th_rules *d = s->defs, *l = s->level;
    const int thob = th_rules_int(d, th_rules_get(d, def, "thob"), 0);
    const th_rnode spec = th_rules_at(l, th_rules_get(l, l->root, "objects"), (uint32_t)(thob - 1));
    const int investigable = th_rules_str(d, th_rules_get(d, def, "research_category"), NULL) != NULL;
    return thob >= 1 && th_rules_int(l, th_rules_get(l, spec, "AvailableForLevel"), 1) &&
           (!investigable || th_rules_int(l, th_rules_get(l, spec, "StartAvail"), 1));
}

static void lista(th_game_ui *g)
{
    const th_sim *s = g->s;
    const th_rules *d = s->defs;
    const th_rnode objetos = th_rules_get(d, d->root, "objetos");
    g->n_obj = 0;
    if (!g->tienda) {
        // furnish_corridor.lua:67-75: los de corridor_object, ordenados por ese número.
        int orden[GW_FILAS];
        for (uint32_t i = 0; i < th_rules_len(d, objetos) && g->n_obj < GW_FILAS; i++) {
            const th_rnode def = th_rules_entry_value(d, objetos, i);
            const th_rnode co = th_rules_get(d, def, "corridor_object");
            if (!co)
                continue;
            const int k = th_rules_int(d, co, 0);
            int j = g->n_obj++;
            while (j > 0 && orden[j - 1] > k) {
                orden[j] = orden[j - 1];
                g->obj_def[j] = g->obj_def[j - 1];
                j--;
            }
            orden[j] = k;
            g->obj_def[j] = def;
        }
    } else {
        const th_rnode sala = th_rules_get(d, th_rules_get(d, d->root, "salas"), s->room_types[s->obra.type]);
        const th_rnode add = th_rules_get(d, sala, "objects_additional");
        for (uint32_t i = 0; i < th_rules_len(d, add) && g->n_obj < GW_FILAS; i++) {
            const char *id = th_rules_str(d, th_rules_at(d, add, i), NULL);
            const th_rnode def = id ? th_rules_get(d, objetos, id) : 0;
            if (def && en_tienda(s, def))
                g->obj_def[g->n_obj++] = def;
        }
    }
    memset(g->obj_qty, 0, sizeof g->obj_qty);
    memset(g->obj_ini, 0, sizeof g->obj_ini);
    // UIEditRoom:purchaseItems (edit_room.lua:569-575): cada fila empieza con
    // lo que ya hay de ese objeto en la lista de la sala (cur_qty, el start_qty
    // de furnish_corridor.lua:65).
    for (int i = 0; g->tienda && s->place.open && i < g->n_obj; i++)
        for (int k = 0; k < s->place.n; k++)
            if (s->place.item[k].def == g->obj_def[i])
                g->obj_ini[i] = g->obj_qty[i] = s->place.item[k].qty;
}

static void textos(th_game_ui *g)
{
    snprintf(g->precio_txt, sizeof g->precio_txt, "%s%ld", th_text(g->env.texts, "buy_objects_window.price"),
             (long)g->precio);
    snprintf(g->total_txt, sizeof g->total_txt, "%s%ld", th_text(g->env.texts, "buy_objects_window.total"),
             (long)g->total);
}

void gw_amueblar_abrir(th_game_ui *g, int tienda)
{
    th_ui_window *w = &g->w;
    th_ui_window_init(w, 215, 175);
    th_ui_window_place(w, 0.5, 0.4, g->env.screen_w, g->env.screen_h);
    w->draw = dibujar;
    g->tienda = tienda;
    g->total = 0;
    g->precio = 0;
    g->hover = -1;
    lista(g);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_AMU_REJILLA_ARRIBA, 0, 0);
    for (int i = 0; i < 8; i++)
        th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_AMU_REJILLA, 0, 19 + 7 * i);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_AMU_REJILLA_ABAJO, 0, 75);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_AMU_PRECIO_ARRIBA, 0, 94);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_AMU_PRECIO, 0, 116);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_AMU_TOTAL, 0, 131);
    int b = gw_boton(w, TH_MT_AMU_CERRAR, 0, 152, 4, 1, 70, 17, TH_MT_AMU_CERRAR_P, TH_GWB_AMU_CERRAR);
    th_ui_button_tooltip(w, b, "tooltip.buy_objects_window.cancel");
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_AMU_LISTA_CABECERA, LISTA_X, 0);
    for (int i = 0; i < GW_FILAS; i++) {
        const int y = FILA_Y0 + i * FILA_H;
        const int p = th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_AMU_FILA, LISTA_X, y);
        if (i >= g->n_obj)
            continue;
        // furnish_corridor.lua:115-121: la fila (izquierdo +1, derecho −1) y
        // «−» y «+» repetidos (17 × 17 a 640: 9 × 9 aquí; el riesgo de los
        // botones pequeños con el stick lo resuelve el puntero de la ROM).
        b = th_ui_make_button(w, p, 7, 0, 88, FILA_H, TH_MT_AMU_FILA, TH_GWB_OBJETO + i);
        th_ui_set_rightclick(w, b, TH_GWB_OBJETO_MENOS1 + i);
        b = gw_boton(w, TH_MT_AMU_MENOS, LISTA_X + 96, y + 1, 0, 0, 9, 9, TH_MT_AMU_MENOS_P, TH_GWB_MENOS + i);
        th_ui_set_repeat(w, b);
        th_ui_button_tooltip(w, b, "tooltip.buy_objects_window.decrease");
        b = gw_boton(w, TH_MT_AMU_MAS, LISTA_X + 120, y + 1, 0, 0, 9, 9, TH_MT_AMU_MAS_P, TH_GWB_MAS + i);
        th_ui_set_repeat(w, b);
        th_ui_button_tooltip(w, b, "tooltip.buy_objects_window.increase");
    }
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_AMU_PIE, LISTA_X, 129);
    b = gw_boton(w, TH_MT_AMU_CONFIRMAR, LISTA_X, 148, 0, 0, 136, 23, TH_MT_AMU_CONFIRMAR_P, TH_GWB_AMU_CONFIRMAR);
    th_ui_button_tooltip(w, b, "tooltip.buy_objects_window.confirm");
    th_ui_region_tooltip(w, "tooltip.buy_objects_window.price", 8, 102, 70, 121);
    th_ui_region_tooltip(w, "tooltip.buy_objects_window.total_value", 8, 124, 70, 143);
    textos(g);
    gw_abrir_ventana(g, TH_GW_AMUEBLAR);
}

// UIFurnishCorridor:purchaseItem (furnish_corridor.lua:136-176), sin Ctrl ni
// Mayúsculas: el tope de 99, el mínimo y el saldo para el total al sumar. El
// mínimo de CorsixTH es min_qty (objects_needed menos los que ya están en la
// sala, edit_room.lua:577-586), y confirm vende lo que baje de start_qty
// (furnish_corridor.lua:190-194 y 199: removeObjects). El núcleo no tiene
// orden para quitar objetos de la lista de la sala (TH_ORD_VENDER no hace nada
// dentro de una obra, build.c), así que aquí no se baja de lo que ya había
// (obj_ini, que en el pasillo es 0): la venta en la tienda de la sala falta.
static void comprar(th_game_ui *g, int i, int qty)
{
    if (i >= g->n_obj)
        return;
    const int32_t precio = th_build_object_cost(g->s, g->obj_def[i]);
    if (qty > 0) {
        if (g->obj_qty[i] >= 99)
            return;
        if (g->s->hosp.balance < g->total + precio) {
            gw_aviso(g, "adviser.warnings.cannot_afford_2");
            return;
        }
        g->obj_qty[i]++;
        g->total += precio;
    } else if (g->obj_qty[i] > g->obj_ini[i]) {
        g->obj_qty[i]--;
        g->total -= precio;
    }
}

// UIFurnishCorridor:confirm (furnish_corridor.lua:178-209). Fuera de una
// obra: TH_ORD_COMPRAR cobra un objeto y abre la lista de colocar con él (y
// cierra la que hubiera), así que se pide el primero y los demás cuando se
// vacíe la lista. En la tienda de una obra: TH_ORD_COMPRAR_SALA por objeto,
// que se suman a la lista de la sala, y vuelta a la obra.
static void confirmar(th_game_ui *g)
{
    gw_compra compra[GW_FILAS];
    int n = 0;
    for (int i = 0; i < g->n_obj; i++) // lo que sube de start_qty (furnish_corridor.lua:185-189)
        if (g->obj_qty[i] > g->obj_ini[i])
            compra[n++] = (gw_compra){g->obj_def[i], (uint8_t)(g->obj_qty[i] - g->obj_ini[i])};
    if (n == 0) { // sin nada que comprar, solo cierra
        if (g->tienda)
            gw_colocar_abrir(g, TH_GW_OBRA);
        else
            gw_cerrar(g);
        return;
    }
    const th_order o = {.kind = g->tienda ? TH_ORD_COMPRAR_SALA : TH_ORD_COMPRAR, .qty = compra[0].qty,
                        .def = compra[0].def};
    if (!gw_mandar(g, &o))
        return; // en pausa: la ventana sigue abierta
    g->n_pend = n - 1;
    memcpy(g->pend, compra + 1, (size_t)(n - 1) * sizeof compra[0]);
    if (g->tienda)
        gw_colocar_abrir(g, TH_GW_OBRA);
    else
        gw_cerrar(g); // la de colocar se abre cuando el núcleo abra la lista
}

void gw_amueblar_pendientes(th_game_ui *g)
{
    const th_sim *s = g->s;
    while (g->n_pend > 0 && !g->in->paused) {
        // En la obra, cuantas quepan en el fotograma; en el pasillo, una cada
        // vez que la lista de colocar se vacía.
        const int obra = s->obra.phase == TH_OBRA_OBJECTS;
        if (!obra && (s->place.open || g->en_vuelo > 0 || (g->active != TH_GW_NONE && g->active != TH_GW_COLOCAR)))
            return;
        if (g->out->n_orders >= TH_GAME_UI_ORDERS)
            return;
        const th_order o = {.kind = obra ? TH_ORD_COMPRAR_SALA : TH_ORD_COMPRAR, .qty = g->pend[0].qty,
                            .def = g->pend[0].def};
        memmove(g->pend, g->pend + 1, (size_t)(--g->n_pend) * sizeof g->pend[0]);
        gw_mandar(g, &o);
        if (!obra)
            return;
    }
}

void gw_amueblar_boton(th_game_ui *g, int id, int mouse)
{
    (void)mouse;
    if (id >= TH_GWB_OBJETO && id < TH_GWB_OBJETO + GW_FILAS)
        comprar(g, id - TH_GWB_OBJETO, 1);
    else if (id >= TH_GWB_OBJETO_MENOS1 && id < TH_GWB_OBJETO_MENOS1 + GW_FILAS)
        comprar(g, id - TH_GWB_OBJETO_MENOS1, -1);
    else if (id >= TH_GWB_MENOS && id < TH_GWB_MENOS + GW_FILAS)
        comprar(g, id - TH_GWB_MENOS, -1);
    else if (id >= TH_GWB_MAS && id < TH_GWB_MAS + GW_FILAS)
        comprar(g, id - TH_GWB_MAS, 1);
    else if (id == TH_GWB_AMU_CERRAR) {
        if (g->tienda) // UIFurnishCorridor:close con edit_dialog: vuelve a la obra
            gw_colocar_abrir(g, TH_GW_OBRA);
        else
            gw_cerrar(g);
    } else if (id == TH_GWB_AMU_CONFIRMAR)
        confirmar(g);
    textos(g);
}

void gw_amueblar_frame(th_game_ui *g)
{
    // UIFurnishCorridor:onMouseMove (furnish_corridor.lua:248-261): la fila
    // bajo el puntero y su precio.
    const int x = g->in->px - g->w.x, y = g->in->py - g->w.y;
    int hover = -1;
    if (x >= LISTA_X && x < LISTA_X + 136 && y >= FILA_Y0 && y < FILA_Y0 + GW_FILAS * FILA_H) {
        const int i = (y - FILA_Y0) / FILA_H;
        if (i < g->n_obj)
            hover = i;
    }
    if (hover >= 0 && hover != g->hover)
        g->precio = th_build_object_cost(g->s, g->obj_def[hover]);
    g->hover = hover;
    textos(g);
}
