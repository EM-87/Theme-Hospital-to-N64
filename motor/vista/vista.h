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

// Bloques especiales de la hoja de bloques del mapa (MBLK / VBLK). El 226 no
// está en los datos: lo añade el conversor (tools/conv/hojas.py) para las
// vistas giradas, y es la sombra del 156 sobre la cara de una pared oeste.
enum {
    TH_BLOCK_SHADOW_FULL = 74,
    TH_BLOCK_SHADOW_HALF = 75,
    TH_BLOCK_SHADOW_WALL = 156,
    TH_BLOCK_SHADOW_WALL_W = 226,
};

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
    TH_SHEET_MUTTON = 9,       // las ventanas de 320 × 200 del juego de PC (DATAM/MUTTON01); de alta no hay una sola hoja
    TH_SHEETS = 10,
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
    // Paletas fantasma (thdf_alt_palette de CorsixTH): el objeto o la persona que
    // sigue al puntero, en gris; en rojo, donde no se puede colocar.
    TH_DRAW_PAL_GREY = 64,
    TH_DRAW_PAL_RED = 128,
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

// Banderas de paleta de una entidad (th_view_entity.flags), detrás de las
// TH_ANIM_* de th/anim.h (bits 0 a 3): thdf_alt_palette de CorsixTH (1 << 4,
// th_gfx.h), que dibuja la animación con su paleta fantasma. CorsixTH le pone
// a cada animación una sola paleta fantasma (setAnimationGhostPalette); aquí
// la bandera dice cuál. Llegan a cada sprite como TH_DRAW_PAL_GREY o
// TH_DRAW_PAL_RED (con las dos, gana la roja). Las usan, como CorsixTH:
//   - el objeto que se coloca y el empleado que se suelta, en gris solo donde
//     NO se puede (place_objects.lua:381-389 y 1025, place_staff.lua:46-50 y
//     130: con la paleta normal donde sí);
//   - las paredes, la puerta y las ventanas del plano de una sala, en rojo
//     donde no vale (th_lua_map.cpp:323-381). CorsixTH solo le pone la paleta
//     roja a las animaciones 124, 126 y 130 (edit_room.lua:37-40); la 120 sale
//     roja porque comparte sprites con la 124 y la paleta fantasma va por
//     sprite (th_gfx.cpp:777-801). Aquí la 120 lleva la bandera igual.
enum {
    TH_ANIM_PAL_GREY = 1u << 4,
    TH_ANIM_PAL_RED = 1u << 5,
};

// Una entidad que dibujar (animation de CorsixTH): dónde está, qué fotograma
// muestra, con qué banderas (TH_ANIM_* y TH_ANIM_PAL_*) y qué capas. Con
// layers = NULL, sin capas (solo los elementos de id 0).
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

// --- Superposiciones por casilla: la capa «ui» de CorsixTH ------------------
//
// Un bloque de la hoja de bloques en una casilla del MUNDO, con banderas
// TH_DRAW_ALPHA25, TH_DRAW_ALPHA50, TH_DRAW_PAL_GREY o TH_DRAW_PAL_RED (las
// demás se ignoran). Es tile_layer::ui de CorsixTH (th_map.h), que se pinta
// con draw_layer después de la pared oeste de su casilla y antes de sus
// entidades (th_map.cpp:1100-1104): encima del suelo y de las paredes norte
// de su línea, debajo de lo demás. Como allí, hay una por casilla: poner otra
// en la misma casilla la sustituye, y el bloque 0 la quita (map:setCell(x, y,
// 4, 0)). Sirve para:
//   - el plano de una sala (updateRoomBlueprint, th_lua_map.cpp:238-312): 24
//     donde vale y 67 donde no, al 50 %; 37 a 40, las flechas de las cuatro
//     casillas del centro (th_view_blueprint lo hace todo);
//   - la huella del objeto que se coloca (good_tile y bad_tile,
//     place_objects.lua:689 y 796-797, puestas con setCell en 843-847): 24 o
//     67 con thdf_alpha_75 (TH_DRAW_ALPHA25);
//   - las marcas de la puerta (26, 27, 30 y 31, la casilla de dentro con la
//     pared al norte, este, sur u oeste; 25, 28, 29 y 32, la otra mitad de las
//     puertas dobles) y de las ventanas (33 a 36), sin banderas
//     (edit_room.lua:1053-1074, 1405-1421 y 1495-1498). Una puerta doble son
//     tres casillas seguidas a lo largo de la pared: marca, 24 y marca (norte:
//     25 al oeste y 26 al este; este: 28 al norte y 27 al sur; sur: 29 al
//     oeste y 30 al este; oeste: 32 al norte y 31 al sur). En la pared oeste
//     CorsixTH pone 30 al norte (placeDoorBlueprint, edit_room.lua:1405-1418:
//     flags - 1 sin dirfix), un sprite de la pared sur; aquí se espera el 32,
//     el que dibuja la mitad norte de la pared oeste.
// Con la cámara girada, la vista pone cada una en su casilla girada
// (th_rot_tile) y cambia las marcas con dirección por las de la dirección
// girada (th_view_overlay_block): quien las pone no sabe nada del giro.
//
// El tope: el plano más grande del nivel 1 es de 6 × 6 casillas (36); uno de
// 12 × 12 son 144, y la huella de un objeto cabe en 3 × 4. Con 256 sobra
// (1 KiB).
#define TH_VIEW_MAX_OVERLAYS 256

typedef struct th_view_overlay {
    uint8_t x, y;      // casilla del mundo
    uint8_t block;     // de la hoja de bloques (TH_SHEET_BLOCKS)
    uint8_t flags;     // TH_DRAW_ALPHA25 | TH_DRAW_ALPHA50 | TH_DRAW_PAL_*
} th_view_overlay;

typedef struct th_view_overlays {
    int count;
    th_view_overlay items[TH_VIEW_MAX_OVERLAYS];
} th_view_overlays;

// Las vacía todas.
void th_view_overlays_clear(th_view_overlays *o);
// Pone el bloque block en la casilla (x, y) del mundo, en lugar del que
// hubiera; block = 0 la quita. Devuelve 0, o -1 si la casilla está fuera del
// mapa o ya hay TH_VIEW_MAX_OVERLAYS (y entonces no cambia nada).
int th_view_overlays_set(th_view_overlays *o, int x, int y, int block, int flags);
// El bloque de una superposición con la cámara girada rot: las marcas de
// puerta y de ventana pasan a la de la dirección girada, y las flechas del
// centro del plano (37 a 40, que apuntan al centro desde su esquina) a la
// de la esquina girada. Los demás bloques, tal cual. La vista girada se ve
// como la de Theme Hospital, así que una puerta sencilla lleva la marca que
// el juego pone en esa pared de la vista (26, 27, 30, 31), y cada mitad de
// una puerta doble (doble != 0) la de la mitad que le toca en la pared girada
// (25 -> 28 -> 30 -> 31 y 26 -> 27 -> 29 -> 32, comprobado con los sprites:
// cada uno dibuja la hoja paralela a su pared y la flecha en el extremo
// contrario al de su nombre). Las dos tablas solo difieren en el 27 y el 31;
// 25, 28, 29 y 32 solo salen en las puertas dobles y van siempre por la suya.
// th_view_scene sabe si es doble mirando las casillas vecinas a lo largo de
// la pared (24 y otra marca de la misma pared).
int th_view_overlay_block(int rot, int block, int doble);

// Todo lo que se dibuja: el mapa, sus hojas y las entidades. Con la cámara
// girada, rot es el mapa girado y las entidades van en casillas de la vista;
// si es NULL, se dibuja el mapa tal cual. Con no_shadows, sin sombras (para
// comparar las vistas giradas con CorsixTH, que no sabe dibujarlas).
typedef struct th_scene {
    const th_map *map;
    const th_rot_map *rot;
    int no_shadows;
    const th_sheet *sheets[TH_SHEETS];
    const th_anims *anims;
    const th_view_entity *entities;
    int entity_count;
    const th_view_overlays *overlays; // NULL o vacías: ninguna
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
// Las superposiciones (th_scene.overlays) entran en la segunda pasada, cada
// una tras la pared oeste (y el decorado oeste) de su casilla.
//
// Una entidad más, dibujada encima de todo lo que ya hay en out (al final de
// la lista, en la parte de las paredes): es como dibuja CorsixTH el objeto y el
// empleado que siguen al puntero, fuera del mapa (UIPlaceObjects:draw,
// place_objects.lua:596-612, y UIPlaceStaff:draw, place_staff.lua:127-140).
// La casilla de e es de la vista, como las de th_scene.entities. Devuelve 0
// o -1 si no cabe.
int th_view_entity_on_top(const th_view *v, const th_scene *sc, const th_view_entity *e, th_drawlist *out);

// El plano de una sala de w × h casillas con la esquina norte en (x, y) del
// mundo, como updateRoomBlueprint (th_lua_map.cpp:238-357): en ov, los suelos
// (24 donde vale, 67 donde no, al 50 %, y las flechas 37 a 40 en las cuatro
// casillas del centro si vale); en out, las paredes del plano, que son
// entidades: la animación 124 en la esquina norte y la 120 en cada tramo (las
// de los lados oeste y este en espejo, con los desplazamientos (0, 0),
// (0, −1), (2, 0) y (2, −1) de set_pixel_offset), con TH_ANIM_PAL_RED donde
// la casilla de dentro no vale. malas: w × h bytes por filas (1 = la casilla
// (x + i, y + j) del mundo no vale), o NULL; todo_malo: todo el plano no vale
// (entire_invalid: demasiado pequeño). Con la cámara girada rot, las paredes
// van en casillas de la vista, en los lados girados. No vacía ov: pone las
// casillas del plano (para moverlo, vaciarlas antes). Devuelve cuántas entidades puso (2 (w + h) − 1, o
// menos si no caben en max), o -1 si el plano se sale del mapa: como
// th_lua_map.cpp:266-268, x + w < ancho e y + h < alto, y además, con la
// cámara girada, el rectángulo girado y su borde sur y este de la vista
// dentro del mapa (tocar x = 0 o y = 0 dejaría tramos de pared en la casilla
// 128 de la vista, que th_view_scene descarta); ese -1 no toca ov.
// La puerta y las ventanas del plano se añaden igual, como entidades de la
// vista: la animación 126 (puerta) o 130 (ventana) en la casilla de dentro,
// con TH_ANIM_FLIP_H si la pared es la oeste (flags = 1 de setDoorBlueprint y
// setWindowBlueprint, edit_room.lua:1360-1400 y 1477-1495) y TH_ANIM_PAL_RED
// si no vale (el +16 de flag_altpal), capa de dibujo 0, y su marca de suelo
// con th_view_overlays_set. Con la cámara girada, la casilla y el espejo los
// da th_view_edge.
int th_view_blueprint(const th_anims *anims, int x, int y, int w, int h, int rot, const uint8_t *malas, int todo_malo,
                   th_view_overlays *ov, th_view_entity *out, int max);

// Un borde del mundo (el lado dir, TH_DIR_*, de la casilla (x, y)) en la
// vista girada rot: la casilla de la vista que lo tiene al norte o al oeste,
// y *oeste = 1 si es su borde oeste (las animaciones de puerta y ventana van
// entonces con TH_ANIM_FLIP_H). Como colocar() con las puertas (objetos.c).
void th_view_edge(int rot, int x, int y, int dir, int *vx, int *vy, int *oeste);

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
