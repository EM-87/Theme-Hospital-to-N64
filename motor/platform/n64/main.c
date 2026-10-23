// Punto de entrada del motor en la N64.
//
// Hito 2 de la fase 5: el mapa isométrico de un nivel con scroll.
//   - Carga la paleta, los bloques del mapa de DATAM (MBLK-0) y un nivel
//     convertido (TMP1); el núcleo (th/map.h) lo deja como CorsixTH al empezar.
//   - Visor: el stick (o la cruceta) mueve la cámara a 30 fps; Z pasa al nivel
//     siguiente; R oculta los datos de pantalla; L mide el RDP en cada
//     fotograma (espera a que termine: en una consola de verdad da su tiempo;
//     ares no lo emula).
//   - Al arrancar se repite la comprobación del hito 1 (un sprite de MSPR-0
//     píxel a píxel).
//
// Con -DTH_PRUEBAS ejecuta además, antes del visor:
//   - las pruebas del núcleo (las mismas que en el PC);
//   - la comparación píxel a píxel de lo que dibuja el RDP con el rasterizador
//     de referencia (vista/raster.c), en varias posiciones de cámara;
//   - un recorrido automático de la cámara que mide la CPU por fotograma;
// y, si corre en un emulador con EMUX (ares en modo Homebrew), cierra el
// emulador al terminar (tools/motor_check.sh).
#include <libdragon.h>
#include <stdarg.h>
#include <stdlib.h>

#include "graficos.h"
#include "mapa_n64.h"
#include "raster.h"
#include "th/map.h"
#include "th/version.h"
#include "vista.h"

#ifdef TH_PRUEBAS
#include "th_test.h"

static void log_debugf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    debugf("TH64 pruebas: %s", buf);
}

void (*th_test_log)(const char *fmt, ...) = log_debugf;
#endif

enum { FUENTE_DEPURACION = 1 };
#define COLOR_FONDO RGBA32(24, 72, 64, 255)
#define ANCHO 320
#define ALTO 240
#define UNIDAD 16 // gráficos de DATAM: casillas de 32 × 16
#define FPS 30
#define NIVELES 12

static th_map mapa;
static th_draw elementos[4096];
static th_drawlist lista = {.items = elementos, .capacity = 4096};

// Primer sprite "de buen tamaño" que cabe en la TMEM: el de la comprobación.
static int sprite_de_prueba(const hoja_t *h)
{
    for (int i = 0; i < h->num; i++) {
        const hoja_entrada_t *e = &h->tabla[i];
        int paso = (e->ancho + 7) & ~7;
        if (e->ancho >= 16 && e->alto >= 24 && paso * e->alto <= 2048)
            return i;
    }
    return 0;
}

// Hito 1: dibuja el sprite a escala 1 sobre el fondo, espera al RDP y compara
// cada píxel del framebuffer con el color que le toca según la hoja y la
// paleta. Devuelve el número de píxeles distintos (-1 si no se pudo comprobar).
static int comprobar_dibujo(const hoja_t *h, const paletas_t *p, int indice, int *comparados)
{
    const int X = 20, Y = 20;
    surface_t s = hoja_superficie(h, indice);
    if (!s.width)
        return -1;
    surface_t *pantalla = display_get();
    rdpq_attach(pantalla, NULL);
    rdpq_set_mode_fill(COLOR_FONDO);
    rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
    graficos_modo_sprites(p->colores[PALETA_NORMAL]);
    hoja_dibujar(h, indice, X, Y, 1, false);
    rdpq_detach_wait();
    data_cache_hit_invalidate(pantalla->buffer, pantalla->stride * pantalla->height);
    const uint16_t fondo = color_to_packed16(COLOR_FONDO);
    const uint16_t *fb = pantalla->buffer;
    const uint8_t *px = s.buffer;
    int distintos = 0;
    *comparados = 0;
    for (int j = 0; j < s.height; j++) {
        for (int i = 0; i < s.width; i++) {
            uint8_t idx = px[j * s.stride + i];
            uint16_t esperado = idx == 255 ? fondo : p->colores[PALETA_NORMAL][idx];
            uint16_t real = fb[(Y + j) * (pantalla->stride / 2) + X + i];
            // El bit bajo del framebuffer de 16 bits es cobertura, no color.
            if ((real & 0xFFFE) != (esperado & 0xFFFE))
                distintos++;
            (*comparados)++;
        }
    }
    display_show(pantalla);
    return distintos;
}

static bool cargar_nivel(int n)
{
    char ruta[64];
    snprintf(ruta, sizeof ruta, "rom:/niveles/nivel%02d.tmp", n);
    int tam = 0;
    uint8_t *datos = asset_load(ruta, &tam);
    if (!datos)
        return false;
    int r = th_map_load(&mapa, datos, (size_t)tam, n);
    free(datos);
    if (r != TH_MAP_OK)
        debugf("TH64 motor: %s no vale (error %d)\n", ruta, r);
    return r == TH_MAP_OK;
}

// Cámara con la casilla (x, y) en el centro de la pantalla.
static th_view vista_en(int x, int y)
{
    th_view v = {.unit = UNIDAD, .w = ANCHO, .h = ALTO};
    th_view_tile(&v, x, y, &v.x, &v.y);
    v.x -= ANCHO / 2;
    v.y += UNIDAD / 2 - ALTO / 2;
    th_view_clamp(&v);
    return v;
}

typedef struct {
    uint32_t lista, rdp; // ticks de CPU: generar la lista y enviar los comandos
    mapa_n64_stats_t st;
} medida_t;

// Dibuja el mapa en la pantalla ya enganchada a rdpq y mide la CPU.
static void dibujar_mapa(const th_view *v, const th_sheet *sh, const hoja_t *bloques, const uint16_t *paleta,
                         medida_t *m)
{
    uint32_t t0 = get_ticks();
    int n = th_view_map(v, &mapa, sh, &lista);
    assertf(n >= 0, "la lista de dibujo no cabe");
    uint32_t t1 = get_ticks();
    mapa_n64_dibujar(&lista, bloques, paleta, ANCHO, ALTO, &m->st);
    m->lista = t1 - t0;
    m->rdp = get_ticks() - t1;
}

#ifdef TH_PRUEBAS
// Hito 2: dibuja con el RDP una vista del mapa, la dibuja también con el
// rasterizador de referencia (en el orden de CorsixTH, sprite a sprite) y
// compara los dos píxel a píxel. En las sombras la mezcla del RDP redondea
// distinto: se cuentan aparte las que difieren en 1 (de 31) por componente.
static int comprobar_vista(const th_view *v, const th_sheet *sh, const hoja_t *bloques, const uint16_t *paleta)
{
    surface_t *pantalla = display_get();
    rdpq_attach(pantalla, NULL);
    rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
    rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
    medida_t m;
    dibujar_mapa(v, sh, bloques, paleta, &m);
    rdpq_detach_wait();
    data_cache_hit_invalidate(pantalla->buffer, pantalla->stride * pantalla->height);

    static uint8_t pal8[256][3];
    for (int i = 0; i < 256; i++) {
        unsigned c = paleta[i], r = c >> 11 & 31, g = c >> 6 & 31, b = c >> 1 & 31;
        pal8[i][0] = (uint8_t)(r << 3 | r >> 2);
        pal8[i][1] = (uint8_t)(g << 3 | g >> 2);
        pal8[i][2] = (uint8_t)(b << 3 | b >> 2);
    }
    th_raster r = {.rgb = malloc(ANCHO * ALTO * 3), .mezcla = malloc(ANCHO * ALTO), .w = ANCHO, .h = ALTO,
                   .paleta = (const uint8_t(*)[3])pal8};
    assertf(r.rgb && r.mezcla, "sin memoria para la referencia");
    th_raster_clear(&r, 0, 0, 0);
    th_raster_draw(&r, sh, &lista);

    const uint16_t *fb = pantalla->buffer;
    int distintos = 0, sombra_1 = 0, sombras = 0;
    for (int y = 0; y < ALTO; y++) {
        for (int x = 0; x < ANCHO; x++) {
            int i = y * ANCHO + x;
            uint16_t c = fb[y * (pantalla->stride / 2) + x];
            int d = 0;
            for (int k = 0; k < 3; k++) {
                int real = c >> (11 - 5 * k) & 31, esperado = r.rgb[3 * i + k] >> 3;
                int dk = real > esperado ? real - esperado : esperado - real;
                if (dk > d)
                    d = dk;
            }
            sombras += r.mezcla[i];
            if (d == 0)
                continue;
            if (d == 1 && r.mezcla[i]) {
                sombra_1++;
            } else {
                if (distintos++ < 4)
                    debugf("TH64 hito2:   (%d,%d): RDP %04x, referencia %02x%02x%02x%s\n", x, y, c, r.rgb[3 * i],
                           r.rgb[3 * i + 1], r.rgb[3 * i + 2], r.mezcla[i] ? " (sombra)" : "");
            }
        }
    }
    free(r.rgb);
    free(r.mezcla);
    debugf("TH64 hito2: vista (%d,%d): %d pixeles comparados, %d distintos; %d de %d de sombra a 1/31; "
           "%d sprites, %d cargas, %d cortes\n",
           v->x, v->y, ANCHO * ALTO, distintos, sombra_1, sombras, m.st.sprites, m.st.cargas, m.st.cortes);
    display_show(pantalla);
    return distintos;
}

// Recorrido automático de la cámara a 30 fps: mide la CPU de cada fotograma
// (lista + comandos del RDP) y el tiempo entre fotogramas. Con texto, dibuja
// además encima las mismas 5 líneas que el visor: el código y los datos del
// texto echan de las cachés los del mapa, como hará la interfaz del juego.
static void recorrido(th_view inicio, const th_sheet *sh, const hoja_t *bloques, const uint16_t *paleta,
                      bool con_texto)
{
    // Tramos de 8 píxeles por fotograma: un rectángulo de 768 × 512 alrededor del inicio.
    static const struct { int dx, dy, n; } tramos[] = {
        {8, 0, 48}, {0, 8, 32}, {-8, 0, 96}, {0, -8, 64}, {8, 0, 96}, {0, 8, 32}, {-8, 0, 48}};
    th_view v = inicio;
    uint64_t suma_cpu = 0, suma_lista = 0, suma_int = 0, suma_spr = 0, suma_pix = 0, suma_carg = 0;
    uint64_t suma_suelos = 0, suma_paredes = 0, suma_mapa = 0;
    uint32_t max_mapa = 0, max_cpu = 0, max_int = 0, max_spr = 0, max_pix = 0, max_carg = 0;
    int fotogramas = 0, saltos = 0;
    uint32_t anterior = 0;
    const uint32_t periodo = TICKS_PER_SECOND / FPS;
    for (unsigned t = 0; t < sizeof tramos / sizeof tramos[0]; t++) {
        for (int k = 0; k < tramos[t].n; k++) {
            surface_t *pantalla = display_get();
            uint32_t ahora = get_ticks();
            if (fotogramas > 0) {
                uint32_t intervalo = ahora - anterior;
                suma_int += intervalo;
                if (intervalo > max_int)
                    max_int = intervalo;
                if (intervalo > periodo + periodo / 2)
                    saltos++;
            }
            anterior = ahora;
            v.x += tramos[t].dx;
            v.y += tramos[t].dy;
            th_view_clamp(&v);
            rdpq_attach(pantalla, NULL);
            medida_t m;
            dibujar_mapa(&v, sh, bloques, paleta, &m);
            if (con_texto) {
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 12, "TH64 %s  nivel %d  (%d,%d)", TH64_VERSION, 1, v.x,
                                 v.y);
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 24, "CPU %.1f ms (lista %.1f)  %.1f fps", 5.5f, 1.6f,
                                 display_get_fps());
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 36, "%d sprites  %d cargas  %d px", m.st.sprites,
                                 m.st.cargas, m.st.pixeles);
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 220, "RAM %d KiB  heap %d/%d KiB", 8192, 778, 7518);
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 232, "stick: mover  Z: nivel  R: datos  L: RDP");
            }
            rdpq_detach_show();
            uint32_t cpu = get_ticks() - ahora;
            suma_cpu += cpu;
            suma_mapa += m.lista + m.rdp;
            if (m.lista + m.rdp > max_mapa)
                max_mapa = m.lista + m.rdp;
            suma_lista += m.lista;
            suma_suelos += m.st.ticks_suelos;
            suma_paredes += m.st.ticks_paredes;
            if (cpu > max_cpu)
                max_cpu = cpu;
            suma_spr += m.st.sprites;
            suma_pix += m.st.pixeles;
            suma_carg += m.st.cargas;
            if ((uint32_t)m.st.sprites > max_spr)
                max_spr = m.st.sprites;
            if ((uint32_t)m.st.pixeles > max_pix)
                max_pix = m.st.pixeles;
            if ((uint32_t)m.st.cargas > max_carg)
                max_carg = m.st.cargas;
            fotogramas++;
        }
    }
    const float us = 1e6f / TICKS_PER_SECOND;
    debugf("TH64 hito2: recorrido %s de %d fotogramas a %d fps: CPU del mapa media %.2f ms (lista %.2f, suelos "
           "%.2f, paredes %.2f), maxima %.2f ms; fotograma entero media %.2f ms, maxima %.2f ms; intervalo medio "
           "%.2f ms, maximo %.2f ms; %d saltos\n",
           con_texto ? "con texto" : "sin texto", fotogramas, FPS, suma_mapa * us / fotogramas / 1000,
           suma_lista * us / fotogramas / 1000, suma_suelos * us / fotogramas / 1000,
           suma_paredes * us / fotogramas / 1000, max_mapa * us / 1000, suma_cpu * us / fotogramas / 1000,
           max_cpu * us / 1000, suma_int * us / (fotogramas - 1) / 1000, max_int * us / 1000, saltos);
    debugf("TH64 hito2: por fotograma: %lu sprites (max %lu), %lu cargas de textura (max %lu), "
           "%lu pixeles de relleno (max %lu)\n",
           (unsigned long)(suma_spr / fotogramas), (unsigned long)max_spr, (unsigned long)(suma_carg / fotogramas),
           (unsigned long)max_carg, (unsigned long)(suma_pix / fotogramas), (unsigned long)max_pix);
}
#endif

int main(void)
{
    debug_init(DEBUG_FEATURE_LOG_USB | DEBUG_FEATURE_LOG_EMU);
    const int ram_kib = get_memory_size() / 1024;
    const bool expansion = is_memory_expanded();
    debugf("TH64 motor %s: RDRAM %d KiB, Expansion Pak %s\n", TH64_VERSION, ram_kib, expansion ? "si" : "no");

#ifdef TH_PRUEBAS
    int fallos = th_run_core_tests();
    debugf("TH64 pruebas: nucleo, %d fallos\n", fallos);
#endif

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    joypad_init();
    asset_init_compression(2);
    const bool dfs_ok = dfs_init(DFS_DEFAULT_LOCATION) == DFS_ESUCCESS;
    rdpq_text_register_font(FUENTE_DEPURACION, rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));

    paletas_t paletas = {0};
    hoja_t sprites = {0}, bloques = {0};
    th_sheet sh = {0};
    int nivel = 1;
    const bool assets = dfs_ok && paletas_cargar(&paletas, "rom:/paleta.tpl") &&
                        hoja_cargar(&sprites, "rom:/datam/mspr-0.tsh") &&
                        hoja_cargar(&bloques, "rom:/datam/mblk-0.tsh") &&
                        th_sheet_init(&sh, bloques.bloque, (size_t)bloques.tam) == 0 && cargar_nivel(nivel);
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    debugf("TH64 motor: heap usado %d KiB de %d KiB; DFS %s; assets %s\n", heap.used / 1024, heap.total / 1024,
           dfs_ok ? "ok" : "no encontrado", assets ? "ok" : "no encontrados (tools/conv/conv.py)");

    if (assets) {
        int actual = sprite_de_prueba(&sprites);
        int comparados = 0;
        int distintos = comprobar_dibujo(&sprites, &paletas, actual, &comparados);
        debugf("TH64 hito1: sprite %d de %d (%dx%d): %d pixeles comparados, %d distintos\n", actual, sprites.num,
               sprites.tabla[actual].ancho, sprites.tabla[actual].alto, comparados, distintos);
        // El visor del mapa no usa los sprites: fuera de la RAM.
        free(sprites.bloque);
        sprites = (hoja_t){0};
        sys_get_heap_stats(&heap);
        debugf("TH64 hito2: nivel %d cargado; heap usado %d KiB (mapa %u KiB y lista de dibujo %u KiB aparte, "
               "estaticos)\n",
               nivel, heap.used / 1024, (unsigned)(sizeof(mapa) / 1024), (unsigned)(sizeof(elementos) / 1024));
    }

    th_view v = {0};
    if (assets) {
        mapa_n64_preparar(&bloques, UNIDAD);
        v = vista_en(mapa.camera_x, mapa.camera_y);
    }
    display_set_fps_limit(FPS);

#ifdef TH_PRUEBAS
    if (assets) {
        // Nivel 1: cámara inicial, una sombra sobre una pared (casilla 71,71),
        // la esquina superior del mapa y el exterior del hospital. Nivel 12:
        // parcelas sin dueño (césped y adornos) con paredes divisorias.
        const struct { int nivel, x, y; } vistas[] = {{1, -1, -1}, {1, 71, 71}, {1, 0, 0}, {1, 80, 40}, {12, 36, 28}};
        int distintos = 0;
        for (unsigned i = 0; i < sizeof vistas / sizeof vistas[0]; i++) {
            if (vistas[i].nivel != nivel && !cargar_nivel(nivel = vistas[i].nivel))
                break;
            th_view c = vistas[i].x < 0 ? v : vista_en(vistas[i].x, vistas[i].y);
            distintos += comprobar_vista(&c, &sh, &bloques, paletas.colores[PALETA_NORMAL]);
        }
        debugf("TH64 hito2: comparacion con la referencia: %d pixeles distintos en total\n", distintos);
        if (distintos)
            fallos++;
        if (nivel != 1)
            cargar_nivel(nivel = 1);
        recorrido(v, &sh, &bloques, paletas.colores[PALETA_NORMAL], false);
        recorrido(v, &sh, &bloques, paletas.colores[PALETA_NORMAL], true);
    }
    debugf("TH64 pruebas: fin, %d fallos\n", fallos);
    if (emux_detect(0))
        emux_ioctl_exit();
#endif

    bool datos = true, medir_rdp = false;
    float cpu_ms = 0, lista_ms = 0, rdp_buf_ms = 0, rdp_pipe_ms = 0;
    mapa_n64_stats_t st = {0};
    for (;;) {
        joypad_poll();
        joypad_inputs_t in = joypad_get_inputs(JOYPAD_PORT_1);
        joypad_buttons_t pulsado = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        if (pulsado.r)
            datos = !datos;
        if (pulsado.l)
            medir_rdp = !medir_rdp;
        if (assets && pulsado.z) {
            nivel = nivel % NIVELES + 1;
            if (cargar_nivel(nivel))
                v = vista_en(mapa.camera_x, mapa.camera_y);
        }
        // Stick con zona muerta; la cruceta, 8 píxeles por fotograma.
        int sx = abs(in.stick_x) > 8 ? in.stick_x / 6 : 0;
        int sy = abs(in.stick_y) > 8 ? -in.stick_y / 6 : 0;
        sx += in.btn.d_right ? 8 : in.btn.d_left ? -8 : 0;
        sy += in.btn.d_down ? 8 : in.btn.d_up ? -8 : 0;
        v.x += sx;
        v.y += sy;
        th_view_clamp(&v);

        surface_t *pantalla = display_get();
        uint32_t t0 = get_ticks();
        if (medir_rdp)
            *DP_STATUS = DP_WSTATUS_RESET_CLOCK_COUNTER | DP_WSTATUS_RESET_CMD_COUNTER | DP_WSTATUS_RESET_PIPE_COUNTER |
                         DP_WSTATUS_RESET_TMEM_COUNTER;
        rdpq_attach(pantalla, NULL);
        rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
        rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
        medida_t m = {0};
        if (assets)
            dibujar_mapa(&v, &sh, &bloques, paletas.colores[PALETA_NORMAL], &m);
        uint32_t t1 = get_ticks();
        if (medir_rdp) {
            // El RDP ha terminado el mapa cuando vuelve rspq_wait. Los
            // contadores cuentan ciclos del RDP (62,5 MHz); ares no los emula.
            rspq_wait();
            rdp_buf_ms = (*DP_BUSY & 0xFFFFFF) / 62500.0f;
            rdp_pipe_ms = (*DP_PIPE_BUSY & 0xFFFFFF) / 62500.0f;
        }
        // Medias de 30 fotogramas.
        const float ms = 1000.0f / TICKS_PER_SECOND;
        cpu_ms += ((t1 - t0) * ms - cpu_ms) / 30;
        lista_ms += (m.lista * ms - lista_ms) / 30;
        st = m.st;
        if (datos) {
            rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 12, "TH64 %s  nivel %d  (%d,%d)", TH64_VERSION, nivel, v.x,
                             v.y);
            if (assets) {
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 24, "CPU %.1f ms (lista %.1f)  %.1f fps", cpu_ms,
                                 lista_ms, display_get_fps());
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 36, "%d sprites  %d cargas  %d px", st.sprites, st.cargas,
                                 st.pixeles);
                if (medir_rdp)
                    rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 48, "RDP ocupado %.2f ms (pipe %.2f ms)", rdp_buf_ms,
                                     rdp_pipe_ms);
            } else {
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 24, "Sin assets: ejecuta tools/conv/conv.py");
            }
            rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 220, "RAM %d KiB  heap %d/%d KiB", ram_kib, heap.used / 1024,
                             heap.total / 1024);
            rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 232, "stick: mover  Z: nivel  R: datos  L: RDP");
        }
        rdpq_detach_show();
    }
}
