// Pruebas del A* (th/path.h), de las animaciones (th/anim.h) y de la acción
// de caminar (th/humanoid.h) con un mapa y unas animaciones sintéticos. La
// comparación con CorsixTH está en th64sim caminos y caminata.
#include <string.h>

#include "th/anim.h"
#include "th/humanoid.h"
#include "th/map.h"
#include "th/path.h"
#include "th_test.h"

#define CABECERA 32
#define CASILLA 10

static uint8_t tmp[CABECERA + CASILLA * TH_MAP_WIDTH * TH_MAP_HEIGHT];
static th_map mapa;
static th_pathfinder buscador;

// Mapa de césped transitable con una pared norte en (20..29, 20).
static void mapa_de_prueba(void)
{
    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, "TMP1", 4);
    tmp[5] = TH_MAP_WIDTH;
    tmp[7] = TH_MAP_HEIGHT;
    tmp[8] = 1;
    tmp[9] = 1;
    tmp[11] = CASILLA;
    for (int i = 0; i < TH_MAP_WIDTH * TH_MAP_HEIGHT; i++) {
        tmp[CABECERA + CASILLA * i] = 1;
        tmp[CABECERA + CASILLA * i + 3] = TH_TILE_PASSABLE;
    }
    for (int x = 20; x < 30; x++)
        tmp[CABECERA + CASILLA * (20 * TH_MAP_WIDTH + x) + 1] = 120;
    // Una casilla no transitable.
    tmp[CABECERA + CASILLA * (5 * TH_MAP_WIDTH + 5) + 3] = 0;
    th_map_load(&mapa, tmp, sizeof(tmp), 0);
}

static void test_caminos(void)
{
    mapa_de_prueba();
    static uint8_t xs[TH_PATH_NODES], ys[TH_PATH_NODES];
    TH_CHECK_EQ(th_path_find(&buscador, &mapa, 10, 10, 13, 12), 5);
    TH_CHECK_EQ(th_path_get(&buscador, xs, ys, TH_PATH_NODES), 6);
    TH_CHECK_EQ(xs[0], 10);
    TH_CHECK_EQ(ys[0], 10);
    TH_CHECK_EQ(xs[5], 13);
    TH_CHECK_EQ(ys[5], 12);
    // Cada paso, a una casilla vecina.
    int vecinos = 1;
    for (int i = 0; i < 5; i++)
        vecinos &= (xs[i + 1] - xs[i]) * (xs[i + 1] - xs[i]) + (ys[i + 1] - ys[i]) * (ys[i + 1] - ys[i]) == 1;
    TH_CHECK(vecinos);
    TH_CHECK_EQ(th_path_find(&buscador, &mapa, 7, 7, 7, 7), 0);
    // La pared obliga a rodear: de (25,19) a (25,20) hay que ir hasta x = 30
    // (5 + 1 + 5 pasos; por x = 19 serían 13).
    TH_CHECK_EQ(th_path_find(&buscador, &mapa, 25, 19, 25, 20), 11);
    TH_CHECK_EQ(th_path_find(&buscador, &mapa, 3, 3, 5, 5), -1); // destino no transitable
    TH_CHECK_EQ(th_path_find(&buscador, &mapa, 3, 3, 200, 5), -1);
    // Desde una casilla no transitable sí se sale.
    TH_CHECK_EQ(th_path_find(&buscador, &mapa, 5, 5, 6, 5), 1);
}

// La búsqueda en anchura da las mismas longitudes que el A* (el primer
// paciente elige así su punto de llegada).
static void test_distancias(void)
{
    mapa_de_prueba();
    static th_pathfinder anchura;
    static const int8_t origen[][2] = {{10, 10}, {25, 19}, {5, 5}, {0, 0}};
    static const int8_t destino[][2] = {{13, 12}, {25, 20}, {5, 5}, {6, 5}, {127, 127}, {30, 21}, {10, 10}};
    int iguales = 0, total = 0;
    for (unsigned o = 0; o < sizeof origen / sizeof origen[0]; o++) {
        th_path_distances(&anchura, &mapa, origen[o][0], origen[o][1]);
        for (unsigned d = 0; d < sizeof destino / sizeof destino[0]; d++) {
            const int a = th_path_find(&buscador, &mapa, origen[o][0], origen[o][1], destino[d][0], destino[d][1]);
            iguales += a == th_path_distance(&anchura, &mapa, destino[d][0], destino[d][1]);
            total++;
        }
    }
    TH_CHECK_EQ(iguales, total);
    th_path_distances(&anchura, &mapa, 25, 19);
    TH_CHECK_EQ(th_path_distance(&anchura, &mapa, 25, 20), 11);
    TH_CHECK_EQ(th_path_distance(&anchura, &mapa, 5, 5), -1); // no transitable
    TH_CHECK_EQ(th_path_distance(&anchura, &mapa, -1, 5), -1);
}

// TAN1 sintético: 27 animaciones de un fotograma cada una (la i, fotograma i),
// salvo la 18 (caminar al este), que encadena los fotogramas 18 → 27 → 18, y
// la 16 (caminar al norte), 16 → 28 → 16.
static uint8_t tan[36 + 27 * 4 + 29 * 8 + 4 + 8];

static void put16(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v >> 16);
    put16(p + 2, v & 0xFFFF);
}

static void anims_de_prueba(th_anims *a)
{
    memset(tan, 0, sizeof(tan));
    memcpy(tan, "TAN1", 4);
    const uint32_t o_anim = 36, o_fot = o_anim + 27 * 4, o_lista = o_fot + 29 * 8, o_ele = o_lista + 4;
    put16(tan + 4, 27);
    put32(tan + 8, 29);
    put32(tan + 12, 2);
    put32(tan + 16, 1);
    put32(tan + 20, o_anim);
    put32(tan + 24, o_fot);
    put32(tan + 28, o_lista);
    put32(tan + 32, o_ele);
    for (int i = 0; i < 27; i++)
        put16(tan + o_anim + 4 * i, (unsigned)i);
    for (int f = 0; f < 29; f++) {
        int sig = f == 18 ? 27 : f == 27 ? 18 : f == 16 ? 28 : f == 28 ? 16 : f;
        put32(tan + o_fot + 8 * f, 0);
        put16(tan + o_fot + 8 * f + 4, (unsigned)sig);
    }
    put16(tan + o_lista, 0);
    put16(tan + o_lista + 2, TH_ANIM_LIST_END);
    TH_CHECK_EQ(th_anims_init(a, tan, sizeof(tan)), 0);
}

static void test_anims(void)
{
    th_anims a;
    anims_de_prueba(&a);
    TH_CHECK_EQ(th_anims_init(&a, tan, 20), -1);
    anims_de_prueba(&a);
    TH_CHECK_EQ(th_anims_first_frame(&a, 18), 18);
    TH_CHECK_EQ(th_anims_next_frame(&a, 18), 27);
    TH_CHECK_EQ(th_anims_next_frame(&a, 27), 18);
    TH_CHECK_EQ(th_anims_next_frame(&a, 500), 500);
    TH_CHECK_EQ(th_anims_first_frame(&a, 500), 0);
    TH_CHECK(th_anims_is_multi_frame(&a, 18));
    TH_CHECK(!th_anims_is_multi_frame(&a, 26));
}

static void test_caminata(void)
{
    mapa_de_prueba();
    th_anims a;
    anims_de_prueba(&a);
    th_world w = {.map = &mapa, .pf = &buscador, .anims = &a};
    static th_humanoid h;
    th_humanoid_init(&w, &h, th_walk_anims_find("Standard Male Patient"));
    TH_CHECK_EQ(h.anim, 26); // quieto mirando al este
    th_humanoid_set_tile(&w, &h, 10, 10);
    // Dos casillas al este: cada paso son 8 ticks, de (-32,-16) a (0,0) en la casilla de destino.
    th_humanoid_set_next_action(&w, &h, th_action_walk(12, 10));
    th_humanoid_queue_action(&h, th_action_idle());
    TH_CHECK_EQ(h.tile_x, 11);
    TH_CHECK_EQ(h.px, -32);
    TH_CHECK_EQ(h.py, -16);
    TH_CHECK_EQ(h.anim, 18);
    TH_CHECK_EQ(h.frame, 18);
    th_humanoid_tick(&w, &h);
    TH_CHECK_EQ(h.px, -28);
    TH_CHECK_EQ(h.py, -14);
    TH_CHECK_EQ(h.frame, 27);
    for (int i = 1; i < 8; i++)
        th_humanoid_tick(&w, &h);
    // Segundo paso: la misma animación sigue sin volver al primer fotograma.
    TH_CHECK_EQ(h.tile_x, 12);
    TH_CHECK_EQ(h.px, -32);
    TH_CHECK_EQ(h.frame, 18);
    for (int i = 0; i < 8; i++)
        th_humanoid_tick(&w, &h);
    // Llega: quieto mirando al este, en la casilla.
    TH_CHECK_EQ(h.tile_x, 12);
    TH_CHECK_EQ(h.px, 0);
    TH_CHECK_EQ(h.py, 0);
    TH_CHECK_EQ(h.anim, 26);
    TH_CHECK_EQ(h.queue_len, 1);
    TH_CHECK_EQ(h.queue[0].kind, TH_ACTION_IDLE);
    // Al oeste: animación del norte en espejo, desde la propia casilla.
    th_humanoid_set_next_action(&w, &h, th_action_walk(11, 10));
    TH_CHECK_EQ(h.tile_x, 12);
    TH_CHECK_EQ(h.anim, 16);
    TH_CHECK_EQ(h.anim_flags, TH_ANIM_FLIP_H);
    TH_CHECK_EQ(h.sx, -4);
    TH_CHECK_EQ(h.sy, -2);
    // Cambiar de destino a mitad de paso: se acaba el paso y luego se va al nuevo.
    th_humanoid_set_next_action(&w, &h, th_action_walk(11, 12));
    TH_CHECK_EQ(h.queue_len, 2);
    for (int i = 0; i < 8; i++)
        th_humanoid_tick(&w, &h);
    TH_CHECK_EQ(h.tile_x, 11);
    TH_CHECK_EQ(h.tile_y, 11); // ya va al sur: casilla de destino del paso
    TH_CHECK_EQ(h.anim, 18);
    TH_CHECK_EQ(h.anim_flags, TH_ANIM_FLIP_H);
    TH_CHECK_EQ(h.px, 32);
    TH_CHECK_EQ(h.py, -16);
    // Sin camino: un tick quieto y se acaba la acción.
    th_humanoid_set_tile(&w, &h, 3, 3);
    h.queue_len = 0;
    th_humanoid_set_next_action(&w, &h, th_action_walk(5, 5));
    TH_CHECK_EQ(h.queue[0].kind, TH_ACTION_WALK);
    th_humanoid_tick(&w, &h);
    TH_CHECK_EQ(h.queue[0].kind, TH_ACTION_IDLE);
}

void th_test_humanoid(void)
{
    test_caminos();
    test_distancias();
    test_anims();
    test_caminata();
}
