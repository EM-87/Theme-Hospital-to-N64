#include "raster.h"

#include <string.h>

#define ALFA 64 // thdf_alpha_75 de CorsixTH: SDL_SetTextureAlphaMod(0x40)

void th_raster_clear(th_raster *r, uint8_t cr, uint8_t cg, uint8_t cb)
{
    for (int i = 0; i < r->w * r->h; i++) {
        r->rgb[3 * i] = cr;
        r->rgb[3 * i + 1] = cg;
        r->rgb[3 * i + 2] = cb;
    }
    if (r->mezcla)
        memset(r->mezcla, 0, (size_t)r->w * r->h);
}

static void pintar(th_raster *r, const th_sheet *sh, const th_draw *d)
{
    int w, h;
    th_sheet_size(sh, d->sprite, &w, &h);
    const uint8_t *px = th_sheet_pixels(sh, d->sprite);
    const int paso = th_sheet_stride(w);
    int y0 = d->y, y1 = d->y + h;
    if (d->flags & TH_DRAW_CLIP) {
        if (d->clip_y0 > y0)
            y0 = d->clip_y0;
        if (d->clip_y1 < y1)
            y1 = d->clip_y1;
    }
    if (y0 < 0)
        y0 = 0;
    if (y1 > r->h)
        y1 = r->h;
    const int mezcla = (d->flags & TH_DRAW_ALPHA25) != 0;
    for (int y = y0; y < y1; y++) {
        const uint8_t *fila = px + (y - d->y) * paso;
        for (int i = 0; i < w; i++) {
            int x = d->x + i;
            uint8_t c = fila[i];
            if (x < 0 || x >= r->w || c == TH_TRANSPARENT)
                continue;
            uint8_t *o = &r->rgb[3 * (y * r->w + x)];
            const uint8_t *p = r->paleta[c];
            if (mezcla) {
                for (int k = 0; k < 3; k++)
                    o[k] = (uint8_t)((p[k] * ALFA + o[k] * (255 - ALFA) + 127) / 255);
            } else {
                o[0] = p[0];
                o[1] = p[1];
                o[2] = p[2];
            }
            if (r->mezcla)
                r->mezcla[y * r->w + x] = (uint8_t)mezcla;
        }
    }
}

void th_raster_draw(th_raster *r, const th_sheet *sh, const th_drawlist *l)
{
    for (int i = 0; i < l->count; i++)
        pintar(r, sh, &l->items[i]);
}
