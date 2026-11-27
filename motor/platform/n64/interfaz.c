#include "interfaz.h"

#include <string.h>

#include "graficos.h"
#include "puntero.h"
#include "th/rules.h"

// Las hojas de la interfaz, en baja resolución.
static hoja_t hojas[TH_SHEETS];
static const hoja_t *hojas_rdp[TH_SHEETS];
static th_sheet sheets[TH_SHEETS];
static const th_sheet *sheets_ref[TH_SHEETS];
static const struct {
    int indice;
    const char *ruta;
} rutas[] = {
    {TH_SHEET_PANEL, "rom:/datam/panel02m.tsh"},       {TH_SHEET_POINTER, "rom:/datam/mpointer.tsh"},
    {TH_SHEET_FONT_MONEY, "rom:/fuentes/font100m.tsh"}, {TH_SHEET_FONT_DATE, "rom:/fuentes/font101m.tsh"},
    {TH_SHEET_FONT_WHITE, "rom:/fuentes/font132m.tsh"}, {TH_SHEET_FONT_PAUSE, "rom:/fuentes/font131m.tsh"},
    {TH_SHEET_FONT_TOOLTIP, "rom:/fuentes/font130m.tsh"},
    // Las piezas de las ventanas de 320 × 200 (motor/ui/mutton01.h). El
    // conversor la deja solo con las piezas 1-78, las de la demo (PIEZAS_USADAS
    // de tools/conv/conv.py): 109.280 B de RAM en lugar de 270.840 (la hoja se
    // descomprime de una vez).
    {TH_SHEET_MUTTON, "rom:/datam/mutton01.tsh"},
};

static th_ui_fonts fuentes;
static th_rules textos;
static th_bottom_panel panel;
// La pila de ventanas (UI.windows): el panel inferior, abajo del todo, y
// encima las que abran las ventanas de la partida (interfaz_pila).
static th_ui_stack pila;
static uint32_t ultimo_ms; // get_ticks_ms del último interfaz_mandos
static th_pointer puntero;
static int ancho_pantalla, alto_pantalla;
static bool izquierdo, derecho; // botones del ratón (A y B) abajo
static int quieto;              // fotogramas con el puntero sobre algo con descripción
static bool forzar_descripcion;

enum {
    PUNTERO_NORMAL = 1,       // cursors_name.default
    FOTOGRAMAS_DESCRIPCION = 16, // tooltip_ticks de CorsixTH: 30 ticks de 18 ms, unos 0,54 s
};

static th_draw elementos[512];
static th_drawlist lista = {.items = elementos, .capacity = 512};

// Los mandos: libdragon los lee en cada barrido y joypad_poll solo copia la
// última lectura. El ratón de N64 da el movimiento desde la lectura anterior,
// así que leyendo a 30 fps se perdería la mitad: se leen en cada barrido y se
// acumula el movimiento (docs/04_construccion.md, «Mandos y ratón»).
static volatile int raton_dx, raton_dy;
static volatile joypad_inputs_t ultimo;
static volatile bool es_raton;

static void en_cada_barrido(void *ctx)
{
    (void)ctx;
    joypad_poll();
    joypad_inputs_t in = joypad_get_inputs(JOYPAD_PORT_1);
    const bool raton = joypad_get_style(JOYPAD_PORT_1) == JOYPAD_STYLE_MOUSE;
    if (raton) {
        raton_dx += in.stick_x;
        raton_dy -= in.stick_y;
    }
    es_raton = raton;
    ultimo = in;
}

static void cargar_textos(const char *ruta)
{
    int tam;
    void *datos = asset_load(ruta, &tam);
    if (!datos || th_rules_init(&textos, datos, (size_t)tam) != 0)
        memset(&textos, 0, sizeof textos);
}

void interfaz_mandos_iniciar(void)
{
    static bool instalado;
    if (!instalado)
        vi_install_vblank_handler(en_cada_barrido, NULL);
    instalado = true;
}

void interfaz_leer(joypad_inputs_t *mando)
{
    disable_interrupts();
    *mando = *(const joypad_inputs_t *)&ultimo;
    enable_interrupts();
}

void interfaz_puntero(int *x, int *y)
{
    *x = th_pointer_x(&puntero);
    *y = th_pointer_y(&puntero);
}

// El draw del panel en la pila: sus paneles y sus textos, con la información
// de la partida que pasa interfaz_lista.
static int dibujar_panel(const th_ui_window *w, th_drawlist *out, void *ctx)
{
    (void)w;
    return th_bottom_panel_draw(&panel, &fuentes, (const th_bottom_info *)ctx, &textos, out);
}

bool interfaz_cargar(int ancho, int alto, bool mandos)
{
    static bool cargada;
    if (mandos)
        interfaz_mandos_iniciar();
    if (cargada)
        return true;
    for (unsigned i = 0; i < sizeof rutas / sizeof rutas[0]; i++) {
        const int k = rutas[i].indice;
        if (!hoja_cargar(&hojas[k], rutas[i].ruta) ||
            th_sheet_init(&sheets[k], hojas[k].bloque, (size_t)hojas[k].tam) != 0)
            return false;
        hojas_rdp[k] = &hojas[k];
        sheets_ref[k] = &sheets[k];
    }
    cargar_textos("rom:/textos/english.trg");
    fuentes = (th_ui_fonts){
        .money = {&sheets[TH_SHEET_FONT_MONEY], TH_SHEET_FONT_MONEY, 0, 0},
        .date = {&sheets[TH_SHEET_FONT_DATE], TH_SHEET_FONT_DATE, 0, 0, 2},
        .white = {&sheets[TH_SHEET_FONT_WHITE], TH_SHEET_FONT_WHITE, 0, -1},
        .pause = {&sheets[TH_SHEET_FONT_PAUSE], TH_SHEET_FONT_PAUSE, 0, 0},
        .tooltip = {&sheets[TH_SHEET_FONT_TOOLTIP], TH_SHEET_FONT_TOOLTIP, 0, 0},
    };
    ancho_pantalla = ancho;
    alto_pantalla = alto;
    th_bottom_panel_init(&panel, 0, ancho, alto);
    panel.win.draw = dibujar_panel; // draw_ctx: el th_bottom_info de interfaz_lista
    th_ui_stack_init(&pila);
    th_ui_stack_screen(&pila, ancho, alto);
    th_ui_stack_open(&pila, &panel.win, NULL);
    th_pointer_init(&puntero, ancho, alto);
    cargada = true;
    return true;
}

const th_sheet *const *interfaz_hojas(void)
{
    return sheets_ref;
}

th_ui_stack *interfaz_pila(void)
{
    return &pila;
}

const th_ui_fonts *interfaz_fuentes(void)
{
    return &fuentes;
}

const th_rules *interfaz_textos(void)
{
    return &textos;
}

// UI:onMouseUp con la pila: el botón del panel (TH_PI_*), el de otra ventana
// o, si ninguna lo toma, el mundo.
static void boton_soltado(interfaz_orden_t *o, int boton)
{
    const th_ui_event e = th_ui_stack_mouse_up(&pila, boton, th_pointer_x(&puntero), th_pointer_y(&puntero));
    if (!e.win) {
        o->mundo_arriba |= 1 << boton;
    } else if (e.id >= 0) {
        if (e.win == &panel.win) {
            if (e.id > 0)
                o->boton = e.id;
        } else if (!o->ventana) {
            o->ventana = e.win;
            o->id = e.id;
            o->raton = boton;
        }
    }
}

static void boton_bajado(interfaz_orden_t *o, int boton)
{
    if (!th_ui_stack_mouse_down(&pila, boton, th_pointer_x(&puntero), th_pointer_y(&puntero)).win)
        o->mundo_abajo |= 1 << boton;
}

interfaz_orden_t interfaz_mandos(joypad_inputs_t *mando)
{
    interfaz_orden_t o = {.id = -1};
    disable_interrupts();
    const joypad_inputs_t in = *(const joypad_inputs_t *)&ultimo;
    const int dx = raton_dx, dy = raton_dy;
    const bool raton = es_raton;
    raton_dx = raton_dy = 0;
    enable_interrupts();
    *mando = in;
    if (raton)
        th_pointer_move(&puntero, dx, dy);
    else
        th_pointer_stick(&puntero, in.stick_x, in.stick_y, in.btn.z);
    const int x = th_pointer_x(&puntero), y = th_pointer_y(&puntero);
    // El panel tiene además el hover de sus botones de gestión; th_ui_mouse_move
    // otra vez en la misma posición no cambia nada.
    th_bottom_panel_mouse_move(&panel, x, y);
    o.sobre = th_ui_stack_mouse_move(&pila, x, y);
    // A y B: los botones izquierdo y derecho, al bajar y al subir.
    if (in.btn.a && !izquierdo)
        boton_bajado(&o, TH_UI_LEFT);
    if (!in.btn.a && izquierdo)
        boton_soltado(&o, TH_UI_LEFT);
    if (in.btn.b && !derecho)
        boton_bajado(&o, TH_UI_RIGHT);
    if (!in.btn.b && derecho)
        boton_soltado(&o, TH_UI_RIGHT);
    izquierdo = in.btn.a;
    derecho = in.btn.b;
    // Los ticks de 18 ms de la interfaz (botones repetidos), por el tiempo
    // pasado; tras una pausa larga (el menú) no se recuperan más de 100 ms.
    const uint32_t ahora = get_ticks_ms();
    uint32_t ms = ahora - ultimo_ms;
    if (!ultimo_ms || ms > 100)
        ms = 100;
    ultimo_ms = ahora;
    th_ui_stack_update(&pila, (int)ms);
    th_pointer_edge_scroll(&puntero, &o.vista_dx, &o.vista_dy);
    // UI:updateTooltip: con el botón izquierdo abajo no hay descripciones, y
    // fuera de todo lo que tiene una la cuenta vuelve a empezar; al pasar de
    // un botón a otro, la nueva sale sin esperar.
    int tx, ty;
    if (izquierdo || !th_ui_stack_tooltip_at(&pila, x, y, &tx, &ty))
        quieto = 0;
    else if (quieto < FOTOGRAMAS_DESCRIPCION)
        quieto++;
    return o;
}

void interfaz_estado(int x, int y, bool pulsado, bool descripcion)
{
    puntero.x = (int32_t)x << 4;
    puntero.y = (int32_t)y << 4;
    th_bottom_panel_mouse_move(&panel, x, y);
    th_ui_stack_mouse_move(&pila, x, y);
    // Por la pila, como interfaz_mandos; al soltar, fuera de todo, para que
    // no se pulse nada.
    if (pulsado && !izquierdo)
        th_ui_stack_mouse_down(&pila, TH_UI_LEFT, x, y);
    else if (!pulsado && izquierdo)
        th_ui_stack_mouse_up(&pila, TH_UI_LEFT, -100, -100);
    izquierdo = pulsado;
    forzar_descripcion = descripcion;
}

int interfaz_lista(const th_bottom_info *info, th_drawlist *out)
{
    // De abajo arriba: el panel (con sus textos) y las demás ventanas.
    panel.win.draw_ctx = (void *)info;
    int r = th_ui_stack_draw(&pila, out);
    const int x = th_pointer_x(&puntero), y = th_pointer_y(&puntero);
    char texto[256];
    int tx, ty;
    if (quieto >= FOTOGRAMAS_DESCRIPCION || forzar_descripcion) {
        const th_ui_window *sobre = NULL;
        for (int i = 0; i < pila.n && !sobre; i++)
            if (th_ui_hit(pila.win[i], x, y))
                sobre = pila.win[i];
        const char *t;
        if (sobre == &panel.win) {
            if (th_bottom_panel_tooltip(&panel, x, y, info, &textos, texto, sizeof texto, &tx, &ty))
                th_font_tooltip(&fuentes.tooltip, texto, tx, ty, th_bottom_panel_tooltip_width(&panel),
                                ancho_pantalla, out);
        } else if (sobre && (t = th_ui_tooltip_at(sobre, x, y, &tx, &ty)) != NULL) {
            // Las demás ventanas: la ruta en los textos, con el ancho del panel.
            th_font_tooltip(&fuentes.tooltip, th_text(&textos, t), tx, ty, th_bottom_panel_tooltip_width(&panel),
                            ancho_pantalla, out);
        }
    }
    if (out->count >= out->capacity)
        return -1;
    out->items[out->count++] =
        (th_draw){.sprite = PUNTERO_NORMAL, .sheet = TH_SHEET_POINTER, .x = (int16_t)x, .y = (int16_t)y};
    return r;
}

void interfaz_dibujar(const th_bottom_info *info, const uint16_t *paleta, mapa_n64_stats_t *st)
{
    lista.count = lista.floor_count = lista.redraws = 0;
    interfaz_lista(info, &lista);
    mapa_n64_dibujar(&lista, hojas_rdp, paleta, ancho_pantalla, alto_pantalla, st);
}
