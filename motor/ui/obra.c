// La obra sobre el mapa (UIEditRoom, dialogs/edit_room.lua): el plano que se
// arrastra en la fase de paredes, la puerta y las ventanas en los bordes del
// plano (screenToWall portado sobre la vista), el despeje y los objetos (la
// lista de la sala, ventana_colocar.c). La ventana es la de colocar con los
// textos de cada fase. Todo se decide en el núcleo con las órdenes
// TH_ORD_SALA, PUERTA, VENTANA, QUITAR_VENTANA, FIN_VENTANAS, COMPRAR_SALA,
// FIN_SALA y ATRAS; aquí solo se enseña lo que harían.
#include <string.h>

#include "ventanas_int.h"

#define PLANO_MAX 12 // un plano de 12 × 12 da 47 paredes: con la puerta y las ventanas, caben en TH_GAME_UI_ENTITIES

static th_rnode sala_def(const th_game_ui *g)
{
    const th_rules *d = g->s->defs;
    return th_rules_get(d, th_rules_get(d, d->root, "salas"), g->s->room_types[g->obra_tipo]);
}

void gw_obra_empezar(th_game_ui *g, int tipo)
{
    g->obra_tipo = tipo;
    g->arrastre = 0;
    g->pw = g->ph = 0;
    g->obra_iniciada = 0;
    g->plano_ok = 0;
    g->borde.hay = 0;
    g->fila = 0;
    g->en_mano = 0;
    g->previa.hay = 0;
    g->active = TH_GW_OBRA; // para que la ventana se monte con la lista de la obra
    gw_colocar_abrir(g, TH_GW_OBRA);
}

static void entidad(th_game_ui *g, int vx, int vy, unsigned anim, int flags)
{
    if (g->n_ents >= TH_GAME_UI_ENTITIES)
        return;
    g->ents[g->n_ents++] = (th_view_entity){.tile_x = (int16_t)vx, .tile_y = (int16_t)vy,
                                            .frame = th_anims_first_frame(g->env.anims, anim),
                                            .anim = (uint16_t)anim, .flags = (uint8_t)flags, .layers = NULL,
                                            .crop = TH_VIEW_NO_CROP};
}

// La puerta (126) o la ventana (130) del plano en un borde del mundo
// (setDoorBlueprint y setWindowBlueprint, edit_room.lua:1360-1400 y
// 1477-1495): en la casilla de la vista que da th_view_edge, en espejo si es
// la pared oeste de la vista, en rojo si no vale; y su marca en el suelo de
// dentro si vale (door_floor_blueprint_markers y
// window_floor_blueprint_markers, edit_room.lua:1062-1074).
static void borde(th_game_ui *g, int x, int y, int side, int ventana, int ok)
{
    static const uint8_t marca_puerta[4] = {26, 27, 30, 31}, marca_ventana[4] = {33, 34, 35, 36};
    int vx, vy, oeste;
    th_view_edge(g->rot, x, y, side, &vx, &vy, &oeste);
    entidad(g, vx, vy, ventana ? 130 : 126, (oeste ? TH_ANIM_FLIP_H : 0) | (ok ? 0 : TH_ANIM_PAL_RED));
    if (ok)
        th_view_overlays_set(&g->ov, x, y, ventana ? marca_ventana[side & 3] : marca_puerta[side & 3], 0);
}

// updateRoomBlueprint (th_lua_map.cpp:225-236): una casilla vale si no es de
// una sala, se puede edificar y es del jugador. El núcleo lo vuelve a mirar al
// aplicar TH_ORD_SALA (con la accesibilidad de alrededor, que aquí no).
static int casilla_mala(const th_sim *s, int x, int y)
{
    const th_tile *t = th_map_tile_c(s->w.map, x, y);
    return (t->flags & TH_TILE_ROOM) || !(t->flags & TH_TILE_BUILDABLE) ||
           th_map_parcel_owner(s->w.map, t->parcel) != 1;
}

static void plano(th_game_ui *g, int x, int y, int w, int h, int validar)
{
    static uint8_t malas[PLANO_MAX * PLANO_MAX];
    int todo_malo = 0;
    // El plano del núcleo no tiene tope (order_sala solo lo recorta al mapa):
    // uno de más de PLANO_MAX por lado (una partida grabada, no esta interfaz)
    // se dibuja sin las casillas malas, que no caben en malas[].
    if (w > PLANO_MAX || h > PLANO_MAX)
        validar = 0;
    if (validar) {
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++)
                malas[j * w + i] = (uint8_t)casilla_mala(g->s, x + i, y + j);
        const int min = th_rules_int(g->s->defs, th_rules_get(g->s->defs, sala_def(g), "minimum_size"), 1);
        todo_malo = w < min || h < min; // entire_invalid: demasiado pequeño
        // El confirmar, solo con el plano bueno: confirm_button:enable(is_valid)
        // de setBlueprintRect (edit_room.lua:1186-1202, updateRoomBlueprint).
        int malo = todo_malo;
        for (int i = 0; i < w * h && !malo; i++)
            malo = malas[i];
        g->plano_ok = !malo;
    }
    const int n = th_view_blueprint(g->env.anims, x, y, w, h, g->rot, validar ? malas : NULL, todo_malo, &g->ov,
                                    g->ents + g->n_ents, TH_GAME_UI_ENTITIES - g->n_ents);
    if (n > 0)
        g->n_ents += n;
}

// La fase de paredes: UIEditRoom:onLeftButtonDown fuera de la ventana fija
// la casilla de origen y onCursorWorldPositionChange hace setBlueprintRect
// con la casilla del puntero (edit_room.lua:1076-1165). Aquí solo el plano
// nuevo; mover y redimensionar uno que ya está no (pendiente).
static void paredes(th_game_ui *g)
{
    const th_game_ui_input *in = g->in;
    const th_obra *b = &g->s->obra;
    if (!g->arrastre && g->en_vuelo == 0 && b->phase == TH_OBRA_WALLS && b->w > 0) {
        // El plano que guarda el núcleo (una orden rechazada o la vuelta atrás).
        g->px = b->x;
        g->py = b->y;
        g->pw = b->w;
        g->ph = b->h;
    }
    // En pausa, UIEditRoom:onMouseDown no empieza el arrastre (edit_room.lua:1135)
    // y onCursorWorldPositionChange no mueve el plano (1502-1510): el que ya
    // está se queda dibujado.
    if ((in->world_down & (1 << TH_UI_LEFT)) && g->sobre_mundo && gw_en_mapa(g->tx, g->ty) && g->en_vuelo == 0 &&
        !in->paused) {
        g->arrastre = 1;
        g->ax = g->tx;
        g->ay = g->ty;
        // La pila no marca el botón pegado al borde de la pantalla
        // (th_ui_stack_mouse_down): ese arrastre solo acaba con world_up.
        g->arrastre_sin_marca = !(g->env.stack->down & (1 << TH_UI_LEFT));
    }
    // UIEditRoom:onMouseUp acaba el arrastre suba donde suba el botón
    // (edit_room.lua:1147-1151): soltado sobre una ventana o el panel, la pila
    // lo toma y no llega world_up, pero ya no lo marca abajo.
    if (g->arrastre && !g->arrastre_sin_marca && !(g->env.stack->down & (1 << TH_UI_LEFT)) &&
        !(in->world_up & (1 << TH_UI_LEFT)))
        g->arrastre = 0;
    // El plano sigue a la casilla del mundo bajo el puntero
    // (onCursorWorldPositionChange): sobre una ventana no cambia.
    if (g->arrastre && g->sobre_mundo && !in->paused) {
        int bx = g->tx < 0 ? 0 : g->tx >= TH_MAP_WIDTH ? TH_MAP_WIDTH - 1 : g->tx;
        int by = g->ty < 0 ? 0 : g->ty >= TH_MAP_HEIGHT ? TH_MAP_HEIGHT - 1 : g->ty;
        // El tope del dibujo: un plano de PLANO_MAX como mucho por lado.
        if (bx > g->ax + PLANO_MAX - 1)
            bx = g->ax + PLANO_MAX - 1;
        if (bx < g->ax - PLANO_MAX + 1)
            bx = g->ax - PLANO_MAX + 1;
        if (by > g->ay + PLANO_MAX - 1)
            by = g->ay + PLANO_MAX - 1;
        if (by < g->ay - PLANO_MAX + 1)
            by = g->ay - PLANO_MAX + 1;
        g->px = g->ax < bx ? g->ax : bx;
        g->py = g->ay < by ? g->ay : by;
        g->pw = (g->ax < bx ? bx - g->ax : g->ax - bx) + 1;
        g->ph = (g->ay < by ? by - g->ay : g->ay - by) + 1;
    }
    if (in->world_up & (1 << TH_UI_LEFT))
        g->arrastre = 0;
    if (g->pw > 0)
        plano(g, g->px, g->py, g->pw, g->ph, 1);
}

// UIEditRoom:screenToWall (edit_room.lua:753-869), en la vista: el plano
// girado, la casilla de la vista bajo el puntero y, en las esquinas, la mitad
// del rombo (WorldToScreen; 16 píxeles de 640 son unit/2 aquí). El resultado,
// de vuelta al mundo. Sin puertas dobles (swing_doors: ninguna sala del
// nivel 1 las tiene; el núcleo rechazaría la posición con TH_REASON_POSICION).
int gw_obra_borde(const th_game_ui *g, int *x, int *y, int *side)
{
    const th_obra *b = &g->s->obra;
    int ax, ay, bx, by;
    th_rot_tile(g->rot, b->x, b->y, &ax, &ay);
    th_rot_tile(g->rot, b->x + b->w - 1, b->y + b->h - 1, &bx, &by);
    const int rx = ax < bx ? ax : bx, ry = ay < by ? ay : by;
    const int rw = (ax < bx ? bx - ax : ax - bx) + 1, rh = (ay < by ? by - ay : ay - by) + 1;
    int cx, cy;
    th_view_pick(g->v, g->in->px, g->in->py, &cx, &cy);
    if (!(cx == rx || cx == rx - 1 || cx == rx + rw || cx == rx + rw - 1 || cy == ry || cy == ry - 1 || cy == ry + rh ||
          cy == ry + rh - 1))
        return 0;
    int sx, sy;
    th_view_tile(g->v, cx, cy, &sx, &sy);
    const int mx = g->in->px, my = g->in->py, medio = g->v->unit / 2;
    int vx = -1, vy = -1, vs = -1;
    if (cx == rx && cy == ry) { // la esquina de arriba
        if (mx >= sx)
            vx = cx + 1, vy = cy, vs = TH_DIR_NORTH;
        else
            vx = cx, vy = cy + 1, vs = TH_DIR_WEST;
    } else if (cx == rx + rw - 1 && cy == ry + rh - 1) { // la de abajo
        if (mx >= sx)
            vx = cx, vy = cy - 1, vs = TH_DIR_EAST;
        else
            vx = cx - 1, vy = cy, vs = TH_DIR_SOUTH;
    } else if (cx == rx && cy == ry + rh - 1) { // la de la izquierda
        if (my >= sy + medio)
            vx = cx + 1, vy = cy, vs = TH_DIR_SOUTH;
        else
            vx = cx, vy = cy - 1, vs = TH_DIR_WEST;
    } else if (cx == rx + rw - 1 && cy == ry) { // la de la derecha
        if (my >= sy + medio)
            vx = cx, vy = cy + 1, vs = TH_DIR_EAST;
        else
            vx = cx - 1, vy = cy, vs = TH_DIR_NORTH;
    } else if ((cx == rx - 1 || cx == rx) && ry <= cy && cy < ry + rh) { // el borde oeste
        vy = cy == ry ? ry + 1 : cy == ry + rh - 1 ? ry + rh - 2 : cy;
        vx = rx, vs = TH_DIR_WEST;
    } else if ((cy == ry - 1 || cy == ry) && rx <= cx && cx < rx + rw) { // el norte
        vx = cx == rx ? rx + 1 : cx == rx + rw - 1 ? rx + rw - 2 : cx;
        vy = ry, vs = TH_DIR_NORTH;
    } else if ((cx == rx + rw || cx == rx + rw - 1) && ry <= cy && cy < ry + rh) { // el este
        vy = cy == ry ? ry + 1 : cy == ry + rh - 1 ? ry + rh - 2 : cy;
        vx = rx + rw - 1, vs = TH_DIR_EAST;
    } else if ((cy == ry + rh || cy == ry + rh - 1) && rx <= cx && cx < rx + rw) { // el sur
        vx = cx == rx ? rx + 1 : cx == rx + rw - 1 ? rx + rw - 2 : cx;
        vy = ry + rh - 1, vs = TH_DIR_SOUTH;
    }
    if (vs < 0)
        return 0;
    th_rot_tile_inv(g->rot, vx, vy, x, y);
    *side = (vs - g->rot) & 3;
    return 1;
}

// La comprobación de la puerta o la ventana (th_build_edge_check), solo con
// la simulación entre ticks; a medio tick, la última.
static int borde_valido(th_game_ui *g, int x, int y, int side, int ventana)
{
    if (g->in->sim_idle) {
        g->borde.ok = th_build_edge_check(g->s, x, y, side, ventana) == TH_REASON_OK;
        g->borde.x = x;
        g->borde.y = y;
        g->borde.side = side;
        g->borde.hay = 1;
    }
    return g->borde.hay ? g->borde.ok : 1;
}

static int ventana_en(const th_obra *b, int x, int y, int side)
{
    for (int k = 0; k < b->n_windows; k++)
        if (b->windows[k].x == x && b->windows[k].y == y && b->windows[k].side == side)
            return 1;
    return 0;
}

// El plano que ya tiene el núcleo (de la fase de la puerta en adelante, hasta
// que finishRoom lo cambia por paredes): sus suelos y paredes, la puerta y las
// ventanas elegidas.
static void plano_nucleo(th_game_ui *g)
{
    const th_obra *b = &g->s->obra;
    plano(g, b->x, b->y, b->w, b->h, 0);
    if (b->has_door)
        borde(g, b->door.x, b->door.y, b->door.side, 0, 1);
    for (int k = 0; k < b->n_windows; k++)
        borde(g, b->windows[k].x, b->windows[k].y, b->windows[k].side, 1, 1);
}

static void puerta_o_ventana(th_game_ui *g, int ventana)
{
    const th_obra *b = &g->s->obra;
    int x, y, side;
    if (!g->sobre_mundo || !gw_obra_borde(g, &x, &y, &side))
        return;
    // Al bajar el botón: UIEditRoom:onMouseDown → onLeftButtonDown confirma la
    // puerta o pone la ventana (edit_room.lua:1076-1104, buildclk.wav en la
    // 1093), y con el derecho tryRemoveWindowFromWall (1137-1141). Con A
    // mantenido y el stick moviéndose, la puerta va donde se pulsó.
    const int izq = (g->in->world_down & (1 << TH_UI_LEFT)) != 0, der = (g->in->world_down & (1 << TH_UI_RIGHT)) != 0;
    const int hay = ventana && ventana_en(b, x, y, side);
    const int ok = hay || borde_valido(g, x, y, side, ventana);
    if (!hay)
        borde(g, x, y, side, ventana, ok);
    if (g->en_vuelo)
        return;
    th_order o = {.x = (uint8_t)x, .y = (uint8_t)y, .dir = (uint8_t)side};
    if (izq && !hay) { // confirmar la puerta, o poner la ventana, si vale
        if (!ok) {
            gw_aviso(g, ventana ? "adviser.placement_info.window_cannot_place"
                                : "adviser.placement_info.door_cannot_place");
            return;
        }
        o.kind = ventana ? TH_ORD_VENTANA : TH_ORD_PUERTA;
        gw_mandar(g, &o);
    } else if (der && hay) { // tryRemoveWindowFromWall
        o.kind = TH_ORD_QUITAR_VENTANA;
        gw_mandar(g, &o);
    }
}

void gw_obra_frame(th_game_ui *g)
{
    const int fase = g->s->obra.phase;
    if (fase >= TH_OBRA_DOOR)
        g->obra_iniciada = 1;
    // Terminada (confirm), abandonada o deshecha en el núcleo: se cierra.
    if (fase == TH_OBRA_NONE && g->obra_iniciada && g->en_vuelo == 0) {
        gw_cerrar(g);
        return;
    }
    gw_colocar_refrescar(g);
    gw_colocar_hover(g);
    switch (fase) {
    case TH_OBRA_NONE:
    case TH_OBRA_WALLS:
        paredes(g);
        gw_colocar_refrescar(g); // el confirmar, con el plano de este fotograma (plano_ok)
        break;
    case TH_OBRA_DOOR:
        plano_nucleo(g);
        puerta_o_ventana(g, 0);
        break;
    case TH_OBRA_WINDOWS:
        plano_nucleo(g);
        puerta_o_ventana(g, 1);
        break;
    case TH_OBRA_CLEAR: plano_nucleo(g); break; // clearArea: esperar a que no quede nadie
    case TH_OBRA_OBJECTS: gw_colocar_mundo(g); break;
    default: break;
    }
}

void gw_obra_cancelar(th_game_ui *g)
{
    // er:cancel: una fase atrás (TH_ORD_ATRAS); desde las paredes, abandona.
    // Con una orden sin respuesta, nada: la fase del núcleo aún no es la que
    // dejará esa orden, y dos «atrás» seguidos retrocederían dos fases (o
    // cerrarían la ventana con el plano ya en el núcleo).
    if (g->en_vuelo)
        return;
    const int fase = g->s->obra.phase;
    if (fase == TH_OBRA_NONE) {
        gw_cerrar(g);
        return;
    }
    const th_order o = {.kind = TH_ORD_ATRAS};
    if (gw_mandar(g, &o) && fase == TH_OBRA_WALLS)
        gw_cerrar(g);
}

void gw_obra_confirmar(th_game_ui *g)
{
    // Con una orden sin respuesta, nada (como gw_obra_cancelar): un segundo
    // confirmar llegaría con la fase ya cambiada y se rechazaría con FASE.
    if (g->en_vuelo)
        return;
    const int fase = g->s->obra.phase;
    th_order o = {0};
    if ((fase == TH_OBRA_NONE || fase == TH_OBRA_WALLS) && g->pw > 0) {
        if (!g->plano_ok)
            return; // el botón está deshabilitado (gw_colocar_refrescar)
        // confirm en la fase de paredes: el plano (UIBuildRoom:buildRoom y setBlueprintRect).
        o = (th_order){.kind = TH_ORD_SALA, .x = (uint8_t)g->px, .y = (uint8_t)g->py, .w = (uint8_t)g->pw,
                       .h = (uint8_t)g->ph, .def = sala_def(g)};
        g->arrastre = 0;
    } else if (fase == TH_OBRA_WINDOWS)
        o.kind = TH_ORD_FIN_VENTANAS;
    else if (fase == TH_OBRA_OBJECTS) {
        if (!th_build_room_confirmable(g->s))
            return; // el botón está deshabilitado: faltan obligatorios (checkEnableConfirm)
        o.kind = TH_ORD_FIN_SALA;
    } else
        return;
    gw_mandar(g, &o);
}
