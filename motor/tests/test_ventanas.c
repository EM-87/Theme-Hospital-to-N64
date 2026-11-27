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
static int a_medio_tick; // 1: el fotograma cae con la simulación a medio tick (sim_idle = 0)
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
                           .paused = pausa, .sim_idle = !a_medio_tick};
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
    // hay orden (decisión 4) y cerrar sin TH_ORD_DEVOLVER dejaría al candidato
    // el primero (CorsixTH lo pasa al final): sigue en la mano, como la obra.
    const uint32_t resumen2 = th_staff_profile_digest(&lista[0]);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    boton(TH_GWB_PESTANA + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    pausa = 1;
    fotograma(W / 2, H / 2, -1, 0, 1, 1);
    pausa = 0;
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    TH_CHECK_EQ(n_ordenes, 1);
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


// --- La revisión adversaria de la demo (tanda B3) ----------------------------------

static th_rnode sala(const char *id)
{
    return th_rules_get(&defs, th_rules_get(&defs, defs.root, "salas"), id);
}

// Una orden aplicada como lo haría el gancho del anfitrión (al empezar el tick).
static th_order_result aplicar(const th_order *o)
{
    th_order_result r;
    th_build_apply(&sim, o, &r);
    th_game_ui_order_result(g, &sim, o, &r);
    return r;
}

// La consulta de demo.tsv (57, 50, 4 × 4, puerta al sur) hasta la fase de
// objetos, con la ventana de la obra abierta: el panel y la lista de construir
// por la interfaz, las órdenes de la obra directamente al núcleo.
static void obra_en_objetos(void)
{
    camara(58, 52);
    fotograma(W / 2, H / 2, -1, TH_PI_SALAS, 0, 1);
    boton(TH_GWB_CATEGORIA + 0, TH_UI_LEFT, 1);
    boton(TH_GWB_SALA + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
    const th_order o[3] = {{.kind = TH_ORD_SALA, .x = 57, .y = 50, .w = 4, .h = 4, .def = sala("gp")},
                           {.kind = TH_ORD_PUERTA, .x = 58, .y = 53, .dir = TH_DIR_SOUTH},
                           {.kind = TH_ORD_FIN_VENTANAS}};
    for (int i = 0; i < 3; i++)
        TH_CHECK_EQ(aplicar(&o[i]).reason, TH_REASON_OK);
    for (int i = 0; i < 8 && sim.obra.phase != TH_OBRA_OBJECTS; i++)
        th_sim_tick(&sim);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_OBJECTS);
    fotograma(W / 2, H / 2, -1, 0, 0, 1);
    fotograma(W / 2, H / 2, -1, 0, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
}

// Lo comprado en la tienda de la sala que queda por pedir (n_pend) no puede
// acabar comprado para el pasillo: si la obra se cierra (Escape: TH_ORD_CERRAR,
// que la abandona o la termina) mientras la primera TH_ORD_COMPRAR_SALA está
// en vuelo, gw_amueblar_pendientes pedía el resto como TH_ORD_COMPRAR, que
// cobra y abre la lista de colocar del pasillo con un objeto de la sala.
static void test_tienda_y_obra_cerrada(void)
{
    obra_en_objetos();
    TH_CHECK_EQ(boton(TH_GWB_COL_COMPRAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_AMUEBLAR);
    TH_CHECK(g->n_obj >= 2);
    boton(TH_GWB_MAS + 0, TH_UI_LEFT, 1);
    boton(TH_GWB_MAS + 1, TH_UI_LEFT, 1);
    const th_rnode segundo = g->obj_def[1];
    n_ordenes = 0;
    // Confirmar y Escape en la obra antes de que llegue el tick (las órdenes
    // quedan en vuelo): COMPRAR_SALA del primero y CERRAR.
    TH_CHECK_EQ(boton(TH_GWB_AMU_CONFIRMAR, TH_UI_LEFT, 0), 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
    fotograma(W / 2, H / 2, -1, 0, 1, 0);
    TH_CHECK_EQ(n_ordenes, 2);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_COMPRAR_SALA);
    TH_CHECK_EQ(ordenes[1].kind, TH_ORD_CERRAR);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    // El tick las aplica: la sala sin sus objetos se abandona.
    aplicar(&ordenes[0]);
    aplicar(&ordenes[1]);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    const int32_t saldo = sim.hosp.balance;
    for (int i = 0; i < 3; i++)
        fotograma(W / 2, H / 2, -1, 0, 0, 1);
    int compras = 0;
    for (int i = 2; i < n_ordenes; i++)
        compras += ordenes[i].kind == TH_ORD_COMPRAR && ordenes[i].def == segundo;
    TH_CHECK_EQ(compras, 0);
    TH_CHECK_EQ(sim.hosp.balance, saldo);
    TH_CHECK(!sim.place.open);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    th_game_ui_close_all(g);
    if (sim.place.open) { // sin el arreglo: la lista del pasillo, para no arrastrarla
        const th_order c = {.kind = TH_ORD_CERRAR};
        th_build_apply(&sim, &c, NULL);
    }
}

// Abrir otra ventana desde el panel con alguien en la mano (colocar personal):
// UI:addWindow cierra UIPlaceStaff, que es «main», y su close() devuelve el
// perfil al final de available_staff (place_staff.lua:69-71), como el derecho
// y Escape. Tiene que salir TH_ORD_DEVOLVER.
static void test_panel_con_persona_en_mano(void)
{
    n_ordenes = 0;
    camara(60, 58);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    boton(TH_GWB_PESTANA + 1, TH_UI_LEFT, 1); // enfermeras
    const th_staff_profile *lista;
    const int n = th_sim_candidates(&sim, TH_HUM_NURSE, &lista);
    TH_CHECK(n > 1);
    const uint32_t resumen = th_staff_profile_digest(&lista[0]);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    fotograma(W / 2, H / 2, -1, TH_PI_SALAS, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_CONSTRUIR);
    TH_CHECK_EQ(n_ordenes, 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_DEVOLVER);
    TH_CHECK_EQ(ordenes[0].def, resumen);
    TH_CHECK_EQ(th_sim_candidates(&sim, TH_HUM_NURSE, &lista), n);
    TH_CHECK_EQ(th_staff_profile_digest(&lista[n - 1]), resumen);
    th_game_ui_close_all(g);
}

// Un clic de colocar personal que el núcleo rechaza (aquí, a medio tick: la
// vista previa repite la de otra casilla) manda al candidato al final de su
// lista (order_contratar: candidate_to_end), pero la persona sigue en la mano
// (th_game_ui_order_result). El clic siguiente, en una casilla buena, tiene
// que contratar a esa persona, no dar sin_candidato para siempre.
static void test_contratar_tras_rechazo(void)
{
    n_ordenes = 0;
    camara(60, 58);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    boton(TH_GWB_PESTANA + 1, TH_UI_LEFT, 1);
    const th_staff_profile *lista;
    const int n = th_sim_candidates(&sim, TH_HUM_NURSE, &lista);
    TH_CHECK(n > 1);
    const uint32_t resumen = th_staff_profile_digest(&lista[0]);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    int px, py;
    pixel(59, 56, &px, &py); // el pasillo: vale
    py -= 7;
    fotograma(px, py, -1, 0, 0, 1);
    // A medio tick, sobre la fachada (60, 66 está dentro; 60, 60 + 30 no es
    // del hospital): la vista previa es la de (59, 56).
    int qx, qy;
    pixel(40, 56, &qx, &qy);
    qy -= 7;
    TH_CHECK(th_build_staff_placement(&sim, TH_HUM_NURSE, 40, 56) != TH_REASON_OK);
    a_medio_tick = 1;
    fotograma(qx, qy, TH_UI_LEFT, 0, 0, 1);
    a_medio_tick = 0;
    TH_CHECK_EQ(n_ordenes, 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_CONTRATAR);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL); // sigue en la mano
    // El núcleo lo mandó al final de la lista.
    TH_CHECK_EQ(th_staff_profile_digest(&lista[n - 1]), resumen);
    // Y ahora en el pasillo, entre ticks.
    fotograma(px, py, TH_UI_LEFT, 0, 0, 1);
    TH_CHECK_EQ(n_ordenes, 2);
    TH_CHECK_EQ(ordenes[1].kind, TH_ORD_CONTRATAR);
    TH_CHECK_EQ(ordenes[1].def, resumen);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE); // contratada: se cierra
    TH_CHECK_EQ(th_sim_candidates(&sim, TH_HUM_NURSE, &lista), n - 1);
    th_game_ui_close_all(g);
}

// La orden en vuelo no se olvida por estar en pausa: con la compra de dos
// objetos de pasillo hecha justo antes de pausar (TH_ORD_COMPRAR del primero
// en la cola, sin tick que la aplique), más de 30 s en pausa ponían en_vuelo a
// 0 y, al quitar la pausa, el segundo TH_ORD_COMPRAR salía antes de que se
// aplicara el primero: el núcleo cerraba la lista del primero (devuelto y
// reembolsado) al abrir la del segundo.
static void test_pausa_larga_con_orden_en_vuelo(void)
{
    n_ordenes = 0;
    camara(60, 66);
    fotograma(W / 2, H / 2, -1, TH_PI_OBJETOS, 0, 1);
    boton(TH_GWB_OBJETO + 1, TH_UI_LEFT, 1); // un banco
    boton(TH_GWB_OBJETO + 2, TH_UI_LEFT, 1); // una máquina de bebidas
    TH_CHECK_EQ(boton(TH_GWB_AMU_CONFIRMAR, TH_UI_LEFT, 0), 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_COMPRAR);
    TH_CHECK_EQ(ordenes[0].def, objeto("bench"));
    pausa = 1;
    for (int i = 0; i < GW_EN_VUELO_MAX + 10; i++)
        fotograma(W / 2, H / 2, -1, 0, 0, 0);
    pausa = 0;
    fotograma(W / 2, H / 2, -1, 0, 0, 0);
    TH_CHECK_EQ(n_ordenes, 1); // la máquina espera a que se coloque el banco
    for (int i = 0; i < n_ordenes; i++)
        aplicar(&ordenes[i]);
    TH_CHECK(sim.place.open);
    TH_CHECK_EQ(sim.place.item[0].def, objeto("bench"));
    th_game_ui_close_all(g);
    if (sim.place.open) {
        const th_order c = {.kind = TH_ORD_CERRAR};
        th_build_apply(&sim, &c, NULL);
    }
}

// Las entidades del plano: el más grande que deja arrastrar la obra
// (PLANO_MAX, 12 × 12) son 47 paredes; con la puerta, 48, el tope
// TH_GAME_UI_ENTITIES, así que en la fase de ventanas no se dibujaba ninguna
// ventana puesta ni la fantasma bajo el puntero (entidad() las tiraba).
static void test_plano_grande_con_ventanas(void)
{
    // Un sitio de 12 × 12 que el núcleo acepte, con la ventana de la obra.
    camara(58, 52);
    fotograma(W / 2, H / 2, -1, TH_PI_SALAS, 0, 1);
    boton(TH_GWB_CATEGORIA + 0, TH_UI_LEFT, 1);
    boton(TH_GWB_SALA + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
    int ok = 0, x0 = 0, y0 = 0;
    for (int y = 30; y < 100 && !ok; y++)
        for (int x = 30; x < 100 && !ok; x++) {
            const th_order o = {.kind = TH_ORD_SALA, .x = (uint8_t)x, .y = (uint8_t)y, .w = 12, .h = 12,
                                .def = sala("gp")};
            th_order_result r;
            th_build_apply(&sim, &o, &r);
            if (r.reason == TH_REASON_OK)
                ok = 1, x0 = x, y0 = y;
        }
    TH_CHECK(ok);
    if (!ok)
        return;
    // La puerta en el primer borde que valga y una ventana en otro.
    int puerta = 0;
    for (int i = 1; i < 11 && !puerta; i++) {
        const th_order o = {.kind = TH_ORD_PUERTA, .x = (uint8_t)(x0 + i), .y = (uint8_t)(y0 + 11), .dir = TH_DIR_SOUTH};
        puerta = aplicar(&o).reason == TH_REASON_OK;
    }
    TH_CHECK(puerta);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WINDOWS);
    int ventanas = 0;
    for (int i = 1; i < 11 && ventanas < 2; i++) {
        const th_order o = {.kind = TH_ORD_VENTANA, .x = (uint8_t)(x0 + 11), .y = (uint8_t)(y0 + i), .dir = TH_DIR_EAST};
        ventanas += aplicar(&o).reason == TH_REASON_OK;
    }
    TH_CHECK_EQ(ventanas, 2);
    camara(x0 + 6, y0 + 6);
    fotograma(W / 2, H / 2, -1, 0, 0, 1);
    int n_ventanas = 0, n_puertas = 0;
    for (int i = 0; i < out.n_entities; i++) {
        n_ventanas += out.entities[i].anim == 130;
        n_puertas += out.entities[i].anim == 126;
    }
    printf("  plano de 12 × 12 en (%d, %d): %d entidades, %d puerta, %d ventanas\n", x0, y0, out.n_entities,
           n_puertas, n_ventanas);
    TH_CHECK_EQ(n_puertas, 1);
    TH_CHECK_EQ(n_ventanas, 2);
    // Y la fantasma en un borde libre: el oeste.
    casilla(x0, y0 + 5, -1, 1);
    n_ventanas = 0;
    for (int i = 0; i < out.n_entities; i++)
        n_ventanas += out.entities[i].anim == 130;
    TH_CHECK_EQ(n_ventanas, 3);
    // Se abandona (Escape: TH_ORD_CERRAR).
    fotograma(W / 2, H / 2, -1, 0, 1, 1);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    th_game_ui_close_all(g);
}

// Las órdenes que salieron sin aplicar (aplicar = 0) desde la k-ésima: como
// el tick siguiente del anfitrión, con su resultado de vuelta.
static void aplicar_desde(int k)
{
    for (; k < n_ordenes; k++) {
        th_order_result r;
        th_build_apply(&sim, &ordenes[k], &r);
        th_game_ui_order_result(g, &sim, &ordenes[k], &r);
    }
}

// La revisión del jugador (tanda B3): el clic de colocar personal que cae a
// medio tick con la vista previa de otra casilla llega al núcleo, que lo
// rechaza y pasa el candidato al final de su lista (decisión 5); la persona
// sigue en la mano y el clic siguiente tiene que pedirla ahí, no en su
// índice de antes (antes, sin_candidato para siempre).
static void test_contratar_rechazado(void)
{
    n_ordenes = 0;
    camara(60, 60);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    boton(TH_GWB_PESTANA + 1, TH_UI_LEFT, 1); // enfermeras
    const th_staff_profile *lista;
    const int n = th_sim_candidates(&sim, TH_HUM_NURSE, &lista);
    TH_CHECK(n > 2);
    const uint32_t resumen = th_staff_profile_digest(&lista[0]);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    int px, py;
    pixel(59, 61, &px, &py);
    py -= 7; // UIPlaceStaff mira 7 píxeles más abajo
    fotograma(px, py, -1, 0, 0, 1); // la vista previa: vale
    int fx, fy;
    pixel(63, 79, &fx, &fy); // la calle: no vale
    fy -= 7;
    a_medio_tick = 1;
    fotograma(fx, fy, TH_UI_LEFT, 0, 0, 1);
    a_medio_tick = 0;
    TH_CHECK_EQ(n_ordenes, 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_CONTRATAR);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    TH_CHECK_EQ(th_staff_profile_digest(&lista[n - 1]), resumen); // al final de la lista
    fotograma(px, py, TH_UI_LEFT, 0, 0, 1);
    TH_CHECK_EQ(n_ordenes, 2);
    TH_CHECK_EQ(ordenes[1].qty, n - 1);
    TH_CHECK_EQ(ordenes[1].def, resumen);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE); // contratada
    TH_CHECK_EQ(th_sim_candidates(&sim, TH_HUM_NURSE, &lista), n - 1);
}

// Dos «atrás» de la obra antes de que vuelva el primero: uno solo (antes, dos
// fases atrás; desde la puerta, la obra abandonada). Y otra ventana abierta
// con TH_ORD_SALA sin respuesta: al aplicarse, la obra no se queda en el
// núcleo sin ventana (antes, el plano cerrado en el mapa sin dibujar).
static void test_obra_en_vuelo(void)
{
    n_ordenes = 0;
    camara(62, 52);
    fotograma(W / 2, H / 2, -1, TH_PI_SALAS, 0, 1);
    boton(TH_GWB_CATEGORIA + 0, TH_UI_LEFT, 1);
    boton(TH_GWB_SALA + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
    int px, py;
    pixel(57, 50, &px, &py);
    th_ui_stack_mouse_move(&pila, px, py);
    th_ui_stack_mouse_down(&pila, TH_UI_LEFT, px, py);
    th_game_ui_input in = {.px = px, .py = py, .id = -1, .sim_idle = 1, .world_down = 1 << TH_UI_LEFT};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    pixel(60, 53, &px, &py);
    th_ui_stack_mouse_move(&pila, px, py);
    th_ui_stack_mouse_up(&pila, TH_UI_LEFT, px, py);
    in = (th_game_ui_input){.px = px, .py = py, .id = -1, .sim_idle = 1, .world_up = 1 << TH_UI_LEFT};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    TH_CHECK_EQ(boton(TH_GWB_COL_CONFIRMAR, TH_UI_LEFT, 1), 1);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    fotograma(W / 2, H / 2, -1, 0, 0, 1);
    TH_CHECK_EQ(boton(TH_GWB_COL_CANCELAR, TH_UI_LEFT, 0), 1);
    TH_CHECK_EQ(boton(TH_GWB_COL_CANCELAR, TH_UI_LEFT, 0), 0);
    aplicar_desde(1);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WALLS);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
    // Escape la abandona. Otra obra con el confirmar del plano sin respuesta;
    // el jugador abre contratar antes de que vuelva.
    fotograma(W / 2, H / 2, -1, 0, 1, 1);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    fotograma(W / 2, H / 2, -1, TH_PI_SALAS, 0, 1);
    boton(TH_GWB_CATEGORIA + 0, TH_UI_LEFT, 1);
    boton(TH_GWB_SALA + 0, TH_UI_LEFT, 1);
    pixel(57, 50, &px, &py);
    th_ui_stack_mouse_move(&pila, px, py);
    th_ui_stack_mouse_down(&pila, TH_UI_LEFT, px, py);
    in = (th_game_ui_input){.px = px, .py = py, .id = -1, .sim_idle = 1, .world_down = 1 << TH_UI_LEFT};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    pixel(60, 53, &px, &py);
    th_ui_stack_mouse_move(&pila, px, py);
    th_ui_stack_mouse_up(&pila, TH_UI_LEFT, px, py);
    in = (th_game_ui_input){.px = px, .py = py, .id = -1, .sim_idle = 1, .world_up = 1 << TH_UI_LEFT};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    const int k = n_ordenes;
    TH_CHECK_EQ(boton(TH_GWB_COL_CONFIRMAR, TH_UI_LEFT, 0), 1);
    TH_CHECK_EQ(ordenes[k].kind, TH_ORD_SALA);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_CONTRATAR);
    aplicar_desde(k);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR); // la obra, en el núcleo sin su ventana
    fotograma(W / 2, H / 2, -1, 0, 0, 1);
    TH_CHECK_EQ(ordenes[n_ordenes - 1].kind, TH_ORD_CERRAR);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_CONTRATAR);
    fotograma(W / 2, H / 2, -1, 0, 1, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
}

// El plano arrastrado y soltado sobre la ventana de la obra (arriba a la
// derecha, adonde va el puntero a por el confirmar): el arrastre acaba ahí y
// el plano es el de la última casilla del mundo (antes seguía al puntero
// hasta el confirmar y mandaba otro plano, de 7 × 6).
static void test_soltar_sobre_ventana(void)
{
    n_ordenes = 0;
    camara(62, 52);
    fotograma(W / 2, H / 2, -1, TH_PI_SALAS, 0, 1);
    boton(TH_GWB_CATEGORIA + 0, TH_UI_LEFT, 1);
    boton(TH_GWB_SALA + 0, TH_UI_LEFT, 1);
    int px, py;
    pixel(57, 50, &px, &py);
    th_ui_stack_mouse_move(&pila, px, py);
    th_ui_stack_mouse_down(&pila, TH_UI_LEFT, px, py);
    th_game_ui_input in = {.px = px, .py = py, .id = -1, .sim_idle = 1, .world_down = 1 << TH_UI_LEFT};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    pixel(60, 53, &px, &py);
    th_ui_stack_mouse_move(&pila, px, py);
    in = (th_game_ui_input){.px = px, .py = py, .id = -1, .sim_idle = 1};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    // Sobre la ventana (su centro) y suelta allí: la pila lo toma.
    const th_ui_window *w = th_game_ui_window(g);
    px = w->x + w->w / 2;
    py = w->y + 40;
    th_ui_stack_mouse_move(&pila, px, py);
    in = (th_game_ui_input){.px = px, .py = py, .id = -1, .sim_idle = 1};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    TH_CHECK(th_ui_stack_mouse_up(&pila, TH_UI_LEFT, px, py).win != NULL);
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    TH_CHECK_EQ(g->arrastre, 0);
    TH_CHECK_EQ(boton(TH_GWB_COL_CONFIRMAR, TH_UI_LEFT, 1), 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_SALA);
    TH_CHECK_EQ(ordenes[0].x, 57);
    TH_CHECK_EQ(ordenes[0].y, 50);
    TH_CHECK_EQ(ordenes[0].w, 4);
    TH_CHECK_EQ(ordenes[0].h, 4);
    fotograma(W / 2, H / 2, -1, 0, 1, 1); // Escape: se abandona
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
}

// Dos derechos sobre el mismo banco antes de que vuelva el primero: un solo
// TH_ORD_RECOGER (antes, el segundo llegaba con la lista abierta y la barra
// decía que la sala no cabía).
static void test_recoger_doble(void)
{
    n_ordenes = 0;
    camara(60, 68);
    fotograma(W / 2, H / 2, -1, TH_PI_OBJETOS, 0, 1);
    boton(TH_GWB_OBJETO + 1, TH_UI_LEFT, 1); // un banco
    TH_CHECK_EQ(boton(TH_GWB_AMU_CONFIRMAR, TH_UI_LEFT, 1), 1);
    int px, py;
    pixel(58, 72, &px, &py);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR);
    casilla(58, 72, TH_UI_LEFT, 1);
    TH_CHECK_EQ(ordenes[1].kind, TH_ORD_COLOCAR);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    const int k = n_ordenes;
    casilla(58, 72, TH_UI_RIGHT, 0);
    casilla(58, 72, TH_UI_RIGHT, 0);
    TH_CHECK_EQ(n_ordenes, k + 1);
    TH_CHECK_EQ(ordenes[k].kind, TH_ORD_RECOGER);
    aplicar_desde(k);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR);
    fotograma(px, py, -1, 0, 1, 1); // Escape: deshacer, el banco vuelve a su sitio
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
}

// ¿Está habilitado el botón de la ventana con ese id?
static int habilitado(int id)
{
    const th_ui_window *w = th_game_ui_window(g);
    for (int i = 0; i < w->n_buttons; i++)
        if (w->buttons[i].id == id)
            return w->buttons[i].enabled;
    return -1;
}

// Un arrastre del plano de la obra de (x0, y0) a (x1, y1) con un fotograma
// por paso: baja, mueve y sube (como baja:, mueve: y sube: del guion).
static void arrastrar(int x0, int y0, int x1, int y1)
{
    int px, py;
    pixel(x0, y0, &px, &py);
    th_ui_stack_mouse_move(&pila, px, py);
    th_ui_stack_mouse_down(&pila, TH_UI_LEFT, px, py);
    th_game_ui_input in = {.px = px, .py = py, .id = -1, .sim_idle = 1, .world_down = 1 << TH_UI_LEFT, .paused = pausa};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    pixel(x1, y1, &px, &py);
    th_ui_stack_mouse_move(&pila, px, py);
    in = (th_game_ui_input){.px = px, .py = py, .id = -1, .sim_idle = 1, .paused = pausa};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    th_ui_stack_mouse_up(&pila, TH_UI_LEFT, px, py);
    in = (th_game_ui_input){.px = px, .py = py, .id = -1, .sim_idle = 1, .world_up = 1 << TH_UI_LEFT, .paused = pausa};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
}

// La revisión del jugador (tanda B3): con el plano en rojo (demasiado
// pequeño, o encima de una sala o fuera del hospital) el confirmar está
// deshabilitado, como en CorsixTH (setBlueprintRect,
// edit_room.lua:1186-1202: confirm_button:enable(is_valid)); antes se podía
// pulsar y el núcleo lo rechazaba («You can't place this room here»).
static void test_plano_malo_sin_confirmar(void)
{
    n_ordenes = 0;
    camara(62, 52);
    fotograma(W / 2, H / 2, -1, TH_PI_SALAS, 0, 1);
    boton(TH_GWB_CATEGORIA + 0, TH_UI_LEFT, 1);
    boton(TH_GWB_SALA + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
    TH_CHECK_EQ(habilitado(TH_GWB_COL_CONFIRMAR), 0); // sin plano
    arrastrar(57, 50, 58, 51); // 2 × 2: la consulta pide 4 (minimum_size)
    TH_CHECK_EQ(habilitado(TH_GWB_COL_CONFIRMAR), 0);
    TH_CHECK_EQ(boton(TH_GWB_COL_CONFIRMAR, TH_UI_LEFT, 1), 0);
    arrastrar(56, 49, 59, 52); // 4 × 4 con la fila de arriba fuera del edificio
    TH_CHECK_EQ(habilitado(TH_GWB_COL_CONFIRMAR), 0);
    TH_CHECK_EQ(boton(TH_GWB_COL_CONFIRMAR, TH_UI_LEFT, 1), 0);
    arrastrar(57, 50, 60, 53); // el bueno, el de demo.tsv
    TH_CHECK_EQ(habilitado(TH_GWB_COL_CONFIRMAR), 1);
    TH_CHECK_EQ(boton(TH_GWB_COL_CONFIRMAR, TH_UI_LEFT, 1), 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_SALA);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    // Atrás vuelve a las paredes con el plano del núcleo, que vale: el
    // confirmar sigue habilitado.
    fotograma(W / 2, H / 2, -1, 0, 0, 1);
    TH_CHECK_EQ(boton(TH_GWB_COL_CANCELAR, TH_UI_LEFT, 1), 1);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WALLS);
    fotograma(W / 2, H / 2, -1, 0, 0, 1);
    TH_CHECK_EQ(habilitado(TH_GWB_COL_CONFIRMAR), 1);
    fotograma(W / 2, H / 2, -1, 0, 1, 1); // Escape: se abandona
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
}

// El botón del panel de una ventana abierta la cierra (bottom_panel.lua:
// 726-731, 750-755 y 774-779); con la tienda de la sala, vuelve a la obra.
static void test_panel_cierra(void)
{
    n_ordenes = 0;
    camara(62, 52);
    static const int botones[3] = {TH_PI_SALAS, TH_PI_OBJETOS, TH_PI_CONTRATAR};
    static const int ventanas[3] = {TH_GW_CONSTRUIR, TH_GW_AMUEBLAR, TH_GW_CONTRATAR};
    for (int k = 0; k < 3; k++) {
        fotograma(10, 10, -1, botones[k], 0, 1);
        TH_CHECK_EQ(th_game_ui_active(g), ventanas[k]);
        fotograma(10, 10, -1, botones[k], 0, 1);
        TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    }
    TH_CHECK_EQ(n_ordenes, 0);
    // Contratar sin categoría elegida: no hay perfil y suena wrong2.wav
    // (hire_staff.lua:134-137) y no se abre colocar personal.
    fotograma(10, 10, -1, TH_PI_CONTRATAR, 0, 1);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(out.reject_sound, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_CONTRATAR);
    fotograma(10, 10, -1, 0, 1, 1); // Escape
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    // La tienda de la sala: la obra en la fase de objetos y «comprar».
    obra_en_objetos();
    TH_CHECK_EQ(boton(TH_GWB_COL_COMPRAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_AMUEBLAR);
    fotograma(10, 10, -1, TH_PI_OBJETOS, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_OBJECTS);
    fotograma(W / 2, H / 2, -1, 0, 1, 1); // Escape: se abandona
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    th_game_ui_close_all(g);
}

// --- La revisión del contrato y el código (tanda B3, segunda vuelta) ---------------

// El TH_ORD_CONTRATAR aceptado de una persona no puede cerrar la ventana de
// colocar de otra. Con la de A en vuelo (el clic justo antes de pausar: en la
// cola del núcleo sin tick que la aplique), el jugador abre contratar desde
// el panel (dejar_lo_abierto no manda DEVOLVER con una contratación en vuelo),
// elige a B y lo tiene en la mano; al quitar la pausa se aplica la de A y
// th_game_ui_order_result cerraba la ventana de B sin mirar de quién era.
static void test_contratado_otro_en_mano(void)
{
    n_ordenes = 0;
    camara(60, 58);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    boton(TH_GWB_PESTANA + 2, TH_UI_LEFT, 1); // bedeles
    const th_staff_profile *lista;
    const int n = th_sim_candidates(&sim, TH_HUM_HANDYMAN, &lista);
    TH_CHECK(n > 2);
    if (n <= 2)
        return;
    const uint32_t a = th_staff_profile_digest(&lista[0]), b = th_staff_profile_digest(&lista[1]);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    int px, py;
    pixel(59, 56, &px, &py);
    py -= 7;
    fotograma(px, py, -1, 0, 0, 1);
    fotograma(px, py, TH_UI_LEFT, 0, 0, 0); // A, en vuelo
    TH_CHECK_EQ(n_ordenes, 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_CONTRATAR);
    TH_CHECK_EQ(ordenes[0].def, a);
    pausa = 1;
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_CONTRATAR);
    boton(TH_GWB_PESTANA + 2, TH_UI_LEFT, 0);
    boton(TH_GWB_SIGUIENTE, TH_UI_LEFT, 0);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 0), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    TH_CHECK_EQ(g->resumen, b);
    pausa = 0;
    aplicar_desde(0); // el tick siguiente: A contratado
    TH_CHECK_EQ(th_sim_candidates(&sim, TH_HUM_HANDYMAN, &lista), n - 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL); // B sigue en la mano
    // Y se coloca: el índice de B ya es otro (0).
    pixel(61, 56, &px, &py);
    py -= 7;
    fotograma(px, py, -1, 0, 0, 1);
    fotograma(px, py, TH_UI_LEFT, 0, 0, 1);
    TH_CHECK_EQ(n_ordenes, 2);
    TH_CHECK_EQ(ordenes[1].kind, TH_ORD_CONTRATAR);
    TH_CHECK_EQ(ordenes[1].def, b);
    TH_CHECK_EQ(ordenes[1].qty, 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    TH_CHECK_EQ(th_sim_candidates(&sim, TH_HUM_HANDYMAN, &lista), n - 2);
    th_game_ui_close_all(g);
}

// En pausa, UIPlaceStaff:onMouseUp no hace nada (place_staff.lua:145: «if
// self.world.user_actions_allowed»): el derecho no la cierra, la persona sigue
// en la mano. Y UIPlaceObjects:onMouseUp tampoco gira el objeto
// (place_objects.lua:503). Escape tampoco la cierra en pausa: CorsixTH la
// cierra y pasa al candidato al final, pero esa orden (TH_ORD_DEVOLVER) no
// puede salir en pausa (decisión 4), como el TH_ORD_CERRAR de la obra.
static void test_pausa_derecho(void)
{
    n_ordenes = 0;
    camara(60, 58);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    boton(TH_GWB_PESTANA + 2, TH_UI_LEFT, 1);
    TH_CHECK_EQ(boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    pausa = 1;
    casilla(59, 56, TH_UI_RIGHT, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    TH_CHECK_EQ(n_ordenes, 0);
    fotograma(W / 2, H / 2, -1, 0, 1, 1); // Escape: en pausa, tampoco (sin TH_ORD_DEVOLVER)
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    TH_CHECK_EQ(n_ordenes, 0);
    pausa = 0;
    fotograma(W / 2, H / 2, -1, 0, 1, 1); // Escape fuera de la pausa: devolver
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    TH_CHECK_EQ(n_ordenes, 1);
    TH_CHECK_EQ(ordenes[0].kind, TH_ORD_DEVOLVER);
    // Colocar: un banco en la mano; el derecho en pausa no lo gira.
    camara(60, 68);
    fotograma(W / 2, H / 2, -1, TH_PI_OBJETOS, 0, 1);
    boton(TH_GWB_OBJETO + 1, TH_UI_LEFT, 1);
    TH_CHECK_EQ(boton(TH_GWB_AMU_CONFIRMAR, TH_UI_LEFT, 1), 1);
    int px, py;
    pixel(58, 70, &px, &py);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR);
    const int orient = g->orient;
    pausa = 1;
    casilla(58, 70, TH_UI_RIGHT, 1);
    TH_CHECK_EQ(g->orient, orient);
    pausa = 0;
    casilla(58, 70, TH_UI_RIGHT, 1); // fuera de la pausa, sí
    TH_CHECK(g->orient != orient);
    fotograma(px, py, -1, 0, 1, 1); // Escape: deshacer
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    TH_CHECK(!sim.place.open);
}


// El botón del panel con su ventana ya abierta la cierra (dialogBuildRoom,
// dialogFurnishCorridor y dialogHireStaff: «if w then w:close()»,
// bottom_panel.lua:722-731, 746-755 y 770-779); antes no hacía nada.
static void test_panel_dos_veces(void)
{
    n_ordenes = 0;
    camara(60, 66);
    static const int botones[3] = {TH_PI_SALAS, TH_PI_OBJETOS, TH_PI_CONTRATAR};
    static const int ventanas[3] = {TH_GW_CONSTRUIR, TH_GW_AMUEBLAR, TH_GW_CONTRATAR};
    for (int k = 0; k < 3; k++) {
        fotograma(W / 2, H / 2, -1, botones[k], 0, 1);
        TH_CHECK_EQ(th_game_ui_active(g), ventanas[k]);
        fotograma(W / 2, H / 2, -1, botones[k], 0, 1);
        TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    }
    TH_CHECK_EQ(n_ordenes, 0);
}

// --- La revisión de fidelidad, del jugador y de la ROM (tanda B3, tercera vuelta) --

// El confirmar de la fase de objetos, solo con los obligatorios en la sala
// (checkEnableConfirm, edit_room.lua:1605-1632); antes estaba siempre
// habilitado y el núcleo rechazaba TH_ORD_FIN_SALA con faltan_objetos. La
// lista va por la huella (UIPlaceObjects:addObjects, place_objects.lua:229-236):
// escritorio, archivador y silla, con el escritorio en la mano. Pulsar la
// fila activa gira el objeto (idx, place_objects.lua:112-124) y elegir otra
// le da su primera orientación (setActiveIndex), también cuando la anterior
// se agota. El modo coger se apaga al coger y al pulsar una fila. Al final
// se abandona la obra.
static void test_confirmar_y_lista(void)
{
    obra_en_objetos();
    camara(62, 52); // como ventanas_partida.txt: la ventana de la obra no tapa la sala
    n_ordenes = 0;
    const th_rnode escritorio = objeto("desk"), archivador = objeto("cabinet"), silla = objeto("chair");
    TH_CHECK_EQ(sim.place.item[0].def, archivador); // el núcleo, en el orden de objects_needed
    TH_CHECK_EQ(g->col_total, 3);
    TH_CHECK_EQ(sim.place.item[g->col_orden[0]].def, escritorio);
    TH_CHECK_EQ(sim.place.item[g->col_orden[1]].def, archivador);
    TH_CHECK_EQ(sim.place.item[g->col_orden[2]].def, silla);
    int px, py;
    pixel(58, 51, &px, &py);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(g->en_mano, escritorio);
    TH_CHECK_EQ(g->orient, TH_DIR_NORTH);
    TH_CHECK_EQ(habilitado(TH_GWB_COL_CONFIRMAR), 0);
    TH_CHECK_EQ(boton(TH_GWB_COL_CONFIRMAR, TH_UI_LEFT, 1), 0); // ni con el clic
    {
        // La descripción, cortada a dos líneas, entera en la ayuda.
        const th_ui_window *w = th_game_ui_window(g);
        int tx, ty;
        const char *ayuda = th_ui_tooltip_at(w, w->x + 20, w->y + 35, &tx, &ty);
        TH_CHECK(ayuda && !strcmp(ayuda, "place_objects_window.place_objects"));
    }
    // Como demo.tsv: el archivador en (60, 50); agotado, la primera fila (el
    // escritorio) vuelve a la mano.
    boton(TH_GWB_COL_FILA + 1, TH_UI_LEFT, 1);
    TH_CHECK_EQ(g->en_mano, archivador);
    casilla(60, 50, TH_UI_LEFT, 1);
    TH_CHECK_EQ(ordenes[0].def, archivador);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(g->en_mano, escritorio);
    // La fila activa gira; la silla, elegida, empieza al norte.
    boton(TH_GWB_COL_FILA + 0, TH_UI_LEFT, 1);
    TH_CHECK(g->orient != TH_DIR_NORTH);
    boton(TH_GWB_COL_FILA + 1, TH_UI_LEFT, 1);
    TH_CHECK_EQ(g->en_mano, silla);
    TH_CHECK_EQ(g->orient, TH_DIR_NORTH);
    casilla(58, 52, TH_UI_RIGHT, 1);
    casilla(58, 52, TH_UI_RIGHT, 1);
    TH_CHECK_EQ(g->orient, TH_DIR_SOUTH);
    casilla(58, 52, TH_UI_LEFT, 1);
    TH_CHECK_EQ(ordenes[1].kind, TH_ORD_COLOCAR);
    TH_CHECK_EQ(ordenes[1].def, silla);
    TH_CHECK_EQ(ordenes[1].dir, TH_DIR_SOUTH);
    // La silla se agotó: el escritorio, al norte, no girado como lo dejó la
    // fila pulsada ni con la orientación de la silla (antes se heredaba).
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(g->en_mano, escritorio);
    TH_CHECK_EQ(g->orient, TH_DIR_NORTH);
    TH_CHECK_EQ(habilitado(TH_GWB_COL_CONFIRMAR), 0);
    casilla(58, 51, TH_UI_LEFT, 1);
    TH_CHECK_EQ(ordenes[2].def, escritorio);
    TH_CHECK_EQ(ordenes[2].dir, TH_DIR_NORTH);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(habilitado(TH_GWB_COL_CONFIRMAR), 1);
    // El modo coger se apaga al coger (Object:onClick → stopPickupItems,
    // object.lua:814): el escritorio vuelve a la mano con su orientación y el
    // clic siguiente lo coloca, no pide otro TH_ORD_RECOGER.
    const int k = n_ordenes;
    boton(TH_GWB_COL_COGER, TH_UI_LEFT, 1);
    TH_CHECK_EQ(g->coger, 1);
    casilla(58, 51, TH_UI_LEFT, 1);
    TH_CHECK_EQ(ordenes[k].kind, TH_ORD_RECOGER);
    TH_CHECK_EQ(g->coger, 0);
    TH_CHECK_EQ(g->en_mano, escritorio);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(habilitado(TH_GWB_COL_CONFIRMAR), 0); // vuelve a faltar uno
    casilla(58, 51, TH_UI_LEFT, 1);
    TH_CHECK_EQ(n_ordenes, k + 2);
    TH_CHECK_EQ(ordenes[k + 1].kind, TH_ORD_COLOCAR);
    TH_CHECK_EQ(ordenes[k + 1].def, escritorio);
    // Pulsar una fila también apaga coger.
    fotograma(px, py, -1, 0, 0, 1);
    boton(TH_GWB_COL_COGER, TH_UI_LEFT, 1);
    casilla(58, 52, TH_UI_LEFT, 1); // la silla vuelve a la lista
    boton(TH_GWB_COL_COGER, TH_UI_LEFT, 1);
    TH_CHECK_EQ(g->coger, 1);
    boton(TH_GWB_COL_FILA + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(g->coger, 0);
    // Atrás hasta salir de la obra (puerta, paredes y fuera).
    for (int i = 0; i < 4 && th_game_ui_active(g) == TH_GW_OBRA; i++) {
        boton(TH_GWB_COL_CANCELAR, TH_UI_LEFT, 1);
        fotograma(W / 2, H / 2, -1, 0, 0, 1);
    }
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
}

// La consulta con los cuatro objetos de su tienda: 7 filas en la lista y 6 a
// la vista. Elegir la última a la vista asoma la séptima, que se puede elegir
// con su botón (antes solo salía al agotarse las de arriba).
static void test_lista_de_siete(void)
{
    obra_en_objetos();
    TH_CHECK_EQ(boton(TH_GWB_COL_COMPRAR, TH_UI_LEFT, 1), 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_AMUEBLAR);
    TH_CHECK_EQ(g->n_obj, 4);
    for (int i = 0; i < 4; i++)
        boton(TH_GWB_MAS + i, TH_UI_LEFT, 1);
    boton(TH_GWB_AMU_CONFIRMAR, TH_UI_LEFT, 1);
    for (int i = 0; i < 6; i++)
        fotograma(W / 2, H / 2, -1, 0, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
    TH_CHECK_EQ(sim.place.n, 7);
    TH_CHECK_EQ(g->col_total, 7);
    TH_CHECK_EQ(g->col_n, GW_COL_FILAS);
    TH_CHECK_EQ(g->col_primera, 0);
    boton(TH_GWB_COL_FILA + GW_COL_FILAS - 1, TH_UI_LEFT, 1);
    fotograma(W / 2, H / 2, -1, 0, 0, 1);
    TH_CHECK_EQ(g->col_primera, 1);
    boton(TH_GWB_COL_FILA + GW_COL_FILAS - 1, TH_UI_LEFT, 1);
    TH_CHECK_EQ(g->en_mano, sim.place.item[g->col_orden[6]].def);
    dibujo_determinista("obra con siete filas");
    fotograma(W / 2, H / 2, -1, 0, 1, 1); // Escape: la abandona
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
}

// El derecho sobre un objeto de pasillo con construir, amueblar o contratar
// abiertas lo coge, como sin ventana (Object:onClick, object.lua:760-823; la
// UIPlaceObjects nueva cierra la «main», ui.lua:1048-1054). El banco de
// test_recoger_doble sigue en (58, 72); la cámara lo deja a la izquierda,
// fuera de las tres ventanas.
static void test_recoger_con_ventana(void)
{
    camara(58, 72);
    vista.x += 130;
    int px, py;
    pixel(58, 72, &px, &py);
    static const int paneles[3] = {TH_PI_SALAS, TH_PI_OBJETOS, TH_PI_CONTRATAR};
    for (int i = 0; i < 3; i++) {
        n_ordenes = 0;
        fotograma(W / 2, H / 2, -1, paneles[i], 0, 1);
        TH_CHECK(th_game_ui_active(g) != TH_GW_NONE);
        casilla(58, 72, TH_UI_RIGHT, 1);
        TH_CHECK_EQ(n_ordenes, 1);
        TH_CHECK_EQ(ordenes[0].kind, TH_ORD_RECOGER);
        fotograma(px, py, -1, 0, 0, 1);
        TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR);
        TH_CHECK(out.n_on_top >= 1); // el banco en la mano
        fotograma(px, py, -1, 0, 1, 1); // Escape: deshacer, vuelve a su sitio
        TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    }
}

// En pausa no se pinta lo que va en la mano: ni el objeto ni su huella
// (UIPlaceObjects:draw, place_objects.lua:600-601, y clearBlueprint en
// 1092-1104), ni la persona (UIPlaceStaff:draw, place_staff.lua:127-128), y
// el plano ni empieza ni se mueve (edit_room.lua:1135 y 1502-1510).
static void test_pausa_sin_fantasmas(void)
{
    camara(60, 68);
    int px, py;
    pixel(58, 70, &px, &py);
    fotograma(W / 2, H / 2, -1, TH_PI_OBJETOS, 0, 1);
    boton(TH_GWB_OBJETO + 1, TH_UI_LEFT, 1);
    boton(TH_GWB_AMU_CONFIRMAR, TH_UI_LEFT, 1);
    fotograma(px, py, -1, 0, 0, 1);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR);
    TH_CHECK(out.n_on_top >= 1);
    TH_CHECK(out.overlays != NULL);
    pausa = 1;
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(out.n_on_top, 0);
    TH_CHECK(out.overlays == NULL);
    pausa = 0;
    fotograma(px, py, -1, 0, 1, 1); // Escape: deshacer
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    // Colocar personal.
    camara(60, 58);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    boton(TH_GWB_PESTANA + 2, TH_UI_LEFT, 1);
    boton(TH_GWB_CONTRATAR, TH_UI_LEFT, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    pixel(59, 56, &px, &py);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(out.n_on_top, 1);
    pausa = 1;
    n_ordenes = 0;
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(out.n_on_top, 0);
    casilla(59, 56, TH_UI_LEFT, 1); // en pausa no se suelta
    TH_CHECK_EQ(n_ordenes, 0);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR_PERSONAL);
    pausa = 0;
    fotograma(px, py, -1, 0, 1, 1); // Escape: devolver
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    // El plano.
    camara(58, 52);
    fotograma(W / 2, H / 2, -1, TH_PI_SALAS, 0, 1);
    boton(TH_GWB_CATEGORIA + 0, TH_UI_LEFT, 1);
    boton(TH_GWB_SALA + 0, TH_UI_LEFT, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_OBRA);
    pausa = 1;
    arrastrar(57, 50, 60, 53);
    TH_CHECK_EQ(g->pw, 0);
    TH_CHECK_EQ(g->n_ents, 0);
    pausa = 0;
    fotograma(W / 2, H / 2, -1, 0, 1, 1); // Escape
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
}

// La vista previa con el mismo objeto en la mano en la misma casilla no se
// repite por pasar horas: solo cuando vuelve una orden (lo construido puede
// haber cambiado). Antes la clave llevaba s->hours y a ×2 o más se pagaba el
// bloqueo de zonas (unos 15 ms en la N64) en casi cada fotograma.
static void test_previa_sin_horas(void)
{
    camara(60, 68);
    int px, py;
    pixel(58, 70, &px, &py);
    fotograma(W / 2, H / 2, -1, TH_PI_OBJETOS, 0, 1);
    boton(TH_GWB_OBJETO + 1, TH_UI_LEFT, 1);
    boton(TH_GWB_OBJETO + 1, TH_UI_LEFT, 1);
    boton(TH_GWB_AMU_CONFIRMAR, TH_UI_LEFT, 1);
    fotograma(px, py, -1, 0, 0, 1);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_COLOCAR);
    const uint32_t antes = g->previa.calculos;
    for (int i = 0; i < 10; i++) {
        th_sim_tick(&sim);
        fotograma(px, py, -1, 0, 0, 1);
    }
    TH_CHECK_EQ(g->previa.calculos, antes);
    // Una orden que vuelve (un banco colocado en otra casilla) obliga a
    // calcularla otra vez.
    casilla(56, 70, TH_UI_LEFT, 1);
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(g->previa.calculos, antes + 2); // (56, 70) y otra vez (58, 70)
    fotograma(px, py, -1, 0, 0, 1);
    TH_CHECK_EQ(g->previa.calculos, antes + 2);
    fotograma(px, py, -1, 0, 1, 1); // Escape: deshacer (el colocado se queda)
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
}

// wrong2.wav donde CorsixTH lo toca: anterior o siguiente en el tope o sin
// categoría (moveBy, hire_staff.lua:231-247); y en amueblar, «+» con 99
// (purchaseItem → cannot_buy, furnish_corridor.lua:103-106 y 140-141).
static void test_sonidos(void)
{
    camara(60, 58);
    fotograma(W / 2, H / 2, -1, TH_PI_CONTRATAR, 0, 1);
    boton(TH_GWB_SIGUIENTE, TH_UI_LEFT, 1);
    TH_CHECK_EQ(out.reject_sound, 1);
    boton(TH_GWB_PESTANA + 0, TH_UI_LEFT, 1); // los médicos
    TH_CHECK_EQ(out.reject_sound, 0);
    boton(TH_GWB_ANTERIOR, TH_UI_LEFT, 1);
    TH_CHECK_EQ(out.reject_sound, 1);
    boton(TH_GWB_SIGUIENTE, TH_UI_LEFT, 1);
    TH_CHECK_EQ(out.reject_sound, 0);
    TH_CHECK_EQ(g->idx, 1);
    fotograma(W / 2, H / 2, -1, 0, 1, 1); // Escape
    // 99 de lo más barato, y uno más.
    fotograma(W / 2, H / 2, -1, TH_PI_OBJETOS, 0, 1);
    int barato = 0;
    for (int i = 1; i < g->n_obj; i++)
        if (th_build_object_cost(&sim, g->obj_def[i]) < th_build_object_cost(&sim, g->obj_def[barato]))
            barato = i;
    TH_CHECK(99 * th_build_object_cost(&sim, g->obj_def[barato]) <= sim.hosp.balance);
    for (int i = 0; i < 99; i++)
        boton(TH_GWB_MAS + barato, TH_UI_LEFT, 1);
    TH_CHECK_EQ(g->obj_qty[barato], 99);
    TH_CHECK_EQ(out.reject_sound, 0);
    boton(TH_GWB_MAS + barato, TH_UI_LEFT, 1);
    TH_CHECK_EQ(out.reject_sound, 1);
    TH_CHECK(out.message && !strcmp(out.message, th_text(&textos, "adviser.warnings.cannot_buy")));
    TH_CHECK(out.message && strcmp(out.message, "adviser.warnings.cannot_buy") != 0); // el texto existe
    fotograma(W / 2, H / 2, -1, 0, 1, 1); // Escape: no se compra nada
    TH_CHECK_EQ(th_game_ui_active(g), TH_GW_NONE);
    // Alguien encima del objeto: el núcleo da PERSONA y CorsixTH no dice nada
    // (place_objects.lua:516-520).
    th_game_ui_close_all(g);
    const th_order o = {.kind = TH_ORD_COLOCAR, .x = 62, .y = 72, .def = objeto("bench")};
    const th_order_result r = {.reason = TH_REASON_PERSONA};
    th_game_ui_order_result(g, &sim, &o, &r);
    fotograma(W / 2, H / 2, -1, 0, 0, 1);
    TH_CHECK(out.message == NULL);
    TH_CHECK_EQ(out.reject_sound, 0);
}

// La puerta se pone al bajar el botón (UIEditRoom:onMouseDown →
// onLeftButtonDown, edit_room.lua:1076-1104 y 1134-1145), no al subirlo: con
// A mantenido y el stick moviéndose, va donde se pulsó.
static void test_puerta_al_bajar(void)
{
    camara(58, 52);
    fotograma(W / 2, H / 2, -1, TH_PI_SALAS, 0, 1);
    boton(TH_GWB_CATEGORIA + 0, TH_UI_LEFT, 1);
    boton(TH_GWB_SALA + 0, TH_UI_LEFT, 1);
    arrastrar(57, 50, 60, 53);
    TH_CHECK_EQ(boton(TH_GWB_COL_CONFIRMAR, TH_UI_LEFT, 1), 1);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    fotograma(W / 2, H / 2, -1, 0, 0, 1);
    int px, py;
    pixel(58, 54, &px, &py);
    th_ui_stack_mouse_move(&pila, px, py);
    th_ui_stack_mouse_down(&pila, TH_UI_LEFT, px, py);
    th_game_ui_input in = {.px = px, .py = py, .id = -1, .sim_idle = 1, .world_down = 1 << TH_UI_LEFT};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    TH_CHECK_EQ(out.n_orders, 1);
    TH_CHECK_EQ(out.orders[0].kind, TH_ORD_PUERTA);
    TH_CHECK_EQ(out.orders[0].x, 58);
    TH_CHECK_EQ(out.orders[0].y, 53);
    const th_order puerta = out.orders[0];
    pixel(59, 54, &px, &py); // se suelta en otra casilla: nada más
    th_ui_stack_mouse_move(&pila, px, py);
    th_ui_stack_mouse_up(&pila, TH_UI_LEFT, px, py);
    in = (th_game_ui_input){.px = px, .py = py, .id = -1, .sim_idle = 1, .world_up = 1 << TH_UI_LEFT};
    th_game_ui_frame(g, &in, &sim, &vista, 0, &out);
    TH_CHECK_EQ(out.n_orders, 0);
    th_game_ui_order_dropped(g, &puerta);
    fotograma(W / 2, H / 2, -1, 0, 1, 1); // Escape: la abandona
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
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
        test_contratar_rechazado();
        test_obra_en_vuelo();
        test_soltar_sobre_ventana();
        test_recoger_doble();
        test_plano_malo_sin_confirmar();
        test_panel_cierra();
        test_tienda_y_obra_cerrada();
        test_panel_con_persona_en_mano();
        test_contratar_tras_rechazo();
        test_pausa_larga_con_orden_en_vuelo();
        test_plano_grande_con_ventanas();
        test_contratado_otro_en_mano();
        test_pausa_derecho();
        test_panel_dos_veces();
        test_confirmar_y_lista();
        test_lista_de_siete();
        test_recoger_con_ventana();
        test_pausa_sin_fantasmas();
        test_previa_sin_horas();
        test_sonidos();
        test_puerta_al_bajar();
    }
    for (int i = 0; i < n_datos; i++)
        free(datos[i]);
    printf("pruebas de ventanas: %d comprobaciones, %d fallos\n", th_test_checks, th_test_failures);
    return th_test_failures ? 1 : 0;
}
