// Vista del mapa: de las casillas y las entidades del núcleo a una lista de
// sprites que dibujar, en el mismo orden que level_map::draw de CorsixTH
// (th_map.cpp), y cada entidad como animation_manager::draw_frame (th_gfx.cpp).
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

#include "girar.h"
#include "th/anim.h"
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

// Hojas de las que sale cada sprite de la lista: las del mapa y las de la
// interfaz (ui/), cada una de alta resolución (CorsixTH) o baja (la N64).
enum {
    TH_SHEET_BLOCKS = 0,
    TH_SHEET_SPRITES = 1,
    TH_SHEET_PANEL = 2,        // panel inferior (PANEL02V / PANEL02M)
    TH_SHEET_POINTER = 3,      // punteros (MPOINTER)
    TH_SHEET_FONT_MONEY = 4,   // cifras del dinero (Font05V / FONT100M)
    TH_SHEET_FONT_DATE = 5,    // la fecha (Font16V / FONT101M)
    TH_SHEET_FONT_WHITE = 6,   // blanca (Font01V / FONT132M)
    TH_SHEET_FONT_PAUSE = 7,   // azul (Font124V / FONT131M)
    TH_SHEET_FONT_TOOLTIP = 8, // las descripciones (Font00V / FONT130M)
    TH_SHEETS = 9,
    // No es una hoja: un rectángulo de color RGBA5551 (sprite) en (x, y), de
    // clip_x1 × clip_y1 píxeles.
    TH_SHEET_FILL = 255,
};

enum {
    TH_DRAW_ALPHA25 = 1, // al 25 % de opacidad (thdf_alpha_75 de CorsixTH)
    TH_DRAW_CLIP = 2,    // solo las filas de pantalla [clip_y0, clip_y1)
    TH_DRAW_ALPHA50 = 4, // al 50 % (thdf_alpha_50)
    TH_DRAW_FLIP_H = 8,  // en espejo
    TH_DRAW_FLIP_V = 16, // cabeza abajo
    TH_DRAW_CROP = 32,   // solo las columnas de pantalla [clip_x0, clip_x1) (thdf_crop)
};

typedef struct th_draw {
    uint16_t sprite;
    uint8_t sheet, flags;
    int16_t x, y;               // esquina superior izquierda en pantalla
    int16_t clip_y0, clip_y1;
    int16_t clip_x0, clip_x1;
} th_draw;

typedef struct th_drawlist {
    th_draw *items;
    int capacity;
    int count;
    int floor_count; // los primeros floor_count son suelos y sus sombras
    int redraws;     // casillas vueltas a pintar junto a una entidad animada
} th_drawlist;

typedef struct th_view {
    int unit;     // 32 (DATA) o 16 (DATAM)
    int x, y;     // esquina superior izquierda de la pantalla, en píxeles del mundo
    int w, h;     // tamaño de la pantalla
} th_view;

// Una entidad que dibujar (animation de CorsixTH): dónde está, qué fotograma
// muestra, con qué banderas (TH_ANIM_*) y qué capas.
typedef struct th_view_entity {
    int16_t tile_x, tile_y;
    int16_t px, py; // desplazamiento en píxeles de 640 × 480
    uint32_t frame;
    uint16_t anim;
    uint8_t flags;
    uint8_t drawing_layer; // orden dentro de la casilla (DrawingLayers)
    uint32_t attach_seq;   // con la misma capa de dibujo, la última en llegar va primero
    const uint8_t *layers; // TH_ANIM_LAYERS
    int8_t crop;           // columna de recorte (thdf_crop) o TH_VIEW_NO_CROP
    uint8_t early;         // va en la lista temprana de la casilla (thdf_early_list)
} th_view_entity;

#define TH_VIEW_NO_CROP (-128)

#define TH_VIEW_MAX_ENTITIES 512 // entidades que pueden caer cerca de la pantalla

// Todo lo que se dibuja: el mapa, sus hojas y las entidades. Con la cámara
// girada, rot_tiles es el mapa girado (th_rot_map) y las entidades van en
// casillas de la vista; si es NULL, se dibuja el mapa tal cual.
typedef struct th_scene {
    const th_map *map;
    const th_vtile *rot_tiles;
    const th_sheet *sheets[TH_SHEETS];
    const th_anims *anims;
    const th_view_entity *entities;
    int entity_count;
} th_scene;

// Posición en pantalla de la casilla (x, y): la esquina norte del rombo.
static inline void th_view_tile(const th_view *v, int x, int y, int *sx, int *sy)
{
    *sx = v->unit * (x - y) - v->x;
    *sy = v->unit / 2 * (x + y) - v->y;
}

// Rellena la lista con lo que se ve: primero, por líneas de casillas de
// arriba abajo y de izquierda a derecha, cada suelo y su sombra; después, por
// cada línea, las paredes norte de derecha a izquierda (con la sombra sobre la
// pared y las entidades de la lista temprana de la casilla) y, de izquierda a
// derecha, la pared oeste y las demás entidades de cada casilla. Junto a una entidad animada se vuelve a pintar lo que la tapa en
// CorsixTH (la pared norte de la casilla de la izquierda y lo que hay en
// ella). Solo entran los sprites que tocan la pantalla. Devuelve el número de
// elementos o -1 si no caben.
int th_view_scene(const th_view *v, const th_scene *sc, th_drawlist *out);
// Solo el mapa.
int th_view_map(const th_view *v, const th_map *m, const th_sheet *blocks, th_drawlist *out);

// Limita la cámara para que el centro de la pantalla no salga del rombo del mapa.
void th_view_clamp(th_view *v);

// Gira la cámara de la rotación from a la to (girar.h) sin mover el punto
// del mundo que hay en el centro de la pantalla.
void th_view_rotate(th_view *v, int from, int to);

// La casilla de la vista bajo el píxel (sx, sy) de la pantalla, en el suelo
// (como Map:ScreenToWorld de CorsixTH). Puede caer fuera del mapa.
void th_view_pick(const th_view *v, int sx, int sy, int *x, int *y);

// Huella de una lista de dibujo (FNV-1a de todos sus campos), para comprobar
// que dos formas de hacerla dan la misma.
uint32_t th_drawlist_hash(const th_drawlist *l);

#endif
