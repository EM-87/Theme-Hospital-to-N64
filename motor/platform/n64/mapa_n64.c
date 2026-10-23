#include "mapa_n64.h"

#include <string.h>

#define ALFA_SOMBRA 64 // 25 % de opacidad, como thdf_alpha_75 de CorsixTH
#define MAX_ORDEN 4096
#define TMEM_TEXELES 2048 // la otra mitad de la TMEM es la paleta (TLUT)

static uint16_t orden[MAX_ORDEN];
static int cargado;          // sprite que está ahora en la TMEM; -1 = ninguno o desconocido
static uint8_t se_sale[256]; // 1 = el bloque no cabe en el rombo de su casilla

void mapa_n64_preparar(const hoja_t *bloques, int unidad)
{
    // El rombo es el del bloque 1: si no tiene el tamaño de una casilla, no se agrupa nada.
    const int w = 2 * unidad, h = unidad;
    surface_t rombo = hoja_superficie(bloques, 1);
    bool hay_rombo = rombo.width == w && rombo.height == h;
    for (int i = 0; i < 256; i++) {
        surface_t s = hoja_superficie(bloques, i);
        se_sale[i] = 1;
        if (!hay_rombo || s.width != w || s.height != h)
            continue;
        const uint8_t *a = s.buffer, *r = rombo.buffer;
        se_sale[i] = 0;
        for (int y = 0; y < h && !se_sale[i]; y++)
            for (int x = 0; x < w; x++)
                if (a[y * s.stride + x] != TH_TRANSPARENT && r[y * rombo.stride + x] == TH_TRANSPARENT) {
                    se_sale[i] = 1;
                    break;
                }
    }
}

static void modo_opaco(void)
{
    rdpq_mode_combiner(RDPQ_COMBINER_TEX);
    rdpq_mode_blender(0);
}

// Color de la textura con alfa = alfa de la textura × 64/255: los píxeles
// transparentes (alfa 0) los descarta la comparación de alfa y el resto se
// mezcla con lo que hay en el framebuffer.
static void modo_sombra(void)
{
    rdpq_mode_combiner(RDPQ_COMBINER1((TEX0, 0, PRIM, 0), (TEX0, 0, PRIM, 0)));
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
}

// Índices de los elementos [ini, fin) con (flags & TH_DRAW_ALPHA25) == alfa,
// ordenados por sprite y, dentro de cada sprite, en el orden de la lista.
static int agrupar(const th_drawlist *l, int ini, int fin, int alfa)
{
    static uint16_t cuenta[257];
    memset(cuenta, 0, sizeof(cuenta));
    for (int i = ini; i < fin; i++)
        if ((l->items[i].flags & TH_DRAW_ALPHA25) == alfa)
            cuenta[l->items[i].sprite + 1]++;
    for (int s = 1; s <= 256; s++)
        cuenta[s] += cuenta[s - 1];
    int n = cuenta[256];
    assertf(n <= MAX_ORDEN, "lista de dibujo demasiado larga: %d", n);
    for (int i = ini; i < fin; i++)
        if ((l->items[i].flags & TH_DRAW_ALPHA25) == alfa)
            orden[cuenta[l->items[i].sprite]++] = (uint16_t)i;
    return n;
}

static void dibujar(const hoja_t *h, const th_draw *d, int ancho, int alto, mapa_n64_stats_t *st)
{
    const hoja_entrada_t *e = &h->tabla[d->sprite];
    int y0 = d->y, y1 = d->y + e->alto;
    if (d->flags & TH_DRAW_CLIP) {
        if (d->clip_y0 > y0)
            y0 = d->clip_y0;
        if (d->clip_y1 < y1)
            y1 = d->clip_y1;
    }
    int vx0 = d->x < 0 ? 0 : d->x, vx1 = d->x + e->ancho > ancho ? ancho : d->x + e->ancho;
    int vy0 = y0 < 0 ? 0 : y0, vy1 = y1 > alto ? alto : y1;
    if (vx1 > vx0 && vy1 > vy0)
        st->pixeles += (vx1 - vx0) * (vy1 - vy0);
    st->sprites++;
    if (th_sheet_stride(e->ancho) * e->alto > TMEM_TEXELES) {
        // No cabe en la TMEM: rdpq_tex_blit lo parte en tiras (un adorno de
        // 32 × 69; ninguno de estos lleva recorte).
        surface_t s = hoja_superficie(h, d->sprite);
        rdpq_tex_blit(&s, d->x, d->y, NULL);
        cargado = -1;
        st->cargas++;
        return;
    }
    if (cargado != d->sprite) {
        surface_t s = hoja_superficie(h, d->sprite);
        rdpq_tex_upload(TILE0, &s, NULL);
        cargado = d->sprite;
        st->cargas++;
    }
    rdpq_texture_rectangle(TILE0, d->x, y0, d->x + e->ancho, y1, 0, y0 - d->y);
}

void mapa_n64_dibujar(const th_drawlist *l, const hoja_t *bloques, const uint16_t *paleta, int ancho, int alto,
                      mapa_n64_stats_t *st)
{
    memset(st, 0, sizeof(*st));
    uint32_t t0 = get_ticks();
    cargado = -1;
    graficos_modo_sprites(paleta);
    rdpq_set_prim_color(RGBA32(255, 255, 255, ALFA_SOMBRA));

    // 1. Suelos, un bloque cada vez, y sus sombras (bloques 74 y 75), por
    // tramos que terminan en cada suelo que se sale del rombo.
    int ini = 0;
    for (int i = 0; i <= l->floor_count; i++) {
        const th_draw *d = &l->items[i];
        if (i < l->floor_count && ((d->flags & TH_DRAW_ALPHA25) || !se_sale[d->sprite]))
            continue;
        int n = agrupar(l, ini, i, 0);
        for (int k = 0; k < n; k++)
            dibujar(bloques, &l->items[orden[k]], ancho, alto, st);
        n = agrupar(l, ini, i, TH_DRAW_ALPHA25);
        if (n) {
            modo_sombra();
            for (int k = 0; k < n; k++)
                dibujar(bloques, &l->items[orden[k]], ancho, alto, st);
            modo_opaco();
        }
        if (i < l->floor_count) {
            dibujar(bloques, d, ancho, alto, st);
            st->cortes++;
        }
        ini = i + 1;
    }

    uint32_t t1 = get_ticks();
    st->ticks_suelos = t1 - t0;

    // 2. Paredes y sombras sobre las paredes, en el orden de la lista.
    int sombra = 0;
    for (int i = l->floor_count; i < l->count; i++) {
        const th_draw *d = &l->items[i];
        int a = (d->flags & TH_DRAW_ALPHA25) != 0;
        if (a != sombra) {
            if (a)
                modo_sombra();
            else
                modo_opaco();
            sombra = a;
        }
        dibujar(bloques, d, ancho, alto, st);
    }
    if (sombra)
        modo_opaco();
    st->ticks_paredes = get_ticks() - t1;
}
