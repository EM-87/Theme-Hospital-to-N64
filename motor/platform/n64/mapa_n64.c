#include "mapa_n64.h"

#include <string.h>

#define ALFA_25 64  // 25 % de opacidad, como thdf_alpha_75 de CorsixTH
#define ALFA_50 128 // thdf_alpha_50
#define MAX_ORDEN 4096
#define TMEM_TEXELES 2048 // la otra mitad de la TMEM es la paleta (TLUT)

static uint16_t orden[MAX_ORDEN];
static int32_t cargado;      // hoja << 16 | sprite en la TMEM; -1 = ninguno o desconocido
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

// Color de la textura con alfa = alfa de la textura × alfa/255: los píxeles
// transparentes (alfa 0) los descarta la comparación de alfa y el resto se
// mezcla con lo que hay en el framebuffer.
static void modo_sombra(int alfa)
{
    rdpq_set_prim_color(RGBA32(255, 255, 255, alfa));
    rdpq_mode_combiner(RDPQ_COMBINER1((TEX0, 0, PRIM, 0), (TEX0, 0, PRIM, 0)));
    rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
}

static int alfa_de(const th_draw *d)
{
    return d->flags & TH_DRAW_ALPHA25 ? ALFA_25 : d->flags & TH_DRAW_ALPHA50 ? ALFA_50 : 255;
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

static void dibujar(const hoja_t *const hojas[TH_SHEETS], const th_draw *d, int ancho, int alto,
                    mapa_n64_stats_t *st)
{
    const hoja_t *h = hojas[d->sheet];
    const hoja_entrada_t *e = &h->tabla[d->sprite];
    int y0 = d->y, y1 = d->y + e->alto;
    if (d->flags & TH_DRAW_CLIP) {
        if (d->clip_y0 > y0)
            y0 = d->clip_y0;
        if (d->clip_y1 < y1)
            y1 = d->clip_y1;
    }
    // Recorte horizontal (thdf_crop): las columnas de pantalla [cx0, cx1).
    int cx0 = d->x, cx1 = d->x + e->ancho;
    if (d->flags & TH_DRAW_CROP) {
        if (d->clip_x0 > cx0)
            cx0 = d->clip_x0;
        if (d->clip_x1 < cx1)
            cx1 = d->clip_x1;
    }
    int vx0 = cx0 < 0 ? 0 : cx0, vx1 = cx1 > ancho ? ancho : cx1;
    int vy0 = y0 < 0 ? 0 : y0, vy1 = y1 > alto ? alto : y1;
    if (vx1 > vx0 && vy1 > vy0)
        st->pixeles += (vx1 - vx0) * (vy1 - vy0);
    st->sprites++;
    st->entidades += d->sheet == TH_SHEET_SPRITES;
    const bool espejo = d->flags & TH_DRAW_FLIP_H, boca_abajo = d->flags & TH_DRAW_FLIP_V;
    if (th_sheet_stride(e->ancho) * e->alto > TMEM_TEXELES) {
        // No cabe en la TMEM: rdpq_tex_blit lo parte en tiras (un adorno del
        // mapa y 29 sprites de MSPR-0; ninguno de estos lleva recorte).
        surface_t s = hoja_superficie(h, d->sprite);
        rdpq_tex_blit(&s, d->x, d->y, &(rdpq_blitparms_t){.flip_x = espejo, .flip_y = boca_abajo});
        cargado = -1;
        st->cargas++;
        return;
    }
    const int32_t clave = (int32_t)d->sheet << 16 | d->sprite;
    if (cargado != clave) {
        surface_t s = hoja_superficie(h, d->sprite);
        rdpq_tex_upload(TILE0, &s, NULL);
        cargado = clave;
        st->cargas++;
    }
    if (!espejo && !boca_abajo) {
        rdpq_texture_rectangle(TILE0, cx0, y0, cx1, y1, cx0 - d->x, y0 - d->y);
        return;
    }
    // Con las coordenadas de pantalla al revés, el RDP recorre la textura al
    // revés. Con recorte, la franja de textura es la que cae en [cx0, cx1).
    int x0 = cx0, x1 = cx1, t0 = y0 - d->y, t1 = y1 - d->y;
    int s0 = cx0 - d->x, s1 = cx1 - d->x;
    if (espejo) {
        x0 = cx1;
        x1 = cx0;
        s0 = e->ancho - (cx1 - d->x);
        s1 = e->ancho - (cx0 - d->x);
    }
    if (boca_abajo) {
        int a = y0;
        y0 = y1;
        y1 = a;
        a = t0;
        t0 = e->alto - t1;
        t1 = e->alto - a;
    }
    rdpq_texture_rectangle_scaled(TILE0, x0, y0, x1, y1, s0, t0, s1, t1);
}

void mapa_n64_dibujar(const th_drawlist *l, const hoja_t *const hojas[TH_SHEETS], const uint16_t *paleta, int ancho,
                      int alto, mapa_n64_stats_t *st)
{
    memset(st, 0, sizeof(*st));
    uint32_t t0 = get_ticks();
    cargado = -1;
    graficos_modo_sprites(paleta);

    // 1. Suelos, un bloque cada vez, y sus sombras (bloques 74 y 75), por
    // tramos que terminan en cada suelo que se sale del rombo.
    int ini = 0;
    for (int i = 0; i <= l->floor_count; i++) {
        const th_draw *d = &l->items[i];
        if (i < l->floor_count && ((d->flags & TH_DRAW_ALPHA25) || !se_sale[d->sprite]))
            continue;
        int n = agrupar(l, ini, i, 0);
        for (int k = 0; k < n; k++)
            dibujar(hojas, &l->items[orden[k]], ancho, alto, st);
        n = agrupar(l, ini, i, TH_DRAW_ALPHA25);
        if (n) {
            modo_sombra(ALFA_25);
            for (int k = 0; k < n; k++)
                dibujar(hojas, &l->items[orden[k]], ancho, alto, st);
            modo_opaco();
        }
        if (i < l->floor_count) {
            dibujar(hojas, d, ancho, alto, st);
            st->cortes++;
        }
        ini = i + 1;
    }

    uint32_t t1 = get_ticks();
    st->ticks_suelos = t1 - t0;

    // 2. Paredes, sombras sobre las paredes y entidades, en el orden de la lista.
    int alfa = 255;
    for (int i = l->floor_count; i < l->count; i++) {
        const th_draw *d = &l->items[i];
        int a = alfa_de(d);
        if (a != alfa) {
            if (a < 255)
                modo_sombra(a);
            else
                modo_opaco();
            alfa = a;
        }
        dibujar(hojas, d, ancho, alto, st);
    }
    if (alfa < 255)
        modo_opaco();
    st->ticks_paredes = get_ticks() - t1;
}
