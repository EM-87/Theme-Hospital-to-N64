#include "objetos.h"

#include <string.h>

#include "girar.h"

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

// --- Las vistas giradas ---------------------------------------------------
//
// Un objeto se coloca en la vista girada como lo colocaría CorsixTH en el
// hospital girado: con la orientación girada, en el sitio en el que su
// huella cubre las casillas giradas. Si el objeto no se puede poner con esa
// orientación (los que solo se dibujaron de cara a la cámara), se pone con
// la opuesta, de frente en lugar de espaldas, sobre las mismas casillas: es
// el sustituto. Las puertas van en el borde girado.

typedef struct {
    int16_t x, y;           // casilla de origen en la vista
    uint8_t dir;            // orientación en la vista
    uint8_t sustituto;      // 1 = no es la girada, sino la opuesta
    int16_t x0, y0, x1, y1; // la caja de su huella en la vista (para los sustitutos)
    // De qué es: la colocación se guarda mientras el objeto no cambie (buscar
    // la huella en las reglas es lo más caro de pasar la simulación a entidades).
    th_rnode def;
    uint8_t ox, oy, odir;
    int8_t rot; // -1: sin hacer
} colocacion;

static colocacion colocados[TH_SIM_OBJECTS];

// Lo que use_object saca de las reglas para quien usa un objeto con la
// orientación de la vista, guardado: es igual en cada instantánea mientras
// no cambien el objeto, la fase ni la animación.
typedef struct {
    const void *clase; // th_walk_anims de quien lo usa
    th_rnode def;      // y el objeto, con su orientación
    uint8_t obj, odir, rot, flip, desde_uso;
    int8_t phase;
    uint16_t anim_mundo, anim; // la animación en el mundo y la de la vista
    uint8_t flags;             // TH_ANIM_FLIP_H de la vista
    int8_t dx, dy;             // render_attach_position
    int16_t px, py;            // animation_offset + added_animation_offset_while_in_use
} uso_guardado;

static uso_guardado usos[64];

// La caja de las casillas de la huella que ocupa el objeto (sin las de solo
// paso) con la orientación spec, respecto a su origen.
static void caja_huella(const th_rules *r, th_rnode spec, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = *y0 = *x1 = *y1 = 0;
    th_rnode fp = th_rules_get(r, spec, "footprint");
    int primera = 1;
    for (uint32_t k = 0; k < th_rules_len(r, fp); k++) {
        th_rnode e = th_rules_at(r, fp, k);
        int x, y;
        if (!pair(r, e, &x, &y) ||
            (th_rules_type(r, e) == TH_RULES_MAP && th_rules_bool(r, th_rules_get(r, e, "only_passable"), 0)))
            continue;
        if (primera || x < *x0)
            *x0 = x;
        if (primera || y < *y0)
            *y0 = y;
        if (primera || x > *x1)
            *x1 = x;
        if (primera || y > *y1)
            *y1 = y;
        primera = 0;
    }
}

static void colocar(const th_sim *s, int i, int rot, colocacion *c)
{
    const th_rules *r = s->defs;
    const th_object *o = &s->obj[i];
    th_rnode ors = th_rules_get(r, o->def, "orientations");
    int vx, vy;
    th_rot_tile(rot, o->x, o->y, &vx, &vy);
    int d = th_rot_dir(rot, o->dir);
    *c = (colocacion){.x = (int16_t)vx, .y = (int16_t)vy, .dir = (uint8_t)d};
    if (!ors) {
        // Una puerta va en el borde norte u oeste de su casilla. Si el borde
        // girado mira al sur o al este, es el norte o el oeste de la vecina.
        if (d == TH_DIR_SOUTH) {
            c->y++;
            c->dir = TH_DIR_NORTH;
        } else if (d == TH_DIR_EAST) {
            c->x++;
            c->dir = TH_DIR_WEST;
        }
        return;
    }
    if (!th_rules_get(r, ors, dir_names[d])) {
        d = (d + 2) & 3;
        c->sustituto = 1;
        if (!th_rules_get(r, ors, dir_names[d])) {
            d = o->dir;
            c->sustituto = 0;
        }
    }
    // Las casillas que ocupa en el mundo, giradas.
    int ax0, ay0, ax1, ay1, bx0, by0, bx1, by1;
    caja_huella(r, th_rules_get(r, ors, dir_names[o->dir]), &ax0, &ay0, &ax1, &ay1);
    th_rot_tile(rot, o->x + ax0, o->y + ay0, &bx0, &by0);
    th_rot_tile(rot, o->x + ax1, o->y + ay1, &bx1, &by1);
    c->x0 = (int16_t)(bx0 < bx1 ? bx0 : bx1);
    c->x1 = (int16_t)(bx0 < bx1 ? bx1 : bx0);
    c->y0 = (int16_t)(by0 < by1 ? by0 : by1);
    c->y1 = (int16_t)(by0 < by1 ? by1 : by0);
    // El origen con el que la huella de la orientación d cubre esas casillas.
    caja_huella(r, th_rules_get(r, ors, dir_names[d]), &ax0, &ay0, &ax1, &ay1);
    c->x = (int16_t)(c->x0 - ax0);
    c->y = (int16_t)(c->y0 - ay0);
    c->dir = (uint8_t)d;
}

static int add_object(const th_sim *s, int i, const colocacion *c, th_view_entity *out, int max)
{
    const th_rules *r = s->defs;
    const th_object *o = &s->obj[i];
    if (o->user != TH_NONE)
        return 0; // quien lo usa lo dibuja con su animación (makeInvisible)
    const int ox0 = c->x, oy0 = c->y, dir = c->dir;
    th_rnode idle = th_rules_get(r, o->def, "idle_animations");
    int flags = 0;
    th_rnode an = th_rules_get(r, idle, dir_names[dir]);
    if (!an) {
        an = th_rules_get(r, idle, dir_names[mirror_dir[dir]]);
        flags = TH_ANIM_FLIP_H;
    }
    if (!an)
        return 0;
    const unsigned anim = (unsigned)th_rules_int(r, an, 0);
    th_rnode spec = th_rules_get(r, th_rules_get(r, o->def, "orientations"), dir_names[dir]);
    int early = th_rules_bool(r, th_rules_get(r, spec, "early_list"), 0);
    const char *cls = th_rules_str(r, th_rules_get(r, o->def, "class"), NULL);
    if (cls && (!strcmp(cls, "Door") || !strcmp(cls, "SwingDoor")) && dir == TH_DIR_NORTH)
        early = 1; // Door:setAnimation (SwingDoor también es Door)
    int ox = 0, oy = 0;
    pair(r, th_rules_get(r, spec, "animation_offset"), &ox, &oy);
    th_view_entity base = {.tile_x = (int16_t)ox0, .tile_y = (int16_t)oy0, .px = (int16_t)ox, .py = (int16_t)oy,
                           .frame = th_anims_first_frame(s->w.anims, anim), .anim = (uint16_t)anim,
                           .flags = (uint8_t)flags, .drawing_layer = (uint8_t)drawing_layer(r, o->def, dir),
                           .attach_seq = (uint32_t)i, .layers = no_layers, .crop = TH_VIEW_NO_CROP,
                           .early = (uint8_t)early};
    th_rnode rap = th_rules_get(r, spec, "render_attach_position");
    th_rnode first = th_rules_at(r, rap, 0);
    if (th_rules_type(r, rap) == TH_RULES_LIST && th_rules_type(r, first) == TH_RULES_MAP) {
        // Partido en columnas (Object:setPosition): cada parte va en su
        // casilla, pero se dibuja donde la primera, con el desplazamiento de
        // la animación: Map:WorldToScreen(1 − x + bx, 1 − y + by).
        int n = 0, bx = 0, by = 0;
        pair(r, first, &bx, &by);
        for (uint32_t k = 0; k < th_rules_len(r, rap) && n < max; k++) {
            th_rnode p = th_rules_at(r, rap, k);
            int dx, dy;
            pair(r, p, &dx, &dy);
            th_view_entity e = base;
            e.tile_x = (int16_t)(ox0 + dx);
            e.tile_y = (int16_t)(oy0 + dy);
            e.crop = (int8_t)th_rules_int(r, th_rules_get(r, p, "column"), 0);
            e.px = (int16_t)(ox + 32 * ((bx - dx) - (by - dy)));
            e.py = (int16_t)(oy + 16 * ((bx - dx) + (by - dy)));
            out[n++] = e;
        }
        return n;
    }
    int dx = 0, dy = 0;
    if (pair(r, rap, &dx, &dy)) {
        base.tile_x = (int16_t)(ox0 + dx);
        base.tile_y = (int16_t)(oy0 + dy);
    }
    if (max < 1)
        return 0;
    out[0] = base;
    return 1;
}

// El fotograma número k (desde 0) de la animación, dando la vuelta.
static uint32_t nth_frame(const th_anims *a, unsigned anim, int k)
{
    const uint32_t f0 = th_anims_first_frame(a, anim);
    uint32_t f = f0;
    for (int i = 0; i < k; i++) {
        f = th_anims_next_frame(a, f);
        if (f == f0)
            break;
    }
    return f;
}

// Qué número de fotograma (desde 0) es frame en la animación (0 si no está).
static int frame_index(const th_anims *a, unsigned anim, uint32_t frame)
{
    const uint32_t f0 = th_anims_first_frame(a, anim);
    uint32_t f = f0;
    for (int i = 0; i < 1024; i++) {
        if (f == frame)
            return i;
        f = th_anims_next_frame(a, f);
        if (f == f0)
            break;
    }
    return 0;
}

static const char *const phase_names[] = {"begin_use", "begin_use_2", "begin_use_3", "begin_use_4", "begin_use_5",
                                          "in_use",    "finish_use",  "finish_use_2", "finish_use_3", "finish_use_4",
                                          "finish_use_5"};

// La animación de la fase para la clase con la orientación dir: la tabla
// de esa orientación o, si no la hay, la de su espejo (*flip).
static th_rnode usage_anims(const th_sim *s, const th_object *o, int dir, int phase, const char *cls, int *flip)
{
    th_rnode ua = th_rules_get(s->defs, o->def, "usage_animations");
    th_rnode t = th_rules_get(s->defs, ua, dir_names[dir]);
    *flip = 0;
    if (!t) {
        t = th_rules_get(s->defs, ua, dir_names[mirror_dir[dir]]);
        *flip = TH_ANIM_FLIP_H;
    }
    return th_rules_get(s->defs, th_rules_get(s->defs, t, phase_names[phase + 5]), cls);
}

// Un elemento de la tabla de una fase: un número, {anim, mirror = …} o una lista.
static unsigned elemento(const th_rules *r, th_rnode n, int *mirror)
{
    *mirror = 0;
    if (th_rules_type(r, n) == TH_RULES_MAP) {
        *mirror = th_rules_bool(r, th_rules_get(r, n, "mirror"), 0) ? TH_ANIM_FLIP_H : 0;
        return (unsigned)th_rules_int(r, th_rules_geti(r, n, 1), 0);
    }
    if (th_rules_type(r, n) == TH_RULES_LIST)
        return (unsigned)th_rules_int(r, th_rules_at(r, n, 0), 0);
    return (unsigned)th_rules_int(r, n, 0);
}

// Quien usa un objeto (use_object, fases −5 a 5): con la animación de la
// orientación del objeto en la vista y donde la pondría use_object
// (setHumanoidTileSpeed) con el objeto colocado ahí. Devuelve 0 si no se
// encuentra la animación que corresponde.
static int persona_usando(const th_sim *s, const th_humanoid *h, int rot, th_view_entity *e)
{
    const th_rules *r = s->defs;
    const th_action *a = h->queue_len ? &h->queue[0] : NULL;
    if (!a || a->kind != TH_ACTION_USE_OBJECT || a->phase < -5 || a->phase > 5 || a->obj >= s->n_obj)
        return 0;
    const th_object *o = &s->obj[a->obj];
    const colocacion *c = &colocados[a->obj];
    const uint8_t flip_mundo = h->anim_flags & TH_ANIM_FLIP_H;
    uso_guardado *u = &usos[(a->obj * 7u + h->anim * 3u + (unsigned)(a->phase + 5) + rot) & 63];
    if (u->clase != h->walk || u->def != o->def || u->obj != a->obj || u->odir != o->dir || u->rot != rot ||
        u->phase != a->phase || u->anim_mundo != h->anim || u->flip != flip_mundo) {
        int fd, fc;
        th_rnode td = usage_anims(s, o, o->dir, a->phase, h->walk->name, &fd);
        th_rnode tc = usage_anims(s, o, c->dir, a->phase, h->walk->name, &fc);
        if (!td || !tc)
            return 0;
        // La misma elección que hizo la simulación entre las de la lista.
        uint32_t k = 0;
        if (th_rules_type(r, td) == TH_RULES_LIST && th_rules_len(r, td) > 1) {
            while (k < th_rules_len(r, td)) {
                int m;
                if (elemento(r, th_rules_at(r, td, k), &m) == h->anim)
                    break;
                k++;
            }
            if (k >= th_rules_len(r, td))
                return 0;
            if (th_rules_type(r, tc) != TH_RULES_LIST || k >= th_rules_len(r, tc))
                return 0;
            tc = th_rules_at(r, tc, k);
        }
        int mirror;
        const unsigned anim = elemento(r, tc, &mirror);
        // setHumanoidTileSpeed con la orientación de la vista.
        th_rnode spec = th_rules_get(r, th_rules_get(r, o->def, "orientations"), dir_names[c->dir]);
        int dx = 0, dy = 0, ox = 0, oy = 0, bx = 0, by = 0;
        const int desde_uso = th_rules_bool(r, th_rules_get(r, spec, "use_animate_from_use_position"), 0);
        if (!desde_uso) {
            th_rnode rap = th_rules_get(r, spec, "render_attach_position");
            th_rnode first = th_rules_at(r, rap, 0);
            if (th_rules_type(r, rap) == TH_RULES_LIST &&
                (th_rules_type(r, first) == TH_RULES_LIST || th_rules_type(r, first) == TH_RULES_MAP))
                rap = first;
            pair(r, rap, &dx, &dy);
        }
        pair(r, th_rules_get(r, spec, "animation_offset"), &ox, &oy);
        pair(r, th_rules_get(r, spec, "added_animation_offset_while_in_use"), &bx, &by);
        *u = (uso_guardado){.clase = h->walk, .def = o->def, .obj = a->obj, .odir = o->dir, .rot = (uint8_t)rot,
                            .flip = flip_mundo,
                            .desde_uso = (uint8_t)desde_uso, .phase = a->phase, .anim_mundo = h->anim,
                            .anim = (uint16_t)anim, .flags = (uint8_t)((fc ^ mirror) & TH_ANIM_FLIP_H),
                            .dx = (int8_t)dx, .dy = (int8_t)dy, .px = (int16_t)(ox + bx), .py = (int16_t)(oy + by)};
    }
    e->anim = u->anim;
    e->flags = (uint8_t)((h->anim_flags & ~TH_ANIM_FLIP_H) | u->flags);
    e->frame = nth_frame(s->w.anims, u->anim, frame_index(s->w.anims, h->anim, h->frame));
    int tx = c->x + u->dx, ty = c->y + u->dy;
    if (u->desde_uso) {
        th_rot_tile(rot, a->old_x, a->old_y, &tx, &ty);
        if (c->sustituto) { // al otro lado de la huella
            tx = c->x0 + c->x1 - tx;
            ty = c->y0 + c->y1 - ty;
        }
    }
    e->tile_x = (int16_t)tx;
    e->tile_y = (int16_t)ty;
    e->px = u->px;
    e->py = u->py;
    return 1;
}

enum { CENTRO, BORDE, SOLO_ESTE };

// Las animaciones de caminar.lua y compañía van por parejas: la del norte
// (que en espejo es la del oeste) y la del este (en espejo, la del sur). Las
// de las puertas cruzan el borde norte u oeste de su casilla. Las de las
// necesidades (vomitar, orinar…) solo miran al este o al sur.
static int pareja(const th_walk_anims *w, unsigned anim, unsigned *norte, unsigned *este)
{
    const uint16_t pares[][2] = {{w->walk_north, w->walk_east},       {w->idle_north, w->idle_east},
                                 {w->knock_north, w->knock_east},     {w->door_leaving, w->door_entering},
                                 {w->swing_leaving, w->swing_entering}};
    for (int i = 0; i < 5; i++)
        if (anim && (anim == pares[i][0] || anim == pares[i][1])) {
            *norte = pares[i][0];
            *este = pares[i][1];
            return i >= 3 ? BORDE : CENTRO;
        }
    const uint16_t solas[] = {w->vomit, w->pee, w->yawn, w->tap_foot, w->check_watch};
    for (int i = 0; i < 5; i++)
        if (anim && anim == solas[i]) {
            *norte = 0;
            *este = anim;
            return SOLO_ESTE;
        }
    return -1;
}

void th_vista_humanoide_rot(const th_humanoid *h, const th_anims *anims, int rot, th_view_entity *e)
{
    unsigned norte = 0, este = 0;
    const int clase = pareja(h->walk, h->anim, &norte, &este);
    const int flip = (h->anim_flags & TH_ANIM_FLIP_H) != 0;
    int x, y;
    th_rot_tile(rot, h->tile_x, h->tile_y, &x, &y);
    if (clase == BORDE) {
        // Cruza el borde norte (si va al norte o al sur) u oeste de su
        // casilla. En la vista, la casilla es la que tiene ese borde al norte
        // o al oeste.
        const int ida = h->anim == norte ? (flip ? TH_DIR_WEST : TH_DIR_NORTH) : (flip ? TH_DIR_SOUTH : TH_DIR_EAST);
        const int borde = th_rot_dir(rot, ida == TH_DIR_NORTH || ida == TH_DIR_SOUTH ? TH_DIR_NORTH : TH_DIR_WEST);
        if (borde == TH_DIR_SOUTH && y < TH_MAP_HEIGHT - 1)
            y++;
        else if (borde == TH_DIR_EAST && x < TH_MAP_WIDTH - 1)
            x++;
    } else {
        // El desplazamiento, en 1/64 de casilla, girado; la casilla, la que
        // tiene ese punto a su norte y oeste, como deja walk.lua a quien camina.
        int qx, qy;
        th_rot_offset(rot, h->px, h->py, &qx, &qy);
        int dx = qx + 2 * qy, dy = 2 * qy - qx;
        while (dx > 0 && x < TH_MAP_WIDTH - 1)
            x++, dx -= 64;
        while (dx < -64 && x > 0)
            x--, dx += 64;
        while (dy > 0 && y < TH_MAP_HEIGHT - 1)
            y++, dy -= 64;
        while (dy < -64 && y > 0)
            y--, dy += 64;
        e->px = (int16_t)((dx - dy) / 2);
        e->py = (int16_t)((dx + dy) / 4);
    }
    e->tile_x = (int16_t)x;
    e->tile_y = (int16_t)y;
    if (clase < 0)
        return; // sin pareja: la misma animación
    const int mira = h->anim == norte && norte ? (flip ? TH_DIR_WEST : TH_DIR_NORTH)
                                               : (flip ? TH_DIR_SOUTH : TH_DIR_EAST);
    int d = th_rot_dir(rot, mira);
    if (!norte && (d == TH_DIR_NORTH || d == TH_DIR_WEST))
        d = (d + 2) & 3; // no hay dibujo de espaldas
    const unsigned anim = d == TH_DIR_NORTH || d == TH_DIR_WEST ? norte : este;
    e->flags = (uint8_t)((h->anim_flags & ~TH_ANIM_FLIP_H) |
                         (d == TH_DIR_WEST || d == TH_DIR_SOUTH ? TH_ANIM_FLIP_H : 0));
    if (anim != h->anim) {
        e->anim = (uint16_t)anim;
        e->frame = nth_frame(anims, anim, frame_index(anims, h->anim, h->frame));
    }
}

// Una persona de la simulación en la vista girada: si usa un objeto, como la
// pondría use_object con el objeto girado; si no, como cualquier humanoide.
static void girar_persona(const th_sim *s, const th_person *p, int rot, th_view_entity *e)
{
    const th_humanoid *h = &p->h;
    if (h->user_of != TH_NONE && persona_usando(s, h, rot, e))
        return;
    th_vista_humanoide_rot(h, s->w.anims, rot, e);
}

// litter_types de objects/litter.lua, en el orden de TH_LITTER_*.
static const uint16_t litter_anims[] = {2248, 2060, 1894, 1896, 1898, 1900};

int th_vista_sim_rot(const th_sim *s, int rot, th_view_entity *out, int max)
{
    rot &= 3;
    int n = 0;
    for (int i = 0; i < s->n_obj && n < max; i++) {
        const th_object *o = &s->obj[i];
        colocacion *c = &colocados[i];
        if (c->rot != rot || c->def != o->def || c->ox != o->x || c->oy != o->y || c->odir != o->dir) {
            if (rot)
                colocar(s, i, rot, c);
            else
                *c = (colocacion){.x = o->x, .y = o->y, .dir = o->dir};
            c->def = o->def;
            c->ox = o->x;
            c->oy = o->y;
            c->odir = o->dir;
            c->rot = (int8_t)rot;
        }
        n += add_object(s, i, c, out + n, max - n);
    }
    // La basura, en la capa del suelo (DrawingLayers.Litter = 0).
    for (int i = 0; i < s->n_litter && n < max; i++) {
        const th_litter *l = &s->litter[i];
        const unsigned anim = litter_anims[l->type];
        int x, y;
        th_rot_tile(rot, l->x, l->y, &x, &y);
        out[n++] = (th_view_entity){.tile_x = (int16_t)x, .tile_y = (int16_t)y,
                                    .frame = th_anims_first_frame(s->w.anims, anim), .anim = (uint16_t)anim,
                                    .flags = (uint8_t)(l->mirror ? TH_ANIM_FLIP_H : 0), .drawing_layer = 0,
                                    .attach_seq = l->attach_seq, .layers = no_layers, .crop = TH_VIEW_NO_CROP};
    }
    for (int k = 0; k < s->n_order && n < max; k++) {
        const th_person *p = &s->hum[s->order[k]];
        const th_humanoid *h = &p->h;
        if (h->tile_x < 0)
            continue;
        // Receptionist:getDrawingLayer: detrás del mostrador si mira al norte o al oeste.
        int layer = h->drawing_layer;
        if (p->kind == TH_HUM_RECEPTIONIST) {
            const int d = th_rot_dir(rot, h->last_dir);
            layer = d == TH_DIR_WEST || d == TH_DIR_NORTH ? 5 : 3;
        }
        th_view_entity *e = &out[n++];
        *e = (th_view_entity){.tile_x = h->tile_x, .tile_y = h->tile_y, .px = h->px, .py = h->py,
                              .frame = h->frame, .anim = h->anim, .flags = h->anim_flags,
                              .drawing_layer = (uint8_t)layer, .attach_seq = h->attach_seq,
                              .layers = h->layers, .crop = TH_VIEW_NO_CROP};
        if (rot)
            girar_persona(s, p, rot, e);
    }
    return n;
}

int th_vista_sim(const th_sim *s, th_view_entity *out, int max)
{
    return th_vista_sim_rot(s, 0, out, max);
}
