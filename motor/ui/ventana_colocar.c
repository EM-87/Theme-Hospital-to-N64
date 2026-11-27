// Colocar (UIPlaceObjects, dialogs/place_objects.lua) y la ventana de la
// obra (UIEditRoom, dialogs/edit_room.lua, que hereda de ella), a 320 × 240
// con las piezas 1–19 de MUTTON01. Medidas:
//   - la cabecera 1 (93 × 30) en y = 0: el hundido del título, filas 9–24 y
//     columnas 9–84 (el texto, de 9 de alto, en y = 12); el de la descripción
//     empieza en su fila 28 y sigue por las tiras 2 (93 × 3), seis como en
//     CorsixTH (place_objects.lua:58-60), desde y = 30, hasta la fila 1 del
//     medio 3 (93 × 11) en y = 48: la descripción va en (10, 30), 74 de ancho
//     y 2 líneas;
//   - los cuatro botones en y = 59: cancelar 4 (26 × 27, con el marco
//     izquierdo en sus columnas 0–3) en x = 0, comprar 6 (22) en x = 26, coger
//     8 (22) en x = 48 y confirmar 10 (26, con el marco derecho en las 22–25)
//     en x = 70: 96 de ancho, 3 más que el resto de la ventana; el hundido de
//     cada botón, filas 0–21; los grises 13–15 de deshabilitado (el 12, el
//     cancelar en gris, CorsixTH no lo usa);
//   - la cabecera de la lista 16 (6) en y = 86 (sin objetos no va, como la
//     123 de CorsixTH) y las filas 17 (17; la activa, la 18) desde y = 92: el
//     nombre en el hundido de las columnas 9–71 (filas 5–13) y la cantidad
//     centrada en la caja de las 76–85; el pie 19 (11) detrás.
// La ventana, de 96 de ancho y 97 a 92 + 17·n + 11 de alto, en (0.9, 0.1).
// CorsixTH la hace crecer con la lista (UIPlaceObjects:resize,
// place_objects.lua:93-140); aquí caben GW_COL_FILAS = 6 filas (205 de alto,
// encima del panel, que empieza en y = 215) y la lista se desplaza: enseña
// la activa, y elegir la última o la primera fila a la vista asoma la
// siguiente o la anterior (la consulta con su tienda llega a 7 clases).
#include <stdio.h>
#include <string.h>

#include "ventanas_int.h"

#define BOTONES_Y 59
#define LISTA_Y 92
#define FILA_H 17
#define NOMBRE_W 62 // el hundido del nombre en la fila 17: columnas 9–71, el texto desde la 10

enum { B_CANCELAR, B_COMPRAR, B_COGER, B_CONFIRMAR, B_FILAS }; // índices de botón

static int dibujar(const th_ui_window *w, th_drawlist *out, void *ctx)
{
    const th_game_ui *g = ctx;
    const th_ui_fonts *f = g->env.fonts;
    if (th_ui_window_draw(w, out) < 0)
        return -1;
    const int x = w->x, y = w->y;
    if (g->col_titulo)
        th_font_draw(&f->white, g->col_titulo, x + 10, y + 12, 0, 0, TH_ALIGN_LEFT, out, NULL, NULL);
    if (g->col_texto)
        th_font_wrapped(&f->white, g->col_texto, strlen(g->col_texto), x + 10, y + 30, 74, 2, 0, TH_ALIGN_LEFT, out);
    // El nombre, recortado al hundido de las columnas 9–71 para no pisar la
    // caja de la cantidad («Fire extinguisher» no cabe); la fila bajo el
    // puntero, en azul (active_hover_index y blue_font, place_objects.lua:640-648).
    for (int i = 0; i < g->col_n; i++) {
        const th_font *fuente = i == g->hover ? &f->pause : &f->white;
        char qty[8], nombre[48];
        snprintf(qty, sizeof qty, "%d", g->col_qty[i]);
        size_t n = (size_t)snprintf(nombre, sizeof nombre, "%s", g->col_nombre[i]);
        if (n >= sizeof nombre)
            n = sizeof nombre - 1;
        while (n > 0 && th_font_text(fuente, nombre, n, 0, 0, NULL) > NOMBRE_W)
            n--;
        nombre[n] = 0;
        if (th_text_row(fuente, nombre, x + 10, qty, x + 76, 10, y + LISTA_Y + i * FILA_H + 5, out) < 0)
            return -1;
    }
    return out->count < out->capacity ? 0 : -1;
}

static int filas_de(const th_game_ui *g, int which)
{
    const th_sim *s = g->s;
    // En la obra, la lista es la de la sala: solo en la fase de objetos.
    if (!s || !s->place.open || (which == TH_GW_OBRA && s->obra.phase != TH_OBRA_OBJECTS))
        return 0;
    return s->place.n < GW_COL_FILAS ? s->place.n : GW_COL_FILAS;
}

// UIPlaceObjects:setActiveIndex (place_objects.lua:369-399): object_orientation
// = "west" y nextOrientation, la primera que tenga el objeto desde el norte.
static int primera_orientacion(const th_rules *defs, th_rnode def)
{
    for (int d = 0; d < 4; d++)
        if (gw_tiene_orientacion(defs, def, d))
            return d;
    return TH_DIR_NORTH;
}

// UIPlaceObjects:addObjects ordena la lista por las casillas de la huella de
// la primera orientación, de más a menos (place_objects.lua:229-236), y
// vuelve a la primera fila. El núcleo guarda el orden de objects_needed (la
// huella de s->place no cambia): el orden es solo de la ventana, estable
// entre iguales (table.sort no lo es; con la consulta da escritorio,
// archivador y silla: el escritorio primero y en la mano, como CorsixTH).
static void ordenar(th_game_ui *g)
{
    const th_place_window *p = &g->s->place;
    int casillas[TH_SIM_PLACE_ITEMS];
    th_fp_tile fp[TH_FOOTPRINT_MAX];
    g->col_total = p->n;
    for (int k = 0; k < p->n; k++) {
        const th_rnode def = p->item[k].def;
        casillas[k] = th_object_footprint(g->s->defs, def, primera_orientacion(g->s->defs, def), fp);
        int j = k;
        while (j > 0 && casillas[g->col_orden[j - 1]] < casillas[k]) {
            g->col_orden[j] = g->col_orden[j - 1];
            j--;
        }
        g->col_orden[j] = (uint8_t)k;
    }
}

// El puesto en la lista ordenada de la fila k del núcleo (−1 si no está).
static int puesto(const th_game_ui *g, int k)
{
    for (int i = 0; i < g->col_total; i++)
        if (g->col_orden[i] == k)
            return i;
    return -1;
}

// Las filas y unidades de la lista, para ver si creció (addObjects: la
// tienda de la sala o un objeto cogido) y volver a la primera fila.
static int unidades(const th_place_window *p)
{
    int n = p->n << 12;
    for (int k = 0; k < p->n; k++)
        n += p->item[k].qty;
    return n;
}

// El modo coger se apaga (UIEditRoom:stopPickupItems, edit_room.lua:609-615)
// al coger un objeto (Object:onClick, object.lua:814) y al pulsar una fila
// (place_objects.lua:120-123).
static void soltar_coger(th_game_ui *g)
{
    if (g->coger && g->w.n_buttons > B_COGER) {
        th_ui_toggle(&g->w, B_COGER);
        g->coger = 0;
    }
}

// setActiveIndex con la fila k del núcleo: su objeto en la mano, con la
// primera orientación.
static void elegir(th_game_ui *g, int k)
{
    g->fila = k;
    g->en_mano = g->s->place.item[k].def;
    g->orient = primera_orientacion(g->s->defs, g->en_mano);
    g->previa.hay = 0;
}

void gw_colocar_recogido(th_game_ui *g, const th_sim *s)
{
    // Object:onClick en la obra (object.lua:810-821): stopPickupItems,
    // addObjects, selectObjectType y setOrientation(initial_orientation). El
    // núcleo deja la fila del objeto activa y su orientación
    // (order_recoger_obra en build.c); la lista creció, pero no se vuelve a la
    // primera fila.
    if (g->active != TH_GW_OBRA || !s->place.open || s->place.active >= s->place.n)
        return;
    soltar_coger(g);
    g->fila = s->place.active;
    g->en_mano = s->place.item[g->fila].def;
    g->orient = s->place.orient;
    g->col_unidades = unidades(&s->place);
    g->previa.hay = 0;
}

static void montar(th_game_ui *g, int which)
{
    th_ui_window *w = &g->w;
    const int n = filas_de(g, which);
    th_ui_window_init(w, 96, n ? LISTA_Y + n * FILA_H + 11 : 97);
    th_ui_window_place(w, 0.9, 0.1, g->env.screen_w, g->env.screen_h);
    w->draw = dibujar;
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_COL_CABECERA, 0, 0);
    for (int i = 0; i < 6; i++)
        th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_COL_TEXTO, 0, 30 + 3 * i);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_COL_MEDIO, 0, 48);
    // La descripción entera en la ayuda del hundido (filas 28–48): en dos
    // líneas de 74 se corta la parte que dice qué hacer («…, then click
    // confirm»). La ruta la pone gw_colocar_refrescar con la fase.
    g->col_region = w->n_regions;
    th_ui_region_tooltip(w, NULL, 9, 28, 85, 48);
    // place_objects.lua:64-84: cancelar, comprar, coger (de dos estados) y confirmar.
    int b = gw_boton(w, TH_MT_COL_CANCELAR, 0, BOTONES_Y, 4, 0, 22, 22, TH_MT_COL_CANCELAR_P, TH_GWB_COL_CANCELAR);
    th_ui_button_tooltip(w, b, "tooltip.place_objects_window.cancel");
    b = gw_boton(w, TH_MT_COL_COMPRAR, 26, BOTONES_Y, 0, 0, 22, 22, TH_MT_COL_COMPRAR_P, TH_GWB_COL_COMPRAR);
    th_ui_set_disabled_sprite(w, b, TH_MT_COL_COMPRAR_GRIS);
    th_ui_button_tooltip(w, b, "tooltip.place_objects_window.buy_sell");
    b = gw_boton(w, TH_MT_COL_COGER, 48, BOTONES_Y, 0, 0, 22, 22, TH_MT_COL_COGER_P, TH_GWB_COL_COGER);
    th_ui_set_disabled_sprite(w, b, TH_MT_COL_COGER_GRIS);
    th_ui_set_toggle(w, b);
    th_ui_button_tooltip(w, b, "tooltip.place_objects_window.pick_up");
    b = gw_boton(w, TH_MT_COL_CONFIRMAR, 70, BOTONES_Y, 0, 0, 22, 22, TH_MT_COL_CONFIRMAR_P, TH_GWB_COL_CONFIRMAR);
    th_ui_set_disabled_sprite(w, b, TH_MT_COL_CONFIRMAR_GRIS);
    th_ui_button_tooltip(w, b, "tooltip.place_objects_window.confirm");
    if (n) {
        th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_COL_LISTA_CABECERA, 0, 86);
        for (int i = 0; i < n; i++) {
            b = gw_boton(w, TH_MT_COL_FILA, 0, LISTA_Y + i * FILA_H, 8, 3, 79, 13, TH_MT_COL_FILA_P,
                         TH_GWB_COL_FILA + i);
            if (i == 0)
                g->col_b0 = b;
        }
        th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_COL_PIE, 0, LISTA_Y + n * FILA_H);
    } else
        th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_COL_PIE, 0, 86);
    g->col_n = n;
    g->coger = 0;
    g->hover = -1;
    if (which == TH_GW_OBRA || which == TH_GW_COLOCAR)
        gw_abrir_ventana(g, which);
}

void gw_colocar_abrir(th_game_ui *g, int which)
{
    const th_sim *s = g->s;
    if (which == TH_GW_COLOCAR) {
        // Una ventana nueva (addObjects y setActiveIndex(1)) con la
        // orientación del núcleo: la primera del objeto si se compró, la que
        // tenía si se cogió (window:setOrientation(initial_orientation),
        // object.lua:819-820; order_recoger en build.c).
        g->fila = -1;
        g->en_mano = 0;
        g->previa.hay = 0;
        g->col_primera = 0;
        if (s && s->place.open && s->place.n) {
            ordenar(g);
            g->fila = g->col_orden[0];
            g->en_mano = s->place.item[g->fila].def;
            g->orient = s->place.orient;
            if (!gw_tiene_orientacion(s->defs, g->en_mano, g->orient))
                g->orient = primera_orientacion(s->defs, g->en_mano);
        }
        g->col_unidades = s ? unidades(&s->place) : 0;
    }
    th_ui_stack_close(g->env.stack, &g->w);
    montar(g, which);
    gw_colocar_refrescar(g);
}

// La fila del núcleo con objetos que se coloca: la elegida o, si se agotó, la
// primera de la lista ordenada que quede (UIPlaceObjects:removeObject quita la
// fila y hace setActiveIndex(1), place_objects.lua:300-304; el núcleo también
// la quita, remove_item, y las de detrás cambian de índice: la elegida se
// reconoce por su objeto); también la primera si la lista creció (addObjects).
static int fila_activa(th_game_ui *g)
{
    const th_place_window *p = &g->s->place;
    if (!p->open || p->n == 0)
        return -1;
    ordenar(g);
    const int u = unidades(p);
    const int crecio = u > g->col_unidades;
    g->col_unidades = u;
    if (crecio || g->fila < 0 || g->fila >= p->n || p->item[g->fila].qty == 0 ||
        (g->en_mano && p->item[g->fila].def != g->en_mano)) {
        g->fila = -1;
        for (int i = 0; i < g->col_total && g->fila < 0; i++)
            if (p->item[g->col_orden[i]].qty)
                g->fila = g->col_orden[i];
    }
    return g->fila;
}

// La lista a la vista desde col_primera, con el puesto d dentro.
static void ver(th_game_ui *g, int d)
{
    const int vis = g->col_n;
    if (d < 0 || vis <= 0)
        return;
    if (d < g->col_primera)
        g->col_primera = d;
    if (d >= g->col_primera + vis)
        g->col_primera = d - vis + 1;
    if (g->col_primera > g->col_total - vis)
        g->col_primera = g->col_total - vis;
    if (g->col_primera < 0)
        g->col_primera = 0;
}

void gw_colocar_refrescar(th_game_ui *g)
{
    const th_sim *s = g->s;
    if (!s)
        return;
    const int obra = g->active == TH_GW_OBRA;
    if (filas_de(g, g->active) != g->col_n) { // la ventana cambia de alto: se rehace (abierta, igual)
        const int which = g->active;
        const int coger = g->coger;
        th_ui_stack_close(g->env.stack, &g->w);
        montar(g, which);
        if (coger && g->w.n_buttons > B_COGER) {
            th_ui_toggle(&g->w, B_COGER);
            g->coger = 1;
        }
    }
    const int activa = fila_activa(g);
    if (activa >= 0)
        ver(g, puesto(g, activa));
    for (int i = 0; i < g->col_n; i++) {
        const int k = g->col_orden[g->col_primera + i];
        g->col_nombre[i] = gw_nombre(g, s->defs, s->place.item[k].def);
        g->col_qty[i] = s->place.item[k].qty;
        // La fila activa, con la pieza 18 (CorsixTH pinta un recuadro naranja,
        // place_objects.lua:637-641).
        th_ui_button *b = &g->w.buttons[g->col_b0 + i];
        b->sprite_normal = k == activa ? TH_MT_COL_FILA_P : TH_MT_COL_FILA;
        if (g->w.active_button != g->col_b0 + i)
            g->w.panels[b->panel].sprite = b->sprite_normal;
    }
    g->col_activa = activa;
    // El título y la descripción de la fase (place_objects_window.*).
    const int fase = obra ? s->obra.phase : -1;
    if (obra) {
        const th_rules *d = s->defs;
        const th_rnode salas = th_rules_get(d, d->root, "salas");
        g->col_titulo = gw_nombre(g, d, th_rules_get(d, salas, s->room_types[g->obra_tipo]));
    } else
        g->col_titulo = th_text(g->env.texts, "rooms_short.corridor_objects");
    const char *t = "place_objects_window.place_objects_in_corridor";
    if (obra) {
        if (fase == TH_OBRA_NONE || fase == TH_OBRA_WALLS)
            t = "place_objects_window.drag_blueprint";
        else if (fase == TH_OBRA_DOOR)
            t = "place_objects_window.place_door";
        else if (fase == TH_OBRA_WINDOWS || fase == TH_OBRA_CLEAR)
            t = "place_objects_window.place_windows";
        else
            t = activa < 0 ? "place_objects_window.confirm_or_buy_objects" : "place_objects_window.place_objects";
    }
    if (g->coger)
        t = "place_objects_window.pick_up_object";
    g->col_texto = th_text(g->env.texts, t);
    if (g->col_region < g->w.n_regions)
        g->w.regions[g->col_region].tooltip = t;
    // Los botones: comprar y coger solo en la fase de objetos de una obra; el
    // confirmar, en la obra salvo en la fase de la puerta (se confirma al
    // ponerla) y en la de despejar, en la de paredes solo con un plano que
    // valga (edit_room.lua:1202: plano_ok, que gw_obra_frame mira otra vez tras
    // mover el plano) y en la de objetos solo con los obligatorios en la sala
    // (checkEnableConfirm, edit_room.lua:1605-1632: al entrar en la fase, al
    // colocar y al coger; th_build_room_confirmable).
    const int objetos = obra && fase == TH_OBRA_OBJECTS;
    th_ui_enable(&g->w, B_COMPRAR, objetos);
    th_ui_enable(&g->w, B_COGER, objetos);
    th_ui_enable(&g->w, B_CONFIRMAR,
                 obra && (fase == TH_OBRA_WINDOWS || (objetos && th_build_room_confirmable(s)) ||
                          ((fase == TH_OBRA_NONE || fase == TH_OBRA_WALLS) && g->pw > 0 && g->plano_ok)));
    if (!objetos && g->coger) {
        th_ui_toggle(&g->w, B_COGER);
        g->coger = 0;
    }
}

// UIPlaceObjects:tryNextOrientation: norte, este, sur y oeste, saltando las
// que el objeto no tiene.
static void girar(th_game_ui *g)
{
    for (int k = 1; k <= 4; k++) {
        const int d = (g->orient + k) & 3;
        if (gw_tiene_orientacion(g->s->defs, g->en_mano, d)) {
            g->orient = d;
            return;
        }
    }
}

// La vista previa: th_build_check_cell solo entre ticks; a medio tick, el
// último resultado (para esa casilla y orientación, o el último que hubo).
// CorsixTH la recalcula en cada fotograma (UIPlaceObjects:setBlueprintCell,
// place_objects.lua:768-850), pero el resultado solo depende de la casilla,
// la orientación, el objeto y lo construido (el mapa, los objetos, las salas
// y la lista de colocar; no la gente: TH_REASON_PERSONA lo mira la orden), y
// lo construido solo lo cambian las órdenes, que vuelven todas por
// th_game_ui_order_result (sube g->construido), y la obra que termina el
// despeje dentro de un tick (finishRoom: cambia la fase). Con lo mismo se
// repite el último resultado: th_build_check_cell cuesta unos 15 ms en la
// N64 con el bloqueo de zonas (build.c, would_break_pathfinding). Con la hora
// en la clave, como antes, a ×2 o más se pagaba en casi cada fotograma.
static int previa(th_game_ui *g, th_rnode def, int x, int y, int dir)
{
    const th_sim *s = g->s;
    if (g->previa.hay && g->previa.def == def && g->previa.x == x && g->previa.y == y && g->previa.dir == dir &&
        g->previa.s == s && g->previa.construido == g->construido && g->previa.fase == s->obra.phase)
        return g->previa.ok;
    if (g->in->sim_idle) {
        int bad;
        g->previa.s = s;
        g->previa.construido = g->construido;
        g->previa.fase = s->obra.phase;
        g->previa.calculos++;
        g->previa.ok = th_build_check_cell(g->s, def, x, y, dir, &bad);
        g->previa.def = def;
        g->previa.x = x;
        g->previa.y = y;
        g->previa.dir = dir;
        g->previa.hay = 1;
    }
    return g->previa.hay ? g->previa.ok : 1;
}

void gw_colocar_mundo(th_game_ui *g)
{
    th_sim *s = g->s;
    const int izq = (g->in->world_up & (1 << TH_UI_LEFT)) != 0, der = (g->in->world_up & (1 << TH_UI_RIGHT)) != 0;
    const int en_mapa = g->sobre_mundo && gw_en_mapa(g->tx, g->ty);
    const int fila = fila_activa(g);
    // Sin nada en la mano, o con coger: el objeto de la casilla vuelve a la
    // lista (Object:onClick; en la obra, solo los de la sala).
    if (fila < 0 || g->coger) {
        if (en_mapa && (der || (izq && g->coger)) && g->en_vuelo == 0) {
            const th_order o = {.kind = TH_ORD_RECOGER, .x = (uint8_t)g->tx, .y = (uint8_t)g->ty};
            gw_mandar(g, &o);
        }
        return;
    }
    const th_rnode def = s->place.item[fila].def;
    if (def != g->en_mano) // otra fila (la anterior se agotó o la lista creció): setActiveIndex
        elegir(g, fila);
    // En pausa, onMouseUp no hace nada (place_objects.lua:503), draw no pinta
    // el objeto (600-601) y onCursorWorldPositionChange quita la huella
    // (1092-1104: clearBlueprint). El clic izquierdo dice «Pausa» (decisión 4).
    if (g->in->paused) {
        if (izq && en_mapa && g->en_vuelo == 0)
            gw_aviso(g, "misc.pause");
        return;
    }
    if (der) { // onMouseUp derecho: girar (swoosh.wav)
        girar(g);
        g->previa.hay = 0;
    }
    if (!en_mapa)
        return;
    // El objeto en la mano (place_objects.lua:596-612: encima de todo, en gris
    // donde no se puede) y su huella (24 o 67 al 25 %, place_objects.lua:689,
    // 796-797 y 843-847). CorsixTH pinta cada casilla de la huella según la
    // suya; aquí todas según el resultado de la casilla entera.
    const int ok = previa(g, def, g->tx, g->ty, g->orient);
    g->n_top = th_vista_objeto_fantasma(s->defs, g->env.anims, def, g->tx, g->ty, g->orient, g->rot,
                                        ok ? 0 : TH_ANIM_PAL_GREY, g->top, (int)(sizeof g->top / sizeof g->top[0]));
    th_fp_tile fp[TH_FOOTPRINT_MAX];
    const int n = th_object_footprint(s->defs, def, g->orient, fp);
    for (int k = 0; k < n; k++)
        if (!(fp[k].f & TH_FP_INVISIBLE))
            th_view_overlays_set(&g->ov, g->tx + fp[k].dx, g->ty + fp[k].dy, ok ? 24 : 67, TH_DRAW_ALPHA25);
    // onMouseUp izquierdo fuera de la ventana: colocar si vale. Con un
    // COLOCAR sin respuesta, nada: la lista del núcleo aún no lo ha descontado
    // y un segundo clic pediría una unidad que quizá ya no está.
    if (izq && g->en_vuelo == 0) {
        if (!ok) {
            gw_aviso(g, "adviser.placement_info.object_cannot_place");
            return;
        }
        const th_order o = {.kind = TH_ORD_COLOCAR, .x = (uint8_t)g->tx, .y = (uint8_t)g->ty, .dir = (uint8_t)g->orient,
                            .def = def};
        gw_mandar(g, &o);
    }
}

void gw_colocar_hover(th_game_ui *g)
{
    // UIPlaceObjects:onMouseMove (place_objects.lua:648-675): la fila bajo el
    // puntero, en la zona de su botón (sin el hueco entre filas), o −1.
    const int x = g->in->px - g->w.x, y = g->in->py - g->w.y - LISTA_Y;
    g->hover = -1;
    if (x >= 8 && x < 87 && y >= 0 && y < g->col_n * FILA_H && y % FILA_H >= 3 && y % FILA_H < 16)
        g->hover = y / FILA_H;
}

void gw_colocar_frame(th_game_ui *g)
{
    gw_colocar_refrescar(g);
    gw_colocar_hover(g);
    gw_colocar_mundo(g);
}

void gw_colocar_boton(th_game_ui *g, int id, int mouse)
{
    (void)mouse;
    const int obra = g->active == TH_GW_OBRA;
    if (id >= TH_GWB_COL_FILA && id < TH_GWB_COL_FILA + GW_COL_FILAS) {
        // idx(i) de resize (place_objects.lua:112-124): la fila activa gira
        // el objeto; otra, setActiveIndex y fuera el modo coger. Elegir la
        // última o la primera a la vista asoma la siguiente o la anterior.
        const int d = g->col_primera + id - TH_GWB_COL_FILA;
        if (d >= g->col_total)
            return;
        const int k = g->col_orden[d];
        if (k == g->col_activa && !g->coger && g->en_mano) {
            girar(g);
            g->previa.hay = 0;
            return;
        }
        soltar_coger(g);
        elegir(g, k);
        if (d == g->col_primera + g->col_n - 1 && d + 1 < g->col_total)
            g->col_primera++;
        else if (d == g->col_primera && d > 0)
            g->col_primera--;
        return;
    }
    switch (id) {
    case TH_GWB_COL_CANCELAR:
        if (obra) {
            gw_obra_cancelar(g);
            break;
        }
        { // UIPlaceObjects:close: devuelve lo que no se colocó
            const th_order o = {.kind = TH_ORD_CERRAR};
            if (gw_mandar(g, &o)) {
                g->n_pend = 0;
                gw_cerrar(g);
            }
        }
        break;
    case TH_GWB_COL_COMPRAR: // en la obra, la tienda de la sala (UIFurnishCorridor con edit_dialog)
        if (obra)
            gw_amueblar_abrir(g, 1);
        break;
    case TH_GWB_COL_COGER: g->coger = g->w.buttons[B_COGER].toggled; break;
    case TH_GWB_COL_CONFIRMAR:
        if (obra)
            gw_obra_confirmar(g);
        break;
    default: break;
    }
}

void gw_colocar_escape(th_game_ui *g)
{
    if (g->active == TH_GW_OBRA) {
        // GameUI:setEditRoom(false): verifyOrAbortRoom (TH_ORD_CERRAR con obra).
        if (g->s->obra.phase != TH_OBRA_NONE) {
            const th_order o = {.kind = TH_ORD_CERRAR};
            if (!gw_mandar(g, &o))
                return;
        }
        gw_cerrar(g);
        return;
    }
    // global_cancel de UIPlaceObjects: undo (close(true)), TH_ORD_DESHACER.
    const th_order o = {.kind = TH_ORD_DESHACER};
    if (gw_mandar(g, &o)) {
        g->n_pend = 0;
        gw_cerrar(g);
    }
}
