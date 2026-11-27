// Pruebas del framework de ventanas (ui/ui.c): clic derecho, botones
// repetidos, deshabilitados, la pila de ventanas y que una ventana dibujada
// con el rasterizador de referencia es determinista. Sin datos del juego: una
// hoja sintética. Con TH_TEST_UI_MAIN es un ejecutable (test_ui) que, si se le
// pasa motor/assets, además monta una ventana de amueblar de 10 filas con
// MUTTON01 y FONT132M y cuenta las cargas de textura de su texto.
#include <string.h>

#include "mutton01.h"
#include "raster.h"
#include "texto.h"
#include "th_test.h"
#include "ui.h"

// Una hoja TSH1 sintética de n sprites de w × h, el sprite i del color i.
static uint8_t hoja_datos[16 + 8 * 8 + 8 * 8 * 8];

static void hoja_sintetica(th_sheet *s)
{
    enum { N = 8, W = 8, H = 8 };
    uint8_t *d = hoja_datos;
    memset(d, 0, sizeof hoja_datos);
    memcpy(d, "TSH1", 4);
    d[5] = N;
    d[11] = 16;         // tabla
    d[15] = 16 + 8 * N; // píxeles (big-endian, < 256)
    for (int i = 0; i < N; i++) {
        uint8_t *e = d + 16 + 8 * i;
        e[2] = (uint8_t)((W * H * i) >> 8);
        e[3] = (uint8_t)(W * H * i);
        e[5] = W;
        e[7] = H;
        memset(d + 16 + 8 * N + W * H * i, i == 0 ? TH_TRANSPARENT : i, W * H);
    }
    TH_CHECK_EQ(th_sheet_init(s, d, sizeof hoja_datos), 0);
}

// Una ventana de 100 × 50 en (10, 20) con un panel por botón.
static void ventana(th_ui_window *w, int x, int y)
{
    th_ui_window_init(w, 100, 50);
    w->x = (int16_t)x;
    w->y = (int16_t)y;
}

static int clicks(th_ui_window *w, int *ultimo_id)
{
    th_ui_click c;
    int n = 0;
    while (th_ui_take_click(w, &c)) {
        n++;
        if (ultimo_id)
            *ultimo_id = c.id;
    }
    return n;
}

static void test_derecho(void)
{
    th_ui_window w;
    ventana(&w, 10, 20);
    const int p0 = th_ui_add_panel(&w, TH_SHEET_PANEL, 1, 0, 0);
    const int p1 = th_ui_add_panel(&w, TH_SHEET_PANEL, 3, 20, 0);
    const int b0 = th_ui_make_button(&w, p0, 0, 0, 10, 10, 2, 7);
    const int b1 = th_ui_make_button(&w, p1, 0, 0, 10, 10, 4, 8);
    th_ui_set_rightclick(&w, b1, 108);
    TH_CHECK_EQ(w.buttons[b0].id_right, -1);
    // El derecho no pulsa un botón sin on_rightclick (window.lua:1645), pero la
    // ventana sí lo toma (hitTest).
    TH_CHECK_EQ(th_ui_mouse_down(&w, TH_UI_RIGHT, 15, 25), 1);
    TH_CHECK_EQ(w.active_button, -1);
    TH_CHECK_EQ(w.panels[p0].sprite, 1);
    TH_CHECK_EQ(th_ui_mouse_up(&w, TH_UI_RIGHT, 15, 25), -1);
    // Con on_rightclick: hundido y, al soltar encima, id_right.
    TH_CHECK_EQ(th_ui_mouse_down(&w, TH_UI_RIGHT, 35, 25), 1);
    TH_CHECK_EQ(w.active_button, b1);
    TH_CHECK_EQ(w.panels[p1].sprite, 4);
    TH_CHECK_EQ(th_ui_mouse_up(&w, TH_UI_RIGHT, 35, 25), 108);
    TH_CHECK_EQ(w.panels[p1].sprite, 3);
    // El izquierdo sigue dando id.
    th_ui_mouse_down(&w, TH_UI_LEFT, 35, 25);
    TH_CHECK_EQ(th_ui_mouse_up(&w, TH_UI_LEFT, 35, 25), 8);
    // Pulsado con el izquierdo y soltado con el derecho: el callback del
    // derecho (on_rightclick), que el primero no tiene.
    th_ui_mouse_down(&w, TH_UI_LEFT, 15, 25);
    TH_CHECK_EQ(th_ui_mouse_up(&w, TH_UI_RIGHT, 15, 25), -1);
    // Soltado fuera: nada.
    th_ui_mouse_down(&w, TH_UI_RIGHT, 35, 25);
    TH_CHECK_EQ(th_ui_mouse_up(&w, TH_UI_RIGHT, 0, 0), -1);
    TH_CHECK_EQ(w.active_button, -1);
}

static void test_repetido(void)
{
    th_ui_window w;
    ventana(&w, 0, 0);
    const int p = th_ui_add_panel(&w, TH_SHEET_PANEL, 5, 0, 0);
    const int b = th_ui_make_button(&w, p, 0, 0, 9, 9, 6, 44);
    th_ui_set_repeat(&w, b);
    th_ui_set_rightclick(&w, b, 45);
    // Al bajar, una pulsación; luego 10 ticks de espera y una cada 3
    // (window.lua:1650-1654, 1904-1917).
    th_ui_mouse_down(&w, TH_UI_LEFT, 4, 4);
    int id = 0;
    TH_CHECK_EQ(clicks(&w, &id), 1);
    TH_CHECK_EQ(id, 44);
    int en[64], n = 0;
    for (int t = 1; t <= 30; t++) {
        th_ui_tick(&w, TH_UI_LEFT);
        if (clicks(&w, NULL) && n < 64)
            en[n++] = t;
    }
    TH_CHECK_EQ(n, 7); // 11, 14, 17, 20, 23, 26, 29
    TH_CHECK_EQ(en[0], 11);
    TH_CHECK_EQ(en[1], 14);
    TH_CHECK_EQ(en[6], 29);
    // Fuera del botón no repite; al volver, sí.
    th_ui_mouse_move(&w, 50, 50);
    for (int t = 0; t < 10; t++)
        th_ui_tick(&w, TH_UI_LEFT);
    TH_CHECK_EQ(clicks(&w, NULL), 0);
    th_ui_mouse_move(&w, 4, 4);
    th_ui_tick(&w, TH_UI_LEFT);
    TH_CHECK_EQ(clicks(&w, NULL), 1);
    // Sin el botón del ratón abajo no repite, y al soltar no se pulsa otra vez.
    th_ui_tick(&w, -1);
    th_ui_tick(&w, -1);
    th_ui_tick(&w, -1);
    TH_CHECK_EQ(clicks(&w, NULL), 0);
    TH_CHECK_EQ(th_ui_mouse_up(&w, TH_UI_LEFT, 4, 4), -1);
    TH_CHECK_EQ(clicks(&w, NULL), 0);
    th_ui_tick(&w, TH_UI_LEFT);
    TH_CHECK_EQ(clicks(&w, NULL), 0);
    // Con el derecho, id_right.
    th_ui_mouse_down(&w, TH_UI_RIGHT, 4, 4);
    TH_CHECK_EQ(clicks(&w, &id), 1);
    TH_CHECK_EQ(id, 45);
    th_ui_mouse_up(&w, TH_UI_RIGHT, 4, 4);

    // En la pila, a 30 fotogramas por segundo: 990 ms son 55 ticks de 18 ms, y
    // dan la pulsación de bajar y las de los ticks 11, 14, …, 53 (15).
    th_ui_stack s;
    th_ui_stack_init(&s);
    TH_CHECK_EQ(th_ui_stack_open(&s, &w, NULL), 0);
    TH_CHECK(th_ui_stack_mouse_down(&s, TH_UI_LEFT, 4, 4).win == &w);
    int total = 0, primer_fotograma = 0;
    th_ui_window *de;
    th_ui_click c;
    for (int f = 1; f <= 30; f++) {
        th_ui_stack_update(&s, 33);
        while (th_ui_stack_take_click(&s, &de, &c)) {
            TH_CHECK(de == &w);
            total++;
            if (total == 2)
                primer_fotograma = f;
        }
    }
    TH_CHECK_EQ(total, 1 + 15);       // la de bajar ya estaba en la cola antes del bucle
    TH_CHECK_EQ(primer_fotograma, 6); // la primera repetición, a los 198 ms
    TH_CHECK_EQ(s.ms, 990 - 55 * 18);
    th_ui_stack_mouse_up(&s, TH_UI_LEFT, 4, 4);
    for (int f = 0; f < 10; f++)
        th_ui_stack_update(&s, 33);
    TH_CHECK_EQ(th_ui_stack_take_click(&s, &de, &c), 0);
}

static void test_deshabilitado(void)
{
    th_ui_window w;
    ventana(&w, 0, 0);
    const int p = th_ui_add_panel(&w, TH_SHEET_PANEL, 10, 0, 0);
    const int b = th_ui_make_button(&w, p, 0, 0, 20, 20, 11, 3);
    TH_CHECK_EQ(w.buttons[b].sprite_disabled, 10); // por defecto, el normal
    th_ui_set_disabled_sprite(&w, b, 12);
    th_ui_enable(&w, b, 0);
    TH_CHECK_EQ(w.panels[p].sprite, 12);
    th_ui_mouse_down(&w, TH_UI_LEFT, 5, 5);
    TH_CHECK_EQ(w.active_button, -1);
    TH_CHECK_EQ(w.panels[p].sprite, 12);
    TH_CHECK_EQ(th_ui_mouse_up(&w, TH_UI_LEFT, 5, 5), -1);
    th_ui_enable(&w, b, 0); // otra vez: no cambia nada
    TH_CHECK_EQ(w.panels[p].sprite, 12);
    th_ui_enable(&w, b, 1);
    TH_CHECK_EQ(w.panels[p].sprite, 10);
    th_ui_mouse_down(&w, TH_UI_LEFT, 5, 5);
    TH_CHECK_EQ(w.panels[p].sprite, 11);
    // Deshabilitado mientras se pulsa: al soltar no se pulsa.
    th_ui_enable(&w, b, 0);
    TH_CHECK_EQ(th_ui_mouse_up(&w, TH_UI_LEFT, 5, 5), -1);
    // Los paneles de color.
    const int f = th_ui_add_fill(&w, 3, 4, 30, 5, 0x1234);
    TH_CHECK(f >= 0);
    th_draw it[4];
    th_drawlist l = {.items = it, .capacity = 4};
    TH_CHECK_EQ(th_ui_window_draw(&w, &l), 0);
    TH_CHECK_EQ(l.count, 2);
    TH_CHECK_EQ(it[1].sheet, TH_SHEET_FILL);
    TH_CHECK_EQ(it[1].sprite, 0x1234);
    TH_CHECK_EQ(it[1].clip_x1, 30);
    TH_CHECK_EQ(it[1].clip_y1, 5);
    // Los límites: 64 paneles y 40 botones.
    th_ui_window g;
    th_ui_window_init(&g, 10, 10);
    int ultimo = 0;
    for (int i = 0; i < TH_UI_PANELS; i++)
        ultimo = th_ui_add_panel(&g, TH_SHEET_PANEL, 1, 0, 0);
    TH_CHECK_EQ(ultimo, 63);
    TH_CHECK_EQ(th_ui_add_panel(&g, TH_SHEET_PANEL, 1, 0, 0), -1);
    for (int i = 0; i < TH_UI_BUTTONS; i++)
        ultimo = th_ui_make_button(&g, i, 0, 0, 1, 1, 1, i);
    TH_CHECK_EQ(ultimo, 39);
    TH_CHECK_EQ(th_ui_make_button(&g, 0, 0, 0, 1, 1, 1, 0), -1);
}

static void test_pila(void)
{
    // a (main) y b solapadas; c (main) aparte; d sin clase.
    static th_ui_window a, b, c, d, e;
    ventana(&a, 0, 0);
    ventana(&b, 50, 0);
    ventana(&c, 200, 0);
    ventana(&d, 0, 100);
    ventana(&e, 0, 200);
    a.modal_class = TH_UI_MAIN;
    c.modal_class = TH_UI_MAIN;
    const int pa = th_ui_add_panel(&a, TH_SHEET_PANEL, 1, 60, 0);
    th_ui_make_button(&a, pa, 0, 0, 10, 10, 2, 1);
    const int pb = th_ui_add_panel(&b, TH_SHEET_PANEL, 1, 10, 0);
    th_ui_make_button(&b, pb, 0, 0, 10, 10, 2, 2);

    th_ui_stack s;
    th_ui_stack_init(&s);
    th_ui_window *cerrada = &e;
    TH_CHECK_EQ(th_ui_stack_open(&s, &d, &cerrada), 0);
    TH_CHECK(cerrada == NULL);
    TH_CHECK_EQ(th_ui_stack_open(&s, &a, NULL), 0);
    TH_CHECK_EQ(th_ui_stack_open(&s, &b, NULL), 0);
    TH_CHECK_EQ(s.n, 3);
    TH_CHECK(s.win[0] == &b && s.win[1] == &a && s.win[2] == &d);
    TH_CHECK(th_ui_stack_modal(&s, TH_UI_MAIN) == &a);

    // (60..70, 0..10) está en el botón de a y en el de b (50 + 10): gana b,
    // la de arriba.
    th_ui_event ev = th_ui_stack_mouse_down(&s, TH_UI_LEFT, 65, 5);
    TH_CHECK(ev.win == &b);
    TH_CHECK_EQ(a.active_button, -1);
    TH_CHECK_EQ(b.active_button, 0);
    // Soltado fuera de todo: lo suelta la que tenía el botón (sin pulsarlo) y
    // no llega al mundo.
    ev = th_ui_stack_mouse_up(&s, TH_UI_LEFT, 500, 500);
    TH_CHECK(ev.win == &b);
    TH_CHECK_EQ(ev.id, -1);
    // Pulsado y soltado encima: el id.
    th_ui_stack_mouse_down(&s, TH_UI_LEFT, 65, 5);
    ev = th_ui_stack_mouse_up(&s, TH_UI_LEFT, 65, 5);
    TH_CHECK(ev.win == &b);
    TH_CHECK_EQ(ev.id, 2);
    // Sobre a sin b encima (x < 50): a lo toma aunque no sea un botón, y sube
    // arriba (Window:bringToTop, window.lua:1683-1685).
    ev = th_ui_stack_mouse_down(&s, TH_UI_LEFT, 20, 20);
    TH_CHECK(ev.win == &a);
    TH_CHECK(s.win[0] == &a && s.win[1] == &b && s.win[2] == &d);
    ev = th_ui_stack_mouse_up(&s, TH_UI_LEFT, 20, 20);
    TH_CHECK(ev.win == &a);
    TH_CHECK_EQ(ev.id, -1);
    // Ahora (60..70, 0..10) es de a, que está arriba.
    TH_CHECK(th_ui_stack_mouse_move(&s, 62, 5) == &a);
    // Y b vuelve arriba con un clic donde a no llega (x ≥ 100).
    ev = th_ui_stack_mouse_down(&s, TH_UI_LEFT, 120, 20);
    TH_CHECK(ev.win == &b);
    TH_CHECK(s.win[0] == &b && s.win[1] == &a && s.win[2] == &d);
    th_ui_stack_mouse_up(&s, TH_UI_LEFT, 120, 20);
    // La de arriba no se mueve, y lo que llega al mundo no cambia el orden.
    th_ui_stack_mouse_down(&s, TH_UI_LEFT, 120, 20);
    th_ui_stack_mouse_up(&s, TH_UI_LEFT, 120, 20);
    th_ui_stack_mouse_down(&s, TH_UI_LEFT, 300, 300);
    th_ui_stack_mouse_up(&s, TH_UI_LEFT, 300, 300);
    TH_CHECK(s.win[0] == &b && s.win[1] == &a && s.win[2] == &d);
    // Fuera de todas: al mundo.
    ev = th_ui_stack_mouse_down(&s, TH_UI_RIGHT, 300, 300);
    TH_CHECK(ev.win == NULL);
    ev = th_ui_stack_mouse_up(&s, TH_UI_RIGHT, 300, 300);
    TH_CHECK(ev.win == NULL);
    TH_CHECK_EQ(ev.id, -1);
    TH_CHECK(th_ui_stack_mouse_move(&s, 300, 300) == NULL);
    TH_CHECK(th_ui_stack_mouse_move(&s, 60, 10) == &b);
    TH_CHECK(th_ui_stack_mouse_move(&s, 10, 110) == &d);
    // Una ventana invisible no toma nada.
    d.visible = 0;
    TH_CHECK(th_ui_stack_mouse_move(&s, 10, 110) == NULL);
    TH_CHECK(th_ui_stack_mouse_down(&s, TH_UI_LEFT, 10, 110).win == NULL);
    th_ui_stack_mouse_up(&s, TH_UI_LEFT, 10, 110);
    d.visible = 1;

    // Abrir c (main) cierra a, con su botón pulsado suelto; b y d siguen.
    th_ui_stack_mouse_down(&s, TH_UI_LEFT, 62, 5); // b
    a.active_button = 0;                           // a con un botón pulsado, para ver que se suelta
    a.panels[pa].sprite = 2;
    TH_CHECK_EQ(th_ui_stack_open(&s, &c, &cerrada), 0);
    TH_CHECK(cerrada == &a);
    TH_CHECK_EQ(a.active_button, -1);
    TH_CHECK_EQ(a.panels[pa].sprite, 1);
    TH_CHECK_EQ(s.n, 3);
    TH_CHECK(s.win[0] == &c && s.win[1] == &b && s.win[2] == &d);
    TH_CHECK(th_ui_stack_modal(&s, TH_UI_MAIN) == &c);
    th_ui_stack_mouse_up(&s, TH_UI_LEFT, 62, 5);
    // Reabrir una que ya está solo la sube.
    TH_CHECK_EQ(th_ui_stack_open(&s, &d, &cerrada), 0);
    TH_CHECK(cerrada == NULL);
    TH_CHECK(s.win[0] == &d && s.n == 3);
    // Llena: 4 como mucho.
    TH_CHECK_EQ(th_ui_stack_open(&s, &e, NULL), 0);
    TH_CHECK_EQ(th_ui_stack_open(&s, &a, &cerrada), 0); // main: cierra c y cabe
    TH_CHECK(cerrada == &c);
    static th_ui_window f;
    ventana(&f, 0, 0);
    TH_CHECK_EQ(th_ui_stack_open(&s, &f, NULL), -1);
    // Cerrar.
    TH_CHECK_EQ(th_ui_stack_close(&s, &a), 1);
    TH_CHECK_EQ(th_ui_stack_close(&s, &a), 0);
    TH_CHECK(th_ui_stack_modal(&s, TH_UI_MAIN) == NULL);
    TH_CHECK_EQ(s.n, 3);
    TH_CHECK(s.win[0] == &e && s.win[1] == &d && s.win[2] == &b);
    // Dibujo de abajo arriba.
    th_draw it[8];
    th_drawlist l = {.items = it, .capacity = 8};
    TH_CHECK_EQ(th_ui_stack_draw(&s, &l), 0);
    TH_CHECK_EQ(l.count, 1); // solo b tiene paneles
    TH_CHECK_EQ(it[0].x, 60);

    // El margen de 3 píxeles de UI:onMouseDown (ui.lua:888-890): pegado al
    // borde, la ventana toma el clic pero el botón no cuenta como abajo.
    th_ui_stack s2;
    th_ui_stack_init(&s2);
    th_ui_stack_screen(&s2, 320, 240);
    th_ui_stack_open(&s2, &d, NULL);
    TH_CHECK(th_ui_stack_mouse_down(&s2, TH_UI_LEFT, 2, 110).win == &d);
    TH_CHECK_EQ(s2.down, 0);
    th_ui_stack_mouse_up(&s2, TH_UI_LEFT, 2, 110);
    TH_CHECK(th_ui_stack_mouse_down(&s2, TH_UI_LEFT, 3, 110).win == &d);
    TH_CHECK_EQ(s2.down, 1u << TH_UI_LEFT);
    th_ui_stack_mouse_up(&s2, TH_UI_LEFT, 3, 110);
    TH_CHECK_EQ(s2.down, 0);

    // Los setters con el −1 de una ventana llena (o un índice fuera) no
    // escriben nada (ASan lo vería).
    const th_ui_button antes = a.buttons[0];
    th_ui_set_rightclick(&a, -1, 5);
    th_ui_set_toggle(&a, -1);
    th_ui_set_repeat(&a, TH_UI_BUTTONS);
    th_ui_set_disabled_sprite(&a, a.n_buttons, 7);
    th_ui_enable(&a, -1, 0);
    th_ui_toggle(&a, -1);
    th_ui_button_tooltip(&a, -1, "x");
    const th_ui_button *ahora = &a.buttons[0];
    TH_CHECK(ahora->id_right == antes.id_right && ahora->is_toggle == antes.is_toggle &&
             ahora->is_repeat == antes.is_repeat && ahora->sprite_disabled == antes.sprite_disabled &&
             ahora->enabled == antes.enabled && ahora->tooltip == antes.tooltip &&
             ahora->sprite_normal == antes.sprite_normal);
    TH_CHECK_EQ(a.n_buttons, 1);
}

// th_drawlist_loads sin hojas: un rectángulo de color hace olvidar el sprite
// cargado (preparar_tiles de mapa_n64.c).
static void test_cargas(void)
{
    th_draw it[8];
    th_drawlist l = {.items = it, .capacity = 8};
    it[l.count++] = (th_draw){.sprite = 5, .sheet = TH_SHEET_MUTTON};
    it[l.count++] = (th_draw){.sprite = 5, .sheet = TH_SHEET_MUTTON};
    TH_CHECK_EQ(th_drawlist_loads(&l, 0, NULL), 1);
    th_drawlist_fill(&l, 0, 0, 4, 4, 0xFFFF);
    it[l.count++] = (th_draw){.sprite = 5, .sheet = TH_SHEET_MUTTON};
    TH_CHECK_EQ(th_drawlist_loads(&l, 0, NULL), 2);
    it[l.count++] = (th_draw){.sprite = 5, .sheet = TH_SHEET_PANEL};
    TH_CHECK_EQ(th_drawlist_loads(&l, 0, NULL), 3);
    TH_CHECK_EQ(th_drawlist_loads(&l, 2, NULL), 2);
}

// draw propio (Window:draw de una clase): los paneles y un rectángulo encima.
static int dibujo_propio(const th_ui_window *w, th_drawlist *out, void *ctx)
{
    if (th_ui_window_draw(w, out) < 0)
        return -1;
    return th_drawlist_fill(out, w->x + 36, w->y + 24, 3, 3, *(const uint16_t *)ctx);
}

static uint32_t fnv(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

static void test_raster(void)
{
    th_sheet hoja;
    hoja_sintetica(&hoja);
    const th_sheet *hojas[TH_SHEETS] = {NULL};
    hojas[TH_SHEET_MUTTON] = &hoja;
    static uint8_t paleta[256][3];
    for (int i = 0; i < 256; i++) {
        paleta[i][0] = (uint8_t)i;
        paleta[i][1] = (uint8_t)(i * 7);
        paleta[i][2] = (uint8_t)(255 - i);
    }
    static th_ui_window w;
    th_ui_window_init(&w, 40, 30);
    th_ui_window_place(&w, 0.5, 0.5, 64, 48);
    TH_CHECK_EQ(w.x, 12);
    TH_CHECK_EQ(w.y, 9);
    th_ui_add_fill(&w, 0, 0, 40, 30, 0x7BDF);
    for (int i = 0; i < 4; i++) {
        const int p = th_ui_add_panel(&w, TH_SHEET_MUTTON, 1 + i, 2 + 9 * i, 2);
        const int b = th_ui_make_button(&w, p, 0, 0, 8, 8, 5 + i, i);
        th_ui_set_disabled_sprite(&w, b, 0);
    }
    uint16_t color = 0xF801;
    w.draw = dibujo_propio;
    w.draw_ctx = &color;
    th_ui_enable(&w, 3, 0); // el último, deshabilitado: el sprite 0 es transparente
    th_ui_stack s;
    th_ui_stack_init(&s);
    th_ui_stack_open(&s, &w, NULL);
    th_ui_stack_mouse_down(&s, TH_UI_LEFT, 12 + 2 + 9 + 1, 9 + 3); // el segundo, hundido

    static uint8_t rgb[2][64 * 48 * 3];
    uint32_t h[2];
    for (int k = 0; k < 2; k++) {
        th_draw it[16];
        th_drawlist l = {.items = it, .capacity = 16};
        TH_CHECK_EQ(th_ui_stack_draw(&s, &l), 0);
        TH_CHECK_EQ(l.count, 6);
        th_raster r = {.rgb = rgb[k], .w = 64, .h = 48, .paleta = (const uint8_t(*)[3])paleta};
        th_raster_clear(&r, 0, 0, 0);
        th_raster_draw(&r, hojas, &l);
        h[k] = fnv(rgb[k], sizeof rgb[k]);
    }
    TH_CHECK_EQ(h[0], h[1]);
    TH_CHECK(memcmp(rgb[0], rgb[1], sizeof rgb[0]) == 0);
    // Píxeles concretos: el fondo, el primer botón (color 1), el segundo
    // hundido (color 6), el deshabilitado (fondo) y el rectángulo del draw.
    const uint8_t *px = rgb[0];
#define PX(x, y) (px + 3 * ((y) * 64 + (x)))
    TH_CHECK_EQ(PX(12 + 2, 9 + 2)[0], 1);
    TH_CHECK_EQ(PX(12 + 11, 9 + 2)[0], 6);
    TH_CHECK_EQ(PX(12 + 20, 9 + 2)[0], 3);
    TH_CHECK_EQ(PX(12 + 29, 9 + 2)[0], PX(12 + 39, 9 + 29)[0]); // deshabilitado: el fondo
    TH_CHECK_EQ(PX(0, 0)[0], 0);                                // fuera de la ventana
    TH_CHECK_EQ(PX(12 + 37, 9 + 25)[0], 255);                   // el rectángulo del draw (0xF801)
#undef PX
}

void th_test_ui(void)
{
    test_derecho();
    test_repetido();
    test_deshabilitado();
    test_pila();
    test_cargas();
    test_raster();
}

#ifdef TH_TEST_UI_MAIN
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "th/rules.h"

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

// Con los assets: una ventana de amueblar de 10 filas a 320 (la columna
// derecha: cabecera, 10 filas de 11 px, pie y confirmar) con los nombres de
// los objetos en FONT132M y la cantidad a la derecha. Cuenta los elementos y
// las cargas de textura, comprueba los tamaños de mutton01.h contra la hoja y
// escribe la ventana en salida.ppm (si se da).
static int medir(const char *assets, const char *salida)
{
    size_t lm, lf, lt, lp;
    uint8_t *dm = leer(assets, "datam/mutton01.tsh", &lm), *df = leer(assets, "fuentes/font132m.tsh", &lf);
    uint8_t *dt = leer(assets, "textos/english.trg", &lt), *dp = leer(assets, "paleta.tpl", &lp);
    th_sheet mutton, fuente;
    th_rules textos;
    if (!dm || !df || !dt || !dp || th_sheet_init(&mutton, dm, lm) || th_sheet_init(&fuente, df, lf) ||
        th_rules_init(&textos, dt, lt) || lp < 8 + 512) {
        printf("medir: no se pueden leer los assets de %s\n", assets);
        return 1;
    }
    TH_CHECK_EQ(mutton.count, TH_MT_ENTRADAS);
    int w, h, piezas = 0;
    // th_sheet_size siempre escribe w y h (0 × 0 si el sprite no existe).
#define TAM(nombre, indice, ancho, alto)                                                                               \
    w = h = -1;                                                                                                        \
    th_sheet_size(&mutton, indice, &w, &h);                                                                            \
    TH_CHECK_EQ(w, ancho);                                                                                             \
    TH_CHECK_EQ(h, alto);                                                                                              \
    piezas++;
    TH_MUTTON01_PIEZAS(TAM)
#undef TAM
    static uint8_t paleta[256][3];
    for (int i = 0; i < 256; i++) { // RGBA5551 big-endian, como tools/conv/paleta.py
        const unsigned c = (unsigned)dp[8 + 2 * i] << 8 | dp[9 + 2 * i];
        paleta[i][0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
        paleta[i][1] = (uint8_t)(((c >> 6) & 31) * 255 / 31);
        paleta[i][2] = (uint8_t)(((c >> 1) & 31) * 255 / 31);
    }
    static const char *objetos[10] = {"object.reception_desk",
                                      "object.bench",
                                      "object.plant",
                                      "object.fire_extinguisher",
                                      "object.bin",
                                      "object.radiator",
                                      "object.drinks_machine",
                                      "object.cabinet",
                                      "object.lamp",
                                      "object.sofa"};
    static th_ui_window win;
    th_ui_window_init(&win, 136, 19 + 10 * 11 + 19 + 23);
    th_ui_window_place(&win, 0.5, 0.4, 320, 240);
    th_ui_add_panel(&win, TH_SHEET_MUTTON, TH_MT_AMU_LISTA_CABECERA, 0, 0);
    for (int i = 0; i < 10; i++)
        th_ui_add_panel(&win, TH_SHEET_MUTTON, TH_MT_AMU_FILA, 0, 19 + 11 * i);
    th_ui_add_panel(&win, TH_SHEET_MUTTON, TH_MT_AMU_PIE, 0, 129);
    const int pc = th_ui_add_panel(&win, TH_SHEET_MUTTON, TH_MT_AMU_CONFIRMAR, 0, 148);
    th_ui_make_button(&win, pc, 0, 0, 136, 23, TH_MT_AMU_CONFIRMAR_P, 1);
    static th_draw it[1024];
    th_drawlist l = {.items = it, .capacity = 1024};
    const th_sheet *hojas[TH_SHEETS] = {NULL};
    hojas[TH_SHEET_MUTTON] = &mutton;
    hojas[TH_SHEET_FONT_WHITE] = &fuente;
    th_ui_window_draw(&win, &l);
    const int n_paneles = l.count, cargas_paneles = th_drawlist_loads(&l, 0, hojas);
    // Cabecera, pie y confirmar (136 × 19, 136 × 19 y 136 × 23) no caben en
    // la TMEM: 2 tiras cada uno (15 filas de 136 por tira); las 10 filas son
    // el mismo sprite seguido: 1 carga.
    TH_CHECK_EQ(cargas_paneles, 2 + 1 + 2 + 2);
    const th_font f = {&fuente, TH_SHEET_FONT_WHITE, 0, -1, 0};
    // La altura de la letra, para centrarla a ojo en la fila de 11 (en 640,
    // CorsixTH la baja 5 píxeles en filas de 19: furnish_corridor.lua:229).
    int alto_letra = 0;
    th_font_draw(&f, "A", 0, 0, 0, 0, TH_ALIGN_LEFT, NULL, NULL, &alto_letra);
    const int dy = (11 - alto_letra) / 2;
    const int letras0 = l.count;
    for (int i = 0; i < 10; i++) {
        char cant[8];
        snprintf(cant, sizeof cant, "%d", i == 0 ? 1 : i * 3);
        // El nombre desde x = 8 y la cantidad centrada en el hueco hundido de
        // la fila entre el «−» y el «+» (columnas 107-117 de TH_MT_AMU_FILA),
        // como la caja de 19 de furnish_corridor.lua:230.
        TH_CHECK_EQ(th_text_row(&f, th_text(&textos, objetos[i]), win.x + 8, cant, win.x + 107, 11,
                                win.y + 19 + 11 * i + dy, &l),
                    0);
    }
    const int letras_filas = l.count - letras0, antes_titulo = l.count;
    const int cargas = th_drawlist_loads(&l, letras0, hojas);
    th_font_draw(&f, th_text(&textos, "buy_objects_window.choose_items"), win.x, win.y, 136, 19, TH_ALIGN_CENTER, &l,
                 NULL, NULL);
    printf("medir: %d piezas de mutton01.h comprobadas contra la hoja\n", piezas);
    printf("medir: amueblar con 10 filas a 320: %d paneles (%d cargas); filas: %d letras, %d cargas; título: %d "
           "letras, %d cargas; en total %d elementos y %d cargas\n",
           n_paneles, cargas_paneles, letras_filas, cargas, l.count - antes_titulo,
           th_drawlist_loads(&l, antes_titulo, hojas), l.count, th_drawlist_loads(&l, 0, hojas));
    static uint8_t rgb[2][320 * 240 * 3];
    for (int k = 0; k < 2; k++) {
        th_raster r = {.rgb = rgb[k], .w = 320, .h = 240, .paleta = (const uint8_t(*)[3])paleta};
        th_raster_clear(&r, 40, 40, 40);
        th_raster_draw(&r, hojas, &l);
    }
    TH_CHECK(memcmp(rgb[0], rgb[1], sizeof rgb[0]) == 0);
    printf("medir: huella de la ventana %08lx\n", (unsigned long)fnv(rgb[0], sizeof rgb[0]));
    if (salida) {
        FILE *o = fopen(salida, "wb");
        if (o) {
            fprintf(o, "P6\n320 240\n255\n");
            fwrite(rgb[0], 1, sizeof rgb[0], o);
            fclose(o);
        }
    }
    free(dm);
    free(df);
    free(dt);
    free(dp);
    return 0;
}

int main(int argc, char **argv)
{
    th_test_ui();
    int r = 0;
    if (argc > 1)
        r = medir(argv[1], argc > 2 ? argv[2] : NULL);
    printf("pruebas de ui: %d comprobaciones, %d fallos\n", th_test_checks, th_test_failures);
    return th_test_failures || r ? 1 : 0;
}
#endif
