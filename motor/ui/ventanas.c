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

void gw_aviso_sonido(th_game_ui *g, const char *ruta)
{
    gw_aviso(g, ruta);
    g->sonar = 1;
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
//
// Con alguien en la mano (colocar personal), UI:addWindow cierra UIPlaceStaff,
// que también es «main», y su close() devuelve el perfil al final de
// available_staff (place_staff.lua:69-71): lo mismo que el derecho y Escape
// (gw_colocar_personal_cancelar: TH_ORD_DEVOLVER). En pausa no: la persona
// sigue en la mano, como la obra (cerrar sin devolver dejaría al candidato el
// primero de su lista, y CorsixTH lo pasa al final), salvo con su
// contratación en vuelo. Fuera de un fotograma no hay salida para esa orden: 0.
static int dejar_lo_abierto(th_game_ui *g, int which)
{
    (void)which;
    const th_sim *s = g->s;
    const int obra = s && s->obra.phase != TH_OBRA_NONE;
    const int lista = s && s->place.open;
    const int persona = g->active == TH_GW_COLOCAR_PERSONAL;
    if ((obra || lista || persona) && (!g->out || !g->in))
        return 0;
    if (obra || lista || (persona && gw_persona_por_devolver(g))) {
        if (g->in->paused) {
            gw_aviso(g, "misc.pause");
            return 0;
        }
    }
    if (obra || lista) {
        const th_order o = {.kind = TH_ORD_CERRAR};
        if (!gw_mandar(g, &o))
            return 0; // sin sitio en la salida del fotograma
    }
    if (persona)
        gw_colocar_personal_cancelar(g);
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
        // (place_staff.lua:58-83) devuelve el perfil al final del mercado y no
        // vuelve a contratar (gw_colocar_personal_cancelar: TH_ORD_DEVOLVER).
        // En pausa, como la obra: no hay orden (decisión 4) y cerrar sin ella
        // dejaría al candidato el primero de su lista; sigue en la mano y la
        // barra dice «Pausa».
        if (g->in->paused && gw_persona_por_devolver(g)) {
            gw_aviso(g, "misc.pause");
            break;
        }
        gw_colocar_personal_cancelar(g);
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

// El botón del panel de una ventana que ya está abierta la cierra, sin abrir
// otra (bottom_panel.lua:726-731, 750-755 y 774-779: getWindow y w:close());
// la tienda de la sala es un UIFurnishCorridor, y cerrarla vuelve a la obra
// (UIFurnishCorridor:close con edit_dialog). Si no, la abre.
static void boton_panel(th_game_ui *g, int which)
{
    if (g->active != which) {
        th_game_ui_open(g, which);
        return;
    }
    if (which == TH_GW_AMUEBLAR && g->tienda)
        gw_colocar_abrir(g, TH_GW_OBRA);
    else
        gw_cerrar(g);
}

// El botón derecho sobre un objeto de pasillo lo coge (Object:onClick,
// object.lua:760-823: TH_ORD_RECOGER); solo si hay uno en la casilla, para no
// llenar la barra de avisos con cada clic. Sin ventana abierta, o con
// construir, amueblar (sin la tienda de una obra) o contratar: esas no se
// quedan con un clic fuera de ellas (Window:onMouseUp, window.lua:1694-1749),
// el clic llega al objeto (ui.lua:911-917) y la UIPlaceObjects que abre
// cierra la «main» que hubiera (UI:addWindow, ui.lua:1048-1054); aquí se
// cierra al mandar la orden. Con colocar personal no: UIPlaceStaff se queda
// con el clic. Con una orden sin respuesta, nada: el segundo derecho sobre el
// mismo objeto llegaría con la lista ya abierta (otra_ventana).
static void recoger_del_pasillo(th_game_ui *g)
{
    const th_sim *s = g->s;
    if (!(g->in->world_up & (1 << TH_UI_RIGHT)) || !gw_en_mapa(g->tx, g->ty) || g->en_vuelo)
        return;
    for (int i = 0; i < s->n_obj; i++) {
        const th_object *o = &s->obj[i];
        if (o->def && (o->oflags & TH_OBJF_ON_MAP) && o->x == g->tx && o->y == g->ty &&
            th_rules_get(s->defs, o->def, "corridor_object")) {
            const th_order ord = {.kind = TH_ORD_RECOGER, .x = (uint8_t)g->tx, .y = (uint8_t)g->ty};
            if (gw_mandar(g, &ord) && g->active != TH_GW_NONE) {
                g->n_pend = 0; // como dejar_lo_abierto: lo comprado sin pedir se olvida
                gw_cerrar(g);
            }
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
    // reenviar todas con th_game_ui_order_result). En pausa no corre el
    // reloj de la simulación y la orden puede seguir en la cola, sin tick que
    // la aplique, todo lo que dure la pausa: esos fotogramas no cuentan.
    if (g->en_vuelo > 0 && !in->paused && ++g->en_vuelo_fotogramas > GW_EN_VUELO_MAX)
        g->en_vuelo = 0;

    // Los botones del panel (bottom_panel.lua: dialogBuildRoom, dialogFurnishCorridor, dialogHireStaff).
    if (in->panel_button == TH_PI_SALAS)
        boton_panel(g, TH_GW_CONSTRUIR);
    else if (in->panel_button == TH_PI_OBJETOS)
        boton_panel(g, TH_GW_AMUEBLAR);
    else if (in->panel_button == TH_PI_CONTRATAR)
        boton_panel(g, TH_GW_CONTRATAR);
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
        // Una obra del núcleo sin su ventana: el TH_ORD_SALA que se aplicó
        // después de que el jugador abriera otra ventana o cerrara la de la
        // obra. Sin esto el plano se queda cerrado en el mapa, sin dibujar,
        // hasta la siguiente ventana; se hace lo que habría hecho abrir la
        // otra (dejar_lo_abierto: verifyOrAbortRoom). La tienda de la sala es
        // parte de la obra.
        if (s->obra.phase != TH_OBRA_NONE && g->active != TH_GW_OBRA &&
            !(g->active == TH_GW_AMUEBLAR && g->tienda) && !in->paused) {
            const th_order o = {.kind = TH_ORD_CERRAR};
            gw_mandar(g, &o);
        }
        gw_amueblar_pendientes(g);
    }

    const int activa = g->active;
    switch (g->active) {
    case TH_GW_NONE: break;
    case TH_GW_CONSTRUIR: gw_construir_frame(g); break;
    case TH_GW_AMUEBLAR: gw_amueblar_frame(g); break;
    case TH_GW_COLOCAR: gw_colocar_frame(g); break;
    case TH_GW_OBRA: gw_obra_frame(g); break;
    case TH_GW_CONTRATAR: gw_contratar_frame(g); break;
    case TH_GW_COLOCAR_PERSONAL: gw_colocar_personal_frame(g); break;
    default: break;
    }
    if (g->active == activa && (activa == TH_GW_NONE || activa == TH_GW_CONSTRUIR || activa == TH_GW_CONTRATAR ||
                                (activa == TH_GW_AMUEBLAR && !g->tienda)))
        recoger_del_pasillo(g);

    if (g->msg_fotogramas > 0) {
        g->msg_fotogramas--;
        out->message = g->msg;
    }
    out->overlays = g->ov.count ? &g->ov : NULL;
    out->entities = g->n_ents ? g->ents : NULL;
    out->n_entities = g->n_ents;
    out->on_top = g->n_top ? g->top : NULL;
    out->n_on_top = g->n_top;
    out->reject_sound = g->sonar; // el sonido va con el aviso del fotograma en que se rechazó
    g->sonar = 0;
    g->in = NULL;
    g->out = NULL;
}

// El aviso de un rechazo del núcleo en la barra (la pieza 6 del plan pide
// los avisos de rechazo con texto, docs/06_plan.md). Los textos son los del
// consejero (adviser.placement_info y adviser.warnings), pero la demo no tiene
// consejero y CorsixTH calla en casi todos estos casos: solo dice
// staff_cannot_place (place_staff.lua:178), el dinero (cannot_afford,
// hire_staff.lua:148-156 y furnish_corridor.lua:106) y las zonas sin acceso
// (edit_room.lua:933-937). Con alguien encima del objeto, nada, como CorsixTH
// (anyHumanoidObscuringArea, place_objects.lua:516-520): NULL.
static const char *aviso_de(const th_order *o, int reason)
{
    switch (reason) {
    case TH_REASON_SIN_DINERO:
        return o->kind == TH_ORD_CONTRATAR ? gw_sin_dinero(o->def) : "adviser.warnings.cannot_afford_2";
    case TH_REASON_ZONAS_INACCESIBLES: return "adviser.room_forbidden_non_reachable_parts";
    case TH_REASON_PERSONA: return NULL;
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
    if (g->en_vuelo > 0)
        g->en_vuelo--;
    if (s)
        g->construido++; // la vista previa de colocar se vuelve a calcular (previa, ventana_colocar.c)
    if (o->kind == TH_ORD_DEVOLVER) // cancelar no avisa de nada
        return;
    if (r->reason != TH_REASON_OK) {
        // Contratar sin perfil (la persona en la mano se perdió: dos meses
        // en la mano, th/build.h) es hire sin perfil: wrong2.wav sin texto
        // (hire_staff.lua:134-137), no el aviso de la casilla que no vale.
        if (o->kind == TH_ORD_CONTRATAR && r->reason == TH_REASON_SIN_CANDIDATO) {
            g->sonar = 1;
        } else {
            const char *aviso = aviso_de(o, r->reason);
            if (aviso)
                gw_aviso(g, aviso);
        }
        // El único rechazo del núcleo que suena en CorsixTH: el plano que deja
        // zonas sin unir, al confirmar (edit_room.lua:933-937, wrong2.wav).
        if (r->reason == TH_REASON_ZONAS_INACCESIBLES)
            g->sonar = 1;
        if (o->kind == TH_ORD_COMPRAR)
            g->n_pend = 0; // no se compró: lo demás tampoco
        if (o->kind == TH_ORD_CONTRATAR && g->active == TH_GW_COLOCAR_PERSONAL && o->def == g->resumen) {
            // La persona sigue en la mano. Con NO_SE_PUEDE el núcleo hace hire
            // más cancelar (decisión 5 de docs/06_plan.md, order_contratar en
            // build.c) y el candidato pasa al final de su lista: el clic
            // siguiente busca su índice por el resumen (indice_en_mano en
            // ventana_contratar.c). Con SIN_CANDIDATO ya no está ni en el
            // mercado ni en el del mes anterior (el núcleo guarda uno: la
            // persona en la mano sobrevive a un cambio de mes): como en
            // UIPlaceStaff sin perfil, la ventana se cierra. Sin s la orden no llegó al núcleo
            // (th_game_ui_order_dropped).
            g->esperando_contrato = 0;
            if (s && r->reason == TH_REASON_SIN_CANDIDATO)
                gw_cerrar(g);
        } else if (o->kind == TH_ORD_CONTRATAR && o->def == g->resumen)
            g->esperando_contrato = 0;
        return;
    }
    // UIPlaceStaff:onMouseUp (place_staff.lua:144-176): colocada, se cierra.
    // Solo si es la persona en la mano: con la contratación de otra en vuelo
    // (el clic justo antes de pausar, y en pausa abrir contratar y elegir a
    // otra), la que se aplica no es la de esta ventana.
    if (o->kind == TH_ORD_CONTRATAR && g->active == TH_GW_COLOCAR_PERSONAL && o->def == g->resumen) {
        g->esperando_contrato = 0;
        gw_cerrar(g);
    }
    if (o->kind == TH_ORD_RECOGER && s && s->obra.phase == TH_OBRA_OBJECTS)
        gw_colocar_recogido(g, s);
}

void th_game_ui_order_dropped(th_game_ui *g, const th_order *o)
{
    // Como un rechazo que no viene del núcleo (ventanas.h): la cola llena.
    const th_order_result r = {.reason = TH_REASON_NO_SE_PUEDE};
    th_game_ui_order_result(g, NULL, o, &r);
}
