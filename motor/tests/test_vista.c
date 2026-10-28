// Pruebas de las cuatro vistas (vista/girar.c y vista.c, hitos 6.2 y 6.2b):
// la geometría de las rotaciones, las tablas de piezas giradas y el sol fijo.
#include <string.h>

#include "girar.h"
#include "th/humanoid.h"
#include "th_test.h"
#include "vista.h"

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
    static th_map m;
    static th_rot_map g;
    memset(&m, 0, sizeof m);
    for (int x = 40; x <= 45; x++) {
        th_map_tile(&m, x, 40)->wall_n = 86;
        th_map_tile(&m, x, 45)->wall_n = 94;
    }
    for (int y = 40; y <= 44; y++) {
        th_map_tile(&m, 40, y)->wall_w = 87;
        th_map_tile(&m, 46, y)->wall_w = 95;
    }
    th_map_tile(&m, 60, 60)->wall_w = 87;
    th_map_tile(&m, 70, 70)->wall_w = 87;
    th_map_tile(&m, 70, 70)->wall_n = 86;
    th_map_update_shadows(&m);
    for (int r = 1; r < 4; r++) {
        memset(&g, 0, sizeof g);
        TH_CHECK_EQ(th_rot_map_update(&g, &m, r), 1);
        int con_sombra = 0, en_pared = 0, en_el_mundo = 0;
        for (int y = 0; y < TH_MAP_HEIGHT; y++)
            for (int x = 0; x < TH_MAP_WIDTH; x++) {
                int vx, vy;
                th_rot_tile(r, x, y, &vx, &vy);
                const uint32_t f = th_map_tile_c(&m, x, y)->flags;
                const uint8_t vf = g.tiles[vy * TH_MAP_WIDTH + vx].flags;
                TH_CHECK_EQ(vf & (TH_VT_SHADOW_HALF | TH_VT_SHADOW_FULL),
                            (int)((f >> 11) & (TH_VT_SHADOW_HALF | TH_VT_SHADOW_FULL)));
                // La sombra sobre la pared: en la cara sur de la pared norte del
                // mundo, que solo se ve con r = 3, como pared oeste de la vista.
                TH_CHECK(!(vf & TH_VT_SHADOW_WALL));
                TH_CHECK_EQ(!!(vf & TH_VT_SHADOW_WALL_W), r == 3 && (f & TH_TILE_SHADOW_WALL));
                if (vf & TH_VT_SHADOW_WALL_W)
                    TH_CHECK_EQ(g.tiles[vy * TH_MAP_WIDTH + vx].wall_w, th_wall_mirror(th_rot_layer(3, 86)));
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
        const th_vtile *t = &g.tiles[vy * TH_MAP_WIDTH + vx];
        const int girada = th_rot_layer(r, 87);
        if (r == 1)
            TH_CHECK_EQ(t->wall_n, th_wall_mirror(girada));
        else if (r == 2)
            TH_CHECK_EQ(t[1].wall_w, th_wall_mirror(girada));
        else
            TH_CHECK_EQ(t[TH_MAP_WIDTH].wall_n, girada);
    }
}

void th_test_vista(void)
{
    test_sol();
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
