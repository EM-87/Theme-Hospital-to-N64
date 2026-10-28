// Las cuatro vistas (fase 6, hito 6.2): la cámara girada 0, 90, 180 o 270
// grados, como en RollerCoaster Tycoon. Theme Hospital solo tiene dibujos
// para una cámara, así que la vista gira el mundo antes de dibujarlo:
//   - cada casilla pasa a su casilla girada;
//   - los suelos y el decorado con dirección (bordillos, estanques,
//     carreteras, setos) pasan a la pieza que dibuja la dirección girada, o a
//     su espejo si solo existe esa, o a la más parecida (sustitutos);
//   - las paredes van por bordes y pasan al borde girado, con la cara que ve
//     la cámara (la interior o la exterior de la sala) y, en las ventanas de
//     dos casillas y las puertas dobles, la mitad que toca;
//   - las sombras se recalculan con las paredes giradas, como las calcula
//     CorsixTH (th_map_update_shadows).
// El núcleo no sabe nada de esto: solo se lee el mapa.
//
// La rotación r lleva la casilla (x, y) del mundo a la casilla de la vista:
//   r = 0: (x, y)   r = 1: (W−1−y, x)   r = 2: (W−1−x, H−1−y)   r = 3: (y, H−1−x)
// y la dirección d del mundo (TH_DIR_NORTH…TH_DIR_WEST) a la dirección d + r.
#ifndef TH_VISTA_GIRAR_H
#define TH_VISTA_GIRAR_H

#include <stdint.h>

#include "th/map.h"

// Una casilla de la vista girada: el suelo, las paredes de sus bordes norte y
// oeste en la vista, y el decorado de la casilla (setos, árboles, farolas) en
// la capa norte o la oeste, donde estaba en el mundo.
typedef struct th_vtile {
    uint8_t floor, wall_n, wall_w, sc_n, sc_w;
    uint8_t flags; // TH_VT_*
} th_vtile;

enum {
    TH_VT_SHADOW_HALF = 1, // como TH_TILE_SHADOW_*, en la vista
    TH_VT_SHADOW_FULL = 2,
    TH_VT_SHADOW_WALL = 4,
    TH_VT_FLIP_FLOOR = 8, // suelo en espejo
    TH_VT_FLIP_SC_N = 16, // decorado en espejo
    TH_VT_FLIP_SC_W = 32,
    TH_VT_TALL_N = 64, // objeto alto junto al borde (como TH_TILE_TALL_*), para las sombras
    TH_VT_TALL_W = 128,
};

// El mapa visto con la cámara girada (rot de 1 a 3). Son 96 KiB: solo hace
// falta mientras la vista está girada.
typedef struct th_rot_map {
    th_vtile tiles[TH_MAP_WIDTH * TH_MAP_HEIGHT]; // por filas de la vista
    const th_map *map;                            // de qué mapa
    uint32_t version;                             // y de qué versión (th_map.version)
    int8_t rot;
} th_rot_map;

// Lo rehace si el mapa, su versión o la rotación han cambiado. Devuelve 1 si
// lo rehizo.
int th_rot_map_update(th_rot_map *g, const th_map *m, int rot);

// Casilla del mundo a casilla de la vista, y al revés.
void th_rot_tile(int rot, int x, int y, int *vx, int *vy);
void th_rot_tile_inv(int rot, int vx, int vy, int *x, int *y);

static inline int th_rot_dir(int rot, int dir)
{
    return (dir + rot) & 3;
}

// Un desplazamiento en píxeles de pantalla (de 640 × 480: px = 32·(dx − dy),
// py = 16·(dx + dy) para un desplazamiento (dx, dy) en casillas) girado.
void th_rot_offset(int rot, int px, int py, int *qx, int *qy);

// Un punto del mundo en unidades de 1/(2u) casillas (lo que da la pantalla
// con u, ver th_view_rotate) de una rotación a otra.
void th_rot_point(int from, int to, int32_t *a, int32_t *b, int32_t lado);

// Las piezas giradas (para las pruebas y las herramientas). th_rot_floor da
// el bloque | TH_ROT_FLIP; th_rot_layer, para un bloque de las capas de pared:
// si es una pared, el bloque girado (que va en el borde girado); si es
// decorado de casilla, bloque | TH_ROT_TILE, con TH_ROT_FLIP si va en espejo.
enum { TH_ROT_FLIP = 0x100, TH_ROT_TILE = 0x200 };
uint16_t th_rot_floor(int rot, int block);
uint16_t th_rot_layer(int rot, int block);

#endif
