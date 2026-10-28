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
};

static th_ui_fonts fuentes;
static th_rules textos;
static th_bottom_panel panel;
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
    th_pointer_init(&puntero, ancho, alto);
    cargada = true;
    return true;
}

const th_sheet *const *interfaz_hojas(void)
{
    return sheets_ref;
}

static int boton_soltado(int boton)
{
    const int id = th_ui_mouse_up(&panel.win, boton, th_pointer_x(&puntero), th_pointer_y(&puntero));
    return id > 0 ? id : 0;
}

interfaz_orden_t interfaz_mandos(joypad_inputs_t *mando)
{
    interfaz_orden_t o = {0};
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
    th_bottom_panel_mouse_move(&panel, x, y);
    // A y B: los botones izquierdo y derecho, al bajar y al subir.
    if (in.btn.a && !izquierdo)
        th_ui_mouse_down(&panel.win, TH_UI_LEFT, x, y);
    if (!in.btn.a && izquierdo)
        o.boton = boton_soltado(TH_UI_LEFT);
    if (in.btn.b && !derecho)
        th_ui_mouse_down(&panel.win, TH_UI_RIGHT, x, y);
    if (!in.btn.b && derecho && !o.boton)
        o.boton = boton_soltado(TH_UI_RIGHT);
    izquierdo = in.btn.a;
    derecho = in.btn.b;
    th_pointer_edge_scroll(&puntero, &o.vista_dx, &o.vista_dy);
    // UI:updateTooltip: con el botón izquierdo abajo no hay descripciones, y
    // fuera de todo lo que tiene una la cuenta vuelve a empezar; al pasar de
    // un botón a otro, la nueva sale sin esperar.
    int tx, ty;
    if (izquierdo || !th_ui_tooltip_at(&panel.win, x, y, &tx, &ty))
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
    if (pulsado)
        th_ui_mouse_down(&panel.win, TH_UI_LEFT, x, y);
    else if (panel.win.active_button >= 0)
        th_ui_mouse_up(&panel.win, TH_UI_LEFT, -100, -100);
    forzar_descripcion = descripcion;
}

void interfaz_lista(const th_bottom_info *info, th_drawlist *out)
{
    th_bottom_panel_draw(&panel, &fuentes, info, &textos, out);
    const int x = th_pointer_x(&puntero), y = th_pointer_y(&puntero);
    char texto[256];
    int tx, ty;
    if ((quieto >= FOTOGRAMAS_DESCRIPCION || forzar_descripcion) &&
        th_bottom_panel_tooltip(&panel, x, y, info, &textos, texto, sizeof texto, &tx, &ty))
        th_font_tooltip(&fuentes.tooltip, texto, tx, ty, th_bottom_panel_tooltip_width(&panel), ancho_pantalla, out);
    if (out->count < out->capacity)
        out->items[out->count++] =
            (th_draw){.sprite = PUNTERO_NORMAL, .sheet = TH_SHEET_POINTER, .x = (int16_t)x, .y = (int16_t)y};
}

void interfaz_dibujar(const th_bottom_info *info, const uint16_t *paleta, mapa_n64_stats_t *st)
{
    lista.count = lista.floor_count = lista.redraws = 0;
    interfaz_lista(info, &lista);
    mapa_n64_dibujar(&lista, hojas_rdp, paleta, ancho_pantalla, alto_pantalla, st);
}
