#include "ui.h"

#include <math.h>
#include <string.h>

void th_ui_window_init(th_ui_window *win, int w, int h)
{
    memset(win, 0, sizeof *win);
    win->w = (int16_t)w;
    win->h = (int16_t)h;
    win->active_button = -1;
    win->visible = 1;
}

void th_ui_window_place(th_ui_window *win, double x, double y, int screen_w, int screen_h)
{
    if (x < 0)
        x = ceil(screen_w - win->w + x);
    else if (x < 1)
        x = floor((screen_w - win->w) * x + 0.5);
    if (y < 0)
        y = ceil(screen_h - win->h + y);
    else if (y < 1)
        y = floor((screen_h - win->h) * y + 0.5);
    win->x = (int16_t)x;
    win->y = (int16_t)y;
}

int th_ui_add_panel(th_ui_window *win, int sheet, int sprite, int x, int y)
{
    if (win->n_panels >= TH_UI_PANELS)
        return -1;
    win->panels[win->n_panels] =
        (th_ui_panel){.x = (int16_t)x, .y = (int16_t)y, .sprite = (uint16_t)sprite, .sheet = (uint8_t)sheet, .visible = 1};
    return win->n_panels++;
}

int th_ui_add_fill(th_ui_window *win, int x, int y, int w, int h, uint16_t color)
{
    const int p = th_ui_add_panel(win, TH_SHEET_FILL, color, x, y);
    if (p >= 0) {
        win->panels[p].w = (int16_t)w;
        win->panels[p].h = (int16_t)h;
    }
    return p;
}

// round(v, 1) de window.lua: floor(v + 0.5).
static int16_t centro(int a, int b)
{
    return (int16_t)floor((a + b) / 2.0 + 0.5);
}

int th_ui_make_button(th_ui_window *win, int panel, int x, int y, int w, int h, int sprite_active, int id)
{
    if (win->n_buttons >= TH_UI_BUTTONS || panel < 0 || panel >= win->n_panels)
        return -1;
    const th_ui_panel *p = &win->panels[panel];
    x += p->x;
    y += p->y;
    win->buttons[win->n_buttons] = (th_ui_button){
        .x = (int16_t)x, .y = (int16_t)y, .r = (int16_t)(x + w), .b = (int16_t)(y + h),
        .panel = (uint8_t)panel, .enabled = 1,
        .sprite_normal = p->sprite, .sprite_active = (uint16_t)sprite_active, .sprite_disabled = p->sprite,
        .id = (uint16_t)id,
        .id_right = -1,
    };
    return win->n_buttons++;
}

// Los setters aceptan el −1 de th_ui_make_button (ventana llena) y no hacen nada.
static int boton_valido(const th_ui_window *win, int button)
{
    return (unsigned)button < win->n_buttons;
}

void th_ui_set_rightclick(th_ui_window *win, int button, int id_right)
{
    if (!boton_valido(win, button))
        return;
    win->buttons[button].id_right = (int16_t)id_right;
}

// Button:makeToggle y Button:makeRepeat (window.lua:646-656): son excluyentes.
void th_ui_set_toggle(th_ui_window *win, int button)
{
    if (!boton_valido(win, button))
        return;
    win->buttons[button].is_toggle = 1;
    win->buttons[button].is_repeat = 0;
    win->buttons[button].toggled = 0;
}

void th_ui_set_repeat(th_ui_window *win, int button)
{
    if (!boton_valido(win, button))
        return;
    win->buttons[button].is_repeat = 1;
    win->buttons[button].is_toggle = 0;
    win->buttons[button].toggled = 0;
}

void th_ui_set_disabled_sprite(th_ui_window *win, int button, int sprite)
{
    if (!boton_valido(win, button))
        return;
    win->buttons[button].sprite_disabled = (uint16_t)sprite;
}

// Button:enable (window.lua:626-644): solo cambia el sprite si cambia el
// estado.
void th_ui_enable(th_ui_window *win, int button, int enable)
{
    if (!boton_valido(win, button))
        return;
    th_ui_button *b = &win->buttons[button];
    if (enable && !b->enabled) {
        b->enabled = 1;
        win->panels[b->panel].sprite = b->sprite_normal;
    } else if (!enable && b->enabled) {
        b->enabled = 0;
        win->panels[b->panel].sprite = b->sprite_disabled;
    }
}

void th_ui_toggle(th_ui_window *win, int button)
{
    if (!boton_valido(win, button))
        return;
    th_ui_button *b = &win->buttons[button];
    const uint16_t s = b->sprite_normal;
    b->sprite_normal = b->sprite_active;
    b->sprite_active = s;
    win->panels[b->panel].sprite = b->sprite_normal;
    b->toggled = !b->toggled;
}

void th_ui_button_tooltip(th_ui_window *win, int button, const char *tooltip)
{
    if (!boton_valido(win, button))
        return;
    th_ui_button *b = &win->buttons[button];
    b->tooltip = tooltip;
    b->tip_x = centro(b->x, b->r);
    b->tip_y = b->y;
}

void th_ui_region_tooltip(th_ui_window *win, const char *tooltip, int x, int y, int r, int b)
{
    if (win->n_regions >= TH_UI_REGIONS)
        return;
    win->regions[win->n_regions++] = (th_ui_region){.x = (int16_t)x, .y = (int16_t)y, .r = (int16_t)r,
                                                    .b = (int16_t)b, .tip_x = centro(x, r), .tip_y = (int16_t)y,
                                                    .tooltip = tooltip};
}

int th_ui_window_draw(const th_ui_window *win, th_drawlist *out)
{
    if (!win->visible)
        return 0;
    for (int i = 0; i < win->n_panels; i++) {
        const th_ui_panel *p = &win->panels[i];
        if (!p->visible)
            continue;
        if (out->count >= out->capacity)
            return -1;
        th_draw d = {.sprite = p->sprite, .sheet = p->sheet, .x = (int16_t)(win->x + p->x),
                     .y = (int16_t)(win->y + p->y)};
        if (p->sheet == TH_SHEET_FILL) { // como th_drawlist_fill (texto.c)
            if (p->w <= 0 || p->h <= 0)
                continue;
            d.clip_x1 = p->w;
            d.clip_y1 = p->h;
        }
        out->items[out->count++] = d;
    }
    return 0;
}

static int dentro(const th_ui_button *b, int x, int y)
{
    return b->x <= x && x < b->r && b->y <= y && y < b->b;
}

// hitTest: el rectángulo de la ventana (UIBottomPanel:hitTest). Las ventanas
// con forma, que miran el píxel de cada panel, no hacen falta: las de
// construir, amueblar, colocar y contratar son rectángulos.
static int encima(const th_ui_window *win, int x, int y)
{
    return x >= 0 && y >= 0 && x < win->w && y < win->h;
}

int th_ui_hit(const th_ui_window *win, int x, int y)
{
    return win->visible && encima(win, x - win->x, y - win->y);
}

// Button:handleClick para los repetidos: la pulsación se guarda hasta que la
// recojan (th_ui_take_click); si no caben, se pierde la más nueva.
static void pulsar(th_ui_window *win, const th_ui_button *b, int mouse)
{
    const int id = mouse == TH_UI_LEFT ? (int)b->id : b->id_right;
    if (id < 0 || win->n_clicks >= TH_UI_CLICKS)
        return;
    win->clicks[win->n_clicks++] = (th_ui_click){.id = (int16_t)id, .mouse = (uint8_t)mouse};
}

// Window:onMouseDown (window.lua:1628-1688), sin ventanas hijas, barras de
// desplazamiento ni arrastrar la ventana.
int th_ui_mouse_down(th_ui_window *win, int button, int x, int y)
{
    if (!win->visible)
        return 0;
    x -= win->x;
    y -= win->y;
    int pulsado = 0;
    if (button == TH_UI_LEFT || button == TH_UI_RIGHT) {
        for (int i = 0; i < win->n_buttons; i++) {
            th_ui_button *b = &win->buttons[i];
            // El derecho solo pulsa los botones con on_rightclick.
            if (b->enabled && dentro(b, x, y) && (button == TH_UI_LEFT || b->id_right >= 0)) {
                win->panels[b->panel].sprite = b->sprite_active;
                win->active_button = (int8_t)i;
                b->active = 1;
                if (b->is_repeat) // se pulsa una vez al bajar y, tras una espera, se repite
                    pulsar(win, b, button);
                win->repeat_delay = TH_UI_REPEAT_FIRST;
                pulsado = 1;
                break;
            }
        }
    }
    return pulsado || encima(win, x, y);
}

// Window:onMouseUp (window.lua:1694-1745).
int th_ui_mouse_up(th_ui_window *win, int button, int x, int y)
{
    x -= win->x;
    y -= win->y;
    if ((button != TH_UI_LEFT && button != TH_UI_RIGHT) || win->active_button < 0)
        return -1;
    th_ui_button *b = &win->buttons[win->active_button];
    win->panels[b->panel].sprite = b->sprite_normal;
    b->active = 0;
    win->active_button = -1;
    win->repeat_delay = 0;
    if (!b->enabled || b->is_repeat || !dentro(b, x, y))
        return -1;
    if (b->is_toggle) // Button:handleClick: un botón de dos estados cambia al soltarlo
        th_ui_toggle(win, (int)(b - win->buttons));
    return button == TH_UI_LEFT ? (int)b->id : b->id_right;
}

// Window:onMouseMove (window.lua:1846-1890).
int th_ui_mouse_move(th_ui_window *win, int x, int y)
{
    x -= win->x;
    y -= win->y;
    if (win->active_button < 0)
        return 0;
    // Con un botón pulsado: hundido mientras el puntero esté encima; si se
    // sale y pasa por otro, la pulsación pasa a ese otro (Window:onMouseMove).
    th_ui_button *b = &win->buttons[win->active_button];
    uint16_t sprite = b->sprite_normal;
    if (dentro(b, x, y)) {
        sprite = b->sprite_active;
        b->active = 1;
    } else {
        b->active = 0;
        for (int i = 0; i < win->n_buttons; i++) {
            th_ui_button *o = &win->buttons[i];
            if (o->enabled && dentro(o, x, y)) {
                win->panels[o->panel].sprite = o->sprite_active;
                o->active = 1;
                win->active_button = (int8_t)i;
                break;
            }
        }
    }
    win->panels[b->panel].sprite = sprite;
    return 1;
}

// Window:onTick (window.lua:1904-1917): solo los botones repetidos, mientras
// el botón del ratón siga abajo y el puntero encima (btn.active).
void th_ui_tick(th_ui_window *win, int mouse)
{
    if (win->active_button < 0 || (mouse != TH_UI_LEFT && mouse != TH_UI_RIGHT))
        return;
    const th_ui_button *b = &win->buttons[win->active_button];
    if (win->repeat_delay > 0)
        win->repeat_delay--;
    else if (b->active && b->is_repeat) {
        win->repeat_delay = TH_UI_REPEAT_NEXT;
        pulsar(win, b, mouse);
    }
}

int th_ui_take_click(th_ui_window *win, th_ui_click *out)
{
    if (!win->n_clicks)
        return 0;
    *out = win->clicks[0];
    win->n_clicks--;
    memmove(win->clicks, win->clicks + 1, win->n_clicks * sizeof win->clicks[0]);
    return 1;
}

const char *th_ui_tooltip_at(const th_ui_window *win, int x, int y, int *tx, int *ty)
{
    x -= win->x;
    y -= win->y;
    if (!win->visible || x < 0 || y < 0 || x >= win->w || y >= win->h)
        return NULL;
    for (int i = 0; i < win->n_buttons; i++) {
        const th_ui_button *b = &win->buttons[i];
        if (win->panels[b->panel].visible && b->tooltip && dentro(b, x, y)) {
            *tx = win->x + b->tip_x;
            *ty = win->y + b->tip_y;
            return b->tooltip;
        }
    }
    for (int i = 0; i < win->n_regions; i++) {
        const th_ui_region *r = &win->regions[i];
        if (r->x <= x && x < r->r && r->y <= y && y < r->b) {
            *tx = win->x + r->tip_x;
            *ty = win->y + r->tip_y;
            return r->tooltip;
        }
    }
    return NULL;
}

// La pila de ventanas (ui.lua).

void th_ui_stack_init(th_ui_stack *s)
{
    memset(s, 0, sizeof *s);
}

static int buscar(const th_ui_stack *s, const th_ui_window *win)
{
    for (int i = 0; i < s->n; i++)
        if (s->win[i] == win)
            return i;
    return -1;
}

static void quitar(th_ui_stack *s, int i)
{
    th_ui_window *w = s->win[i];
    // Window:close: si tenía un botón pulsado, se suelta sin pulsarlo.
    if (w->active_button >= 0) {
        th_ui_button *b = &w->buttons[w->active_button];
        w->panels[b->panel].sprite = b->enabled ? b->sprite_normal : b->sprite_disabled;
        b->active = 0;
        w->active_button = -1;
    }
    w->repeat_delay = 0;
    w->n_clicks = 0;
    s->n--;
    memmove(&s->win[i], &s->win[i + 1], (size_t)(s->n - i) * sizeof s->win[0]);
}

th_ui_window *th_ui_stack_modal(const th_ui_stack *s, int modal_class)
{
    if (modal_class == TH_UI_MODAL_NONE)
        return NULL;
    for (int i = 0; i < s->n; i++)
        if (s->win[i]->modal_class == modal_class)
            return s->win[i];
    return NULL;
}

// UI:addWindow (ui.lua:1044-1060) y Window:addWindow (window.lua:521-544):
// las ventanas nuevas van arriba (no hay ventanas «on_top»).
int th_ui_stack_open(th_ui_stack *s, th_ui_window *win, th_ui_window **closed)
{
    if (closed)
        *closed = NULL;
    int i = buscar(s, win);
    if (i >= 0) {
        quitar(s, i);
    } else {
        th_ui_window *prev = th_ui_stack_modal(s, win->modal_class);
        if (prev) {
            quitar(s, buscar(s, prev));
            if (closed)
                *closed = prev;
        }
    }
    if (s->n >= TH_UI_STACK)
        return -1;
    memmove(&s->win[1], &s->win[0], (size_t)s->n * sizeof s->win[0]);
    s->win[0] = win;
    s->n++;
    return 0;
}

int th_ui_stack_close(th_ui_stack *s, th_ui_window *win)
{
    const int i = buscar(s, win);
    if (i < 0)
        return 0;
    quitar(s, i);
    return 1;
}

void th_ui_stack_screen(th_ui_stack *s, int screen_w, int screen_h)
{
    s->screen_w = (int16_t)screen_w;
    s->screen_h = (int16_t)screen_h;
}

// UI:onMouseDown (ui.lua:874-894) y Window:onMouseDown de UI, que es una
// ventana con las demás de hijas (window.lua:1628-1688).
th_ui_event th_ui_stack_mouse_down(th_ui_stack *s, int button, int x, int y)
{
    th_ui_event e = {NULL, -1};
    // buttons_down solo se marca a más de 3 píxeles del borde de la pantalla
    // (ui.lua:888-890): pegado al borde, un botón repetido no se repite.
    const int lejos_del_borde =
        !s->screen_w || (x >= 3 && y >= 3 && x < s->screen_w - 3 && y < s->screen_h - 3);
    if ((button == TH_UI_LEFT || button == TH_UI_RIGHT) && lejos_del_borde)
        s->down |= (uint8_t)(1u << button);
    int i = 0;
    for (; i < s->n && !e.win; i++)
        if (th_ui_mouse_down(s->win[i], button, x, y))
            e.win = s->win[i];
    // La hija que lo toma sube arriba del todo (Window:bringToTop y
    // sendToTop, window.lua:1683-1685 y 1952-1980; no hay ventanas «on_top»).
    if (e.win && --i > 0) {
        memmove(&s->win[1], &s->win[0], (size_t)i * sizeof s->win[0]);
        s->win[0] = e.win;
    }
    return e;
}

th_ui_event th_ui_stack_mouse_up(th_ui_stack *s, int button, int x, int y)
{
    th_ui_event e = {NULL, -1};
    if (button == TH_UI_LEFT || button == TH_UI_RIGHT)
        s->down &= (uint8_t) ~(1u << button);
    // Window.onMouseUp: la primera hija que lo toma (la que tiene un botón
    // pulsado).
    for (int i = 0; i < s->n; i++)
        if (s->win[i]->active_button >= 0) {
            e.win = s->win[i];
            e.id = th_ui_mouse_up(e.win, button, x, y);
            return e;
        }
    // Si no, UI:onMouseUp hace cursor_entity:onClick, que es nil con el
    // puntero sobre una ventana (GameUI:onCursorWorldPositionChange).
    for (int i = 0; i < s->n && !e.win; i++)
        if (th_ui_hit(s->win[i], x, y))
            e.win = s->win[i];
    return e;
}

th_ui_window *th_ui_stack_mouse_move(th_ui_stack *s, int x, int y)
{
    th_ui_window *sobre = NULL;
    for (int i = 0; i < s->n; i++) {
        th_ui_mouse_move(s->win[i], x, y);
        if (!sobre && th_ui_hit(s->win[i], x, y))
            sobre = s->win[i];
    }
    return sobre;
}

void th_ui_stack_update(th_ui_stack *s, int ms)
{
    // buttons_down.mouse_left antes que mouse_right (window.lua:1907).
    const int mouse = s->down & (1u << TH_UI_LEFT) ? TH_UI_LEFT : s->down & (1u << TH_UI_RIGHT) ? TH_UI_RIGHT : -1;
    int t = s->ms + (ms > 0 ? ms : 0);
    for (; t >= TH_UI_TICK_MS; t -= TH_UI_TICK_MS)
        for (int i = 0; i < s->n; i++)
            th_ui_tick(s->win[i], mouse);
    s->ms = (int16_t)t;
}

int th_ui_stack_take_click(th_ui_stack *s, th_ui_window **win, th_ui_click *out)
{
    for (int i = 0; i < s->n; i++)
        if (th_ui_take_click(s->win[i], out)) {
            if (win)
                *win = s->win[i];
            return 1;
        }
    return 0;
}

int th_ui_stack_draw(const th_ui_stack *s, th_drawlist *out)
{
    for (int i = s->n - 1; i >= 0; i--) {
        const th_ui_window *w = s->win[i];
        if (!w->visible)
            continue;
        if ((w->draw ? w->draw(w, out, w->draw_ctx) : th_ui_window_draw(w, out)) < 0)
            return -1;
    }
    return 0;
}

const char *th_ui_stack_tooltip_at(const th_ui_stack *s, int x, int y, int *tx, int *ty)
{
    for (int i = 0; i < s->n; i++)
        if (th_ui_hit(s->win[i], x, y))
            return th_ui_tooltip_at(s->win[i], x, y, tx, ty);
    return NULL;
}
