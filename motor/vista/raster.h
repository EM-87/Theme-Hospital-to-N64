// Rasterizador por software de una lista de dibujo (vista.h): pinta sprite a
// sprite, en el orden de la lista, como CorsixTH. Es la referencia con la que
// se compara lo que dibuja el RDP (en la ROM de pruebas) y lo que dibuja
// CorsixTH (capturas a 640 × 480, tools/oraculo_mapa).
#ifndef TH_RASTER_H
#define TH_RASTER_H

#include <stdint.h>

#include "vista.h"

typedef struct th_raster {
    uint8_t *rgb;       // w × h × 3 bytes
    uint8_t *mezcla;    // opcional (w × h): 1 donde el último sprite fue semitransparente
    int w, h;
    const uint8_t (*paleta)[3]; // 256 colores RGB de 8 bits
} th_raster;

void th_raster_clear(th_raster *r, uint8_t cr, uint8_t cg, uint8_t cb);
// Pinta la lista entera. Los sprites semitransparentes se mezclan con
// (c·a + fondo·(255 − a) + 127) / 255, a = 64.
void th_raster_draw(th_raster *r, const th_sheet *sh, const th_drawlist *l);

#endif
