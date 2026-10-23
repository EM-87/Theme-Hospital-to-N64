#include "graficos.h"

#include <string.h>

bool hoja_cargar(hoja_t *h, const char *ruta)
{
    int tam = 0;
    uint8_t *b = asset_load(ruta, &tam);
    if (!b || tam < 16 || memcmp(b, "TSH1", 4) != 0)
        return false;
    // El fichero ya está en big-endian, como la N64.
    uint32_t ini_tabla = *(uint32_t *)(b + 8), ini_pix = *(uint32_t *)(b + 12);
    h->bloque = b;
    h->tam = tam;
    h->num = *(uint16_t *)(b + 4);
    h->tabla = (const hoja_entrada_t *)(b + ini_tabla);
    h->pixeles = b + ini_pix;
    assertf(((uintptr_t)h->pixeles & 7) == 0, "píxeles de %s sin alinear a 8", ruta);
    // El RDP lee la RDRAM directamente: hay que sacar los datos de la caché.
    data_cache_hit_writeback(b, tam);
    return true;
}

bool paletas_cargar(paletas_t *p, const char *ruta)
{
    int tam = 0;
    uint8_t *b = asset_load(ruta, &tam);
    if (!b || tam < 8 || memcmp(b, "TPL1", 4) != 0)
        return false;
    p->bloque = b;
    p->num = *(uint16_t *)(b + 4);
    if (p->num > PALETAS_MAX)
        p->num = PALETAS_MAX;
    for (int i = 0; i < p->num; i++)
        p->colores[i] = (uint16_t *)(b + 8 + 512 * i);
    data_cache_hit_writeback(b, tam);
    return true;
}

surface_t hoja_superficie(const hoja_t *h, int indice)
{
    if (indice < 0 || indice >= h->num || h->tabla[indice].ancho == 0)
        return (surface_t){0};
    const hoja_entrada_t *e = &h->tabla[indice];
    int paso = (e->ancho + 7) & ~7;
    return surface_make(h->pixeles + e->desplazamiento, FMT_CI8, e->ancho, e->alto, paso);
}

void graficos_modo_sprites(const uint16_t *paleta)
{
    rdpq_set_mode_standard();
    rdpq_mode_tlut(TLUT_RGBA16);
    rdpq_mode_alphacompare(1);
    rdpq_tex_upload_tlut((uint16_t *)paleta, 0, 256);
}

void hoja_dibujar(const hoja_t *h, int indice, int x, int y, int escala, bool espejo)
{
    surface_t s = hoja_superficie(h, indice);
    if (!s.width)
        return;
    if (s.stride * s.height > 2048) {
        // No cabe en la mitad de la TMEM que deja la paleta: rdpq_tex_blit lo
        // parte en tiras (29 de los 4.601 sprites de DATAM, docs/04_construccion.md).
        rdpq_tex_blit(&s, x, y, &(rdpq_blitparms_t){.scale_x = escala, .scale_y = escala, .flip_x = espejo});
        return;
    }
    rdpq_tex_upload(TILE0, &s, NULL);
    int x0 = x, x1 = x + s.width * escala;
    if (espejo) {
        x0 = x1;
        x1 = x;
    }
    rdpq_texture_rectangle_scaled(TILE0, x0, y, x1, y + s.height * escala, 0, 0, s.width, s.height);
}
