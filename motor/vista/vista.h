// Vista del mapa: de las casillas del núcleo a una lista de sprites que
// dibujar, en el mismo orden que level_map::draw de CorsixTH (th_map.cpp).
// Es C portable: la misma lista la ejecuta el RDP en la N64
// (platform/n64/mapa_n64.c) y, en el PC y en las pruebas, un rasterizador por
// software (raster.c) que sirve de referencia.
//
// La proyección es la de CorsixTH con una «unidad» u: la casilla (x, y) cae en
// (u·(x−y), u/2·(x+y)) de la pantalla. u = 32 con los gráficos de DATA
// (640 × 480, los de CorsixTH) y u = 16 con los de DATAM (320 × 240, la N64).
// El núcleo no sabe nada de la cámara: la vista solo lee el mapa.
#ifndef TH_VISTA_H
#define TH_VISTA_H

#include <stddef.h>
#include <stdint.h>

#include "th/map.h"

// Hoja de sprites TSH1 de tools/conv (big-endian): solo lectura, sin copiar.
typedef struct th_sheet {
    const uint8_t *data;
    uint16_t count;
    uint16_t max_height; // el sprite más alto: margen inferior de la vista
    uint32_t table, pixels;
} th_sheet;

int th_sheet_init(th_sheet *s, const uint8_t *data, size_t len); // 0 = bien
// Tamaño del sprite i (0 × 0 si no existe o está vacío).
void th_sheet_size(const th_sheet *s, int i, int *w, int *h);
// Índices de color del sprite i, fila a fila, con filas de th_sheet_stride bytes.
const uint8_t *th_sheet_pixels(const th_sheet *s, int i);
static inline int th_sheet_stride(int w)
{
    return (w + 7) & ~7;
}

#define TH_TRANSPARENT 255 // índice de color transparente

// Bloques especiales de la hoja de bloques del mapa (MBLK / VBLK).
enum { TH_BLOCK_SHADOW_FULL = 74, TH_BLOCK_SHADOW_HALF = 75, TH_BLOCK_SHADOW_WALL = 156 };

enum {
    TH_DRAW_ALPHA25 = 1, // al 25 % de opacidad (thdf_alpha_75 de CorsixTH)
    TH_DRAW_CLIP = 2,    // solo las filas de pantalla [clip_y0, clip_y1)
};

typedef struct th_draw {
    uint8_t sprite, flags;
    int16_t x, y;               // esquina superior izquierda en pantalla
    int16_t clip_y0, clip_y1;
} th_draw;

typedef struct th_drawlist {
    th_draw *items;
    int capacity;
    int count;
    int floor_count; // los primeros floor_count son suelos y sus sombras
} th_drawlist;

typedef struct th_view {
    int unit;     // 32 (DATA) o 16 (DATAM)
    int x, y;     // esquina superior izquierda de la pantalla, en píxeles del mundo
    int w, h;     // tamaño de la pantalla
} th_view;

// Posición en pantalla de la casilla (x, y): la esquina norte del rombo.
static inline void th_view_tile(const th_view *v, int x, int y, int *sx, int *sy)
{
    *sx = v->unit * (x - y) - v->x;
    *sy = v->unit / 2 * (x + y) - v->y;
}

// Rellena la lista con lo que se ve del mapa: primero, por líneas de casillas
// de arriba abajo y de izquierda a derecha, cada suelo y su sombra; después,
// por cada línea, las paredes norte de derecha a izquierda (con la sombra
// sobre la pared) y las oeste de izquierda a derecha. Solo entran los sprites
// que tocan la pantalla. Devuelve el número de elementos o -1 si no caben.
int th_view_map(const th_view *v, const th_map *m, const th_sheet *blocks, th_drawlist *out);

// Limita la cámara para que el centro de la pantalla no salga del rombo del mapa.
void th_view_clamp(th_view *v);

#endif
