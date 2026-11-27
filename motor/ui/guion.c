// El intérprete de los pasos del guion de las ventanas (guion.h). Sale de
// th64sim ventanas (platform/pc/th64sim.c, orden_ventanas, pieza 5 de la demo)
// para que la ROM de pruebas recorra los mismos estados: los mismos pasos, los
// mismos fotogramas y en el mismo orden.
#include "guion.h"

#include <stdio.h>
#include <string.h>

#include "girar.h"

void th_guion_init(th_guion *gs, th_ui_stack *stack, th_bottom_panel *panel, th_game_ui *g, int screen_w,
                   int screen_h)
{
    memset(gs, 0, sizeof *gs);
    gs->stack = stack;
    gs->panel = panel;
    gs->g = g;
    gs->screen_w = screen_w;
    gs->screen_h = screen_h;
}

void th_guion_pixel(const th_guion *gs, int x, int y, int *px, int *py)
{
    int vx, vy;
    th_rot_tile(gs->rot, x, y, &vx, &vy);
    th_view_tile(&gs->v, vx, vy, px, py);
    *py += gs->v.unit / 2;
}

// La cámara con la casilla (cx, cy) del mundo en el centro de la pantalla.
static void camara(th_guion *gs, int cx, int cy)
{
    gs->v = (th_view){.unit = 16, .w = gs->screen_w, .h = gs->screen_h};
    int vx, vy, sx, sy;
    th_rot_tile(gs->rot, cx, cy, &vx, &vy);
    th_view_tile(&gs->v, vx, vy, &sx, &sy);
    gs->v.x = sx - gs->screen_w / 2;
    gs->v.y = sy - gs->screen_h / 2;
}

int th_guion_linea(th_guion *gs, const char *linea)
{
    int cx, cy, rot, usado;
    if (linea[0] == '#' || sscanf(linea, "%63s %d %d %d%n", gs->nombre, &cx, &cy, &rot, &usado) != 4)
        return -1;
    gs->rot = rot & 3;
    camara(gs, cx, cy);
    gs->px = gs->screen_w / 2;
    gs->py = gs->screen_h / 2;
    gs->pausa = 0;
    gs->p = linea + usado;
    gs->acabado = 0;
    gs->horas = 0;
    gs->fotogramas = 0;
    gs->error[0] = 0;
    return 0;
}

// El centro del botón con ese id (o id_right) de la ventana de las ventanas.
static int boton(const th_guion *gs, int id, int derecho, int *px, int *py)
{
    const th_ui_window *w = th_game_ui_window(gs->g);
    for (int i = 0; i < w->n_buttons; i++) {
        const th_ui_button *b = &w->buttons[i];
        if ((derecho ? b->id_right : b->id) == id) {
            *px = w->x + (b->x + b->r) / 2;
            *py = w->y + (b->y + b->b) / 2;
            return 0;
        }
    }
    return -1;
}

static int fotograma(th_guion *gs, const th_guion_host *h, const th_game_ui_input *in, th_game_ui_output *out)
{
    gs->fotogramas++;
    return h->frame(h->ctx, in, &gs->v, gs->rot, out);
}

static void hora(th_guion *gs, const th_guion_host *h)
{
    gs->horas++;
    h->tick(h->ctx);
}

// Un fotograma del paso y, si manda órdenes (y el paso no es tick:n), la hora
// siguiente las aplica, y un fotograma sin clics deja las ventanas como las
// vería el siguiente (los botones que la fase habilita, por ejemplo); si este
// manda más, otra hora. Con el mando del anfitrión, la pila la actualiza él.
static void fotograma_paso(th_guion *gs, const th_guion_host *h, const th_game_ui_input *in, th_game_ui_output *out,
                           int ticks)
{
    if (fotograma(gs, h, in, out) && !ticks) {
        hora(gs, h);
        const th_game_ui_input quieto = {.px = in->px, .py = in->py, .id = -1, .paused = gs->pausa, .sim_idle = 1};
        if (!h->mando)
            th_ui_stack_update(gs->stack, 33);
        if (fotograma(gs, h, &quieto, out))
            hora(gs, h);
    }
}

int th_guion_paso(th_guion *gs, const th_guion_host *h, th_game_ui_output *out)
{
    if (gs->acabado)
        return 0;
    char paso[64];
    int k;
    for (;;) {
        if (sscanf(gs->p, "%63s%n", paso, &k) != 1) {
            strcpy(paso, "fin");
            k = 0;
        }
        gs->p += k;
        // Los pasos sin fotograma.
        int a = 0, b = 0;
        if (sscanf(paso, "pausa:%d", &a) == 1)
            gs->pausa = a;
        else if (sscanf(paso, "camara:%d,%d", &a, &b) == 2)
            camara(gs, a, b);
        else
            break;
    }
    int a = 0, b = 0, raton = TH_UI_LEFT, baja = 0, sube = 0, ticks = 0, esc = 0, boton_panel = 0;
    if (!strcmp(paso, "fin")) {
        gs->acabado = 1;
    } else if (sscanf(paso, "boton:%d", &a) == 1 || sscanf(paso, "botond:%d", &a) == 1) {
        raton = paso[5] == 'd' ? TH_UI_RIGHT : TH_UI_LEFT;
        if (boton(gs, a, raton == TH_UI_RIGHT, &gs->px, &gs->py) < 0) {
            snprintf(gs->error, sizeof gs->error, "no hay botón %d", a);
            return -1;
        }
        baja = sube = 1;
    } else if (sscanf(paso, "clic:%d,%d", &a, &b) == 2 || sscanf(paso, "clicd:%d,%d", &a, &b) == 2) {
        raton = paso[4] == 'd' ? TH_UI_RIGHT : TH_UI_LEFT;
        th_guion_pixel(gs, a, b, &gs->px, &gs->py);
        baja = sube = 1;
    } else if (sscanf(paso, "baja:%d,%d", &a, &b) == 2) {
        th_guion_pixel(gs, a, b, &gs->px, &gs->py);
        baja = 1;
    } else if (sscanf(paso, "sube:%d,%d", &a, &b) == 2) {
        th_guion_pixel(gs, a, b, &gs->px, &gs->py);
        sube = 1;
    } else if (sscanf(paso, "mueve:%d,%d", &a, &b) == 2 || sscanf(paso, "puntero:%d,%d", &a, &b) == 2) {
        th_guion_pixel(gs, a, b, &gs->px, &gs->py);
    } else if (sscanf(paso, "px:%d,%d", &a, &b) == 2) {
        gs->px = a;
        gs->py = b;
    } else if (sscanf(paso, "tick:%d", &a) == 1) {
        ticks = a;
    } else if (!strcmp(paso, "esc")) {
        esc = 1;
    } else if (!strcmp(paso, "panel:salas")) {
        boton_panel = TH_PI_SALAS;
    } else if (!strcmp(paso, "panel:objetos")) {
        boton_panel = TH_PI_OBJETOS;
    } else if (!strcmp(paso, "panel:contratar")) {
        boton_panel = TH_PI_CONTRATAR;
    } else {
        snprintf(gs->error, sizeof gs->error, "paso desconocido «%s»", paso);
        return -1;
    }
    const int px = gs->px, py = gs->py;
    for (int t = 0; t < (ticks ? ticks : 1); t++) {
        if (ticks)
            hora(gs, h);
        th_game_ui_input in = {.px = px, .py = py, .id = -1, .escape = esc, .paused = gs->pausa, .sim_idle = 1,
                               .panel_button = boton_panel};
        th_bottom_panel_mouse_move(gs->panel, px, py);
        th_ui_stack_mouse_move(gs->stack, px, py);
        if (h->mando) {
            // Con el mando del anfitrión: el botón baja en un fotograma y sube
            // en el siguiente, y es el anfitrión quien lo pasa por la pila.
            if (baja) {
                h->mando(h->ctx, raton, 1);
                fotograma_paso(gs, h, &in, out, ticks);
                in.panel_button = 0;
                in.escape = 0;
            }
            if (sube) {
                h->mando(h->ctx, raton, 0);
                fotograma_paso(gs, h, &in, out, ticks);
            }
            if (!baja && !sube)
                fotograma_paso(gs, h, &in, out, ticks);
            continue;
        }
        if (baja) {
            const th_ui_event e = th_ui_stack_mouse_down(gs->stack, raton, px, py);
            if (!e.win)
                in.world_down |= 1 << raton;
        }
        if (sube) {
            const th_ui_event e = th_ui_stack_mouse_up(gs->stack, raton, px, py);
            if (!e.win)
                in.world_up |= 1 << raton;
            else if (e.win == &gs->panel->win && e.id >= 0)
                in.panel_button = e.id;
            else if (e.id >= 0) {
                in.win = e.win;
                in.id = e.id;
                in.mouse = raton;
            }
        }
        th_ui_stack_update(gs->stack, 33);
        fotograma_paso(gs, h, &in, out, ticks);
    }
    return gs->acabado ? 0 : 1;
}
