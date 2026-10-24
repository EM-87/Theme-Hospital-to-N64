#include "objetos.h"

#include <string.h>

static const char *const dir_names[] = {"north", "east", "south", "west"};
static const uint8_t mirror_dir[] = {TH_DIR_WEST, TH_DIR_SOUTH, TH_DIR_EAST, TH_DIR_NORTH};

// Par (x, y) de una lista [x, y] o de un mapa {#1 = x, #2 = y}.
static int pair(const th_rules *r, th_rnode n, int *x, int *y)
{
    if (th_rules_type(r, n) == TH_RULES_LIST) {
        *x = th_rules_int(r, th_rules_at(r, n, 0), 0);
        *y = th_rules_int(r, th_rules_at(r, n, 1), 0);
        return 1;
    }
    if (th_rules_type(r, n) == TH_RULES_MAP) {
        *x = th_rules_int(r, th_rules_geti(r, n, 1), 0);
        *y = th_rules_int(r, th_rules_geti(r, n, 2), 0);
        return 1;
    }
    return 0;
}

// getDrawingLayer: las puertas (0), los objetos de pared (según su lado) y los demás (4).
static int drawing_layer(const th_rules *r, th_rnode def, int dir)
{
    const char *cls = th_rules_str(r, th_rules_get(r, def, "class"), NULL);
    if (cls && (!strcmp(cls, "Door") || !strcmp(cls, "SwingDoor")))
        return 0;
    if (cls && !strcmp(cls, "SideObject")) {
        if (dir == TH_DIR_NORTH)
            return TH_LAYER_NORTH_SIDE;
        if (dir == TH_DIR_WEST)
            return 2;
        if (dir == TH_DIR_EAST)
            return th_rules_int(r, th_rules_get(r, def, "thob"), 0) == 50 ? 2 : TH_LAYER_EAST_SIDE;
        return TH_LAYER_SOUTH_SIDE;
    }
    return TH_LAYER_ENTITY;
}

static const uint8_t no_layers[TH_ANIM_LAYERS];

static int add_object(const th_sim *s, int i, th_view_entity *out, int max)
{
    const th_rules *r = s->defs;
    const th_object *o = &s->obj[i];
    if (o->user != TH_NONE)
        return 0; // quien lo usa lo dibuja con su animación (makeInvisible)
    th_rnode idle = th_rules_get(r, o->def, "idle_animations");
    int flags = 0;
    th_rnode an = th_rules_get(r, idle, dir_names[o->dir]);
    if (!an) {
        an = th_rules_get(r, idle, dir_names[mirror_dir[o->dir]]);
        flags = TH_ANIM_FLIP_H;
    }
    if (!an)
        return 0;
    const unsigned anim = (unsigned)th_rules_int(r, an, 0);
    th_rnode spec = th_rules_get(r, th_rules_get(r, o->def, "orientations"), dir_names[o->dir]);
    int early = th_rules_bool(r, th_rules_get(r, spec, "early_list"), 0);
    if (o->type == TH_OBJ_DOOR && o->dir == TH_DIR_NORTH)
        early = 1; // Door:setAnimation
    int ox = 0, oy = 0;
    pair(r, th_rules_get(r, spec, "animation_offset"), &ox, &oy);
    th_view_entity base = {.tile_x = o->x, .tile_y = o->y, .px = (int16_t)ox, .py = (int16_t)oy,
                           .frame = th_anims_first_frame(s->w.anims, anim), .anim = (uint16_t)anim,
                           .flags = (uint8_t)flags, .drawing_layer = (uint8_t)drawing_layer(r, o->def, o->dir),
                           .attach_seq = (uint32_t)i, .layers = no_layers, .crop = TH_VIEW_NO_CROP,
                           .early = (uint8_t)early};
    th_rnode rap = th_rules_get(r, spec, "render_attach_position");
    th_rnode first = th_rules_at(r, rap, 0);
    if (th_rules_type(r, rap) == TH_RULES_LIST && th_rules_type(r, first) == TH_RULES_MAP) {
        // Partido en columnas: la primera parte en su casilla con su
        // desplazamiento; las demás, en la suya, vuelven a la posición de la
        // primera (Map:WorldToScreen(1 − x, 1 − y)).
        int n = 0;
        for (uint32_t k = 0; k < th_rules_len(r, rap) && n < max; k++) {
            th_rnode p = th_rules_at(r, rap, k);
            int dx, dy;
            pair(r, p, &dx, &dy);
            th_view_entity e = base;
            e.tile_x = (int16_t)(o->x + dx);
            e.tile_y = (int16_t)(o->y + dy);
            e.crop = (int8_t)th_rules_int(r, th_rules_get(r, p, "column"), 0);
            if (k > 0) {
                e.px = (int16_t)(32 * (dy - dx));
                e.py = (int16_t)(-16 * (dx + dy));
            }
            out[n++] = e;
        }
        return n;
    }
    int dx = 0, dy = 0;
    if (pair(r, rap, &dx, &dy)) {
        base.tile_x = (int16_t)(o->x + dx);
        base.tile_y = (int16_t)(o->y + dy);
    }
    if (max < 1)
        return 0;
    out[0] = base;
    return 1;
}

int th_vista_sim(const th_sim *s, th_view_entity *out, int max)
{
    int n = 0;
    for (int i = 0; i < s->n_obj && n < max; i++)
        n += add_object(s, i, out + n, max - n);
    for (int k = 0; k < s->n_order && n < max; k++) {
        const th_person *p = &s->hum[s->order[k]];
        const th_humanoid *h = &p->h;
        if (h->tile_x < 0)
            continue;
        // Receptionist:getDrawingLayer: detrás del mostrador si mira al norte o al oeste.
        int layer = h->drawing_layer;
        if (p->kind == TH_HUM_RECEPTIONIST)
            layer = h->last_dir == TH_DIR_WEST || h->last_dir == TH_DIR_NORTH ? 5 : 3;
        out[n++] = (th_view_entity){.tile_x = h->tile_x, .tile_y = h->tile_y, .px = h->px, .py = h->py,
                                    .frame = h->frame, .anim = h->anim, .flags = h->anim_flags,
                                    .drawing_layer = (uint8_t)layer, .attach_seq = h->attach_seq,
                                    .layers = h->layers, .crop = TH_VIEW_NO_CROP};
    }
    return n;
}
