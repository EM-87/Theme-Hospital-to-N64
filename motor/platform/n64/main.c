// Punto de entrada del motor en la N64.
//
// Hito 3 de la fase 5: humanoides animados que caminan por el mapa siguiendo
// el A* de CorsixTH.
//   - Carga la paleta, los bloques del mapa y los sprites de DATAM, las tablas
//     de animación y un nivel convertido; el núcleo lo deja como CorsixTH.
//   - El mundo avanza a la velocidad Normal de CorsixTH: un tick cada 54 ms.
//     Cada humanoide camina a una casilla del hospital elegida al azar y, al
//     llegar, a otra.
//   - Visor: el stick (o la cruceta) mueve la cámara; A hace que siga al primer
//     humanoide; B añade 10 humanoides; Z pasa al nivel siguiente; R oculta los
//     datos; L mide el RDP en cada fotograma (solo en una consola: ares no lo
//     emula).
//   - Al arrancar se repite la comprobación del hito 1 (un sprite de MSPR-0
//     píxel a píxel).
//
// Con -DTH_PRUEBAS ejecuta además, antes del visor:
//   - las pruebas del núcleo (las mismas que en el PC);
//   - la comparación píxel a píxel de lo que dibuja el RDP con el rasterizador
//     de referencia (vista/raster.c): vistas del mapa (hito 2) y humanoides
//     caminando hacia cada lado, en espejo y junto a paredes (hito 3);
//   - recorridos que miden la CPU por fotograma: la cámara sola (hito 2), la
//     cámara siguiendo a un humanoide y una multitud de 50 (hito 3);
// y, si corre en un emulador con EMUX (ares en modo Homebrew), cierra el
// emulador al terminar (tools/motor_check.sh).
#include <libdragon.h>
#include <stdarg.h>
#include <stdlib.h>

#include "graficos.h"
#include "mapa_n64.h"
#include "raster.h"
#include "th/anim.h"
#include "th/humanoid.h"
#include "th/map.h"
#include "th/path.h"
#include "th/rng.h"
#include "th/rules.h"
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
#define MAX_HUMANOIDES 64
// Velocidad Normal de CorsixTH: un tick del mundo cada 54 ms (tick_rate 3 de 18 ms).
#define MS_POR_TICK 54

static th_map mapa;
static th_pathfinder buscador;
static th_anims anims;
static th_world mundo = {.map = &mapa, .pf = &buscador, .anims = &anims};
static th_humanoid humanoides[MAX_HUMANOIDES];
static th_view_entity entidades[MAX_HUMANOIDES];
static int n_humanoides;
static th_rng azar;
static th_draw elementos[4096];
static th_drawlist lista = {.items = elementos, .capacity = 4096};

static paletas_t paletas;
static hoja_t bloques, sprites;
static th_sheet sh_bloques, sh_sprites;
static const hoja_t *const hojas[TH_SHEETS] = {&bloques, &sprites};

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
    n_humanoides = 0;
    return r == TH_MAP_OK;
}

static bool cargar_anims(const char *ruta)
{
    int tam = 0;
    uint8_t *datos = asset_load(ruta, &tam);
    return datos && th_anims_init(&anims, datos, (size_t)tam) == 0;
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

// Cámara centrada en un humanoide (en su posición en píxeles, no en su casilla).
static th_view vista_de(const th_humanoid *h)
{
    th_view v = {.unit = UNIDAD, .w = ANCHO, .h = ALTO};
    th_view_tile(&v, h->tile_x, h->tile_y, &v.x, &v.y);
    v.x += h->px * UNIDAD / 32 - ANCHO / 2;
    v.y += h->py * UNIDAD / 32 + UNIDAD / 2 - ALTO / 2 - 16;
    th_view_clamp(&v);
    return v;
}

// Zona en la que los humanoides eligen destino: todo el mapa o, para medir
// con todos en pantalla, un cuadrado alrededor de una casilla.
static int zona_x0 = 0, zona_y0 = 0, zona_x1 = TH_MAP_WIDTH - 1, zona_y1 = TH_MAP_HEIGHT - 1;

// Una casilla transitable al azar de la zona; dentro del hospital si se puede.
static void casilla_al_azar(int *x, int *y)
{
    for (int intento = 0; intento < 4000; intento++) {
        *x = th_rng_range(&azar, zona_x0, zona_x1);
        *y = th_rng_range(&azar, zona_y0, zona_y1);
        uint32_t f = th_map_tile_c(&mapa, *x, *y)->flags;
        if ((f & TH_TILE_PASSABLE) && ((f & TH_TILE_HOSPITAL) || intento > 3000))
            return;
    }
}

// Un paciente («Standard Male Patient», con cabeza, ropa y zapatos al azar
// como en initPatient) en (x, y), que se pone a caminar.
static th_humanoid *nuevo_humanoide(int x, int y)
{
    if (n_humanoides == MAX_HUMANOIDES)
        return NULL;
    th_humanoid *h = &humanoides[n_humanoides++];
    th_humanoid_init(&mundo, h, th_walk_anims_find("Standard Male Patient"));
    th_humanoid_set_layer(h, 0, th_rng_range(&azar, 1, 5) * 2);
    th_humanoid_set_layer(h, 1, th_rng_range(&azar, 0, 3) * 2);
    th_humanoid_set_layer(h, 2, th_rng_range(&azar, 0, 1) * 2);
    th_humanoid_set_tile(&mundo, h, x, y);
    return h;
}

typedef struct {
    int ticks, caminos;
    uint32_t cpu_ticks, cpu_caminos; // get_ticks: los ticks de los humanoides y las búsquedas de camino
    uint32_t max_camino;             // la búsqueda más larga
} sim_t;

// Manda al humanoide a una casilla al azar de la zona (y, al llegar, quieto).
static void a_pasear(th_humanoid *h, sim_t *s)
{
    int x, y;
    casilla_al_azar(&x, &y);
    uint32_t c0 = get_ticks();
    th_humanoid_set_next_action(&mundo, h, th_action_walk(x, y));
    th_humanoid_queue_action(h, th_action_idle());
    uint32_t c = get_ticks() - c0;
    s->cpu_caminos += c;
    if (c > s->max_camino)
        s->max_camino = c;
    s->caminos++;
}

// Un tick del mundo: cada humanoide avanza y, si está quieto, sale hacia otra
// casilla del hospital.
static void tick_mundo(sim_t *s)
{
    uint32_t t0 = get_ticks(), caminos0 = s->cpu_caminos;
    for (int i = 0; i < n_humanoides; i++) {
        th_humanoid *h = &humanoides[i];
        th_humanoid_tick(&mundo, h);
        if (h->queue_len == 1 && h->queue[0].kind == TH_ACTION_IDLE)
            a_pasear(h, s);
    }
    s->ticks++;
    s->cpu_ticks += get_ticks() - t0 - (s->cpu_caminos - caminos0);
}

typedef struct {
    uint32_t lista, rdp; // ticks de CPU: generar la lista y enviar los comandos
    mapa_n64_stats_t st;
} medida_t;

// Dibuja el mapa y los humanoides en la pantalla ya enganchada a rdpq.
static void dibujar_escena(const th_view *v, medida_t *m)
{
    uint32_t t0 = get_ticks();
    for (int i = 0; i < n_humanoides; i++) {
        const th_humanoid *h = &humanoides[i];
        entidades[i] = (th_view_entity){.tile_x = h->tile_x, .tile_y = h->tile_y, .px = h->px, .py = h->py,
                                        .frame = h->frame, .anim = h->anim, .flags = h->anim_flags,
                                        .drawing_layer = h->drawing_layer, .attach_seq = h->attach_seq,
                                        .layers = h->layers};
    }
    th_scene sc = {.map = &mapa, .sheets = {&sh_bloques, &sh_sprites}, .anims = &anims, .entities = entidades,
                   .entity_count = n_humanoides};
    int n = th_view_scene(v, &sc, &lista);
    assertf(n >= 0, "la lista de dibujo no cabe");
    uint32_t t1 = get_ticks();
    mapa_n64_dibujar(&lista, hojas, paletas.colores[PALETA_NORMAL], ANCHO, ALTO, &m->st);
    m->lista = t1 - t0;
    m->rdp = get_ticks() - t1;
}

#ifdef TH_PRUEBAS
// Dibuja con el RDP la vista, la dibuja también con el rasterizador de
// referencia (en el orden de CorsixTH, sprite a sprite) y compara los dos
// píxel a píxel. En las mezclas el RDP redondea distinto: se cuentan aparte
// las que difieren en 1 (de 31) por componente.
static int comprobar_vista(const char *hito, const th_view *v)
{
    surface_t *pantalla = display_get();
    rdpq_attach(pantalla, NULL);
    rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
    rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
    medida_t m;
    dibujar_escena(v, &m);
    rdpq_detach_wait();
    data_cache_hit_invalidate(pantalla->buffer, pantalla->stride * pantalla->height);

    static uint8_t pal8[256][3];
    const uint16_t *paleta = paletas.colores[PALETA_NORMAL];
    for (int i = 0; i < 256; i++) {
        unsigned c = paleta[i], r = c >> 11 & 31, g = c >> 6 & 31, b = c >> 1 & 31;
        pal8[i][0] = (uint8_t)(r << 3 | r >> 2);
        pal8[i][1] = (uint8_t)(g << 3 | g >> 2);
        pal8[i][2] = (uint8_t)(b << 3 | b >> 2);
    }
    th_raster r = {.rgb = malloc(ANCHO * ALTO * 3), .mezcla = malloc(ANCHO * ALTO), .hoja = malloc(ANCHO * ALTO),
                   .w = ANCHO, .h = ALTO, .paleta = (const uint8_t(*)[3])pal8};
    assertf(r.rgb && r.mezcla && r.hoja, "sin memoria para la referencia");
    th_raster_clear(&r, 0, 0, 0);
    const th_sheet *const sheets[TH_SHEETS] = {&sh_bloques, &sh_sprites};
    th_raster_draw(&r, sheets, &lista);

    const uint16_t *fb = pantalla->buffer;
    int distintos = 0, mezcla_1 = 0, mezclas = 0, de_entidades = 0;
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
            mezclas += r.mezcla[i];
            de_entidades += r.hoja[i] == 1 + TH_SHEET_SPRITES;
            if (d == 0)
                continue;
            if (d == 1 && r.mezcla[i]) {
                mezcla_1++;
            } else {
                if (distintos++ < 4)
                    debugf("TH64 %s:   (%d,%d): RDP %04x, referencia %02x%02x%02x%s\n", hito, x, y, c, r.rgb[3 * i],
                           r.rgb[3 * i + 1], r.rgb[3 * i + 2], r.mezcla[i] ? " (mezcla)" : "");
            }
        }
    }
    free(r.rgb);
    free(r.mezcla);
    free(r.hoja);
    debugf("TH64 %s: vista (%d,%d): %d pixeles comparados, %d distintos; %d de %d mezclados a 1/31; "
           "%d de entidades; %d sprites, %d cargas, %d cortes, %d casillas redibujadas\n",
           hito, v->x, v->y, ANCHO * ALTO, distintos, mezcla_1, mezclas, de_entidades, m.st.sprites, m.st.cargas,
           m.st.cortes, lista.redraws);
    display_show(pantalla);
    return distintos;
}

typedef struct {
    uint64_t cpu, mapa, lista, suelos, paredes, intervalo, sprites, pixeles, cargas, entidades;
    uint32_t max_cpu, max_mapa, max_intervalo, max_sprites, max_pixeles, max_cargas;
    int fotogramas, saltos;
    uint32_t anterior;
} cuentas_t;

static void contar(cuentas_t *c, uint32_t inicio, uint32_t fin, const medida_t *m)
{
    const uint32_t periodo = TICKS_PER_SECOND / FPS;
    if (c->fotogramas > 0) {
        uint32_t intervalo = inicio - c->anterior;
        c->intervalo += intervalo;
        if (intervalo > c->max_intervalo)
            c->max_intervalo = intervalo;
        if (intervalo > periodo + periodo / 2)
            c->saltos++;
    }
    c->anterior = inicio;
    uint32_t cpu = fin - inicio, mapa_cpu = m->lista + m->rdp;
    c->cpu += cpu;
    c->mapa += mapa_cpu;
    c->lista += m->lista;
    c->suelos += m->st.ticks_suelos;
    c->paredes += m->st.ticks_paredes;
    c->sprites += m->st.sprites;
    c->pixeles += m->st.pixeles;
    c->cargas += m->st.cargas;
    c->entidades += m->st.entidades;
#define MAXIMO(campo, v)    \
    if ((v) > c->campo)     \
        c->campo = (v);
    MAXIMO(max_cpu, cpu)
    MAXIMO(max_mapa, mapa_cpu)
    MAXIMO(max_sprites, (uint32_t)m->st.sprites)
    MAXIMO(max_pixeles, (uint32_t)m->st.pixeles)
    MAXIMO(max_cargas, (uint32_t)m->st.cargas)
#undef MAXIMO
    c->fotogramas++;
}

static void informar(const char *hito, const char *nombre, const cuentas_t *c)
{
    const float ms = 1000.0f / TICKS_PER_SECOND;
    const int n = c->fotogramas;
    debugf("TH64 %s: recorrido %s de %d fotogramas a %d fps: CPU del dibujo media %.2f ms (lista %.2f, suelos "
           "%.2f, paredes y entidades %.2f), maxima %.2f ms; fotograma entero media %.2f ms, maxima %.2f ms; "
           "intervalo medio %.2f ms, maximo %.2f ms; %d saltos\n",
           hito, nombre, n, FPS, c->mapa * ms / n, c->lista * ms / n, c->suelos * ms / n, c->paredes * ms / n,
           c->max_mapa * ms, c->cpu * ms / n, c->max_cpu * ms, c->intervalo * ms / (n - 1), c->max_intervalo * ms,
           c->saltos);
    debugf("TH64 %s: por fotograma: %lu sprites (max %lu; %lu de entidades), %lu cargas de textura (max %lu), "
           "%lu pixeles de relleno (max %lu)\n",
           hito, (unsigned long)(c->sprites / n), (unsigned long)c->max_sprites, (unsigned long)(c->entidades / n),
           (unsigned long)(c->cargas / n), (unsigned long)c->max_cargas, (unsigned long)(c->pixeles / n),
           (unsigned long)c->max_pixeles);
}

// Hito 2: la cámara sola recorre a 30 fps un rectángulo de 768 × 512 alrededor
// del inicio, a 8 píxeles por fotograma. Con texto, dibuja además encima las
// mismas 5 líneas que el visor.
static void recorrido_mapa(th_view inicio, bool con_texto)
{
    static const struct { int dx, dy, n; } tramos[] = {
        {8, 0, 48}, {0, 8, 32}, {-8, 0, 96}, {0, -8, 64}, {8, 0, 96}, {0, 8, 32}, {-8, 0, 48}};
    th_view v = inicio;
    cuentas_t c = {0};
    for (unsigned t = 0; t < sizeof tramos / sizeof tramos[0]; t++) {
        for (int k = 0; k < tramos[t].n; k++) {
            surface_t *pantalla = display_get();
            uint32_t ahora = get_ticks();
            v.x += tramos[t].dx;
            v.y += tramos[t].dy;
            th_view_clamp(&v);
            rdpq_attach(pantalla, NULL);
            medida_t m;
            dibujar_escena(&v, &m);
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
            contar(&c, ahora, get_ticks(), &m);
        }
    }
    informar("hito2", con_texto ? "con texto" : "sin texto", &c);
}

// Hito 3: el mundo a la velocidad Normal (fotograma f: tick f·30/54 entero,
// sin depender del reloj) durante 'fotogramas'. Si 'seguir', la cámara va con
// el primer humanoide.
static void recorrido_mundo(const char *nombre, th_view v, bool seguir, int fotogramas, sim_t s)
{
    cuentas_t c = {0};
    for (int f = 1; f <= fotogramas; f++) {
        surface_t *pantalla = display_get();
        uint32_t ahora = get_ticks();
        while (s.ticks < f * (1000 / FPS) / MS_POR_TICK)
            tick_mundo(&s);
        if (seguir)
            v = vista_de(&humanoides[0]);
        rdpq_attach(pantalla, NULL);
        medida_t m;
        dibujar_escena(&v, &m);
        rdpq_detach_show();
        contar(&c, ahora, get_ticks(), &m);
    }
    informar("hito3", nombre, &c);
    const float ms = 1000.0f / TICKS_PER_SECOND;
    debugf("TH64 hito3: %s: %d humanoides, %d ticks del mundo: %.3f ms por tick en mover a todos "
           "(%.1f us por humanoide); %d caminos buscados, %.2f ms de media cada uno y %.2f ms el mas largo\n",
           nombre, n_humanoides, s.ticks, s.cpu_ticks * ms / s.ticks,
           s.cpu_ticks * ms * 1000 / s.ticks / (n_humanoides ? n_humanoides : 1), s.caminos,
           s.caminos ? s.cpu_caminos * ms / s.caminos : 0.0f, s.max_camino * ms);
}

// Un humanoide que camina de (x1, y1) a (x2, y2), parado en el tick 'tick',
// como en bench/scenarios/caminata.lua (capas 4, 2 y 2).
static th_humanoid *caminata(int x1, int y1, int x2, int y2, int tick)
{
    n_humanoides = 0;
    th_humanoid *h = nuevo_humanoide(x1, y1);
    th_humanoid_set_layer(h, 0, 4);
    th_humanoid_set_layer(h, 1, 2);
    th_humanoid_set_layer(h, 2, 2);
    th_humanoid_set_next_action(&mundo, h, th_action_walk(x2, y2));
    th_humanoid_queue_action(h, th_action_idle());
    for (int i = 0; i < tick; i++)
        th_humanoid_tick(&mundo, h);
    return h;
}
#endif

#ifdef TH_PRUEBAS
// Hito 0: las reglas se leen y validan en la N64 (big-endian) igual que en el
// PC; tools/motor_check.sh compara las huellas con las de th64sim reglas.
static bool comprobar_reglas(void)
{
    static const char *const rutas[] = {"rom:/reglas/definiciones.trg", "rom:/reglas/full01.trg",
                                        "rom:/textos/english.trg", "rom:/textos/spanish.trg"};
    bool ok = true;
    for (unsigned i = 0; i < sizeof rutas / sizeof rutas[0]; i++) {
        FILE *f = fopen(rutas[i], "rb");
        if (!f) {
            debugf("TH64 reglas: falta %s (tools/conv/conv.py reglas)\n", rutas[i]);
            continue;
        }
        fclose(f);
        int tam = 0;
        uint8_t *datos = asset_load(rutas[i], &tam);
        th_rules r;
        const uint32_t t0 = get_ticks();
        const bool valido = datos && th_rules_init(&r, datos, (size_t)tam) == 0;
        const uint32_t t1 = get_ticks();
        if (!valido) {
            debugf("TH64 reglas: %s no vale\n", rutas[i]);
            ok = false;
        } else {
            debugf("TH64 reglas: %s: %d B, validado en %.2f ms, huella %08lx\n", rutas[i] + 5, tam,
                   (t1 - t0) * 1000.0f / TICKS_PER_SECOND, (unsigned long)th_rules_hash(&r, r.root));
        }
        free(datos);
    }
    // Las fuentes de QDATAM, convertidas con mkfont.
    static const char *const fuentes[] = {"font100m", "font101m", "font130m", "font131m", "font132m"};
    int cargadas = 0;
    for (unsigned i = 0; i < sizeof fuentes / sizeof fuentes[0]; i++) {
        char ruta[64];
        snprintf(ruta, sizeof ruta, "rom:/fuentes/%s.font64", fuentes[i]);
        FILE *f = fopen(ruta, "rb");
        if (!f)
            continue;
        fclose(f);
        rdpq_font_t *fuente = rdpq_font_load(ruta);
        cargadas += fuente != NULL;
        rdpq_font_free(fuente);
    }
    debugf("TH64 fuentes: %d de %u cargadas\n", cargadas, (unsigned)(sizeof fuentes / sizeof fuentes[0]));
    return ok;
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
    th_rng_seed(&azar, 64);

    int nivel = 1;
    const bool assets = dfs_ok && paletas_cargar(&paletas, "rom:/paleta.tpl") &&
                        hoja_cargar(&sprites, "rom:/datam/mspr-0.tsh") &&
                        hoja_cargar(&bloques, "rom:/datam/mblk-0.tsh") &&
                        th_sheet_init(&sh_bloques, bloques.bloque, (size_t)bloques.tam) == 0 &&
                        th_sheet_init(&sh_sprites, sprites.bloque, (size_t)sprites.tam) == 0 &&
                        cargar_anims("rom:/datam/animaciones.tan") && cargar_nivel(nivel);
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    debugf("TH64 motor: heap usado %d KiB de %d KiB; DFS %s; assets %s\n", heap.used / 1024, heap.total / 1024,
           dfs_ok ? "ok" : "no encontrado", assets ? "ok" : "no encontrados (tools/conv/conv.py)");

    th_view v = {0};
    if (assets) {
        int actual = sprite_de_prueba(&sprites);
        int comparados = 0;
        int distintos = comprobar_dibujo(&sprites, &paletas, actual, &comparados);
        debugf("TH64 hito1: sprite %d de %d (%dx%d): %d pixeles comparados, %d distintos\n", actual, sprites.num,
               sprites.tabla[actual].ancho, sprites.tabla[actual].alto, comparados, distintos);
        mapa_n64_preparar(&bloques, UNIDAD);
        v = vista_en(mapa.camera_x, mapa.camera_y);
        debugf("TH64 hito3: nivel %d cargado; heap usado %d KiB; estaticos: mapa %u KiB, buscador de caminos %u "
               "KiB, %d humanoides de %u B, lista de dibujo %u KiB\n",
               nivel, heap.used / 1024, (unsigned)(sizeof(mapa) / 1024), (unsigned)(sizeof(buscador) / 1024),
               MAX_HUMANOIDES, (unsigned)sizeof(th_humanoid), (unsigned)(sizeof(elementos) / 1024));
    }
    display_set_fps_limit(FPS);

#ifdef TH_PRUEBAS
    if (assets && !comprobar_reglas())
        fallos++;
    if (assets) {
        // Hito 2. Nivel 1: cámara inicial, una sombra sobre una pared (casilla
        // 71,71), la esquina superior del mapa y el exterior del hospital.
        // Nivel 12: parcelas sin dueño (césped y adornos) con paredes divisorias.
        const struct { int nivel, x, y; } vistas[] = {{1, -1, -1}, {1, 71, 71}, {1, 0, 0}, {1, 80, 40}, {12, 36, 28}};
        int distintos = 0;
        for (unsigned i = 0; i < sizeof vistas / sizeof vistas[0]; i++) {
            if (vistas[i].nivel != nivel && !cargar_nivel(nivel = vistas[i].nivel))
                break;
            th_view c = vistas[i].x < 0 ? v : vista_en(vistas[i].x, vistas[i].y);
            distintos += comprobar_vista("hito2", &c);
        }
        debugf("TH64 hito2: comparacion con la referencia: %d pixeles distintos en total\n", distintos);
        if (distintos)
            fallos++;
        if (nivel != 1)
            cargar_nivel(nivel = 1);
        recorrido_mapa(v, false);
        recorrido_mapa(v, true);

        // Hito 3: un humanoide hacia el este, el norte, el oeste (en espejo, junto
        // a una pared norte: se redibuja) y el sur (en espejo), y quieto al llegar.
        const struct { int x1, y1, x2, y2, tick; } pasos[] = {
            {62, 75, 70, 65, 12}, {62, 75, 70, 65, 37}, {86, 70, 54, 70, 60}, {70, 65, 62, 75, 50}, {62, 75, 70, 65, 199}};
        distintos = 0;
        for (unsigned i = 0; i < sizeof pasos / sizeof pasos[0]; i++) {
            th_humanoid *h = caminata(pasos[i].x1, pasos[i].y1, pasos[i].x2, pasos[i].y2, pasos[i].tick);
            th_view c = vista_de(h);
            distintos += comprobar_vista("hito3", &c);
        }
        debugf("TH64 hito3: comparacion con la referencia: %d pixeles distintos en total\n", distintos);
        if (distintos)
            fallos++;

        // Un humanoide que la cámara sigue por todo el hospital y una multitud
        // de 50 que deambula delante de la cámara inicial (destinos a 5
        // casillas o menos). Salen ya caminando: las primeras búsquedas de
        // camino no cuentan en los fotogramas.
        n_humanoides = 0;
        sim_t s = {0};
        a_pasear(nuevo_humanoide(mapa.camera_x, mapa.camera_y), &s);
        recorrido_mundo("siguiendo a un humanoide", v, true, 450, s);
        n_humanoides = 0;
        zona_x0 = mapa.camera_x - 5;
        zona_x1 = mapa.camera_x + 5;
        zona_y0 = mapa.camera_y - 5;
        zona_y1 = mapa.camera_y + 5;
        s = (sim_t){0};
        for (int i = 0; i < 50; i++) {
            int x, y;
            casilla_al_azar(&x, &y);
            a_pasear(nuevo_humanoide(x, y), &s);
        }
        s = (sim_t){0};
        recorrido_mundo("con 50 humanoides en pantalla", v, false, 300, s);
        zona_x0 = zona_y0 = 0;
        zona_x1 = TH_MAP_WIDTH - 1;
        zona_y1 = TH_MAP_HEIGHT - 1;
    }
    debugf("TH64 pruebas: fin, %d fallos\n", fallos);
    if (emux_detect(0))
        emux_ioctl_exit();
#endif

    if (assets) {
        n_humanoides = 0;
        nuevo_humanoide(mapa.camera_x, mapa.camera_y);
    }
    bool datos = true, medir_rdp = false, seguir = true;
    float cpu_ms = 0, lista_ms = 0, sim_ms = 0, rdp_buf_ms = 0, rdp_pipe_ms = 0;
    mapa_n64_stats_t st = {0};
    sim_t s = {0};
    uint32_t reloj = get_ticks(), acumulado = 0;
    const uint32_t por_tick = TICKS_PER_SECOND / 1000 * MS_POR_TICK;
    for (;;) {
        joypad_poll();
        joypad_inputs_t in = joypad_get_inputs(JOYPAD_PORT_1);
        joypad_buttons_t pulsado = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        if (pulsado.r)
            datos = !datos;
        if (pulsado.l)
            medir_rdp = !medir_rdp;
        if (pulsado.a)
            seguir = !seguir;
        if (assets && pulsado.b) {
            for (int i = 0; i < 10; i++) {
                int x, y;
                casilla_al_azar(&x, &y);
                th_humanoid *h = nuevo_humanoide(x, y);
                if (h)
                    a_pasear(h, &s);
            }
        }
        if (assets && pulsado.z) {
            nivel = nivel % NIVELES + 1;
            if (cargar_nivel(nivel)) {
                v = vista_en(mapa.camera_x, mapa.camera_y);
                nuevo_humanoide(mapa.camera_x, mapa.camera_y);
            }
        }
        // Stick con zona muerta; la cruceta, 8 píxeles por fotograma. Mover la
        // cámara deja de seguir al humanoide.
        int sx = abs(in.stick_x) > 8 ? in.stick_x / 6 : 0;
        int sy = abs(in.stick_y) > 8 ? -in.stick_y / 6 : 0;
        sx += in.btn.d_right ? 8 : in.btn.d_left ? -8 : 0;
        sy += in.btn.d_down ? 8 : in.btn.d_up ? -8 : 0;
        if (sx || sy)
            seguir = false;
        v.x += sx;
        v.y += sy;
        th_view_clamp(&v);

        surface_t *pantalla = display_get();
        uint32_t t0 = get_ticks();
        // El mundo avanza un tick cada 54 ms de reloj (como mucho 4 por fotograma).
        acumulado += t0 - reloj;
        reloj = t0;
        uint32_t sim0 = s.cpu_ticks + s.cpu_caminos;
        for (int k = 0; assets && acumulado >= por_tick && k < 4; k++) {
            acumulado -= por_tick;
            tick_mundo(&s);
        }
        if (acumulado >= por_tick)
            acumulado = 0;
        uint32_t sim = s.cpu_ticks + s.cpu_caminos - sim0;
        if (assets && seguir && n_humanoides > 0)
            v = vista_de(&humanoides[0]);
        if (medir_rdp)
            *DP_STATUS = DP_WSTATUS_RESET_CLOCK_COUNTER | DP_WSTATUS_RESET_CMD_COUNTER | DP_WSTATUS_RESET_PIPE_COUNTER |
                         DP_WSTATUS_RESET_TMEM_COUNTER;
        rdpq_attach(pantalla, NULL);
        rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
        rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
        medida_t m = {0};
        if (assets)
            dibujar_escena(&v, &m);
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
        sim_ms += (sim * ms - sim_ms) / 30;
        st = m.st;
        if (datos) {
            rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 12, "TH64 %s  nivel %d  (%d,%d)", TH64_VERSION, nivel, v.x,
                             v.y);
            if (assets) {
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 24, "CPU %.1f ms (mundo %.1f, lista %.1f) %.1f fps",
                                 cpu_ms, sim_ms, lista_ms, display_get_fps());
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 36, "%d humanoides  %d sprites  %d cargas", n_humanoides,
                                 st.sprites, st.cargas);
                if (medir_rdp)
                    rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 48, "RDP ocupado %.2f ms (pipe %.2f ms)", rdp_buf_ms,
                                     rdp_pipe_ms);
            } else {
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 24, "Sin assets: ejecuta tools/conv/conv.py");
            }
            rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 220, "RAM %d KiB  heap %d/%d KiB", ram_kib, heap.used / 1024,
                             heap.total / 1024);
            rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 232, "A: seguir  B: +10  Z: nivel  R: datos  L: RDP");
        }
        rdpq_detach_show();
    }
}
