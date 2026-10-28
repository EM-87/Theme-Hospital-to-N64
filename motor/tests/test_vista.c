// Pruebas de las cuatro vistas (vista/girar.c y vista.c, hito 6.2): la
// geometría de las rotaciones y las tablas de piezas giradas.
#include "girar.h"
#include "th/humanoid.h"
#include "th_test.h"
#include "vista.h"

void th_test_vista(void)
{
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
