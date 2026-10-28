// El puntero de la interfaz en la N64: el stick hace de ratón, con
// aceleración, como proponía la fase 4 (docs/04_decision.md, «Controles»).
// También lo mueve el ratón de N64. Cuando el puntero toca un borde de la
// pantalla, la vista se desplaza hacia ese lado, como en CorsixTH
// (GameUI:onMouseMove: en pantalla completa, la zona del borde es de 1 píxel).
// Es C portable, sin libdragon: recibe el stick y los botones ya leídos.
#ifndef TH_UI_PUNTERO_H
#define TH_UI_PUNTERO_H

#include <stdint.h>

typedef struct th_pointer {
    int32_t x, y;      // en dieciseisavos de píxel
    int16_t w, h;      // la pantalla
    uint8_t borde;     // fotogramas seguidos en un borde (para acelerar el desplazamiento)
} th_pointer;

void th_pointer_init(th_pointer *p, int w, int h);
static inline int th_pointer_x(const th_pointer *p)
{
    return p->x >> 4;
}
static inline int th_pointer_y(const th_pointer *p)
{
    return p->y >> 4;
}

// Un fotograma del stick (−85 a 85 en cada eje, y hacia arriba positiva):
// zona muerta de 8 y velocidad de 1 a 8 píxeles por fotograma, que crece con
// el cuadrado de lo que se inclina. rapido (Z mantenido) la multiplica por 2,5.
void th_pointer_stick(th_pointer *p, int sx, int sy, int rapido);

// Movimiento relativo del ratón de N64, en píxeles.
void th_pointer_move(th_pointer *p, int dx, int dy);

// Desplazamiento de la vista de este fotograma por estar en un borde, en
// píxeles de 320 × 240 a 30 fps. CorsixTH mueve 7 píxeles de 640 por tick
// (unos 55 por segundo) y lo multiplica por tick_scroll_mult, que sube 0,02
// por tick de 1 a 2: aquí, de 6,5 a 13 píxeles por fotograma en 0,9 s.
void th_pointer_edge_scroll(th_pointer *p, int *dx, int *dy);

#endif
