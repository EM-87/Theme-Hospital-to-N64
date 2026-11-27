// Pruebas de las cuatro vistas (vista/girar.c y vista.c, hitos 6.2 y 6.2b):
// la geometría de las rotaciones, las tablas de piezas giradas y el sol fijo;
// y del dibujo de la demo (06_plan.md, pieza 3): las superposiciones por
// casilla, las paletas fantasma y los fantasmas.
#include <string.h>

#include "girar.h"
#include "objetos.h"
#include "raster.h"
#include "th/humanoid.h"
#include "th/rules.h"
#include "th_test.h"
#include "vista.h"

// Un mapa y un mapa girado para todas las pruebas (son grandes).
static th_map mapa_prueba;
static th_rot_map giro_prueba;

// Hito 6.2b: el sol no gira con la cámara.
static void test_sol(void)
{
    // La pareja en espejo de una pared: la misma pieza en el otro borde, con
    // la mitad de pantalla cambiada. Deshacerla da la misma pieza.
    TH_CHECK_EQ(th_wall_mirror(86), 87);  // azul por dentro, norte ↔ oeste
    TH_CHECK_EQ(th_wall_mirror(95), 94);  // azul por fuera
    TH_CHECK_EQ(th_wall_mirror(124), 125); // ventana de fuera: norte izquierda ↔ oeste derecha
    TH_CHECK_EQ(th_wall_mirror(159), 160); // pilar: norte izquierda ↔ oeste derecha
    TH_CHECK_EQ(th_wall_mirror(120), 121); // ventana larga
    TH_CHECK_EQ(th_wall_mirror(1), 1);     // un suelo no tiene pareja
    TH_CHECK_EQ(th_wall_mirror(156), 156); // ni la sombra
    int parejas = 0;
    for (int b = 82; b <= 164; b++) {
        TH_CHECK_EQ(th_wall_mirror(th_wall_mirror(b)), b);
        parejas += th_wall_mirror(b) != b;
    }
    TH_CHECK_EQ(parejas, 82); // todas menos la sombra (156)
    // La luz de cada vista. La pieza norte está pintada al sol y la oeste a la
    // sombra; en espejo se dibuja la pareja, con la otra luz. La cara que se
    // ve de una pared de borde norte de la vista mira al sur de la vista, y la
    // de una de borde oeste, al este; en el mundo, al sur o al este menos rot.
    // Con el sol al suroeste, están al sol las que miran al sur o al oeste.
    for (int r = 0; r < 4; r++)
        for (int oeste = 0; oeste < 2; oeste++) {
            const int cara = ((oeste ? TH_DIR_EAST : TH_DIR_SOUTH) - r) & 3;
            const int al_sol = cara == TH_DIR_SOUTH || cara == TH_DIR_WEST;
            const int pieza_norte = !oeste ^ th_rot_wall_flip(r, oeste);
            TH_CHECK_EQ(pieza_norte, al_sol);
        }
    TH_CHECK(!th_rot_wall_flip(0, 0) && !th_rot_wall_flip(0, 1)); // la vista original, como TH

    // Las sombras de la vista girada son las del mundo. Un hospital pequeño:
    // una sala de paredes azules de (40, 40) a (45, 44), una pared oeste suelta
    // en (60, 60) y otra con su pared norte en (70, 70).
    th_map *m = &mapa_prueba;
    th_rot_map *g = &giro_prueba;
    memset(m, 0, sizeof *m);
    for (int x = 40; x <= 45; x++) {
        th_map_tile(m, x, 40)->wall_n = 86;
        th_map_tile(m, x, 45)->wall_n = 94;
    }
    for (int y = 40; y <= 44; y++) {
        th_map_tile(m, 40, y)->wall_w = 87;
        th_map_tile(m, 46, y)->wall_w = 95;
    }
    th_map_tile(m, 60, 60)->wall_w = 87;
    th_map_tile(m, 70, 70)->wall_w = 87;
    th_map_tile(m, 70, 70)->wall_n = 86;
    th_map_update_shadows(m);
    for (int r = 1; r < 4; r++) {
        memset(g, 0, sizeof *g);
        TH_CHECK_EQ(th_rot_map_update(g, m, r), 1);
        int con_sombra = 0, en_pared = 0, en_el_mundo = 0;
        for (int y = 0; y < TH_MAP_HEIGHT; y++)
            for (int x = 0; x < TH_MAP_WIDTH; x++) {
                int vx, vy;
                th_rot_tile(r, x, y, &vx, &vy);
                const uint32_t f = th_map_tile_c(m, x, y)->flags;
                const uint8_t vf = g->tiles[vy * TH_MAP_WIDTH + vx].flags;
                TH_CHECK_EQ(vf & (TH_VT_SHADOW_HALF | TH_VT_SHADOW_FULL),
                            (int)((f >> 11) & (TH_VT_SHADOW_HALF | TH_VT_SHADOW_FULL)));
                // La sombra sobre la pared: en la cara sur de la pared norte del
                // mundo, que solo se ve con r = 3, como pared oeste de la vista.
                TH_CHECK(!(vf & TH_VT_SHADOW_WALL));
                TH_CHECK_EQ(!!(vf & TH_VT_SHADOW_WALL_W), r == 3 && (f & TH_TILE_SHADOW_WALL));
                if (vf & TH_VT_SHADOW_WALL_W)
                    TH_CHECK_EQ(g->tiles[vy * TH_MAP_WIDTH + vx].wall_w, th_wall_mirror(th_rot_layer(3, 86)));
                con_sombra += (vf & (TH_VT_SHADOW_HALF | TH_VT_SHADOW_FULL)) != 0;
                en_el_mundo += (f & (TH_TILE_SHADOW_HALF | TH_TILE_SHADOW_FULL)) != 0;
                en_pared += (vf & TH_VT_SHADOW_WALL_W) != 0;
            }
        TH_CHECK(con_sombra > 10 && con_sombra == en_el_mundo);
        TH_CHECK_EQ(en_pared, r == 3 ? 2 : 0); // las esquinas noroeste de la sala y de (70, 70)
        // La pared oeste suelta, en la vista: con r = 1 está en el borde norte de
        // la vista (a la sombra: la pareja en espejo de la pieza girada); con
        // r = 2, en el borde este, que es la pared oeste de la casilla vecina
        // (al sol: en espejo); con r = 3, en el borde sur (al sol: tal cual).
        int vx, vy;
        th_rot_tile(r, 60, 60, &vx, &vy);
        const th_vtile *t = &g->tiles[vy * TH_MAP_WIDTH + vx];
        const int girada = th_rot_layer(r, 87);
        if (r == 1)
            TH_CHECK_EQ(t->wall_n, th_wall_mirror(girada));
        else if (r == 2)
            TH_CHECK_EQ(t[1].wall_w, th_wall_mirror(girada));
        else
            TH_CHECK_EQ(t[TH_MAP_WIDTH].wall_n, girada);
    }
}

// --- La pieza del dibujo de la demo: superposiciones, paletas y fantasmas ----
//
// Con hojas, animaciones y paletas sintéticas (sin datos de TH): la hoja de
// bloques tiene el suelo 1 (un rectángulo de 32 × 16 del color 10), el 24 (del
// 20), el 67 (del 21) y las marcas 26, 27, 30 y 31 (de 40 a 43); la de
// sprites, el sprite 0 (4 × 4 del color 30); las animaciones 0 y 1 son un solo
// fotograma con ese sprite en el origen. Las paletas: la normal da (i, 0, 0),
// la gris (0, i, 0) y la roja (0, 0, i).
enum { HOJA_BLOQUES = 16 + 8 * 256 + 32 * 16 * 12, HOJA_SPRITES = 16 + 8 + 8 * 4 };
static uint8_t hoja_b[HOJA_BLOQUES], hoja_s[HOJA_SPRITES], tan_prueba[36 + 8 + 8 + 4 + 6];
static uint8_t pal_n[256][3], pal_g[256][3], pal_r[256][3];
static th_draw elementos_prueba[512];
static uint8_t rgb_a[128 * 96 * 3], rgb_b[128 * 96 * 3];

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}

static void be16(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)(v >> 8), p[1] = (uint8_t)v;
}

static void preparar_dibujo(th_sheet *bloques, th_sheet *sprites, th_anims *anims)
{
    // Hoja de bloques: 256 entradas; 12 con píxeles, todas de 32 × 16.
    memset(hoja_b, 0, sizeof hoja_b);
    memcpy(hoja_b, "TSH1", 4);
    be16(hoja_b + 4, 256);
    be32(hoja_b + 8, 16);
    be32(hoja_b + 12, 16 + 8 * 256);
    static const uint8_t con[][2] = {{1, 10}, {24, 20}, {67, 21}, {26, 40}, {27, 41}, {30, 42}, {31, 43},
                                        {37, 44}, {25, 45}, {28, 46}, {29, 47}, {32, 48}};
    for (unsigned k = 0; k < sizeof con / sizeof con[0]; k++) {
        uint8_t *e = hoja_b + 16 + 8 * con[k][0];
        be32(e, 32 * 16 * k);
        be16(e + 4, 32);
        be16(e + 6, 16);
        memset(hoja_b + 16 + 8 * 256 + 32 * 16 * k, con[k][1], 32 * 16);
    }
    TH_CHECK_EQ(th_sheet_init(bloques, hoja_b, sizeof hoja_b), 0);
    // Hoja de sprites: un sprite de 4 × 4 (filas de 8 bytes; el relleno, transparente).
    memset(hoja_s, TH_TRANSPARENT, sizeof hoja_s);
    memcpy(hoja_s, "TSH1", 4);
    be16(hoja_s + 4, 1);
    be32(hoja_s + 8, 16);
    be32(hoja_s + 12, 24);
    be32(hoja_s + 16, 0);
    be16(hoja_s + 20, 4);
    be16(hoja_s + 22, 4);
    for (int y = 0; y < 4; y++)
        memset(hoja_s + 24 + 8 * y, 30, 4);
    TH_CHECK_EQ(th_sheet_init(sprites, hoja_s, sizeof hoja_s), 0);
    // Animaciones: 2, un fotograma (que se sigue a sí mismo), una lista [0, fin]
    // y un elemento: el sprite 0 en (141, 186), el origen.
    memset(tan_prueba, 0, sizeof tan_prueba);
    memcpy(tan_prueba, "TAN1", 4);
    be16(tan_prueba + 4, 2);
    be32(tan_prueba + 8, 1);
    be32(tan_prueba + 12, 2);
    be32(tan_prueba + 16, 1);
    be32(tan_prueba + 20, 36);
    be32(tan_prueba + 24, 44);
    be32(tan_prueba + 28, 52);
    be32(tan_prueba + 32, 56);
    be16(tan_prueba + 36 + 2, 1); // anim 0: fotograma 0, largo 1
    be16(tan_prueba + 40 + 2, 1); // anim 1
    be16(tan_prueba + 52 + 2, TH_ANIM_LIST_END);
    tan_prueba[56 + 2] = 141;
    tan_prueba[56 + 3] = 186;
    TH_CHECK_EQ(th_anims_init(anims, tan_prueba, sizeof tan_prueba), 0);
    for (int i = 0; i < 256; i++) {
        pal_n[i][0] = pal_g[i][1] = pal_r[i][2] = (uint8_t)i;
        pal_n[i][1] = pal_n[i][2] = pal_g[i][0] = pal_g[i][2] = pal_r[i][0] = pal_r[i][1] = 0;
    }
}

static const uint8_t *pixel(const uint8_t *rgb, int x, int y)
{
    return rgb + 3 * (y * 128 + x);
}

#define TH_CHECK_RGB(p, r, g, b) TH_CHECK((p)[0] == (r) && (p)[1] == (g) && (p)[2] == (b))

static void pintar_prueba(const th_view *v, const th_scene *sc, th_drawlist *l, uint8_t *rgb)
{
    th_raster r = {.rgb = rgb, .w = v->w, .h = v->h, .paleta = (const uint8_t(*)[3])pal_n,
                   .paleta_gris = (const uint8_t(*)[3])pal_g, .paleta_roja = (const uint8_t(*)[3])pal_r};
    th_raster_clear(&r, 0, 0, 0);
    th_raster_draw(&r, sc->sheets, l);
}

static void test_dibujo(th_map *m, th_rot_map *g)
{
    static th_sheet bloques, sprites;
    static th_anims anims;
    preparar_dibujo(&bloques, &sprites, &anims);
    memset(m, 0, sizeof *m);
    for (int y = 0; y < TH_MAP_HEIGHT; y++)
        for (int x = 0; x < TH_MAP_WIDTH; x++)
            th_map_tile(m, x, y)->floor = 1;

    // Las superposiciones: una por casilla; la segunda la sustituye y el
    // bloque 0 la quita; fuera del mapa o sin sitio, -1 sin tocar nada.
    static th_view_overlays ov;
    th_view_overlays_clear(&ov);
    TH_CHECK_EQ(th_view_overlays_set(&ov, 10, 10, 67, TH_DRAW_ALPHA50), 0);
    TH_CHECK_EQ(th_view_overlays_set(&ov, 10, 10, 24, TH_DRAW_ALPHA50), 0);
    TH_CHECK_EQ(ov.count, 1);
    TH_CHECK_EQ(ov.items[0].block, 24);
    TH_CHECK_EQ(th_view_overlays_set(&ov, 11, 10, 26, 0), 0);
    TH_CHECK_EQ(th_view_overlays_set(&ov, 11, 10, 0, 0), 0);
    TH_CHECK_EQ(ov.count, 1);
    TH_CHECK_EQ(th_view_overlays_set(&ov, TH_MAP_WIDTH, 0, 24, 0), -1);
    TH_CHECK_EQ(th_view_overlays_set(&ov, -1, 0, 24, 0), -1);
    for (int i = 0; ov.count < TH_VIEW_MAX_OVERLAYS; i++)
        TH_CHECK_EQ(th_view_overlays_set(&ov, 40 + i % 16, 40 + i / 16, 24, 0), 0);
    TH_CHECK_EQ(th_view_overlays_set(&ov, 100, 100, 24, 0), -1);
    TH_CHECK_EQ(th_view_overlays_set(&ov, 40, 40, 67, 0), 0); // sustituir sí cabe
    th_view_overlays_clear(&ov);
    TH_CHECK_EQ(ov.count, 0);

    // Las marcas con dirección, giradas: la puerta norte, con la cámara girada
    // una vez, es la del este; las flechas del centro, la de la esquina
    // siguiente (noroeste → noreste); los suelos, tal cual.
    TH_CHECK_EQ(th_view_overlay_block(0, 26, 0), 26);
    TH_CHECK_EQ(th_view_overlay_block(1, 26, 0), 27);
    TH_CHECK_EQ(th_view_overlay_block(2, 26, 0), 30);
    TH_CHECK_EQ(th_view_overlay_block(3, 26, 0), 31);
    TH_CHECK_EQ(th_view_overlay_block(1, 36, 0), 33);
    TH_CHECK_EQ(th_view_overlay_block(1, 39, 0), 38);
    TH_CHECK_EQ(th_view_overlay_block(2, 39, 0), 40);
    TH_CHECK_EQ(th_view_overlay_block(1, 24, 0), 24);
    TH_CHECK_EQ(th_view_overlay_block(3, 67, 1), 67);
    // Las mitades de las puertas dobles (edit_room.lua:1053-1060): la doble
    // norte (25 al oeste, 26 al este), girada una vez, es la este (28 al
    // norte, 27 al sur); la este, girada, la sur (29 al oeste, 30 al este).
    TH_CHECK_EQ(th_view_overlay_block(1, 25, 1), 28);
    TH_CHECK_EQ(th_view_overlay_block(1, 26, 1), 27);
    TH_CHECK_EQ(th_view_overlay_block(1, 28, 1), 30);
    TH_CHECK_EQ(th_view_overlay_block(1, 27, 1), 29);
    TH_CHECK_EQ(th_view_overlay_block(1, 27, 0), 30); // sencilla: la sur
    TH_CHECK_EQ(th_view_overlay_block(3, 25, 0), 31); // 25 solo es de las dobles
    for (int d = 0; d < 2; d++)
        for (int b = 0; b < 256; b++) {
            if (!d && (b == 25 || b == 28 || b == 29 || b == 32))
                continue;
            int c = b;
            for (int r = 0; r < 4; r++)
                c = th_view_overlay_block(1, c, d);
            TH_CHECK_EQ(c, b);
            TH_CHECK_EQ(th_view_overlay_block(2, b, d), th_view_overlay_block(1, th_view_overlay_block(1, b, d), d));
        }

    // La casilla (10, 10) cae en (64, 40) de una pantalla de 128 × 96: su
    // suelo ocupa de (48, 40) a (79, 55).
    th_view v = {.unit = 16, .x = -64, .y = 120, .w = 128, .h = 96};
    th_drawlist l = {.items = elementos_prueba, .capacity = 512};
    th_scene sc = {.map = m, .sheets = {&bloques, &sprites}, .anims = &anims};
    TH_CHECK(th_view_scene(&v, &sc, &l) > 0);
    const uint32_t sin_nada = th_drawlist_hash(&l);
    const int suelos = l.floor_count, total = l.count;
    TH_CHECK_EQ(suelos, total); // solo suelos
    pintar_prueba(&v, &sc, &l, rgb_a);
    TH_CHECK_RGB(pixel(rgb_a, 64, 48), 10, 0, 0);
    // Con unas superposiciones vacías, la misma lista.
    sc.overlays = &ov;
    TH_CHECK(th_view_scene(&v, &sc, &l) > 0);
    TH_CHECK_EQ(th_drawlist_hash(&l), sin_nada);

    // El bloque 24 al 50 % sobre el suelo de (10, 10): un elemento más, detrás
    // de los suelos, y el color mezclado (20·128 + 10·127 + 127) / 255 = 15.
    TH_CHECK_EQ(th_view_overlays_set(&ov, 10, 10, 24, TH_DRAW_ALPHA50), 0);
    TH_CHECK(th_view_scene(&v, &sc, &l) > 0);
    TH_CHECK_EQ(l.floor_count, suelos);
    TH_CHECK_EQ(l.count, total + 1);
    const th_draw *d = &l.items[l.floor_count];
    TH_CHECK(d->sheet == TH_SHEET_BLOCKS && d->sprite == 24 && d->flags == TH_DRAW_ALPHA50 && d->x == 48 &&
             d->y == 40);
    pintar_prueba(&v, &sc, &l, rgb_a);
    TH_CHECK_RGB(pixel(rgb_a, 64, 48), 15, 0, 0);
    TH_CHECK_RGB(pixel(rgb_a, 48, 40), 15, 0, 0);
    TH_CHECK_RGB(pixel(rgb_a, 80, 48), 10, 0, 0); // la casilla de al lado, sin tocar
    // Con la paleta fantasma roja, el 24 (20) en rojo: (0·128 + 10·127 + 127) / 255 = 5 de rojo
    // y (20·128 + 127) / 255 = 10 de azul.
    TH_CHECK_EQ(th_view_overlays_set(&ov, 10, 10, 24, TH_DRAW_ALPHA50 | TH_DRAW_PAL_RED), 0);
    TH_CHECK(th_view_scene(&v, &sc, &l) > 0);
    pintar_prueba(&v, &sc, &l, rgb_a);
    TH_CHECK_RGB(pixel(rgb_a, 64, 48), 5, 0, 10);

    // Un empleado fantasma en (10, 10), en gris: entra entre las entidades y se
    // pinta encima del plano, con la paleta gris (0, 30, 0).
    TH_CHECK_EQ(th_view_overlays_set(&ov, 10, 10, 24, TH_DRAW_ALPHA50), 0);
    static const th_walk_anims clase = {.name = "Prueba", .idle_north = 0, .idle_east = 1};
    th_view_entity e;
    th_vista_persona_fantasma(&clase, &anims, 10, 10, TH_DIR_EAST, 0, TH_ANIM_PAL_GREY, NULL, &e);
    TH_CHECK(e.tile_x == 10 && e.tile_y == 10 && e.anim == 1 && e.flags == TH_ANIM_PAL_GREY);
    th_vista_persona_fantasma(&clase, &anims, 10, 10, TH_DIR_SOUTH, 0, 0, NULL, &e);
    TH_CHECK(e.anim == 1 && e.flags == TH_ANIM_FLIP_H);
    th_vista_persona_fantasma(&clase, &anims, 10, 10, TH_DIR_EAST, 0, TH_ANIM_PAL_GREY, NULL, &e);
    sc.entities = &e;
    sc.entity_count = 1;
    TH_CHECK(th_view_scene(&v, &sc, &l) > 0);
    const uint32_t con_fantasma = th_drawlist_hash(&l);
    d = &l.items[l.count - 1];
    TH_CHECK(d->sheet == TH_SHEET_SPRITES && d->flags == TH_DRAW_PAL_GREY && d->x == 64 && d->y == 40);
    pintar_prueba(&v, &sc, &l, rgb_a);
    TH_CHECK_RGB(pixel(rgb_a, 64, 40), 0, 30, 0);
    TH_CHECK_RGB(pixel(rgb_a, 67, 43), 0, 30, 0);
    TH_CHECK_RGB(pixel(rgb_a, 68, 43), 15, 0, 0); // el plano, al lado
    // Sin paleta gris en el rasterizador, la de su hoja.
    th_raster r = {.rgb = rgb_b, .w = v.w, .h = v.h, .paleta = (const uint8_t(*)[3])pal_n};
    th_raster_clear(&r, 0, 0, 0);
    th_raster_draw(&r, sc.sheets, &l);
    TH_CHECK_RGB(pixel(rgb_b, 64, 40), 30, 0, 0);
    // Determinista: la misma lista y los mismos píxeles otra vez.
    TH_CHECK(th_view_scene(&v, &sc, &l) > 0);
    TH_CHECK_EQ(th_drawlist_hash(&l), con_fantasma);
    pintar_prueba(&v, &sc, &l, rgb_b);
    TH_CHECK(memcmp(rgb_a, rgb_b, sizeof rgb_a) == 0);

    // Encima de todo (como UIPlaceStaff): el mismo sprite al final de la lista.
    sc.entity_count = 0;
    TH_CHECK(th_view_scene(&v, &sc, &l) > 0);
    const int n0 = l.count;
    th_vista_persona_fantasma(&clase, &anims, 10, 10, TH_DIR_EAST, 0, TH_ANIM_PAL_RED | TH_ANIM_PAL_GREY, NULL, &e);
    TH_CHECK_EQ(th_view_entity_on_top(&v, &sc, &e, &l), 0);
    TH_CHECK_EQ(l.count, n0 + 1);
    d = &l.items[l.count - 1];
    TH_CHECK(d->sheet == TH_SHEET_SPRITES && d->flags == TH_DRAW_PAL_RED && d->x == 64 && d->y == 40);
    pintar_prueba(&v, &sc, &l, rgb_a);
    TH_CHECK_RGB(pixel(rgb_a, 64, 40), 0, 0, 30); // con las dos banderas, la roja
    l.capacity = l.count; // sin sitio
    TH_CHECK_EQ(th_view_entity_on_top(&v, &sc, &e, &l), -1);
    l.capacity = 512;

    // Con la cámara girada una vez, la superposición de (10, 10) del mundo va
    // en su casilla de la vista, y la marca de puerta norte de (11, 10), como
    // la del este.
    th_view_overlays_clear(&ov);
    TH_CHECK_EQ(th_view_overlays_set(&ov, 10, 10, 24, TH_DRAW_ALPHA50), 0);
    TH_CHECK_EQ(th_view_overlays_set(&ov, 11, 10, 26, 0), 0);
    TH_CHECK_EQ(th_rot_map_update(g, m, 1), 1);
    int vx, vy, sx, sy, wx, wy;
    th_rot_tile(1, 10, 10, &vx, &vy);
    th_rot_tile(1, 11, 10, &wx, &wy);
    th_view vg = v;
    th_view_tile(&vg, vx, vy, &sx, &sy);
    vg.x += sx - 64;
    vg.y += sy - 40;
    sc.rot = g;
    TH_CHECK(th_view_scene(&vg, &sc, &l) > 0);
    int vistos = 0;
    for (int i = l.floor_count; i < l.count; i++) {
        const th_draw *o = &l.items[i];
        int tx, ty;
        th_view_tile(&vg, o->sprite == 24 ? vx : wx, o->sprite == 24 ? vy : wy, &tx, &ty);
        TH_CHECK((o->sprite == 24 || o->sprite == 27) && o->x == tx - 16 && o->y == ty);
        vistos++;
    }
    TH_CHECK_EQ(vistos, 2);

    // Una puerta doble este en (11, 10..12): 28 al norte, 24 y 27 al sur.
    // Girada una vez, es la doble sur de la vista: 30 en la mitad este y 29
    // en la oeste (una sencilla, el 27, sería el 30). Y dos superposiciones
    // puestas a mano fuera del mapa (x e y son uint8_t, sin pasar por
    // th_view_overlays_set) no se dibujan ni se salen de los bits de casillas.
    th_view_overlays_clear(&ov);
    TH_CHECK_EQ(th_view_overlays_set(&ov, 11, 10, 28, 0), 0);
    TH_CHECK_EQ(th_view_overlays_set(&ov, 11, 11, 24, 0), 0);
    TH_CHECK_EQ(th_view_overlays_set(&ov, 11, 12, 27, 0), 0);
    ov.items[ov.count++] = (th_view_overlay){.x = 255, .y = 127, .block = 24};
    ov.items[ov.count++] = (th_view_overlay){.x = 3, .y = 200, .block = 24};
    TH_CHECK(th_view_scene(&vg, &sc, &l) > 0);
    int xn, yn, xs, ys, n_norte = 0, n_sur = 0, n_ovl = 0;
    th_rot_tile(1, 11, 10, &wx, &wy);
    th_view_tile(&vg, wx, wy, &xn, &yn);
    th_rot_tile(1, 11, 12, &wx, &wy);
    th_view_tile(&vg, wx, wy, &xs, &ys);
    for (int i = l.floor_count; i < l.count; i++) {
        const th_draw *o = &l.items[i];
        n_ovl++;
        n_norte += o->sprite == 30 && o->x == xn - 16 && o->y == yn;
        n_sur += o->sprite == 29 && o->x == xs - 16 && o->y == ys;
    }
    TH_CHECK_EQ(n_ovl, 3);
    TH_CHECK_EQ(n_norte, 1);
    TH_CHECK_EQ(n_sur, 1);
    sc.rot = NULL;

    // El plano de una sala de 3 × 2 en (20, 20): 6 suelos (las flechas en las
    // cuatro casillas del centro, en la esquina) y 2 (3 + 2) − 1 = 9 paredes (la esquina en lugar de un tramo); con
    // la casilla (22, 21) mala, su suelo es el 67 y el tramo sur de su
    // columna, rojo.
    th_view_overlays_clear(&ov);
    static th_view_entity paredes[16];
    TH_CHECK_EQ(th_view_blueprint(&anims, 20, 20, 3, 2, 0, NULL, 0, &ov, paredes, 16), 9);
    TH_CHECK_EQ(ov.count, 6);
    int flechas = 0;
    for (int i = 0; i < ov.count; i++) {
        const th_view_overlay *o = &ov.items[i];
        TH_CHECK(o->flags == TH_DRAW_ALPHA50);
        if (o->x == 20 && o->y == 20)
            TH_CHECK_EQ(o->block, 39);
        if (o->x == 21 && o->y == 21)
            TH_CHECK_EQ(o->block, 40);
        if (o->x == 22)
            TH_CHECK_EQ(o->block, 24);
        flechas += o->block >= 37 && o->block <= 40;
    }
    TH_CHECK_EQ(flechas, 4);
    TH_CHECK(paredes[0].anim == 124 && paredes[0].tile_x == 20 && paredes[0].tile_y == 20 && paredes[0].flags == 0);
    for (int i = 1; i < 9; i++)
        TH_CHECK(paredes[i].anim == 120 && !(paredes[i].flags & TH_ANIM_PAL_RED));
    const uint8_t malas[6] = {0, 0, 0, 0, 0, 1};
    th_view_overlays_clear(&ov);
    TH_CHECK_EQ(th_view_blueprint(&anims, 20, 20, 3, 2, 0, malas, 0, &ov, paredes, 16), 9);
    int rojas = 0;
    for (int i = 0; i < 9; i++)
        if (paredes[i].flags & TH_ANIM_PAL_RED) {
            rojas++;
            // el tramo sur de la columna 22 y el tramo este de la fila 21
            TH_CHECK((paredes[i].tile_x == 22 && paredes[i].tile_y == 22 && paredes[i].py == -1 &&
                      !(paredes[i].flags & TH_ANIM_FLIP_H)) ||
                     (paredes[i].tile_x == 23 && paredes[i].tile_y == 21 && paredes[i].px == 2 &&
                      (paredes[i].flags & TH_ANIM_FLIP_H)));
        }
    TH_CHECK_EQ(rojas, 2);
    for (int i = 0; i < ov.count; i++)
        if (ov.items[i].x == 22 && ov.items[i].y == 21)
            TH_CHECK_EQ(ov.items[i].block, 67);
    // Todo malo: sin flechas, todo 67 y todo rojo; girado, las mismas paredes
    // en el rectángulo girado; fuera del mapa, -1.
    th_view_overlays_clear(&ov);
    TH_CHECK_EQ(th_view_blueprint(&anims, 20, 20, 3, 2, 1, NULL, 1, &ov, paredes, 16), 9);
    for (int i = 0; i < ov.count; i++)
        TH_CHECK_EQ(ov.items[i].block, 67);
    th_rot_tile(1, 20, 20, &vx, &vy); // la esquina norte del mundo; en la vista, la noreste
    th_rot_tile(1, 22, 21, &wx, &wy);
    for (int i = 0; i < 9; i++)
        TH_CHECK(paredes[i].flags & TH_ANIM_PAL_RED);
    TH_CHECK(paredes[0].anim == 124 && paredes[0].tile_x == (vx < wx ? vx : wx) &&
             paredes[0].tile_y == (vy < wy ? vy : wy));
    TH_CHECK_EQ(th_view_blueprint(&anims, 125, 20, 3, 2, 0, NULL, 0, &ov, paredes, 16), -1);
    // En la esquina (0, 0) del mundo cabe sin girar; girado, su borde sur o
    // este de la vista sería la casilla 128: -1 sin tocar ov. Al lado, sí.
    for (int r = 0; r < 4; r++) {
        th_view_overlays_clear(&ov);
        TH_CHECK_EQ(th_view_blueprint(&anims, 0, 0, 3, 2, r, NULL, 0, &ov, paredes, 16), r ? -1 : 9);
        TH_CHECK_EQ(ov.count, r ? 0 : 6);
        th_view_overlays_clear(&ov);
        TH_CHECK_EQ(th_view_blueprint(&anims, 1, 1, 3, 2, r, NULL, 0, &ov, paredes, 16), 9);
        for (int i = 0; i < 9; i++)
            TH_CHECK(paredes[i].tile_x < TH_MAP_WIDTH && paredes[i].tile_y < TH_MAP_HEIGHT);
    }
    TH_CHECK_EQ(th_view_blueprint(&anims, 20, 20, 3, 2, 0, NULL, 0, &ov, paredes, 4), 4);

    // Los bordes: el norte de (10, 10), girado una vez, es el este de su
    // casilla girada, que es el oeste de la vecina.
    int ex, ey, oeste;
    th_view_edge(0, 10, 10, TH_DIR_NORTH, &ex, &ey, &oeste);
    TH_CHECK(ex == 10 && ey == 10 && !oeste);
    th_view_edge(0, 10, 10, TH_DIR_SOUTH, &ex, &ey, &oeste);
    TH_CHECK(ex == 10 && ey == 11 && !oeste);
    th_rot_tile(1, 10, 10, &vx, &vy);
    th_view_edge(1, 10, 10, TH_DIR_NORTH, &ex, &ey, &oeste);
    TH_CHECK(ex == vx + 1 && ey == vy && oeste);
    th_view_edge(1, 10, 10, TH_DIR_WEST, &ex, &ey, &oeste);
    TH_CHECK(ex == vx && ey == vy && !oeste);

    // El objeto fantasma: un banco de una casilla con idle_animations norte y
    // este; al sur, el espejo del este, con su animation_offset.
    static uint8_t trg[2048];
    const char *json = "{\"bench\": {\"id\": \"bench\", \"class\": \"Object\", \"thob\": 2,"
                       " \"idle_animations\": {\"north\": 0, \"east\": 1},"
                       " \"orientations\": {\"north\": {\"footprint\": [[0, 0]], \"animation_offset\": [3, -2]},"
                       " \"east\": {\"footprint\": [[0, 0]]},"
                       " \"south\": {\"footprint\": [[0, 0]], \"render_attach_position\": [0, 1],"
                       " \"animation_offset\": [5, 6]},"
                       " \"west\": {\"footprint\": [[0, 0]]}}}}";
    const size_t n = th_test_trg(json, trg, sizeof trg);
    TH_CHECK(n > 0);
    th_rules reglas;
    TH_CHECK_EQ(th_rules_init(&reglas, trg, n), 0);
    const th_rnode banco = th_rules_get(&reglas, reglas.root, "bench");
    th_view_entity o[4];
    TH_CHECK_EQ(th_vista_objeto_fantasma(&reglas, &anims, banco, 10, 10, TH_DIR_NORTH, 0, TH_ANIM_PAL_GREY, o, 4), 1);
    TH_CHECK(o[0].tile_x == 10 && o[0].tile_y == 10 && o[0].anim == 0 && o[0].flags == TH_ANIM_PAL_GREY &&
             o[0].px == 3 && o[0].py == -2 && o[0].drawing_layer == TH_LAYER_ENTITY);
    TH_CHECK_EQ(th_vista_objeto_fantasma(&reglas, &anims, banco, 10, 10, TH_DIR_SOUTH, 0, 0, o, 4), 1);
    TH_CHECK(o[0].tile_x == 10 && o[0].tile_y == 11 && o[0].anim == 1 && o[0].flags == TH_ANIM_FLIP_H &&
             o[0].px == 5 && o[0].py == 6);
    // Girado: el norte del mundo es el este de la vista, en la casilla girada.
    TH_CHECK_EQ(th_vista_objeto_fantasma(&reglas, &anims, banco, 10, 10, TH_DIR_NORTH, 1, TH_ANIM_PAL_RED, o, 4), 1);
    th_rot_tile(1, 10, 10, &vx, &vy);
    TH_CHECK(o[0].tile_x == vx && o[0].tile_y == vy && o[0].anim == 1 && o[0].flags == TH_ANIM_PAL_RED);
    TH_CHECK_EQ(th_vista_objeto_fantasma(&reglas, &anims, banco, 10, 10, TH_DIR_NORTH, 0, 0, o, 0), 0);
    // En la escena y encima de todo, en gris.
    TH_CHECK_EQ(th_vista_objeto_fantasma(&reglas, &anims, banco, 10, 10, TH_DIR_NORTH, 0, TH_ANIM_PAL_GREY, o, 4), 1);
    th_view_overlays_clear(&ov);
    sc.entities = o;
    sc.entity_count = 1;
    TH_CHECK(th_view_scene(&v, &sc, &l) > 0);
    d = &l.items[l.count - 1];
    TH_CHECK(d->sheet == TH_SHEET_SPRITES && d->flags == TH_DRAW_PAL_GREY && d->x == 64 + 3 / 2 && d->y == 40 - 1);
}

void th_test_vista(void)
{
    test_sol();
    test_dibujo(&mapa_prueba, &giro_prueba);
    // Casillas: la inversa deshace la rotación y cuatro giros son ninguno.
    static const int casillas[][2] = {{0, 0}, {127, 0}, {0, 127}, {127, 127}, {62, 75}, {5, 100}};
    for (unsigned i = 0; i < sizeof casillas / sizeof casillas[0]; i++)
        for (int r = 0; r < 4; r++) {
            int vx, vy, x, y, a = casillas[i][0], b = casillas[i][1];
            th_rot_tile(r, a, b, &vx, &vy);
            th_rot_tile_inv(r, vx, vy, &x, &y);
            TH_CHECK(x == a && y == b);
            for (int k = 0; k < 4; k++)
                th_rot_tile(r, a, b, &a, &b);
            TH_CHECK(a == casillas[i][0] && b == casillas[i][1]);
        }
    // Una rotación lleva el norte al este (r = 1: (x, y) a (W−1−y, x)).
    int vx, vy, wx, wy;
    th_rot_tile(1, 10, 20, &vx, &vy);
    th_rot_tile(1, 10, 19, &wx, &wy); // la casilla del norte
    TH_CHECK(wx == vx + 1 && wy == vy && th_rot_dir(1, TH_DIR_NORTH) == TH_DIR_EAST);
    // Desplazamientos: (−32, −16) es una casilla hacia el oeste (−x); girado
    // r = 1 va hacia el norte (−y): (32, −16).
    th_rot_offset(1, -32, -16, &vx, &vy);
    TH_CHECK(vx == 32 && vy == -16);
    // La cámara: girar y deshacer el giro deja el centro donde estaba.
    th_view v = {.unit = 16, .x = -368, .y = 976, .w = 320, .h = 240};
    for (int r = 1; r < 4; r++) {
        th_view g = v;
        th_view_rotate(&g, 0, r);
        th_view_rotate(&g, r, 0);
        TH_CHECK(g.x == v.x && g.y == v.y);
    }
    // El puntero: el centro del rombo de una casilla es esa casilla.
    for (unsigned i = 0; i < sizeof casillas / sizeof casillas[0]; i++) {
        int sx, sy, x, y;
        th_view_tile(&v, casillas[i][0], casillas[i][1], &sx, &sy);
        th_view_pick(&v, sx, sy + v.unit / 2, &x, &y);
        TH_CHECK(x == casillas[i][0] && y == casillas[i][1]);
    }
    // Las paredes: girar r1 y después r2 es girar r1 + r2, y cuatro giros
    // dejan la misma pieza (menos las ventanas largas, que solo existen por
    // fuera: por dentro pasan a la pared lisa).
    int paredes = 0;
    for (int b = 82; b <= 164; b++) {
        if (b == 120 || b == 121 || b == 156)
            continue;
        for (int r1 = 1; r1 < 4; r1++)
            for (int r2 = 1; r2 < 4; r2++) {
                const int una = th_rot_layer(r1, b);
                const int dos = th_rot_layer(r2, una);
                TH_CHECK_EQ(dos, (r1 + r2) % 4 ? th_rot_layer((r1 + r2) % 4, b) : b);
            }
        paredes++;
    }
    TH_CHECK_EQ(paredes, 80);
    // Una pared norte por dentro, girada 90°: el borde mira al este, así que
    // es la pared oeste de la casilla vecina vista desde el otro lado (la de
    // fuera). La azul: 86 → 95.
    TH_CHECK_EQ(th_rot_layer(1, 86), 95);
    // Una ventana de dos casillas cambia de mitad al recorrer el borde al revés.
    TH_CHECK_EQ(th_rot_layer(2, 116), 126); // norte _1 por fuera → norte _2 por dentro
    // Los bordillos de césped y camino y los setos (familias completas): sin
    // espejos, y cuatro giros dejan la misma pieza.
    static const uint8_t completas[] = {6, 7, 8, 9, 10, 11, 12, 13, 14, 61, 62, 63, 59, 60, 65, 68, 77, 78, 79, 80};
    for (unsigned i = 0; i < sizeof completas; i++) {
        int b = completas[i];
        for (int k = 0; k < 4; k++) {
            const int g = th_rot_floor(1, b);
            TH_CHECK(!(g & TH_ROT_FLIP));
            b = g & 0xFF;
        }
        TH_CHECK_EQ(b, completas[i]);
    }
    for (int s = 176; s <= 186; s++) {
        int b = s;
        for (int k = 0; k < 4; k++) {
            const int g = th_rot_layer(1, b);
            TH_CHECK((g & TH_ROT_TILE) && !(g & TH_ROT_FLIP));
            b = g & 0xFF;
        }
        TH_CHECK_EQ(b, s);
    }
    // Las carreteras van de norte a sur: giradas 90°, en espejo.
    TH_CHECK_EQ(th_rot_floor(1, 50), 50 | TH_ROT_FLIP);
    TH_CHECK_EQ(th_rot_floor(2, 50), 58);
    TH_CHECK_EQ(th_rot_floor(3, 50), 58 | TH_ROT_FLIP);
}
