// Las ventanas de la partida (ventanas.h): el fotograma, el reparto de botones
// y clics, las órdenes, los avisos y lo que comparten las ventanas
// (ventanas_int.h). Cada ventana va en su fichero.
#include <stdio.h>
#include <string.h>

#include "ventanas_int.h"

static th_game_ui la_unica;

th_game_ui *th_game_ui_instance(void)
{
    return &la_unica;
}

th_ui_window *th_game_ui_window(th_game_ui *g)
{
    return &g->w;
}

void th_game_ui_init(th_game_ui *g, const th_game_ui_env *env)
{
    memset(g, 0, sizeof *g);
    g->env = *env;
    g->cat = -1;
    g->hover = -1;
    g->clase = -1;
}

int th_game_ui_active(const th_game_ui *g)
{
    return g->active;
}

// ---------------------------------------------------------------------------
// Lo común

int gw_mandar(th_game_ui *g, const th_order *o)
{
    // Decisión 4 de docs/06_plan.md: como CorsixTH sin
    // allow_user_actions_while_paused (world.user_actions_allowed), en pausa
    // no se manda nada; la barra lo dice.
    if (g->in->paused) {
        gw_aviso(g, "misc.pause");
        return 0;
    }
    th_game_ui_output *out = g->out;
    if (out->n_orders >= TH_GAME_UI_ORDERS)
        return 0;
    out->orders[out->n_orders++] = *o;
    g->en_vuelo++;
    g->en_vuelo_fotogramas = 0;
    return 1;
}

void gw_aviso(th_game_ui *g, const char *ruta)
{
    snprintf(g->msg, sizeof g->msg, "%s", th_text(g->env.texts, ruta));
    g->msg_fotogramas = 90; // 3 s a 30 fotogramas por segundo
}

void gw_abrir_ventana(th_game_ui *g, int which)
{
    th_ui_window *cerrada = NULL;
    g->w.modal_class = TH_UI_MAIN;
    g->w.draw_ctx = g;
    th_ui_stack_open(g->env.stack, &g->w, &cerrada);
    g->active = which;
}

void gw_cerrar(th_game_ui *g)
{
    th_ui_stack_close(g->env.stack, &g->w);
    g->active = TH_GW_NONE;
    g->arrastre = 0;
    g->coger = 0;
}

const char *gw_nombre(const th_game_ui *g, const th_rules *defs, th_rnode def)
{
    // «name»: {"$ref": "_S.rooms_short.gps_office"} en definiciones.trg: la
    // ruta de los textos sin el «_S.».
    const char *ref = th_rules_str(defs, th_rules_get(defs, th_rules_get(defs, def, "name"), "$ref"), NULL);
    if (!ref)
        return "";
    if (!strncmp(ref, "_S.", 3))
        ref += 3;
    return th_text(g->env.texts, ref);
}

int gw_tiene_orientacion(const th_rules *defs, th_rnode def, int dir)
{
    static const char *const nombres[4] = {"north", "east", "south", "west"};
    return th_rules_get(defs, th_rules_get(defs, def, "orientations"), nombres[dir & 3]) != 0;
}

int gw_boton(th_ui_window *w, int sprite, int x, int y, int bx, int by, int bw, int bh, int activo, int id)
{
    const int p = th_ui_add_panel(w, TH_SHEET_MUTTON, sprite, x, y);
    return p < 0 ? -1 : th_ui_make_button(w, p, bx, by, bw, bh, activo, id);
}

void gw_casilla(const th_game_ui *g, int sx, int sy, int *x, int *y)
{
    int vx, vy;
    th_view_pick(g->v, sx, sy, &vx, &vy);
    th_rot_tile_inv(g->rot, vx, vy, x, y);
}

// ---------------------------------------------------------------------------
// Abrir y cerrar

// Antes de abrir otra ventana «main» desde el panel: GameUI:setEditRoom(false)
// (game_ui.lua:1259-1284) con la obra (verifyOrAbortRoom: TH_ORD_CERRAR, que
// el núcleo convierte en eso), y UI:addWindow, que cierra la de colocar del
// pasillo (UIPlaceObjects:close: TH_ORD_CERRAR devuelve lo que no se colocó).
// TH_ORD_COMPRAR y TH_ORD_SALA ya lo hacen al aplicarse, pero la ventana puede
// cerrarse sin mandarlas (y contratar no lo hace): se manda siempre. Devuelve 0
// si no se puede: en pausa, con algo que cerrar en el núcleo (la ventana de
// antes sigue abierta y la barra dice «Pausa»), o fuera de un fotograma
// (th_game_ui_open llamada entre fotogramas: no hay salida para TH_ORD_CERRAR,
// y abrir la otra dejaría la obra o la lista abiertas en el núcleo sin ventana
// que las lleve).
static int dejar_lo_abierto(th_game_ui *g, int which)
{
    (void)which;
    const th_sim *s = g->s;
    const int obra = s && s->obra.phase != TH_OBRA_NONE;
    const int lista = s && s->place.open;
    if (obra || lista) {
        if (!g->out || !g->in)
            return 0;
        if (g->in->paused) {
            gw_aviso(g, "misc.pause");
            return 0;
        }
        const th_order o = {.kind = TH_ORD_CERRAR};
        if (!gw_mandar(g, &o))
            return 0; // sin sitio en la salida del fotograma
    }
    g->n_pend = 0; // lo comprado sin pedir no se cobró: se olvida
    return 1;
}

int th_game_ui_open(th_game_ui *g, int which)
{
    if (which != TH_GW_CONSTRUIR && which != TH_GW_AMUEBLAR && which != TH_GW_CONTRATAR)
        return -1;
    if (!g->s) // hace falta la simulación de un fotograma anterior (las listas salen de ella)
        return -1;
    if (g->active == which)
        return 0;
    if (!dejar_lo_abierto(g, which))
        return -1;
    gw_cerrar(g);
    if (which == TH_GW_CONSTRUIR)
        gw_construir_abrir(g);
    else if (which == TH_GW_AMUEBLAR)
        gw_amueblar_abrir(g, 0);
    else
        gw_contratar_abrir(g);
    return 0;
}

void th_game_ui_close_all(th_game_ui *g)
{
    gw_cerrar(g);
    g->n_pend = 0;
    g->en_vuelo = 0;
    g->msg_fotogramas = 0;
}

// ---------------------------------------------------------------------------
// El fotograma

static void escape(th_game_ui *g)
{
    switch (g->active) {
    case TH_GW_COLOCAR:
    case TH_GW_OBRA: gw_colocar_escape(g); break;
    case TH_GW_COLOCAR_PERSONAL:
        // UIPlaceStaff con esc_closes (place_staff.lua:35): close() sin colocar
        // (place_staff.lua:58-83) devuelve el perfil al mercado y no vuelve a
        // contratar. Aquí el perfil nunca salió del mercado (la orden es la
        // que lo saca), así que basta cerrar.
        gw_cerrar(g);
        break;
    case TH_GW_AMUEBLAR:
        if (g->tienda) { // la tienda de la obra vuelve a la obra
            gw_colocar_abrir(g, TH_GW_OBRA);
            break;
        }
        /* fallthrough */
    default: gw_cerrar(g); break;
    }
}

static void boton(th_game_ui *g, int id, int mouse)
{
    switch (g->active) {
    case TH_GW_CONSTRUIR: gw_construir_boton(g, id, mouse); break;
    case TH_GW_AMUEBLAR: gw_amueblar_boton(g, id, mouse); break;
    case TH_GW_COLOCAR:
    case TH_GW_OBRA: gw_colocar_boton(g, id, mouse); break;
    case TH_GW_CONTRATAR: gw_contratar_boton(g, id, mouse); break;
    default: break;
    }
}

// Sin ventana abierta, el botón derecho sobre un objeto de pasillo lo coge
// (Object:onClick, entities/object.lua: TH_ORD_RECOGER); solo si hay uno en
// la casilla, para no llenar la barra de avisos con cada clic.
static void recoger_sin_ventana(th_game_ui *g)
{
    const th_sim *s = g->s;
    if (!(g->in->world_up & (1 << TH_UI_RIGHT)) || !gw_en_mapa(g->tx, g->ty))
        return;
    for (int i = 0; i < s->n_obj; i++) {
        const th_object *o = &s->obj[i];
        if (o->def && (o->oflags & TH_OBJF_ON_MAP) && o->x == g->tx && o->y == g->ty &&
            th_rules_get(s->defs, o->def, "corridor_object")) {
            const th_order ord = {.kind = TH_ORD_RECOGER, .x = (uint8_t)g->tx, .y = (uint8_t)g->ty};
            gw_mandar(g, &ord);
            return;
        }
    }
}

void th_game_ui_frame(th_game_ui *g, const th_game_ui_input *in, th_sim *s, const th_view *v, int rot,
                      th_game_ui_output *out)
{
    memset(out, 0, sizeof *out);
    g->in = in;
    g->s = s;
    g->v = v;
    g->rot = rot & 3;
    g->out = out;
    g->n_ents = 0;
    g->n_top = 0;
    th_view_overlays_clear(&g->ov);
    gw_casilla(g, in->px, in->py, &g->tx, &g->ty);
    g->sobre_mundo = 1;
    for (int i = 0; i < g->env.stack->n; i++)
        if (th_ui_hit(g->env.stack->win[i], in->px, in->py))
            g->sobre_mundo = 0;
    // Una orden sin respuesta se olvida pasado un rato (el anfitrión debería
    // reenviar todas con th_game_ui_order_result).
    if (g->en_vuelo > 0 && ++g->en_vuelo_fotogramas > GW_EN_VUELO_MAX)
        g->en_vuelo = 0;

    // Los botones del panel (bottom_panel.lua: dialogBuildRoom, dialogFurnishCorridor, dialogHireStaff).
    if (in->panel_button == TH_PI_SALAS)
        th_game_ui_open(g, TH_GW_CONSTRUIR);
    else if (in->panel_button == TH_PI_OBJETOS)
        th_game_ui_open(g, TH_GW_AMUEBLAR);
    else if (in->panel_button == TH_PI_CONTRATAR)
        th_game_ui_open(g, TH_GW_CONTRATAR);
    // Escape (GameUI:onKeyDown, global_cancel: esc_closes de la ventana de arriba).
    if (in->escape)
        escape(g);
    // Los botones de la ventana: el pulsado y soltado, y los repetidos.
    if (in->win == &g->w && in->id >= 0)
        boton(g, in->id, in->mouse);
    th_ui_window *w;
    th_ui_click c;
    while (th_ui_stack_take_click(g->env.stack, &w, &c))
        if (w == &g->w)
            boton(g, c.id, c.mouse);

    // Lo que abre o cierra el núcleo: la lista de colocar se abre al aplicarse
    // TH_ORD_COMPRAR o TH_ORD_RECOGER (UIPlaceObjects), y se cierra al colocar
    // el último objeto.
    if (g->en_vuelo == 0) {
        if (g->active == TH_GW_NONE && s->place.open && s->obra.phase == TH_OBRA_NONE)
            gw_colocar_abrir(g, TH_GW_COLOCAR);
        else if (g->active == TH_GW_COLOCAR && !s->place.open) {
            if (g->n_pend == 0)
                gw_cerrar(g);
        }
        gw_amueblar_pendientes(g);
    }

    switch (g->active) {
    case TH_GW_NONE: recoger_sin_ventana(g); break;
    case TH_GW_CONSTRUIR: gw_construir_frame(g); break;
    case TH_GW_AMUEBLAR: gw_amueblar_frame(g); break;
    case TH_GW_COLOCAR: gw_colocar_frame(g); break;
    case TH_GW_OBRA: gw_obra_frame(g); break;
    case TH_GW_CONTRATAR: gw_contratar_frame(g); break;
    case TH_GW_COLOCAR_PERSONAL: gw_colocar_personal_frame(g); break;
    default: break;
    }

    if (g->msg_fotogramas > 0) {
        g->msg_fotogramas--;
        out->message = g->msg;
    }
    out->overlays = g->ov.count ? &g->ov : NULL;
    out->entities = g->n_ents ? g->ents : NULL;
    out->n_entities = g->n_ents;
    out->on_top = g->n_top ? g->top : NULL;
    out->n_on_top = g->n_top;
    g->in = NULL;
    g->out = NULL;
}

// El aviso de un rechazo, con los textos del consejero de CorsixTH
// (adviser.placement_info y adviser.warnings, que dice en esos casos).
static const char *aviso_de(const th_order *o, int reason)
{
    switch (reason) {
    case TH_REASON_SIN_DINERO:
        return o->kind == TH_ORD_CONTRATAR ? "adviser.warnings.cannot_afford" : "adviser.warnings.cannot_afford_2";
    case TH_REASON_ZONAS_INACCESIBLES: return "adviser.room_forbidden_non_reachable_parts";
    default: break;
    }
    switch (o->kind) {
    case TH_ORD_CONTRATAR: return "adviser.placement_info.staff_cannot_place";
    case TH_ORD_SALA: return "adviser.placement_info.room_cannot_place";
    case TH_ORD_PUERTA: return "adviser.placement_info.door_cannot_place";
    case TH_ORD_VENTANA: return "adviser.placement_info.window_cannot_place";
    case TH_ORD_COLOCAR: return "adviser.placement_info.object_cannot_place";
    case TH_ORD_FIN_SALA: return "place_objects_window.place_objects";
    default: return "adviser.placement_info.room_cannot_place_2";
    }
}

void th_game_ui_order_result(th_game_ui *g, const th_sim *s, const th_order *o, const th_order_result *r)
{
    (void)s;
    if (g->en_vuelo > 0)
        g->en_vuelo--;
    if (r->reason != TH_REASON_OK) {
        gw_aviso(g, aviso_de(o, r->reason));
        if (o->kind == TH_ORD_COMPRAR)
            g->n_pend = 0; // no se compró: lo demás tampoco
        if (o->kind == TH_ORD_CONTRATAR)
            g->esperando_contrato = 0; // la persona sigue en la mano
        return;
    }
    // UIPlaceStaff:onMouseUp (place_staff.lua:144-176): colocada, se cierra.
    if (o->kind == TH_ORD_CONTRATAR && g->active == TH_GW_COLOCAR_PERSONAL) {
        g->esperando_contrato = 0;
        gw_cerrar(g);
    }
}

void th_game_ui_order_dropped(th_game_ui *g, const th_order *o)
{
    // Como un rechazo que no viene del núcleo (ventanas.h): la cola llena.
    const th_order_result r = {.reason = TH_REASON_NO_SE_PUEDE};
    th_game_ui_order_result(g, NULL, o, &r);
}
