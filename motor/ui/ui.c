#include "ui.h"

#include <math.h>
#include <string.h>

void th_ui_window_init(th_ui_window *win, int w, int h)
{
    memset(win, 0, sizeof *win);
    win->w = (int16_t)w;
    win->h = (int16_t)h;
    win->active_button = -1;
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
    };
    return win->n_buttons++;
}

void th_ui_set_toggle(th_ui_window *win, int button)
{
    win->buttons[button].is_toggle = 1;
    win->buttons[button].toggled = 0;
}

void th_ui_toggle(th_ui_window *win, int button)
{
    th_ui_button *b = &win->buttons[button];
    const uint16_t s = b->sprite_normal;
    b->sprite_normal = b->sprite_active;
    b->sprite_active = s;
    win->panels[b->panel].sprite = b->sprite_normal;
    b->toggled = !b->toggled;
}

void th_ui_button_tooltip(th_ui_window *win, int button, const char *tooltip)
{
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
    for (int i = 0; i < win->n_panels; i++) {
        const th_ui_panel *p = &win->panels[i];
        if (!p->visible)
            continue;
        if (out->count >= out->capacity)
            return -1;
        out->items[out->count++] = (th_draw){.sprite = p->sprite, .sheet = p->sheet, .x = (int16_t)(win->x + p->x),
                                             .y = (int16_t)(win->y + p->y)};
    }
    return 0;
}

static int dentro(const th_ui_button *b, int x, int y)
{
    return b->x <= x && x < b->r && b->y <= y && y < b->b;
}

// hitTest: el rectángulo de la ventana (UIBottomPanel:hitTest). Las ventanas
// con forma, que miran el píxel de cada panel, llegarán con los diálogos.
static int encima(const th_ui_window *win, int x, int y)
{
    return x >= 0 && y >= 0 && x < win->w && y < win->h;
}

int th_ui_mouse_down(th_ui_window *win, int button, int x, int y)
{
    x -= win->x;
    y -= win->y;
    int pulsado = 0;
    if (button == TH_UI_LEFT || button == TH_UI_RIGHT) {
        // Los botones solo responden al izquierdo: ninguno tiene on_rightclick aún.
        for (int i = 0; i < win->n_buttons && button == TH_UI_LEFT; i++) {
            th_ui_button *b = &win->buttons[i];
            if (b->enabled && dentro(b, x, y)) {
                win->panels[b->panel].sprite = b->sprite_active;
                win->active_button = (int8_t)i;
                b->active = 1;
                pulsado = 1;
                break;
            }
        }
    }
    return pulsado || encima(win, x, y);
}

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
    if (!b->enabled || !dentro(b, x, y))
        return -1;
    if (b->is_toggle) // Button:handleClick: un botón de dos estados cambia al soltarlo
        th_ui_toggle(win, (int)(b - win->buttons));
    return b->id;
}

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

const char *th_ui_tooltip_at(const th_ui_window *win, int x, int y, int *tx, int *ty)
{
    x -= win->x;
    y -= win->y;
    if (x < 0 || y < 0 || x >= win->w || y >= win->h)
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
