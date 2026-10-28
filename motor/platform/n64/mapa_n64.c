#include "mapa_n64.h"

#include <string.h>

#define ALFA_25 64  // 25 % de opacidad, como thdf_alpha_75 de CorsixTH
#define ALFA_50 128 // thdf_alpha_50
#define MAX_ORDEN 4096
#define TMEM_TEXELES 2048 // la otra mitad de la TMEM es la paleta (TLUT)

static uint16_t orden[MAX_ORDEN];
static int32_t cargado;      // hoja << 16 | sprite en la TMEM; -1 = ninguno o desconocido
static int paso_tile0;       // el paso con que está configurado TILE0; -1 = desconocido
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

// TILE1 es el de las cargas (texeles de 16 bits, sin paso: LOAD_BLOCK) y
// TILE0 el de dibujar, con el tamaño máximo para no tener que cambiarlo en
// cada sprite: los rectángulos solo leen texeles del sprite y, como mucho,
// la columna de relleno, que es transparente.
static void preparar_tiles(void)
{
    rdpq_set_tile(TILE1, FMT_RGBA16, 0, 0, NULL);
    rdpq_set_tile_size(TILE0, 0, 0, 1024, 1024);
    paso_tile0 = -1;
    cargado = -1;
}

#ifdef TH_MEDIR
int mapa_n64_modo_carga;
static void cargar_exec(const hoja_t *h, int sprite);
#endif

// Carga un sprite que cabe en la TMEM: el bloque entero (paso × alto bytes,
// con el relleno transparente de cada fila) con un LOAD_BLOCK de texeles de
// 16 bits. Son 2 comandos, más SET_TILE si cambia el paso, frente a los 5 de
// rdpq_tex_upload, y menos SYNC: un 80 % menos de CPU por carga (hito 5).
// Con 11 palabras de 64 bits por fila o más, el LOAD_BLOCK pierde precisión
// en las filas impares (el DXT de rdpq_tex.c): esos van por rdpq_tex_upload
// (en MBLK-0 y MSPR-0 no hay ninguno).
static void cargar(const hoja_t *h, int sprite)
{
    const hoja_entrada_t *e = &h->tabla[sprite];
    const int paso = th_sheet_stride(e->ancho);
#ifdef TH_MEDIR
    if (mapa_n64_modo_carga == MAPA_N64_CARGA_RDPQ_TEX) {
        surface_t s = hoja_superficie(h, sprite);
        rdpq_tex_upload(TILE0, &s, NULL);
        return;
    }
    if (mapa_n64_modo_carga == MAPA_N64_CARGA_EXEC && paso < 11 * 8) {
        cargar_exec(h, sprite);
        paso_tile0 = paso;
        return;
    }
#endif
    if (paso >= 11 * 8) {
        surface_t s = hoja_superficie(h, sprite);
        rdpq_tex_upload(TILE0, &s, NULL);
        rdpq_set_tile_size(TILE0, 0, 0, 1024, 1024);
        paso_tile0 = paso;
        return;
    }
    rdpq_set_texture_image_raw(0, PhysicalAddr(h->pixeles + e->desplazamiento), FMT_RGBA16, paso / 2, e->alto);
    rdpq_load_block(TILE1, 0, 0, paso * e->alto / 2, paso);
    if (paso != paso_tile0) {
        rdpq_set_tile(TILE0, FMT_CI8, 0, paso, NULL);
        paso_tile0 = paso;
    }
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
        preparar_tiles();
        st->cargas++;
        return;
    }
    const int32_t clave = (int32_t)d->sheet << 16 | d->sprite;
    if (cargado != clave) {
#ifdef TH_MEDIR
        const uint32_t c0 = get_ticks();
#endif
        cargar(h, d->sprite);
        cargado = clave;
        st->cargas++;
#ifdef TH_MEDIR
        st->ticks_cargas += get_ticks() - c0;
#endif
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
    graficos_modo_sprites(paleta);
    preparar_tiles();

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
        if (d->sheet == TH_SHEET_FILL) {
            // Un rectángulo de color (el recuadro de una descripción): en modo
            // relleno, y se vuelve al de los sprites.
            rdpq_set_mode_fill(color_from_packed16(d->sprite));
            rdpq_fill_rectangle(d->x, d->y, d->x + d->clip_x1, d->y + d->clip_y1);
            graficos_modo_sprites(paleta);
            preparar_tiles();
            alfa = 255;
            st->sprites++;
            continue;
        }
        int a = alfa_de(d);
        if (a != alfa) {
            if (a < 255)
                modo_sombra(a);
            else
                modo_opaco();
            alfa = a;
        }
#ifdef TH_MEDIR
        const uint32_t e0 = get_ticks();
        dibujar(hojas, d, ancho, alto, st);
        if (d->sheet == TH_SHEET_SPRITES)
            st->ticks_entidades += get_ticks() - e0;
#else
        dibujar(hojas, d, ancho, alto, st);
#endif
    }
    if (alfa < 255)
        modo_opaco();
    st->ticks_paredes = get_ticks() - t1;
}

#ifdef TH_MEDIR
// Hito 5: la carga con los comandos del RDP ya hechos (rdpq_exec), uno por
// sprite, preparados la primera vez que se dibuja: SYNC_PIPE, SYNC_LOAD,
// SYNC_TILE, SET_TEXTURE_IMAGE, LOAD_BLOCK y SET_TILE (TILE0, CI8). La CPU
// envía un solo comando por carga, pero no es la carga normal: en cada uno,
// el RSP espera a que el RDP haya empezado el búfer anterior
// (RSPQCmd_RdpSetBuffer), y ares no lo puede decir, porque ejecuta los
// comandos del RDP al instante.
#define CMD(id, w0, w1) (((uint64_t)((uint32_t)(id) << 24 | (w0)) << 32) | (uint32_t)(w1))
enum { EXEC_COMANDOS = 6 };
static const hoja_t *exec_hoja[TH_SHEETS];
static uint64_t **exec_tabla[TH_SHEETS]; // por sprite, en el heap la primera vez (no cuenta en la memoria del arranque)

static uint64_t *preparar_exec(const hoja_t *h, int sprite)
{
    const hoja_entrada_t *e = &h->tabla[sprite];
    const uint32_t paso = th_sheet_stride(e->ancho), dir = PhysicalAddr(h->pixeles + e->desplazamiento);
    const uint32_t texeles = paso * e->alto / 2, palabras = paso / 8, dxt = (2048 + palabras - 1) / palabras;
    uint64_t *c = malloc(EXEC_COMANDOS * sizeof(uint64_t));
    assertf(c && ((uintptr_t)c & 7) == 0, "sin memoria para los comandos de carga");
    c[0] = CMD(0xE7, 0, 0);                                          // SYNC_PIPE
    c[1] = CMD(0xE6, 0, 0);                                          // SYNC_LOAD
    c[2] = CMD(0xE8, 0, 0);                                          // SYNC_TILE
    c[3] = CMD(0xFD, (2u << 19) | (paso / 2 - 1), dir);              // SET_TEXTURE_IMAGE, RGBA16
    c[4] = CMD(0xF3, 0, (1u << 24) | ((texeles - 1) << 12) | dxt);   // LOAD_BLOCK con TILE1
    c[5] = CMD(0xF5, (2u << 21) | (1u << 19) | (palabras << 9), 0);  // SET_TILE TILE0, CI8
    data_cache_hit_writeback(c, EXEC_COMANDOS * sizeof(uint64_t));
    return c;
}

static void cargar_exec(const hoja_t *h, int sprite)
{
    int k = 0;
    while (exec_hoja[k] && exec_hoja[k] != h)
        k++;
    if (!exec_hoja[k]) {
        exec_hoja[k] = h;
        exec_tabla[k] = calloc(h->num, sizeof(uint64_t *));
        assertf(exec_tabla[k], "sin memoria para los comandos de carga");
    }
    uint64_t **c = &exec_tabla[k][sprite];
    if (!*c)
        *c = preparar_exec(h, sprite);
    rdpq_exec(*c, EXEC_COMANDOS * sizeof(uint64_t));
}

// Lo que cuesta cargar cada sprite de la hoja h que cabe en la TMEM y dibujar
// un rectángulo pequeño con él, con las tres cargas, con el RDP libre al
// empezar. Escribe líneas «H5 c».
void mapa_n64_medir_cargas(const hoja_t *h, const uint16_t *paleta)
{
    enum { N = 256 };
    static uint16_t cuales[N];
    int n = 0;
    for (int i = 0; i < h->num && n < N; i++) {
        const hoja_entrada_t *e = &h->tabla[i];
        if (e->ancho && th_sheet_stride(e->ancho) * e->alto <= TMEM_TEXELES && th_sheet_stride(e->ancho) < 11 * 8)
            cuales[n++] = (uint16_t)i;
    }
    rdpq_attach(display_get(), NULL);
    graficos_modo_sprites(paleta);
    static const char *const nombres[] = {"directa", "rdpq_tex_upload", "exec"};
    for (int prueba = 0; prueba < 5; prueba++) {
        mapa_n64_modo_carga = prueba < 3 ? prueba : MAPA_N64_CARGA_DIRECTA;
        for (int rep = 0; rep < 2; rep++) {
            preparar_tiles();
            for (int k = 0; k < n; k++) // los comandos de exec, preparados antes
                cargar(h, cuales[k]);
            rspq_wait();
            const uint32_t t0 = get_ticks();
            for (int k = 0; k < n; k++) {
                const hoja_entrada_t *e = &h->tabla[cuales[k]];
                if (prueba < 3)
                    cargar(h, cuales[k]);
                if (prueba < 4)
                    rdpq_texture_rectangle(TILE0, 0, 0, e->ancho < 8 ? e->ancho : 8, 1, 0, 0);
            }
            const uint32_t t1 = get_ticks();
            rspq_wait();
            const uint32_t t2 = get_ticks();
            debugf("H5 c %s %d: %d sprites; CPU %lu ciclos por sprite; hasta que acaban el RSP y el RDP, %lu\n",
                   prueba < 3 ? nombres[prueba] : prueba == 3 ? "solo_rectangulo" : "nada", rep, n,
                   (unsigned long)((t1 - t0) * 2 / n), (unsigned long)((t2 - t0) * 2 / n));
        }
    }
    mapa_n64_modo_carga = MAPA_N64_CARGA_DIRECTA;
    rdpq_detach_show();
}
#endif
