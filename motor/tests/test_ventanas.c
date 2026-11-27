// Pruebas de las ventanas de la partida (ui/ventanas.h, la demo jugable,
// pieza 5) con los assets del motor y el escenario de la demo
// (motor/assets/escenarios/demo.trg): una sesión guiada por th_game_ui_frame
// con un anfitrión como el de la ROM (la pila de ventanas con el panel, el
// ratón repartido entre ventanas y mundo, las órdenes a la cola del núcleo y
// su resultado de vuelta). Sin assets no hace nada (los datos de TH no están
// en el repositorio).
//   test_ventanas <assets>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "th/build.h"
#include "th/scenario.h"
#include "th_test.h"
#include "ventanas.h"
#include "ventanas_int.h" // para mirar las cantidades de la tienda de la sala

int th_test_failures;
int th_test_checks;

static void log_stdout(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void (*th_test_log)(const char *fmt, ...) = log_stdout;

static uint8_t *leer(const char *dir, const char *nombre, size_t *len)
{
    char ruta[1024];
    snprintf(ruta, sizeof ruta, "%s/%s", dir, nombre);
    FILE *f = fopen(ruta, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    *len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(*len);
    if (d && fread(d, 1, *len, f) != *len) {
        free(d);
        d = NULL;
    }
    fclose(f);
    return d;
}

// --- El escenario y la interfaz ------------------------------------------------

static th_map mapa;
static th_pathfinder buscador;
static th_sim sim;
static th_anims anims;
static th_rules defs, nivel, esc, textos;
static th_sheet hojas[TH_SHEETS];
static const th_sheet *sheets[TH_SHEETS];
static th_ui_fonts fuentes;
static uint8_t *datos[TH_SHEETS + 6];
static int n_datos;

static int cargar_reglas(const char *assets, const char *nombre, th_rules *r)
{
    size_t len;
    uint8_t *d = leer(assets, nombre, &len);
    if (!d || th_rules_init(r, d, len) != 0)
        return -1;
    datos[n_datos++] = d;
    return 0;
}

// Como th64sim_cargar_escenario (platform/pc/hito4.c): el nivel del escenario
// con sus casillas, las reglas, las animaciones y la simulación (semilla 1).
static int cargar(const char *assets)
{
    static const char *const nombres[TH_SHEETS] = {"datam/mblk-0", "datam/mspr-0", "datam/panel02m",
                                                   "datam/mpointer", "fuentes/font100m", "fuentes/font101m",
                                                   "fuentes/font132m", "fuentes/font131m", "fuentes/font130m",
                                                   "datam/mutton01"};
    if (cargar_reglas(assets, "escenarios/demo.trg", &esc) || cargar_reglas(assets, "reglas/definiciones.trg", &defs) ||
        cargar_reglas(assets, "reglas/full01.trg", &nivel) || cargar_reglas(assets, "textos/english.trg", &textos))
        return -1;
    size_t len;
    uint8_t *tmp = leer(assets, "niveles/nivel01.tmp", &len);
    if (!tmp || th_map_load(&mapa, tmp, len, 1) != TH_MAP_OK)
        return -1;
    free(tmp);
    if (th_scenario_apply_cells(&mapa, &esc, th_rules_get(&esc, esc.root, "celdas")) < 0)
        return -1;
    uint8_t *tan = leer(assets, "datam/animaciones.tan", &len);
    if (!tan || th_anims_init(&anims, tan, len) != 0)
        return -1;
    datos[n_datos++] = tan;
    if (th_sim_init(&sim, &mapa, &buscador, &anims, &defs, &nivel, &esc, 1) != 0)
        return -1;
    for (int i = 0; i < TH_SHEETS; i++) {
        char nombre[64];
        snprintf(nombre, sizeof nombre, "%s.tsh", nombres[i]);
        uint8_t *d = leer(assets, nombre, &len);
        if (!d || th_sheet_init(&hojas[i], d, len) != 0)
            return -1;
        datos[n_datos++] = d;
        sheets[i] = &hojas[i];
    }
    fuentes = (th_ui_fonts){
        .money = {sheets[TH_SHEET_FONT_MONEY], TH_SHEET_FONT_MONEY, 0, 0, 0},
        .date = {sheets[TH_SHEET_FONT_DATE], TH_SHEET_FONT_DATE, 0, 0, 2},
        .white = {sheets[TH_SHEET_FONT_WHITE], TH_SHEET_FONT_WHITE, 0, -1, 0},
        .pause = {sheets[TH_SHEET_FONT_PAUSE], TH_SHEET_FONT_PAUSE, 0, 0, 0},
        .tooltip = {sheets[TH_SHEET_FONT_TOOLTIP], TH_SHEET_FONT_TOOLTIP, 0, 0, 0},
    };
    return 0;
}

// --- El anfitrión --------------------------------------------------------------

enum { W = 320, H = 240 };
static th_ui_stack pila;
static th_bottom_panel panel;
static th_view vista = {.unit = 16, .w = W, .h = H};
static th_game_ui *g;
static th_game_ui_output out;
static int pausa;
static th_order ordenes[64];
static int n_ordenes; // las que salieron, en orden

static void al_aplicar(void *ctx, const th_sim *s, const th_order *o, const th_order_result *r)
{
    th_game_ui_order_result(ctx, s, o, r);
}

static void empezar(const char *assets)
{
    TH_CHECK_EQ(cargar(assets), 0);
    th_ui_stack_init(&pila);
    th_ui_stack_screen(&pila, W, H);
    th_bottom_panel_init(&panel, 0, W, H);
    th_ui_stack_open(&pila, &panel.win, NULL);
    g = th_game_ui_instance();
    const th_game_ui_env env = {.stack = &pila, .fonts = &fuentes, .texts = &textos, .anims = &anims, .walk = NULL,
                                .screen_w = W, .screen_h = H};
    th_game_ui_init(g, &env);
    sim.ev.ctx = g;
    sim.ev.order = al_aplicar;
}

// La cámara con la casilla (x, y) en el centro.
static void camara(int x, int y)
{
    vista.x = vista.y = 0;
    int sx, sy;
    th_view_tile(&vista, x, y, &sx, &sy);
    vista.x = sx - W / 2;
    vista.y = sy - H / 2;
}

// El centro del rombo de una casilla del mundo (la cámara sin girar).
static void pixel(int x, int y, int *px, int *py)
{
    th_view_tile(&vista, x, y, px, py);
    *py += vista.unit / 2;
}

// Un fotograma: el puntero en (px, py), y un clic (baja y sube) con raton si
// raton >= 0; panel_button y escape tal cual. Las órdenes que salen se
// guardan; aplicar = 1 las pasa por th_build_apply (y por
// th_game_ui_order_result, como el gancho del anfitrión).
static void fotograma(int px, int py, int raton, int panel_button, int escape, int aplicar)
{
    th_game_ui_input in = {.px = px, .py = py, .id = -1, .panel_button = panel_button, .escape = escape,
                           .paused = pausa, .sim_idle = 1};
    th_bottom_panel_mouse_move(&panel, px, py);
    th_ui_stack_mouse_move(&pila, px, py);
    if (raton >= 0) {
        th_ui_event e = th_ui_stack_mouse_down(&pila, raton, px, py);
        if (!e.win)
            in.world_down |= 1 << raton;
        e = th_ui_stack_mouse_up(&pila, raton, px, py);
        if (!e.win)
            in.world_up |= 1 << raton;
        else if (e.win == &panel.win && e.id >= 0)
            in.panel_button = e.id;
        else if (e.id >= 0) {
            in.win = e.win;
            in.id = e.id;
            in.mouse = raton;
        }
    }
    th_ui_stack_update(&pila, 33);
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    for (int i = 0; i < out.n_orders; i++) {
        if (n_ordenes < 64)
            ordenes[n_ordenes++] = out.orders[i];
        if (aplicar) {
            th_order_result r;
            th_build_apply(&sim, &out.orders[i], &r);
            th_game_ui_order_result(g, &sim, &out.orders[i], &r);
        }
    }
}

// Un clic en el botón de la ventana con ese id (o id_right).
static int boton(int id, int raton, int aplicar)
{
    const th_ui_window *w = th_game_ui_window(g);
    for (int i = 0; i < w->n_buttons; i++) {
        const th_ui_button *b = &w->buttons[i];
        if ((raton == TH_UI_RIGHT ? b->id_right : b->id) == id) {
            const int n = n_ordenes;
            fotograma(w->x + (b->x + b->r) / 2, w->y + (b->y + b->b) / 2, raton, 0, 0, aplicar);
            return n_ordenes - n;
        }
    }
    th_test_log("FALLO: no hay botón %d\n", id);
    th_test_failures++;
    return -1;
}

static void casilla(int x, int y, int raton, int aplicar)
{
    int px, py;
    pixel(x, y, &px, &py);
    fotograma(px, py, raton, 0, 0, aplicar);
}

// La lista de dibujo de la ventana abierta, dos veces: la misma huella.
static void dibujo_determinista(const char *que)
{
    static th_draw e1[2048], e2[2048];
    th_drawlist a = {.items = e1, .capacity = 2048}, b = {.items = e2, .capacity = 2048};
    TH_CHECK_EQ(th_ui_stack_draw(&pila, &a), 0);
    TH_CHECK_EQ(th_ui_stack_draw(&pila, &b), 0);
    TH_CHECK_EQ(a.count, b.count);
    TH_CHECK_EQ(th_drawlist_hash(&a), th_drawlist_hash(&b));
    TH_CHECK(a.count > 45); // el panel solo son 33 (contratar sin categoría, 60)
    printf("  %s: %d elementos, huella %08lx\n", que, a.count, (unsigned long)th_drawlist_hash(&a));
}

static th_rnode objeto(const char *id)
{
    return th_rules_get(&defs, th_rules_get(&defs, defs.root, "objetos"), id);
}

// --- Las pruebas -----------------------------------------------------------------

static void test_amueblar_y_colocar(void)
{
    n_ordenes = 0;
    camara(60, 66);
    fotograma(W / 2, H / 2, -1, TH_PI_OBJETOS, 0, 1); // el botón del panel
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_AMUEBLAR);
    dibujo_determinista("amueblar");
    // La primera fila es el mostrador (corridor_object 1): «+» una vez
    // (repetido: se pulsa al bajar), y confirmar.
    TH_CHECK_EQ(boton(TH_GWB_MAS + 0, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(boton(TH_GWB_AMU_CONFIRMAR, TH_UI_LEFT, 1), 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_COMPRAR);
    TH_CHECK_EQ(ordenes[0].qty, 1);
    TH_CHECK_EQ(ordenes[0].def, objeto("reception_desk"));
    TH_CHECK_EQ(sim.hosp.balance, 40000 - 150);
    TH_CHECK(sim.place.open);
    // La lista de colocar se abre al fotograma siguiente.
    int px, py;
    pixel(62, 74, &px, &py);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR);
    TH_CHECK(out.n_on_top >= 1);   // el mostrador en la mano
    TH_CHECK(out.overlays != NULL); // y su huella
    dibujo_determinista("colocar");
    // El clic en (62, 74): TH_ORD_COLOCAR con esa casilla, al norte. Sin
    // aplicarlo, un segundo clic no manda otro (la orden está en vuelo, y solo
    // hay un mostrador en la lista); luego se aplica como lo haría el gancho.
    casilla(62, 74, TH_UI_LEFT, 0);
    TH_CHECK_EQ(n_ordenes, 2);
    casilla(62, 74, TH_UI_LEFT, 0);
    TH_CHECK_EQ(n_ordenes, 2);
    {
        th_order_result r;
        th_build_apply(&sim, &ordenes[1], &r);
        TH_CHECK_EQ(r.reason, TH_REASON_OK);
        th_game_ui_order_result(g, &sim, &ordenes[1], &r);
    }
    TH_CHECK_EQ(ordenes[1].kind, TH_ORD_COLOCAR);
    TH_CHECK_EQ(ordenes[1].x, 62);
    TH_CHECK_EQ(ordenes[1].y, 74);
    TH_CHECK_EQ(ordenes[1].dir, TH_DIR_NORTH);
    TH_CHECK_EQ(ordenes[1].def, objeto("reception_desk"));
    TH_CHECK(!sim.place.open); // colocado el único: el núcleo cierra la lista
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
}

static void test_contratar(void)
{
    n_ordenes = 0;
    camara(60, 58);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_CONTRATAR);
    dibujo_determinista("contratar sin categoría");
    // Sin categoría (UIHireStaff recién abierta), contratar no hace nada.
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_CONTRATAR);
    // Las pestañas son de dos estados: la de médicos, y otra vez, la suelta.
    boton(TH_GWB_PESTANA + 0, TH_UI_LEFT, 1);
    boton(TH_GWB_PESTANA + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_CONTRATAR);
    boton(TH_GWB_PESTANA + 0, TH_UI_LEFT, 1);
    dibujo_determinista("contratar");
    const th_staff_profile *lista;
    TH_CHECK(th_sim_candidates(&sim, TH_HUM_DOCTOR, &lista) > 0);
    const uint32_t resumen = th_staff_profile_digest(&lista[0]);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    // El derecho cancela y cierra (UIPlaceStaff:close): TH_ORD_DEVOLVER manda
    // el candidato al final de su lista (place_staff.lua:69-71).
    casilla(59, 56, TH_UI_RIGHT, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    TH_CHECK_EQ(n_ordenes, 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_DEVOLVER);
    TH_CHECK_EQ(ordenes[0].dir, TH_HUM_DOCTOR);
    TH_CHECK_EQ(ordenes[0].qty, 0);
    TH_CHECK_EQ(ordenes[0].def, resumen);
    int n = th_sim_candidates(&sim, TH_HUM_DOCTOR, &lista);
    TH_CHECK(n > 1);
    TH_CHECK(th_staff_profile_digest(&lista[0]) != resumen);
    TH_CHECK_EQ(th_staff_profile_digest(&lista[n - 1]), resumen);
    // Escape, igual (esc_closes), con el candidato 0 de ahora. En pausa no
    // manda la orden (decisión 4): solo cierra.
    const uint32_t resumen2 = th_staff_profile_digest(&lista[0]);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    boton(TH_GWB_PESTANA + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    pausa = 1;
    fotograma(W / 2, H / 2, -1, 0, 1, 1);
    pausa = 0;
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    TH_CHECK_EQ(n_ordenes, 1);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    boton(TH_GWB_PESTANA + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    fotograma(W / 2, H / 2, -1, 0, 1, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    TH_CHECK_EQ(n_ordenes, 2);
    TH_CHECK_EQ(ordenes[1].kind, TH_ORD_DEVOLVER);
    TH_CHECK_EQ(ordenes[1].def, resumen2);
    n = th_sim_candidates(&sim, TH_HUM_DOCTOR, &lista);
    TH_CHECK_EQ(th_staff_profile_digest(&lista[n - 1]), resumen2);
    // Al abrirla otra vez, sin la categoría de antes.
    n_ordenes = 0;
    const uint32_t resumen3 = th_staff_profile_digest(&lista[0]);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_CONTRATAR);
    boton(TH_GWB_PESTANA + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    // UIPlaceStaff mira la casilla de 14 píxeles de 640 (7 aquí) más abajo del puntero.
    int px, py;
    pixel(59, 56, &px, &py);
    py -= 7;
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(out.n_on_top, 1); // la persona en la mano
    // En pausa, el clic no manda nada (decisión 4) y la barra lo dice.
    pausa = 1;
    fotograma(px, py, TH_UI_LEFT, 0, 0, 1);
    TH_CHECK_EQ(n_ordenes, 0);
    TH_CHECK(out.message != NULL);
    pausa = 0;
    fotograma(px, py, TH_UI_LEFT, 0, 0, 1);
    TH_CHECK_EQ(n_ordenes, 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_CONTRATAR);
    TH_CHECK_EQ(ordenes[0].dir, TH_HUM_DOCTOR);
    TH_CHECK_EQ(ordenes[0].qty, 0);
    TH_CHECK_EQ(ordenes[0].def, resumen3);
    TH_CHECK_EQ(ordenes[0].x, 59);
    TH_CHECK_EQ(ordenes[0].y, 56);
    // Aceptada: la de colocar personal se cierra.
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
}

static void test_construir(void)
{
    n_ordenes = 0;
    camara(58, 52);
    fotograma(W / 2, H / 2, -1, TH_PI_SALAS, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_CONSTRUIR);
    boton(TH_GWB_CATEGORIA + 0, TH_UI_LEFT, 1); // diagnóstico: la consulta va la primera
    dibujo_determinista("construir");
    boton(TH_GWB_SALA + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
    // El plano: el izquierdo baja en (57, 50) y el puntero va a (60, 53).
    int px, py;
    pixel(57, 50, &px, &py);
    th_game_ui_input in = {.px = px, .py = py, .id = -1, .sim_idle = 1};
    th_ui_stack_mouse_move(&pila, px, py);
    const th_ui_event e = th_ui_stack_mouse_down(&pila, TH_UI_LEFT, px, py);
    TH_CHECK(e.win == NULL);
    in.world_down = 1 << TH_UI_LEFT;
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    pixel(60, 53, &px, &py);
    th_ui_stack_mouse_move(&pila, px, py);
    in = (th_game_ui_input){.px = px, .py = py, .id = -1, .sim_idle = 1};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    TH_CHECK_EQ(out.overlays ? out.overlays->count : 0, 16); // las 16 casillas del plano
    TH_CHECK_EQ(out.n_entities, 15);                         // 2 (4 + 4) − 1 paredes
    th_ui_stack_mouse_up(&pila, TH_UI_LEFT, px, py);
    in.world_up = 1 << TH_UI_LEFT;
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    dibujo_determinista("obra");
    TH_CHECK_EQ(boton(TH_GWB_COL_CONFIRMAR, TH_UI_LEFT, 1), 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_SALA);
    TH_CHECK_EQ(ordenes[0].x, 57);
    TH_CHECK_EQ(ordenes[0].y, 50);
    TH_CHECK_EQ(ordenes[0].w, 4);
    TH_CHECK_EQ(ordenes[0].h, 4);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    TH_CHECK_EQ(n_ordenes, 1);
    // Entre fotogramas no se puede abrir otra: TH_ORD_CERRAR no tendría salida
    // y la obra quedaría en el núcleo sin ventana.
    TH_CHECK_EQ(th_game_ui_open(g, TH_GW_AMUEBLAR), -1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    // La puerta: el puntero bajo el borde sur, en (58, 54): screenToWall da (58, 53) al sur.
    casilla(58, 54, -1, 1);
    TH_CHECK(out.n_entities > 15); // el plano y la puerta fantasma
    casilla(58, 54, TH_UI_LEFT, 1);
    TH_CHECK_EQ(n_ordenes, 2);
    TH_CHECK_EQ(ordenes[1].kind, TH_ORD_PUERTA);
    TH_CHECK_EQ(ordenes[1].x, 58);
    TH_CHECK_EQ(ordenes[1].y, 53);
    TH_CHECK_EQ(ordenes[1].dir, TH_DIR_SOUTH);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WINDOWS);
    // Sin ventanas: confirmar (TH_ORD_FIN_VENTANAS) lleva a la fase de objetos
    // (un fotograma antes, para que la ventana habilite el botón de la fase).
    fotograma(W / 2, H / 2, -1, 0, 0, 1);
    TH_CHECK_EQ(boton(TH_GWB_COL_CONFIRMAR, TH_UI_LEFT, 1), 1);
    TH_CHECK_EQ(ordenes[2].kind, TH_ORD_FIN_VENTANAS);
    for (int i = 0; i < 4 && sim.obra.phase != TH_OBRA_OBJECTS; i++) {
        th_sim_tick(&sim);
        fotograma(W / 2, H / 2, -1, 0, 0, 1);
    }
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_OBJECTS);
    TH_CHECK(sim.place.open);
    fotograma(W / 2, H / 2, -1, 0, 0, 1); // la ventana, con los botones de la fase
    // La tienda de la sala (UIEditRoom:purchaseItems): uno de la primera fila.
    TH_CHECK_EQ(boton(TH_GWB_COL_COMPRAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_AMUEBLAR);
    TH_CHECK(g->n_obj > 0);
    TH_CHECK_EQ(g->obj_qty[0], 0);
    boton(TH_GWB_MAS + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(boton(TH_GWB_AMU_CONFIRMAR, TH_UI_LEFT, 1), 1);
    TH_CHECK_EQ(ordenes[3].kind, TH_ORD_COMPRAR_SALA);
    TH_CHECK_EQ(ordenes[3].qty, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
    // Otra vez: la fila empieza con el que ya está en la lista (cur_qty), el
    // «−» no baja de ahí (no hay venta en el núcleo) y confirmar solo pide la
    // diferencia.
    TH_CHECK_EQ(boton(TH_GWB_COL_COMPRAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(g->obj_qty[0], 1);
    boton(TH_GWB_MENOS + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(g->obj_qty[0], 1);
    boton(TH_GWB_MAS + 0, TH_UI_LEFT, 1);
    boton(TH_GWB_MAS + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(g->obj_qty[0], 3);
    TH_CHECK_EQ(boton(TH_GWB_AMU_CONFIRMAR, TH_UI_LEFT, 1), 1);
    TH_CHECK_EQ(ordenes[4].kind, TH_ORD_COMPRAR_SALA);
    TH_CHECK_EQ(ordenes[4].qty, 2);
    TH_CHECK_EQ(ordenes[4].def, ordenes[3].def);
    // Escape: setEditRoom(false), TH_ORD_CERRAR (la sala no tiene sus objetos:
    // se abandona), y la ventana se cierra.
    fotograma(W / 2, H / 2, -1, 0, 1, 1);
    TH_CHECK_EQ(n_ordenes, 6);
    TH_CHECK_EQ(ordenes[5].kind, TH_ORD_CERRAR);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
}

static void test_escape_y_pausa(void)
{
    n_ordenes = 0;
    // En pausa las ventanas se abren y se recorren, pero no sale ninguna orden.
    pausa = 1;
    fotograma(W / 2, H / 2, -1, TH_PI_OBJETOS, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_AMUEBLAR);
    boton(TH_GWB_MAS + 1, TH_UI_LEFT, 1);
    boton(TH_GWB_AMU_CONFIRMAR, TH_UI_LEFT, 1);
    TH_CHECK_EQ(n_ordenes, 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_AMUEBLAR); // sigue abierta
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_CONTRATAR);
    boton(TH_GWB_SIGUIENTE, TH_UI_LEFT, 1);
    // Escape cierra.
    fotograma(W / 2, H / 2, -1, 0, 1, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    TH_CHECK_EQ(pila.n, 1); // solo el panel
    TH_CHECK_EQ(n_ordenes, 0);
    pausa = 0;
    th_game_ui_close_all(g);
}

// Dos objetos en una compra: TH_ORD_COMPRAR lleva uno, así que el segundo se
// pide cuando la lista del primero se vacía.
static void test_dos_objetos(void)
{
    n_ordenes = 0;
    camara(60, 66);
    fotograma(W / 2, H / 2, -1, TH_PI_OBJETOS, 0, 1);
    boton(TH_GWB_OBJETO + 1, TH_UI_LEFT, 1);            // un banco (la fila, izquierdo: +1)
    boton(TH_GWB_OBJETO + 2, TH_UI_LEFT, 1);            // una máquina de bebidas
    boton(TH_GWB_OBJETO + 2, TH_UI_LEFT, 1);            // y otra
    boton(TH_GWB_OBJETO_MENOS1 + 2, TH_UI_RIGHT, 1);    // el derecho la quita
    TH_CHECK_EQ(boton(TH_GWB_AMU_CONFIRMAR, TH_UI_LEFT, 1), 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_COMPRAR);
    TH_CHECK_EQ(ordenes[0].def, objeto("bench"));
    TH_CHECK_EQ(ordenes[0].qty, 1);
    int px, py;
    pixel(60, 70, &px, &py);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR);
    casilla(60, 70, TH_UI_LEFT, 1); // el banco: la lista se vacía
    TH_CHECK_EQ(n_ordenes, 2);
    TH_CHECK_EQ(ordenes[1].kind, TH_ORD_COLOCAR);
    fotograma(px, py, -1, 0, 0, 1); // y sale la compra de la máquina
    TH_CHECK_EQ(n_ordenes, 3);
    TH_CHECK_EQ(ordenes[2].kind, TH_ORD_COMPRAR);
    TH_CHECK_EQ(ordenes[2].def, objeto("drinks_machine"));
    TH_CHECK_EQ(ordenes[2].qty, 1);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR);
    TH_CHECK(sim.place.open);
    // Cancelar devuelve lo que no se colocó (TH_ORD_CERRAR) y cierra.
    boton(TH_GWB_COL_CANCELAR, TH_UI_LEFT, 1);
    TH_CHECK_EQ(ordenes[3].kind, TH_ORD_CERRAR);
    TH_CHECK(!sim.place.open);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
}

int main(int argc, char **argv)
{
    // Sin assets, 77: el código de «saltada» de ctest (SKIP_RETURN_CODE en
    // motor/CMakeLists.txt), para que no pase por buena sin probar nada.
    size_t len;
    uint8_t *demo = argc < 2 ? NULL : leer(argv[1], "escenarios/demo.trg", &len);
    if (!demo) {
        printf("pruebas de ventanas: sin assets (test_ventanas <assets>), nada que probar\n");
        return 77;
    }
    free(demo);
    empezar(argv[1]);
    if (th_test_failures == 0) {
        test_amueblar_y_colocar();
        test_contratar();
        test_construir();
        test_escape_y_pausa();
        test_dos_objetos();
    }
    for (int i = 0; i < n_datos; i++)
        free(datos[i]);
    printf("pruebas de ventanas: %d comprobaciones, %d fallos\n", th_test_checks, th_test_failures);
    return th_test_failures ? 1 : 0;
}
