// Texto de la interfaz: las fuentes de mapa de bits de Theme Hospital como
// bitmap_font de CorsixTH (th_gfx_font.cpp) y el formato de sus cadenas
// (TH.stringProxy.format, string_extensions.lua). Es C portable: el texto se
// coloca como sprites de una lista de dibujo (vista.h), que dibujan el RDP en
// la N64 y el rasterizador de referencia en el PC.
#ifndef TH_UI_TEXTO_H
#define TH_UI_TEXTO_H

#include <stddef.h>
#include <stdint.h>

#include "th/rules.h"
#include "vista.h"

// Una fuente de TH es una hoja de sprites en la que el carácter c de CP437 es
// el sprite c − 31. sep_x y sep_y son set_separation de CorsixTH. espacio,
// si no es 0, es el ancho del espacio en lugar del de su sprite (en FONT101M,
// la fuente de la fecha en baja resolución, el espacio mide 1 píxel).
typedef struct th_font {
    const th_sheet *sheet;
    uint8_t sheet_id; // TH_SHEET_FONT_*: la hoja en la lista de dibujo
    int8_t sep_x, sep_y;
    int8_t espacio;
} th_font;

// text_layout de CorsixTH.
typedef struct th_text_layout {
    int end_x, end_y; // dónde acaba el texto
    int row_count;
    int width;        // la línea más ancha
} th_text_layout;

enum { TH_ALIGN_LEFT = 0, TH_ALIGN_CENTER = 1, TH_ALIGN_RIGHT = 2 }; // text_alignment

// next_utf8_codepoint (th_strings.cpp): el siguiente carácter y avanza *s.
uint32_t th_utf8_next(const char **s, const char *end);
// unicode_to_codepage_437 (th_strings.cpp): '?' si no está en CP437.
unsigned th_unicode_to_cp437(uint32_t cp);

// draw_text: una línea, sin cortar, desde (x, y). Con out a NULL solo mide.
// Devuelve la x en la que acabaría la siguiente letra. -1 si la lista se llena.
int th_font_text(const th_font *f, const char *s, size_t n, int x, int y, th_drawlist *out);

// draw_text_wrapped: corta en los espacios para no pasar de width y en «//»
// o en dos saltos de línea seguidos (con una línea en blanco). Con out a NULL
// solo mide (get_text_dimensions).
th_text_layout th_font_wrapped(const th_font *f, const char *s, size_t n, int x, int y, int width, int max_rows,
                               int skip_rows, int align, th_drawlist *out);

// font:draw de Lua (l_font_draw): si w y h > 0, alinea (TH_ALIGN_*) en
// horizontal dentro de w y centra en vertical dentro de h. Devuelve en
// *end_x y *end_y (si no son NULL) lo mismo que la función de Lua.
void th_font_draw(const th_font *f, const char *s, int x, int y, int w, int h, int align, th_drawlist *out, int *end_x,
                  int *end_y);

// font:drawTooltip de Lua (l_font_draw_tooltip): un recuadro blanco con borde
// negro y el texto cortado a w − 4 píxeles; (x, y) es su esquina inferior
// izquierda, y se mete dentro de la pantalla (screen_w de ancho y desde y = 0).
// Devuelve la y en la que acaba el texto.
int th_font_tooltip(const th_font *f, const char *s, int x, int y, int w, int screen_w, th_drawlist *out);

// Rectángulo de color en la lista (TH_SHEET_FILL); color en RGBA5551.
int th_drawlist_fill(th_drawlist *out, int x, int y, int w, int h, uint16_t color);

// Un argumento de th_text_format.
typedef struct th_text_arg {
    const char *str; // cadena, o NULL si es un número
    int32_t num;
} th_text_arg;

static inline th_text_arg th_text_num(int32_t n)
{
    return (th_text_arg){NULL, n};
}
static inline th_text_arg th_text_str(const char *s)
{
    return (th_text_arg){s, 0};
}

// TH.stringProxy.format: «%%» es «%»; «%s» y «%d», el siguiente argumento si
// es del tipo (un número vale para %s); «%n%» (n de 1 a 9), el argumento n; y
// «%n:tabla%», el elemento del argumento n en la tabla de textos (_S.tabla,
// con puntos, desde textos). Lo que no se puede sustituir se deja tal cual.
// Escribe en out (siempre terminado en 0, cortado si no cabe) y devuelve la
// longitud del resultado.
size_t th_text_format(char *out, size_t cap, const char *fmt, const th_text_arg *args, int nargs,
                      const th_rules *textos);

// La cadena de los textos en la ruta (puntos, como th_rules_path), o «» si no está.
const char *th_text(const th_rules *textos, const char *path);

#endif
