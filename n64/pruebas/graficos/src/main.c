// Prueba de APIs gráficas de libdragon para el motor de TH64:
//  - coste de CPU de rdpq_tex_blit con texturas CI8 (paleta en TMEM),
//    con y sin espejo, con y sin LOAD_BLOCK, y con sprites que no caben en TMEM;
//  - coste de cambiar la TLUT por sprite (paleta "fantasma");
//  - rdpq_sprite_blit (sube la paleta en cada llamada);
//  - fuente de TH convertida a BMFont + mkfont, con texto en español (UTF-8).
// Escribe los resultados por ISViewer (debugf) y deja un fotograma fijo para la captura.
#include <libdragon.h>

#define N 1000

static uint16_t pal_a[256] __attribute__((aligned(8)));
static uint16_t pal_g[256] __attribute__((aligned(8)));

static surface_t mk(int w, int h, int seed)
{
    surface_t s = surface_alloc(FMT_CI8, w, h);
    uint8_t *p = s.buffer;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < s.stride; x++) {
            int v = (x * 7 + y * 13 + seed) & 0xFF;
            // borde transparente (índice 255) y una diagonal para ver el espejo
            if (x >= w || x == 0 || y == 0 || x + y < 4) v = 255;
            else if (x == y) v = 10;
            if (v == 255 && x > 0 && y > 0 && x < w && x + y >= 4) v = 254;
            p[y * s.stride + x] = v;
        }
    data_cache_hit_writeback(s.buffer, s.stride * h);
    return s;
}

static void heap(const char *what)
{
    heap_stats_t h;
    sys_get_heap_stats(&h);
    debugf("TH64 gfx: heap %-28s usado %5d KiB libre %5d KiB\n", what, h.used / 1024, (h.total - h.used) / 1024);
}

static void setmode(void)
{
    rdpq_set_mode_standard();
    rdpq_mode_alphacompare(1);
    rdpq_mode_tlut(TLUT_RGBA16);
}


// Camino a mano para un sprite CI8 con ancho múltiplo de 8 y stride == ancho:
// los mismos 6 comandos que genera rdpq_tex_blit con LOAD_BLOCK, sin el cargador genérico.
static inline void blit_raw(const surface_t *s, int x, int y, bool flip)
{
    int w = s->width, h = s->height;
    rdpq_set_texture_image_raw(0, PhysicalAddr(s->buffer), FMT_RGBA16, w / 2, h);
    rdpq_set_tile(TILE1, FMT_RGBA16, 0, 0, NULL);
    rdpq_set_tile(TILE0, FMT_CI8, 0, w, NULL);
    rdpq_load_block(TILE1, 0, 0, w * h / 2, w);
    rdpq_set_tile_size(TILE0, 0, 0, w, h);
    if (flip) rdpq_texture_rectangle(TILE0, x + w, y, x, y + h, 0, 0);
    else      rdpq_texture_rectangle(TILE0, x, y, x + w, y + h, 0, 0);
}

typedef struct { const char *name; surface_t *s; bool flip; int tlut_each; } caso_t;

int main(void)
{
    debug_init_emulog();
    dfs_init(DFS_DEFAULT_LOCATION);
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    heap("tras display+rdpq");

    for (int i = 0; i < 256; i++) {
        int r = (i * 37) & 0xFF, g = (i * 91 + 13) & 0xFF, b = (255 - i * 53) & 0xFF;
        pal_a[i] = color_to_packed16(RGBA32(r, g, b, 255));
        int gr = (r * 77 + g * 150 + b * 29) >> 8;
        pal_g[i] = color_to_packed16(RGBA32(gr, gr, gr, 255));
    }
    pal_a[255] = pal_g[255] = 0; // transparente: alfa 0
    data_cache_hit_writeback(pal_a, sizeof(pal_a));
    data_cache_hit_writeback(pal_g, sizeof(pal_g));

    surface_t s11 = mk(11, 13, 1);   // mediana de DATAM (11x13), stride 11: LOAD_TILE
    surface_t s16 = mk(16, 13, 2);   // mismo, filas rellenas a 16: LOAD_BLOCK
    surface_t s24 = mk(24, 30, 3);   // p90 (24x30)
    surface_t s80 = mk(80, 74, 4);   // el mayor (73x74 -> 80 de pitch): 5920 B, 3 tiras
    sprite_t *spr = sprite_load("rom:/ci8_impar.sprite");
    heap("tras surfaces y sprite");

    rdpq_font_t *fnt = rdpq_font_load("rom:/font01v.font64");
    rdpq_text_register_font(1, fnt);
    heap("tras cargar font01v.font64");

    caso_t casos[] = {
        {"11x13 stride11 (LOAD_TILE)", &s11, false, 0},
        {"16x13 stride16 (LOAD_BLOCK)", &s16, false, 0},
        {"16x13 flip_x", &s16, true, 0},
        {"24x30", &s24, false, 0},
        {"80x74 (3 tiras)", &s80, false, 0},
        {"16x13 + TLUT 256 cada blit", &s16, false, 256},
        {"16x13 + TLUT 16 cada blit", &s16, false, 16},
    };
    int ncasos = sizeof(casos) / sizeof(casos[0]);

    for (int c = 0; c < ncasos; c++) {
        surface_t *disp = display_get();
        rdpq_attach_clear(disp, NULL);
        setmode();
        rdpq_tex_upload_tlut(pal_a, 0, 256);
        rspq_wait();
        uint32_t t0 = get_ticks();
        for (int i = 0; i < N; i++) {
            if (casos[c].tlut_each)
                rdpq_tex_upload_tlut((i & 1) ? pal_g : pal_a, 0, casos[c].tlut_each);
            rdpq_tex_blit(casos[c].s, (i * 7) % 300, (i * 13) % 220,
                          &(rdpq_blitparms_t){ .flip_x = casos[c].flip });
        }
        uint32_t t1 = get_ticks();
        rdpq_detach_show();
        rspq_wait();
        uint32_t t2 = get_ticks();
        debugf("TH64 gfx: %-30s CPU %6lu ciclos/blit (%4lu us/1000) | hasta fin RSP+RDP %6lu us/1000\n",
               casos[c].name, (unsigned long)((t1 - t0) * 2 / N),
               (unsigned long)TICKS_TO_US(t1 - t0), (unsigned long)TICKS_TO_US(t2 - t0));
    }

    // Separar CPU y RSP+RDP: grabar los blits en un bloque rspq (la CPU solo escribe
    // comandos en RAM, sin esperar al RSP) y luego ejecutar el bloque.
    for (int c = 0; c < ncasos; c++) {
        surface_t *disp = display_get();
        rdpq_attach_clear(disp, NULL);
        rspq_wait();
        uint32_t t0 = get_ticks();
        rspq_block_begin();
        setmode();
        rdpq_tex_upload_tlut(pal_a, 0, 256);
        for (int i = 0; i < N; i++) {
            if (casos[c].tlut_each)
                rdpq_tex_upload_tlut((i & 1) ? pal_g : pal_a, 0, casos[c].tlut_each);
            rdpq_tex_blit(casos[c].s, (i * 7) % 300, (i * 13) % 220,
                          &(rdpq_blitparms_t){ .flip_x = casos[c].flip });
        }
        rspq_block_t *blk = rspq_block_end();
        uint32_t t1 = get_ticks();
        rspq_block_run(blk);
        rspq_wait();
        uint32_t t2 = get_ticks();
        rspq_block_run(blk);
        rspq_wait();
        uint32_t t3 = get_ticks();
        rdpq_detach_show();
        rspq_wait();
        rspq_block_free(blk);
        debugf("TH64 gfx: bloque %-24s CPU %6lu ciclos/blit | RSP+RDP 1a %6lu us/1000, 2a %6lu us/1000\n",
               casos[c].name, (unsigned long)((t1 - t0) * 2 / N),
               (unsigned long)TICKS_TO_US(t2 - t1), (unsigned long)TICKS_TO_US(t3 - t2));
    }
    // Camino mínimo: textura ya en TMEM, solo rectángulos (lo que costaría un blit sin carga)
    {
        surface_t *disp = display_get();
        rdpq_attach_clear(disp, NULL);
        setmode();
        rdpq_tex_upload_tlut(pal_a, 0, 256);
        rdpq_tex_upload(TILE0, &s16, NULL);
        rspq_wait();
        uint32_t t0 = get_ticks();
        for (int i = 0; i < N; i++)
            rdpq_texture_rectangle(TILE0, (i * 7) % 300, (i * 13) % 220, (i * 7) % 300 + 16, (i * 13) % 220 + 13, 0, 0);
        uint32_t t1 = get_ticks();
        rdpq_detach_show();
        rspq_wait();
        uint32_t t2 = get_ticks();
        debugf("TH64 gfx: %-30s CPU %6lu ciclos/rect | hasta fin RSP+RDP %6lu us/1000\n",
               "solo rdpq_texture_rectangle", (unsigned long)((t1 - t0) * 2 / N), (unsigned long)TICKS_TO_US(t2 - t0));
    }

    for (int modo = 0; modo < 3; modo++) {
        const char *nm[] = {"rdpq_tex_upload + rect 16x13", "a mano LOAD_BLOCK 16x13", "a mano 16x13 flip"};
        surface_t *disp = display_get();
        rdpq_attach_clear(disp, NULL);
        setmode();
        rdpq_tex_upload_tlut(pal_a, 0, 256);
        rspq_wait();
        uint32_t t0 = get_ticks();
        for (int i = 0; i < N; i++) {
            int x = (i * 7) % 300, y = (i * 13) % 220;
            if (modo == 0) { rdpq_tex_upload(TILE0, &s16, NULL); rdpq_texture_rectangle(TILE0, x, y, x + 16, y + 13, 0, 0); }
            else blit_raw(&s16, x, y, modo == 2);
        }
        uint32_t t1 = get_ticks();
        rdpq_detach_show();
        rspq_wait();
        uint32_t t2 = get_ticks();
        debugf("TH64 gfx: %-30s CPU %6lu ciclos/blit | hasta fin RSP+RDP %6lu us/1000\n",
               nm[modo], (unsigned long)((t1 - t0) * 2 / N), (unsigned long)TICKS_TO_US(t2 - t0));
    }

    // rdpq_sprite_blit: sube la paleta del sprite en cada llamada
    {
        surface_t *disp = display_get();
        rdpq_attach_clear(disp, NULL);
        rdpq_set_mode_standard();
        rdpq_mode_alphacompare(1);
        rspq_wait();
        uint32_t t0 = get_ticks();
        for (int i = 0; i < N; i++)
            rdpq_sprite_blit(spr, (i * 7) % 300, (i * 13) % 220, NULL);
        uint32_t t1 = get_ticks();
        rdpq_detach_show();
        rspq_wait();
        uint32_t t2 = get_ticks();
        debugf("TH64 gfx: %-30s CPU %6lu ciclos/blit (%4lu us/1000) | hasta fin RSP+RDP %6lu us/1000\n",
               "rdpq_sprite_blit 13x7 (+TLUT)", (unsigned long)((t1 - t0) * 2 / N),
               (unsigned long)TICKS_TO_US(t1 - t0), (unsigned long)TICKS_TO_US(t2 - t0));
    }

    // Texto: coste de imprimir una línea
    {
        surface_t *disp = display_get();
        rdpq_attach_clear(disp, NULL);
        rspq_wait();
        uint32_t t0 = get_ticks();
        for (int i = 0; i < 100; i++)
            rdpq_text_print(NULL, 1, 4, 20 + (i % 20) * 10, "¿Qué ocurrió? ¡Señal de pingüino! Ñandú 1234");
        uint32_t t1 = get_ticks();
        rdpq_detach_show();
        rspq_wait();
        debugf("TH64 gfx: texto 44 car. (rdpq_text_print)  CPU %6lu us por línea\n",
               (unsigned long)TICKS_TO_US(t1 - t0) / 100);
    }
    // Texto: maquetar una vez (rdpq_paragraph_build) y dibujar muchas
    {
        const char *txt = "¿Qué ocurrió? ¡Señal de pingüino! Ñandú 1234";
        int nb = strlen(txt);
        uint32_t t0 = get_ticks();
        rdpq_paragraph_t *par = rdpq_paragraph_build(NULL, 1, txt, &nb);
        uint32_t t1 = get_ticks();
        surface_t *disp = display_get();
        rdpq_attach_clear(disp, NULL);
        rspq_wait();
        uint32_t t2 = get_ticks();
        for (int i = 0; i < 100; i++)
            rdpq_paragraph_render(par, 4, 20 + (i % 20) * 10);
        uint32_t t3 = get_ticks();
        rdpq_detach_show();
        rspq_wait();
        rdpq_paragraph_free(par);
        debugf("TH64 gfx: texto 44 car.: rdpq_paragraph_build %lu us una vez; rdpq_paragraph_render %lu us por línea\n",
               (unsigned long)TICKS_TO_US(t1 - t0), (unsigned long)TICKS_TO_US(t3 - t2) / 100);
    }

    // Fotograma fijo para la captura: texto, sprite normal, espejado, gris y al 50 %
    for (int fr = 0;; fr++) {
        if (fr == 90) debugf("TH64 gfx: fin\n");
        surface_t *disp = display_get();
        rdpq_attach_clear(disp, NULL);
        rdpq_set_mode_fill(RGBA32(40, 60, 40, 255));
        rdpq_fill_rectangle(0, 0, 320, 240);
        rdpq_text_print(&(rdpq_textparms_t){ .width = 300, .wrap = WRAP_WORD }, 1, 10, 20,
            "¿Qué ocurrió? ¡Señal de pingüino! Ñandú, camión, árbol, útil.\nFuente FONT01V de TH vía BMFont + mkfont (CI4).");
        setmode();
        rdpq_tex_upload_tlut(pal_a, 0, 256);
        rdpq_tex_blit(&s24, 20, 80, &(rdpq_blitparms_t){ .scale_x = 3, .scale_y = 3 });
        rdpq_tex_blit(&s24, 110, 80, &(rdpq_blitparms_t){ .scale_x = 3, .scale_y = 3, .flip_x = true });
        rdpq_tex_upload_tlut(pal_g, 0, 256);
        rdpq_tex_blit(&s24, 200, 80, &(rdpq_blitparms_t){ .scale_x = 3, .scale_y = 3 });
        rdpq_tex_upload_tlut(pal_a, 0, 256);
        blit_raw(&s24, 20, 180, false);
        blit_raw(&s24, 60, 180, true);
        rdpq_set_mode_copy(true);
        rdpq_mode_tlut(TLUT_RGBA16);
        blit_raw(&s24, 110, 180, false);
        blit_raw(&s24, 150, 180, true);
        setmode();
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,TEX0), (TEX0,0,PRIM,0)));
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(RGBA32(255, 255, 255, 128));
        rdpq_tex_blit(&s80, 230, 160, NULL);
        rdpq_tex_blit(&s80, 260, 150, NULL);
        rdpq_detach_show();
    }
}
