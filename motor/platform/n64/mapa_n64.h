// Dibujo con el RDP de la lista de la vista del mapa (vista/vista.h).
//
// Los suelos se agrupan por bloque: una carga de textura por bloque distinto
// y un rectángulo por casilla; después van las sombras del suelo (al 25 %,
// también agrupadas) y, por último, las paredes en el orden de la lista.
// Los suelos que caben en el rombo de su casilla no se tapan entre sí, así
// que da igual en qué orden se pinten. Los pocos bloques que se salen del
// rombo (7 en MBLK-0) cortan la agrupación: se pinta todo lo anterior, luego
// ese bloque, y se sigue. Así el resultado es el mismo que pintar en el orden
// de CorsixTH (lo comprueba la ROM de pruebas contra vista/raster.c).
#ifndef TH64_MAPA_N64_H
#define TH64_MAPA_N64_H

#include "graficos.h"
#include "vista.h"

typedef struct {
    int sprites;  // rectángulos (o blits) enviados al RDP
    int cargas;   // cargas de textura en la TMEM
    int pixeles;  // píxeles cubiertos, contando los transparentes (relleno del RDP)
    int cortes;   // suelos que se salen del rombo y cortan la agrupación
    uint32_t ticks_suelos, ticks_paredes; // CPU de cada pasada (get_ticks)
} mapa_n64_stats_t;

// Mira qué bloques se salen del rombo de una casilla (el del bloque 1, el
// césped). Hay que llamarla una vez con la hoja de bloques, antes de dibujar.
void mapa_n64_preparar(const hoja_t *bloques, int unidad);

void mapa_n64_dibujar(const th_drawlist *l, const hoja_t *bloques, const uint16_t *paleta, int ancho, int alto,
                      mapa_n64_stats_t *st);

#endif
