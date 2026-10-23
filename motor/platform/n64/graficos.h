// Hojas de sprites (TSH1) y paletas (TPL1) que genera tools/conv, y su dibujo
// con el RDP en CI8. Formatos documentados en tools/conv/hojas.py y conv.py.
#ifndef TH64_GRAFICOS_H
#define TH64_GRAFICOS_H

#include <libdragon.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t desplazamiento; // desde el inicio de los píxeles
    uint16_t ancho, alto;    // ancho 0 = sprite vacío
} hoja_entrada_t;

typedef struct {
    void *bloque;            // fichero entero, tal como lo cargó asset_load
    uint16_t num;
    const hoja_entrada_t *tabla;
    uint8_t *pixeles;
} hoja_t;

enum { PALETA_NORMAL = 0, PALETA_FANTASMA_GRIS = 1, PALETA_FANTASMA_ROJO = 2, PALETAS_MAX = 3 };

typedef struct {
    void *bloque;
    uint16_t num;
    uint16_t *colores[PALETAS_MAX]; // 256 entradas RGBA5551 cada una
} paletas_t;

bool hoja_cargar(hoja_t *h, const char *ruta);
bool paletas_cargar(paletas_t *p, const char *ruta);

// Superficie CI8 de un sprite (sin copiar píxeles); ancho 0 si está vacío.
surface_t hoja_superficie(const hoja_t *h, int indice);

// Modo de dibujo de sprites: estándar de un ciclo, TLUT y alfa por comparación.
void graficos_modo_sprites(const uint16_t *paleta);

// Dibuja un sprite con la carga de textura y un rectángulo (APIs estables).
// escala >= 1; espejo = reflejo horizontal.
void hoja_dibujar(const hoja_t *h, int indice, int x, int y, int escala, bool espejo);

#endif
