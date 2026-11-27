// Las ventanas de la partida (ventanas.h). Esqueleto: la pieza 5 de la demo
// (docs/06_plan.md) lo rellena; mientras, no abre nada ni manda órdenes.
#include "ventanas.h"

#include <string.h>

struct th_game_ui {
    th_game_ui_env env;
    int active; // TH_GW_*
};

static th_game_ui la_unica;

th_game_ui *th_game_ui_instance(void)
{
    return &la_unica;
}

void th_game_ui_init(th_game_ui *g, const th_game_ui_env *env)
{
    memset(g, 0, sizeof *g);
    g->env = *env;
}

void th_game_ui_frame(th_game_ui *g, const th_game_ui_input *in, th_sim *s, const th_view *v, int rot,
                      th_game_ui_output *out)
{
    (void)g;
    (void)in;
    (void)s;
    (void)v;
    (void)rot;
    memset(out, 0, sizeof *out);
}

void th_game_ui_order_result(th_game_ui *g, const th_sim *s, const th_order *o, const th_order_result *r)
{
    (void)g;
    (void)s;
    (void)o;
    (void)r;
}

int th_game_ui_open(th_game_ui *g, int which)
{
    (void)g;
    (void)which;
    return -1;
}

void th_game_ui_close_all(th_game_ui *g)
{
    g->active = TH_GW_NONE;
}

int th_game_ui_active(const th_game_ui *g)
{
    return g->active;
}
