// Rasterizador por software de una lista de dibujo (vista.h): pinta sprite a
// sprite, en el orden de la lista, como CorsixTH. Es la referencia con la que
// se compara lo que dibuja el RDP (en la ROM de pruebas) y lo que dibuja
// CorsixTH (capturas a 640 × 480, tools/oraculo_mapa).
#ifndef TH_RASTER_H
#define TH_RASTER_H

#include <stddef.h>
#include <stdint.h>

#include "vista.h"

typedef struct th_raster {
    uint8_t *rgb;       // w × h × 3 bytes
    uint8_t *mezcla;    // opcional (w × h): 1 donde el último sprite fue semitransparente
    uint8_t *hoja;      // opcional (w × h): 1 + hoja (TH_SHEET_*) del último sprite pintado, 255 si fue un rectángulo
    int w, h;
    const uint8_t (*paleta)[3]; // 256 colores RGB de 8 bits
    // Opcional: otra paleta para una hoja (la de las descripciones de
    // CorsixTH, Font00V, usa Pref01V.pal).
    const uint8_t (*paletas[TH_SHEETS])[3];
    // Opcionales: las paletas fantasma (thdf_alt_palette), la gris y la roja
    // (las variantes 1 y 2 del .tpl, tools/conv/paleta.py), para los sprites con
    // TH_DRAW_PAL_GREY o TH_DRAW_PAL_RED; con las dos banderas, la roja. Si
    // faltan, esos sprites van con la paleta de su hoja.
    const uint8_t (*paleta_gris)[3];
    const uint8_t (*paleta_roja)[3];
} th_raster;

// La paleta i (0 normal, 1 fantasma gris, 2 fantasma roja) de un .tpl del
// conversor (TPL1: 256 entradas RGBA5551 big-endian por paleta), a 8 bits por
// componente como el RDP (los 5 bits arriba y repetidos abajo). Devuelve 0, o
// -1 si el fichero no es un .tpl o no tiene esa paleta.
int th_raster_paleta_tpl(const uint8_t *datos, size_t len, int i, uint8_t pal[256][3]);

void th_raster_clear(th_raster *r, uint8_t cr, uint8_t cg, uint8_t cb);
// Pinta la lista entera, con las hojas de cada sprite (sheets[TH_SHEET_*]),
// y los rectángulos de color (TH_SHEET_FILL). Los sprites semitransparentes se mezclan con (c·a + fondo·(255 − a) + 127) / 255,
// con a = 64 (al 25 %) o 128 (al 50 %).
void th_raster_draw(th_raster *r, const th_sheet *const sheets[TH_SHEETS], const th_drawlist *l);

#endif
