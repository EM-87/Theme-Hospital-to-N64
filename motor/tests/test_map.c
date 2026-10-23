// Pruebas del mapa (th/map.h) con niveles TMP1 sintéticos: errores de
// formato, tránsito, sombras, parcelas con y sin dueño, paredes divisorias y
// las correcciones de map.lua. La comparación con los niveles reales está en
// th64sim mapa (contra el volcado del oráculo de CorsixTH).
#include <string.h>

#include "th/map.h"
#include "th_test.h"

#define CASILLA 10
#define CABECERA 32

static uint8_t tmp[CABECERA + CASILLA * TH_MAP_WIDTH * TH_MAP_HEIGHT];
static th_map mapa;

static void tmp_nuevo(int parcelas)
{
    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, "TMP1", 4);
    tmp[5] = TH_MAP_WIDTH;
    tmp[7] = TH_MAP_HEIGHT;
    tmp[8] = 1;
    tmp[9] = (uint8_t)parcelas;
    tmp[11] = CASILLA;
    for (int i = 0; i < TH_MAP_WIDTH * TH_MAP_HEIGHT; i++)
        tmp[CABECERA + CASILLA * i] = 1; // césped
}

static uint8_t *tmp_casilla(int x, int y)
{
    return &tmp[CABECERA + CASILLA * (y * TH_MAP_WIDTH + x)];
}

// Rectángulo de suelo de interior con todas las banderas del fichero.
static void tmp_interior(int x0, int y0, int x1, int y1, int parcela)
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            uint8_t *c = tmp_casilla(x, y);
            c[0] = 17;
            c[3] = TH_TILE_FILE_FLAGS;
            c[8] = (uint8_t)parcela;
        }
}

static int cargar(int nivel)
{
    return th_map_load(&mapa, tmp, sizeof(tmp), nivel);
}

static uint32_t banderas(int x, int y)
{
    return th_map_tile(&mapa, x, y)->flags;
}

static void test_formato(void)
{
    tmp_nuevo(1);
    TH_CHECK_EQ(th_map_load(&mapa, tmp, 31, 0), TH_MAP_ERR_SHORT);
    TH_CHECK_EQ(th_map_load(&mapa, tmp, sizeof(tmp) - 1, 0), TH_MAP_ERR_SHORT);
    tmp[0] = 'X';
    TH_CHECK_EQ(cargar(0), TH_MAP_ERR_MAGIC);
    tmp_nuevo(1);
    tmp[5] = 64;
    TH_CHECK_EQ(cargar(0), TH_MAP_ERR_FORMAT);
    tmp_nuevo(0);
    TH_CHECK_EQ(cargar(0), TH_MAP_ERR_PARCELS);
    tmp_nuevo(2);
    tmp_casilla(3, 3)[8] = 2; // parcela que no existe
    TH_CHECK_EQ(cargar(0), TH_MAP_ERR_PARCELS);
    tmp_nuevo(1);
    tmp[12 + 1] = 62;
    tmp[14 + 1] = 75;
    TH_CHECK_EQ(cargar(0), TH_MAP_OK);
    TH_CHECK_EQ(mapa.camera_x, 62);
    TH_CHECK_EQ(mapa.camera_y, 75);
    TH_CHECK_EQ(mapa.players, 1);
    TH_CHECK_EQ(th_map_hash(&mapa), th_map_hash(&mapa));
}

static void test_transito(void)
{
    tmp_nuevo(1);
    tmp_casilla(10, 10)[1] = 120; // pared norte
    tmp_casilla(20, 20)[2] = 121; // pared oeste
    TH_CHECK_EQ(cargar(0), TH_MAP_OK);
    const uint32_t todas = TH_TILE_TRAVEL_N | TH_TILE_TRAVEL_E | TH_TILE_TRAVEL_S | TH_TILE_TRAVEL_W;
    TH_CHECK_EQ(banderas(50, 50) & todas, todas);
    TH_CHECK_EQ(banderas(0, 0) & todas, TH_TILE_TRAVEL_E | TH_TILE_TRAVEL_S);
    TH_CHECK_EQ(banderas(127, 127) & todas, TH_TILE_TRAVEL_N | TH_TILE_TRAVEL_W);
    TH_CHECK_EQ(banderas(10, 10) & todas, todas & ~TH_TILE_TRAVEL_N);
    TH_CHECK_EQ(banderas(10, 9) & todas, todas & ~TH_TILE_TRAVEL_S);
    TH_CHECK_EQ(banderas(20, 20) & todas, todas & ~TH_TILE_TRAVEL_W);
    TH_CHECK_EQ(banderas(19, 20) & todas, todas & ~TH_TILE_TRAVEL_E);
}

static void test_sombras(void)
{
    const uint32_t s = TH_TILE_SHADOW_HALF | TH_TILE_SHADOW_FULL | TH_TILE_SHADOW_WALL;
    tmp_nuevo(1);
    tmp_casilla(30, 30)[2] = 100; // pared oeste sola: sombra en el norte y su esquina
    tmp_casilla(40, 40)[2] = 100; // dos paredes oeste seguidas
    tmp_casilla(40, 39)[2] = 100;
    tmp_casilla(50, 50)[1] = 100; // pared norte y oeste: sombra sobre la pared
    tmp_casilla(50, 50)[2] = 100;
    tmp_casilla(60, 60)[2] = 200; // adorno, no es pared
    tmp_casilla(5, 0)[2] = 82;    // fila 0: no hay vecino al norte
    tmp_casilla(70, 70)[2] = 164;
    TH_CHECK_EQ(cargar(0), TH_MAP_OK);
    TH_CHECK_EQ(banderas(30, 30) & s, TH_TILE_SHADOW_HALF);
    TH_CHECK_EQ(banderas(30, 29) & s, TH_TILE_SHADOW_FULL);
    TH_CHECK_EQ(banderas(29, 29) & s, TH_TILE_SHADOW_FULL);
    TH_CHECK_EQ(banderas(31, 29) & s, 0);
    TH_CHECK_EQ(banderas(40, 40) & s, TH_TILE_SHADOW_HALF);
    TH_CHECK_EQ(banderas(40, 39) & s, TH_TILE_SHADOW_HALF | TH_TILE_SHADOW_FULL);
    TH_CHECK_EQ(banderas(39, 39) & s, 0);
    TH_CHECK_EQ(banderas(40, 38) & s, TH_TILE_SHADOW_FULL);
    TH_CHECK_EQ(banderas(39, 38) & s, TH_TILE_SHADOW_FULL);
    TH_CHECK_EQ(banderas(50, 50) & s, TH_TILE_SHADOW_HALF | TH_TILE_SHADOW_WALL);
    TH_CHECK_EQ(banderas(50, 49) & s, 0);
    TH_CHECK_EQ(banderas(60, 60) & s, 0);
    TH_CHECK_EQ(banderas(5, 0) & s, TH_TILE_SHADOW_HALF);
    TH_CHECK_EQ(banderas(70, 70) & s, TH_TILE_SHADOW_HALF);
    // Un objeto alto cuenta como pared.
    th_map_tile(&mapa, 80, 80)->flags |= TH_TILE_TALL_W;
    th_map_tile(&mapa, 80, 80)->flags |= TH_TILE_TALL_N;
    th_map_update_shadows(&mapa);
    TH_CHECK_EQ(banderas(80, 80) & s, TH_TILE_SHADOW_HALF | TH_TILE_SHADOW_WALL);
    TH_CHECK_EQ(banderas(80, 79) & s, 0);
}

static void test_parcelas(void)
{
    // Parcela 1 (del jugador) entre la 3 a su oeste y la 2 a su este y al sur.
    tmp_nuevo(4);
    tmp_interior(10, 10, 19, 19, 1);
    tmp_interior(20, 10, 29, 19, 2);
    tmp_interior(10, 20, 19, 24, 2);
    tmp_interior(5, 10, 9, 19, 3);
    for (int y = 10; y <= 19; y++) {
        tmp_casilla(20, y)[5] = 143; // divisorias originales (bloque 141 en crudo)
        tmp_casilla(10, y)[5] = 143;
    }
    for (int x = 10; x <= 19; x++)
        tmp_casilla(x, 20)[4] = 142;
    TH_CHECK_EQ(cargar(1), TH_MAP_OK);
    TH_CHECK_EQ(th_map_parcel_owner(&mapa, 0), 0);
    TH_CHECK_EQ(th_map_parcel_owner(&mapa, 1), 1);
    TH_CHECK_EQ(th_map_parcel_owner(&mapa, 2), 0);
    TH_CHECK_EQ(th_map_parcel_owner(&mapa, 3), 0);
    TH_CHECK_EQ(mapa.parcel_tiles[1], 100);
    TH_CHECK_EQ(mapa.parcel_tiles[2], 150);
    TH_CHECK_EQ(mapa.parcel_tiles[3], 50);
    // Sin dueño: césped a rayas, un adorno y sin banderas.
    TH_CHECK_EQ(th_map_tile(&mapa, 21, 15)->floor, 3);
    TH_CHECK_EQ(th_map_tile(&mapa, 22, 15)->floor, 1);
    TH_CHECK_EQ(th_map_tile(&mapa, 24, 16)->wall_n, 192 + (24 ^ 16) % 9);
    TH_CHECK_EQ(banderas(22, 15) & TH_TILE_FILE_FLAGS, 0);
    // Divisorias: bloque 141 + capa del lado sin dueño, 143 + capa del lado con dueño.
    TH_CHECK_EQ(th_map_tile(&mapa, 20, 15)->wall_w, 143);
    TH_CHECK_EQ(th_map_tile(&mapa, 15, 20)->wall_n, 142);
    TH_CHECK_EQ(th_map_tile(&mapa, 10, 15)->wall_w, 145);
    TH_CHECK_EQ(th_map_tile(&mapa, 15, 15)->floor, 17);
    TH_CHECK(!(banderas(19, 15) & TH_TILE_TRAVEL_E));
    TH_CHECK(banderas(20, 15) & TH_TILE_SHADOW_HALF);
    TH_CHECK(banderas(15, 15) & TH_TILE_HOSPITAL);

    // Comprar la parcela 2 quita sus 20 divisorias y restaura el fichero.
    TH_CHECK_EQ(th_map_set_parcel_owner(&mapa, 2, 1), 20);
    TH_CHECK_EQ(th_map_tile(&mapa, 20, 15)->wall_w, 0);
    TH_CHECK_EQ(th_map_tile(&mapa, 15, 20)->wall_n, 0);
    TH_CHECK_EQ(th_map_tile(&mapa, 24, 16)->wall_n, 0);
    TH_CHECK_EQ(th_map_tile(&mapa, 21, 15)->floor, 17);
    TH_CHECK_EQ(th_map_tile(&mapa, 10, 15)->wall_w, 145);
    TH_CHECK(banderas(19, 15) & TH_TILE_TRAVEL_E);
    TH_CHECK(banderas(22, 15) & TH_TILE_HOSPITAL);
    TH_CHECK(!(banderas(20, 15) & TH_TILE_SHADOW_HALF));
    // Venderla la deja como al principio.
    TH_CHECK_EQ(th_map_set_parcel_owner(&mapa, 2, 0), 0);
    TH_CHECK_EQ(th_map_tile(&mapa, 20, 15)->wall_w, 143);
    uint32_t h = th_map_hash(&mapa);
    TH_CHECK_EQ(cargar(1), TH_MAP_OK);
    TH_CHECK_EQ(th_map_hash(&mapa), h);
    // Argumentos que no valen.
    TH_CHECK_EQ(th_map_set_parcel_owner(&mapa, 0, 1), -1);
    TH_CHECK_EQ(th_map_set_parcel_owner(&mapa, 4, 1), -1);
    TH_CHECK_EQ(th_map_set_parcel_owner(&mapa, 1, -1), -1);
}

static void test_correcciones(void)
{
    // Un suelo de exterior que el fichero marca como hospital y transitable.
    tmp_nuevo(2);
    tmp_casilla(30, 30)[0] = 30;
    tmp_casilla(30, 30)[3] = TH_TILE_FILE_FLAGS;
    tmp_casilla(31, 30)[0] = 4; // camino: transitable pero no hospital
    tmp_casilla(31, 30)[3] = TH_TILE_FILE_FLAGS;
    tmp_casilla(57, 71)[0] = 17;
    tmp_casilla(57, 71)[3] = TH_TILE_FILE_FLAGS;
    TH_CHECK_EQ(cargar(0), TH_MAP_OK); // no es de la campaña: sin corregir
    TH_CHECK_EQ(banderas(30, 30) & TH_TILE_FILE_FLAGS, TH_TILE_FILE_FLAGS);
    TH_CHECK_EQ(cargar(1), TH_MAP_OK);
    TH_CHECK_EQ(banderas(30, 30) & TH_TILE_FILE_FLAGS, 0);
    TH_CHECK_EQ(banderas(31, 30) & TH_TILE_FILE_FLAGS, TH_TILE_FILE_FLAGS & ~TH_TILE_HOSPITAL);
    TH_CHECK_EQ(banderas(57, 71) & TH_TILE_FILE_FLAGS, TH_TILE_FILE_FLAGS);
    TH_CHECK_EQ(cargar(6), TH_MAP_OK); // nivel 6: dos casillas más
    TH_CHECK_EQ(banderas(55, 70) & TH_TILE_FILE_FLAGS, TH_TILE_FILE_FLAGS & ~TH_TILE_PASSABLE);
    TH_CHECK_EQ(banderas(57, 71) & TH_TILE_FILE_FLAGS, TH_TILE_FILE_FLAGS & ~TH_TILE_PASSABLE);
}

void th_test_map(void)
{
    test_formato();
    test_transito();
    test_sombras();
    test_parcelas();
    test_correcciones();
}
