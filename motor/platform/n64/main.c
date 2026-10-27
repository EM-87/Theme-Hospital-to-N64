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
#include "simulacion.h"
#include "th/anim.h"
#include "th/humanoid.h"
#include "th/map.h"
#include "th/path.h"
#include "th/rng.h"
#include "objetos.h"
#include "th/rules.h"
#include "th/scenario.h"
#include "th/sim.h"
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

enum { FUENTE_DEPURACION = 1, FUENTE_DINERO = 2 };
#define COLOR_FONDO RGBA32(24, 72, 64, 255)
#define ANCHO 320
#define ALTO 240
#define UNIDAD 16 // gráficos de DATAM: casillas de 32 × 16
#define FPS 30
#define NIVELES 12
#define MAX_HUMANOIDES 64
// Velocidad Normal de CorsixTH: un tick del mundo cada 54 ms (tick_rate 3 de 18 ms).
#define MS_POR_TICK 54
// Horas que el hospital avanza al arrancar (make HORAS=N; para capturas).
#ifndef TH_HOSPITAL_HORAS
#define TH_HOSPITAL_HORAS 0
#endif

static th_map mapa;
static th_pathfinder buscador;
static th_anims anims;
static th_world mundo = {.map = &mapa, .pf = &buscador, .anims = &anims};
static th_humanoid humanoides[MAX_HUMANOIDES];
static th_view_entity entidades[TH_VISTA_SIM_MAX];
// Con la simulación repartida entre fotogramas (simulacion.h), el dibujo y el
// marcador leen la instantánea del último tick acabado, no la simulación.
static th_view_entity foto[TH_VISTA_SIM_MAX];
static bool repartida;
// Hito 4: el hospital del escenario (rom:/escenarios/hito4.trg), si lo hay.
static th_sim sim;
static bool hospital;
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

static th_view vista_en(int x, int y);
static th_view vista_de(const th_humanoid *h);

static bool existe(const char *ruta)
{
    FILE *f = fopen(ruta, "rb");
    if (f)
        fclose(f);
    return f != NULL;
}

// Lee unas reglas (TRG1) y las deja cargadas para siempre.
static bool cargar_reglas_rom(const char *ruta, th_rules *r)
{
    if (!existe(ruta))
        return false;
    int tam = 0;
    uint8_t *datos = asset_load(ruta, &tam);
    return datos && th_rules_init(r, datos, (size_t)tam) == 0;
}

// Memoria: el heap tras cada paso de la carga (ROM de medida del hito 5).
#ifdef TH_MEDIR
static bool paso(const char *nombre, bool ok)
{
    heap_stats_t h;
    sys_get_heap_stats(&h);
    debugf("TH64 memoria: %s: heap usado %d KiB de %d KiB%s\n", nombre, h.used / 1024, h.total / 1024,
           ok ? "" : " (FALLO)");
    return ok;
}
#define PASO(nombre, expr) paso(nombre, (expr))
#else
#define PASO(nombre, expr) (expr)
#endif
// El escenario que abre la ROM: el del hito 4 o, en la ROM de medida, el del
// hito 5 (el del hito 4 con 18 llegadas al mes; make ESCENARIO=hito5 para la
// ROM normal).
#ifndef TH_ESCENARIO
#ifdef TH_MEDIR
#define TH_ESCENARIO "hito5"
#else
#define TH_ESCENARIO "hito4"
#endif
#endif
static const char *const escenario_rom = "rom:/escenarios/" TH_ESCENARIO ".trg";

// Hito 4: el nivel del escenario con sus casillas y el hospital listo para
// empezar (th_sim_init), con la semilla dada.
static bool cargar_hospital(uint32_t semilla)
{
    static th_rules defs, nivel_cfg, esc;
    static bool reglas = false;
    if (!reglas) {
        if (!PASO("escenario", cargar_reglas_rom(escenario_rom, &esc)))
            return false;
        char ruta[64];
        const char *dif = th_rules_str(&esc, th_rules_get(&esc, esc.root, "dificultad"), NULL);
        snprintf(ruta, sizeof ruta, "rom:/reglas/%s%02d.trg", dif ? dif : "full",
                 (int)th_rules_int(&esc, th_rules_get(&esc, esc.root, "nivel"), 1));
        if (!PASO("definiciones", cargar_reglas_rom("rom:/reglas/definiciones.trg", &defs)) ||
            !PASO("reglas del nivel", cargar_reglas_rom(ruta, &nivel_cfg)))
            return false;
        reglas = true;
    }
    const int n = th_rules_int(&esc, th_rules_get(&esc, esc.root, "nivel"), 1);
    hospital = false;
    if (!cargar_nivel(n) || th_scenario_apply_cells(&mapa, &esc, th_rules_get(&esc, esc.root, "celdas")) < 0)
        return false;
    const int e = th_sim_init(&sim, &mapa, &buscador, &anims, &defs, &nivel_cfg, &esc, semilla);
    if (e)
        debugf("TH64 hito4: el escenario no se puede cargar (%d)\n", e);
    hospital = e == 0;
    return hospital;
}

// Cámara inicial del hospital: el mostrador de recepción.
static th_view vista_hospital(void)
{
    for (int i = 0; i < sim.n_obj; i++)
        if (sim.obj[i].type == TH_OBJ_RECEPTION_DESK)
            return vista_en(sim.obj[i].x, sim.obj[i].y - 2);
    return vista_en(mapa.camera_x, mapa.camera_y);
}

// El último paciente que ha llegado y sigue en el mapa (o -1).
static int ultimo_paciente(void)
{
    int mejor = -1, id = 0;
    for (int k = 0; k < sim.n_order; k++) {
        const th_person *p = &sim.hum[sim.order[k]];
        if (p->kind == TH_HUM_PATIENT && p->h.tile_x >= 0 && p->u.p.id > id) {
            id = p->u.p.id;
            mejor = sim.order[k];
        }
    }
    return mejor;
}

// El dinero y la fecha con FONT100M, una fuente de contador: solo tiene
// dígitos y el guion (lo demás son casillas vacías). Como en el panel de
// CorsixTH, el dinero va sin símbolo.
static void dibujar_marcador(void)
{
    const instantanea_t *f = simulacion_instantanea();
    const int32_t saldo = repartida ? f->saldo : sim.hosp.balance;
    const th_date fecha = repartida ? f->fecha : sim.date;
    rdpq_text_printf(NULL, FUENTE_DINERO, 8, ALTO - 8, "%ld", (long)saldo);
    rdpq_text_printf(NULL, FUENTE_DINERO, ANCHO - 72, ALTO - 8, "%02d-%02d-%d", fecha.day, fecha.month, fecha.year);
}

#if defined(TH_PRUEBAS) || defined(TH_MEDIR)
// Sin repartir la simulación (como en el hito 4): la pasada de temperatura
// pendiente (sim.temp_defer), hasta el instante límite (en ticks del reloj) o
// hasta acabarla. Devuelve el tiempo gastado.
static uint32_t temperatura_hasta(uint32_t limite)
{
    const uint32_t t0 = get_ticks();
    while (sim.temp_pending && (int32_t)(limite - get_ticks()) > 0)
        th_sim_temperature_step(&sim, 16);
    return get_ticks() - t0;
}
#endif

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
#ifdef TH_MEDIR
    uint32_t vista_sim; // de lista: pasar los objetos y las personas a entidades (th_vista_sim)
#endif
} medida_t;

// Dibuja el mapa y los humanoides en la pantalla ya enganchada a rdpq.
static void dibujar_escena(const th_view *v, medida_t *m)
{
    uint32_t t0 = get_ticks();
    int n = 0;
    const th_view_entity *ents = entidades;
    if (hospital && repartida) {
        const instantanea_t *f = simulacion_instantanea();
        ents = f->entidades;
        n = f->n_entidades;
#ifdef TH_MEDIR
        m->vista_sim = 0;
#endif
    } else if (hospital) {
        n = th_vista_sim(&sim, entidades, (int)(sizeof entidades / sizeof entidades[0]));
#ifdef TH_MEDIR
        m->vista_sim = get_ticks() - t0;
#endif
    } else {
        for (int i = 0; i < n_humanoides; i++) {
            const th_humanoid *h = &humanoides[i];
            entidades[n++] = (th_view_entity){.tile_x = h->tile_x, .tile_y = h->tile_y, .px = h->px, .py = h->py,
                                              .frame = h->frame, .anim = h->anim, .flags = h->anim_flags,
                                              .drawing_layer = h->drawing_layer, .attach_seq = h->attach_seq,
                                              .layers = h->layers, .crop = TH_VIEW_NO_CROP};
        }
    }
    th_scene sc = {.map = &mapa, .sheets = {&sh_bloques, &sh_sprites}, .anims = &anims, .entities = ents,
                   .entity_count = n};
    n = th_view_scene(v, &sc, &lista);
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

#endif

#if defined(TH_PRUEBAS) || defined(TH_MEDIR)
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
#endif

#ifdef TH_PRUEBAS

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

// Hito 4: el hospital a la velocidad Normal durante 'fotogramas', con la
// cámara quieta en 'v'. Mide la CPU del dibujo y de la simulación.
static void recorrido_hospital(th_view v, int fotogramas)
{
    cuentas_t c = {0};
    int ticks = 0;
    uint32_t cpu_sim = 0, max_sim = 0, cpu_temp = 0, max_temp = 0;
    // Como en el juego: la pasada de temperatura se reparte entre fotogramas.
    sim.temp_defer = 1;
    for (int f = 1; f <= fotogramas; f++) {
        surface_t *pantalla = display_get();
        uint32_t ahora = get_ticks();
        while (ticks < f * (1000 / FPS) / MS_POR_TICK) {
            const uint32_t t0 = get_ticks();
            th_sim_tick(&sim);
            const uint32_t d = get_ticks() - t0;
            cpu_sim += d;
            if (d > max_sim)
                max_sim = d;
            ticks++;
        }
        rdpq_attach(pantalla, NULL);
        rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
        rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
        medida_t m;
        dibujar_escena(&v, &m);
        dibujar_marcador();
        rdpq_detach_show();
        const uint32_t d = temperatura_hasta(ahora + TICKS_FROM_MS(26));
        cpu_temp += d;
        if (d > max_temp)
            max_temp = d;
        contar(&c, ahora, get_ticks(), &m);
    }
    th_sim_temperature_step(&sim, TH_SIM_TEMP_WORDS);
    sim.temp_defer = 0;
    informar("hito4", "del hospital en marcha", &c);
    const float ms = 1000.0f / TICKS_PER_SECOND;
    debugf("TH64 hito4: %d ticks del hospital: %.3f ms por tick de media, %.3f ms el mas largo; temperatura entre "
           "fotogramas: %.3f ms de media, %.3f ms como mucho; %d personas\n",
           ticks, cpu_sim * ms / (ticks ? ticks : 1), max_sim * ms, cpu_temp * ms / fotogramas, max_temp * ms,
           sim.n_order);
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


// Unidades de trabajo fijo (de algo menos de 1 ms) hechas en ms milisegundos,
// llamando a mixer_try_play entre una y otra si suena audio, como haría el
// bucle del juego. Con y sin audio, da la CPU que se lleva el audio (el mismo
// método que n64/pruebas/audio en la fase 4).
static volatile uint32_t sumidero;
static uint32_t unidades(int ms, bool audio)
{
    const uint32_t t0 = get_ticks();
    uint32_t n = 0;
    while (TICKS_DISTANCE(t0, get_ticks()) < ms * (int)(TICKS_PER_SECOND / 1000)) {
        for (uint32_t i = 0; i < 20000; i++)
            sumidero += i * 7;
        n++;
        if (audio)
            mixer_try_play();
    }
    return n;
}

// Hito 0, el sonido: el SoundFont recortado y una pieza de música, con un
// efecto encima, buscado en la tabla de sonidos del idioma como haría un
// fotograma de animación. Suena 3 s mientras se mide la CPU que se lleva.
static bool comprobar_sonido(void)
{
    if (!existe("rom:/musica/banco.sf64") || !existe("rom:/sonido/english.trg")) {
        debugf("TH64 sonido: faltan los assets (tools/conv/conv.py sonido)\n");
        return true;
    }
    const uint32_t base = unidades(1000, false);
    audio_init(22050, AUDIO_DEFAULT_LATENCY);
    mixer_init(32);
    heap_stats_t h0, h1;
    sys_get_heap_stats(&h0);
    sf64_bank_t *banco = sf64_load("rom:/musica/banco.sf64");
    sys_get_heap_stats(&h1);
    sf64_synth_t *synth = sf64_synth_create(banco);
    sf64_synth_set_mode(synth, SF64_MODE_GM1);
    for (int i = 0; i < 16; i++)
        mixer_ch_set_limits(i, 16, 96000, 0); // sin esto, la primera nota aborta (fase 4)
    sf64_synth_set_channels(synth, 0, 16, MIXER_PRIORITY_MUSIC);
    mid64player_t *mid = mid64player_load("rom:/musica/fortune.mid64");
    mid64player_play(mid, sf64_synth_midi_target(synth));

    // Sonido 10 de la tabla inglesa: [nombre, ruta sin extensión].
    int tam = 0;
    uint8_t *datos = asset_load("rom:/sonido/english.trg", &tam);
    th_rules tabla;
    char ruta[96] = "";
    const char *nombre = "?";
    wav64_t *efecto = NULL;
    if (th_rules_init(&tabla, datos, (size_t)tam) == 0) {
        const th_rnode e = th_rules_at(&tabla, tabla.root, 10 - 1);
        nombre = th_rules_str(&tabla, th_rules_at(&tabla, e, 0), NULL);
        const char *r = th_rules_str(&tabla, th_rules_at(&tabla, e, 1), NULL);
        if (r) {
            snprintf(ruta, sizeof ruta, "rom:/%s.wav64", r);
            efecto = wav64_load(ruta, NULL);
            wav64_set_loop(efecto, true);
            wav64_play(efecto, 16);
        }
    }
    const uint32_t con_audio = unidades(3000, true);
    const unsigned va_por = (unsigned)mid64player_tell_ms(mid);
    debugf("TH64 sonido: banco de %d presets, %d KiB en RAM; FORTUNE dura %lu ms y va por %u ms; efecto %s (%s) "
           "%s; el audio se lleva el %.1f %% de la CPU (%lu unidades por segundo con audio, %lu sin)\n",
           sf64_preset_count(banco), (h1.used - h0.used) / 1024, (unsigned long)mid64player_get_duration_ms(mid),
           va_por, nombre ? nombre : "?", ruta, efecto && mixer_ch_playing(16) ? "sonando" : "NO suena",
           100.0 * (1.0 - con_audio / 3.0 / base), (unsigned long)(con_audio / 3), (unsigned long)base);
    const bool ok = efecto && va_por > 2000;
    mixer_ch_stop(16);
    if (efecto)
        wav64_close(efecto);
    free(datos);
    mid64player_stop(mid);
    mid64player_close(mid);
    sf64_synth_close(synth);
    sf64_close(banco);
    mixer_close();
    audio_close();
    return ok;
}
#endif

#ifdef TH_MEDIR
// Hito 5 (make MEDIR=1; lo ejecuta y lo resume tools/hito5/medir.sh). Con el
// escenario del hito 5 (el hospital del hito 4 con 18 llegadas al mes):
//   1. simula tan deprisa como se pueda, con la pasada de temperatura en el
//      tick, hasta que haya 50 pacientes en el mapa; una línea por tick con
//      su carga (th_sim_load_count), las búsquedas de camino, los nodos que
//      recorren y lo que tarda: «H5 t hora personas pacientes andando
//      casillas_temp búsquedas nodos ticks»;
//   2. con la cámara donde se ven más personas, lo que cuesta cargar y
//      dibujar un sprite con cada carga de textura de mapa_n64.c («H5 c»), si
//      las tres dan el mismo fotograma y lo que cuesta la lista de dibujo con
//      y sin las entidades;
//   3. con la música sonando, 900 fotogramas a velocidad Normal (18,5 horas
//      por segundo), 300 a «Max speed» (27,8) y 300 a «And then some more»
//      (55,6), cada velocidad sin repartir y repartida (par_de_recorridos);
//      una línea por fotograma: «H5 f velocidad n cpu lista suelos paredes
//      entidades cargas mundo temperatura audio ticks_del_mundo sprites
//      de_entidades cargas intervalo vista_sim pendientes» (en ticks del
//      reloj, salvo las cuentas);
//   4. sigue tan deprisa como se pueda hasta la hora 9.000 y escribe la
//      huella, que tiene que ser la de th64sim carga.
static mid64player_t *musica;

static void musica_empezar(void)
{
    audio_init(22050, AUDIO_DEFAULT_LATENCY);
    mixer_init(32);
    sf64_bank_t *banco = sf64_load("rom:/musica/banco.sf64");
    sf64_synth_t *synth = sf64_synth_create(banco);
    sf64_synth_set_mode(synth, SF64_MODE_GM1);
    for (int i = 0; i < 16; i++)
        mixer_ch_set_limits(i, 16, 96000, 0);
    sf64_synth_set_channels(synth, 0, 16, MIXER_PRIORITY_MUSIC);
    musica = mid64player_load("rom:/musica/fortune.mid64");
    mid64player_set_loop(musica, true);
    mid64player_play(musica, sf64_synth_midi_target(synth));
}

static int tick_medido(void)
{
    th_sim_load l;
    th_sim_load_count(&sim, &l);
    const uint32_t b0 = buscador.total_searches, n0 = buscador.total_expanded, c0 = sim.temp_cells_total;
    const uint32_t t0 = get_ticks();
    th_sim_tick(&sim);
    const uint32_t d = get_ticks() - t0;
    debugf("H5 t %lu %u %u %u %lu %lu %lu %lu\n", (unsigned long)sim.hours, l.people, l.patients, l.walking,
           (unsigned long)(sim.temp_cells_total - c0), (unsigned long)(buscador.total_searches - b0), (unsigned long)(buscador.total_expanded - n0),
           (unsigned long)d);
    return l.patients;
}

// La cámara (centrada en una persona del mapa) que deja ver a más personas.
static th_view vista_con_mas_gente(int *cuantas)
{
    th_view mejor = vista_hospital();
    *cuantas = -1;
    for (int k = 0; k < sim.n_order; k++) {
        const th_humanoid *c = &sim.hum[sim.order[k]].h;
        if (c->tile_x < 0)
            continue;
        const th_view v = vista_en(c->tile_x, c->tile_y);
        int n = 0;
        for (int j = 0; j < sim.n_order; j++) {
            const th_humanoid *h = &sim.hum[sim.order[j]].h;
            int sx, sy;
            if (h->tile_x < 0)
                continue;
            th_view_tile(&v, h->tile_x, h->tile_y, &sx, &sy);
            n += sx >= -UNIDAD && sx < ANCHO + UNIDAD && sy >= 0 && sy < ALTO + 3 * UNIDAD;
        }
        if (n > *cuantas) {
            *cuantas = n;
            mejor = v;
        }
    }
    return mejor;
}

// 'fotogramas' a 'horas_por_s' (en décimas) con la cámara quieta en v. Sin
// repartir, como en el hito 4: los ticks que tocan antes de dibujar y la
// temperatura entre fotogramas. Repartida (simulacion.h): primero se dibuja
// la instantánea y después la simulación sigue hasta 28 ms después de
// empezar el fotograma; lo que no le dé tiempo, en el siguiente.
static void recorrido_medido(const char *nombre, th_view v, int fotogramas, int horas_por_s_x10, bool repartir)
{
    cuentas_t c = {0};
    int ticks = 0, pedidos = 0, max_atraso = 0, con_atraso = 0;
    uint32_t cpu_sim = 0, max_sim = 0, cpu_temp = 0, cpu_audio = 0;
    sim.temp_defer = 1;
    if (repartir) {
        simulacion_iniciar(&sim, foto, (int)(sizeof foto / sizeof foto[0]));
        repartida = true;
    }
    const simulacion_stats_t *ss = simulacion_stats();
    for (int f = 1; f <= fotogramas; f++) {
        surface_t *pantalla = display_get();
        const uint32_t ahora = get_ticks();
        const uint32_t a0 = get_ticks();
        mixer_try_play();
        const uint32_t audio = get_ticks() - a0;
        // Los ticks que tocan hasta este fotograma.
        const int objetivo = (int)((int64_t)f * horas_por_s_x10 / (10 * FPS));
        int hechos = 0;
        uint32_t t_sim = 0, t_temp = 0;
        if (!repartir) {
            while (ticks < objetivo) {
                const uint32_t t0 = get_ticks();
                th_sim_tick(&sim);
                const uint32_t d = get_ticks() - t0;
                t_sim += d;
                if (d > max_sim)
                    max_sim = d;
                ticks++;
                hechos++;
            }
        } else {
            simulacion_pedir(objetivo - pedidos);
            pedidos = objetivo;
        }
        rdpq_attach(pantalla, NULL);
        rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
        rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
        medida_t m;
        dibujar_escena(&v, &m);
        dibujar_marcador();
        rdpq_detach_show();
        if (!repartir) {
            t_temp = temperatura_hasta(ahora + TICKS_FROM_MS(26));
        } else {
            const uint32_t cpu0 = ss->cpu, temp0 = ss->cpu_temperatura, ticks0 = ss->ticks;
            simulacion_avanzar(ahora + TICKS_FROM_MS(28));
            t_temp = ss->cpu_temperatura - temp0;
            t_sim = ss->cpu - cpu0 - t_temp;
            hechos = (int)(ss->ticks - ticks0);
            ticks += hechos;
            const int atraso = simulacion_pendientes();
            con_atraso += atraso > 1;
            if (atraso > max_atraso)
                max_atraso = atraso;
        }
        cpu_sim += t_sim;
        cpu_temp += t_temp;
        cpu_audio += audio;
        const uint32_t fin = get_ticks();
        debugf("H5 f %s %d %lu %lu %lu %lu %lu %lu %lu %lu %lu %d %d %d %d %lu %lu %d\n", nombre, f,
               (unsigned long)(fin - ahora), (unsigned long)m.lista, (unsigned long)m.st.ticks_suelos,
               (unsigned long)m.st.ticks_paredes, (unsigned long)m.st.ticks_entidades,
               (unsigned long)m.st.ticks_cargas, (unsigned long)t_sim, (unsigned long)t_temp, (unsigned long)audio,
               hechos, m.st.sprites, m.st.entidades, m.st.cargas,
               (unsigned long)(c.fotogramas ? ahora - c.anterior : 0), (unsigned long)m.vista_sim,
               repartir ? simulacion_pendientes() : 0);
        contar(&c, ahora, fin, &m);
    }
    if (repartir) {
        simulacion_terminar();
        max_sim = ss->tick_mas_largo;
        repartida = false;
    } else {
        th_sim_temperature_step(&sim, TH_SIM_TEMP_WORDS);
    }
    sim.temp_defer = 0;
    informar("hito5", nombre, &c);
    const float ms = 1000.0f / TICKS_PER_SECOND;
    debugf("TH64 hito5: %s: %d ticks del mundo: %.3f ms de media, %.3f ms el mas largo; temperatura %.3f ms por "
           "fotograma; audio %.3f ms por fotograma; %d personas",
           nombre, ticks, cpu_sim * ms / (ticks ? ticks : 1), max_sim * ms, cpu_temp * ms / fotogramas,
           cpu_audio * ms / fotogramas, sim.n_order);
    if (repartir)
        debugf("; atraso de mas de 1 tick en %d fotogramas, como mucho %d; %lu pausas; el tramo sin pausas mas "
               "largo, %.3f ms (etapa %d, hora %lu)",
               con_atraso, max_atraso, (unsigned long)ss->pausas, ss->tramo_mas_largo * ms, ss->etapa_tramo,
               (unsigned long)ss->hora_tramo);
    debugf("\n");
}

// Lo que cuesta la lista de dibujo (th_view_scene) con y sin las entidades y
// lo que cuesta solo recorrer los elementos de sus fotogramas.
static void medir_lista(th_view v)
{
    enum { VECES = 20 };
    const int n = th_vista_sim(&sim, entidades, (int)(sizeof entidades / sizeof entidades[0]));
    th_scene sc = {.map = &mapa, .sheets = {&sh_bloques, &sh_sprites}, .anims = &anims, .entities = entidades,
                   .entity_count = n};
    uint32_t t[2], huella = 0;
    int sprites[2], de_entidades = 0;
    for (int k = 0; k < 2; k++) {
        sc.entity_count = k ? 0 : n;
        const uint32_t t0 = get_ticks();
        for (int r = 0; r < VECES; r++)
            th_view_scene(&v, &sc, &lista);
        t[k] = (get_ticks() - t0) / VECES;
        sprites[k] = lista.count;
        if (!k) {
            for (int i = 0; i < lista.count; i++)
                de_entidades += lista.items[i].sheet == TH_SHEET_SPRITES;
            huella = th_drawlist_hash(&lista);
        }
    }
    // Los elementos de los fotogramas de todas las entidades, leídos como add_entity.
    uint32_t elementos = 0, suma = 0;
    const uint32_t t0 = get_ticks();
    for (int r = 0; r < VECES; r++)
        for (int i = 0; i < n; i++)
            for (uint32_t j = th_anims_frame_list(&anims, entidades[i].frame);; j++) {
                const uint16_t e = th_anims_list_entry(&anims, j);
                if (e == TH_ANIM_LIST_END || e >= anims.elements)
                    break;
                th_anim_element el;
                th_anims_element(&anims, e, &el);
                suma += el.sprite + el.layer_id;
                elementos++;
            }
    const uint32_t t_elem = get_ticks() - t0;
    debugf("TH64 hito5: lista de dibujo: %lu us con %d entidades (%d sprites, %d de entidades, huella %08lx), %lu us "
           "sin ellas (%d sprites); sus fotogramas tienen %lu elementos, %lu ciclos cada uno en leerlos (%lu)\n",
           (unsigned long)TICKS_TO_US(t[0]), n, sprites[0], de_entidades, (unsigned long)huella,
           (unsigned long)TICKS_TO_US(t[1]), sprites[1], (unsigned long)(elementos / VECES),
           (unsigned long)(t_elem * 2 / (elementos ? elementos : 1)), (unsigned long)suma);
}

// Una velocidad dos veces, sin repartir y repartida, con las mismas horas:
// se guarda la simulación antes y se vuelve a ella (el núcleo no tiene más
// estado que th_sim, y los ticks no tocan el mapa). Con 'cargas', además, la
// repartida con las otras dos cargas de textura (mapa_n64.h).
static void par_de_recorridos(const char *nombre, th_view v, int fotogramas, int horas_por_s_x10, bool cargas)
{
    static th_sim *copia;
    if (!copia)
        copia = malloc(sizeof sim);
    assertf(copia, "sin memoria para la copia de la simulación");
    memcpy(copia, &sim, sizeof sim);
    recorrido_medido(nombre, v, fotogramas, horas_por_s_x10, false);
    char rep[32];
    for (int modo = cargas ? MAPA_N64_CARGA_EXEC : MAPA_N64_CARGA_DIRECTA; modo >= MAPA_N64_CARGA_DIRECTA; modo--) {
        static const char *const sufijo[] = {"rep", "rdpq_tex", "exec"};
        snprintf(rep, sizeof rep, "%s_%s", nombre, sufijo[modo]);
        memcpy(&sim, copia, sizeof sim);
        mapa_n64_modo_carga = modo;
        recorrido_medido(rep, v, fotogramas, horas_por_s_x10, true);
    }
    mapa_n64_modo_carga = MAPA_N64_CARGA_DIRECTA;
}

// Píxeles distintos entre el fotograma con la carga directa y con las otras dos.
static void comparar_cargas(th_view v)
{
    uint16_t *ref = malloc(ANCHO * ALTO * sizeof(uint16_t));
    assertf(ref, "sin memoria para comparar las cargas");
    for (int modo = MAPA_N64_CARGA_DIRECTA; modo <= MAPA_N64_CARGA_EXEC; modo++) {
        mapa_n64_modo_carga = modo;
        surface_t *pantalla = display_get();
        rdpq_attach(pantalla, NULL);
        rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
        rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
        medida_t m;
        dibujar_escena(&v, &m);
        rdpq_detach_wait();
        const uint16_t *px = pantalla->buffer;
        int distintos = 0;
        for (int y = 0; y < ALTO; y++)
            for (int x = 0; x < ANCHO; x++) {
                const uint16_t c = px[y * (pantalla->stride / 2) + x];
                if (modo == MAPA_N64_CARGA_DIRECTA)
                    ref[y * ANCHO + x] = c;
                else
                    distintos += c != ref[y * ANCHO + x];
            }
        display_show(pantalla);
        if (modo != MAPA_N64_CARGA_DIRECTA)
            debugf("TH64 hito5: carga %s frente a la directa: %d pixeles distintos de %d (%d cargas)\n",
                   modo == MAPA_N64_CARGA_RDPQ_TEX ? "rdpq_tex_upload" : "exec", distintos, ANCHO * ALTO, m.st.cargas);
    }
    free(ref);
    mapa_n64_modo_carga = MAPA_N64_CARGA_DIRECTA;
}

static void medir_hito5(void)
{
    const int pacientes_objetivo = 50, horas_total = 9000;
    debugf("TH64 hito5: simulacion %u KiB (%d humanoides de %u B), mapa %u KiB, buscador %u KiB, lista de dibujo "
           "%u KiB\n",
           (unsigned)(sizeof sim / 1024), TH_SIM_HUMANOIDS, (unsigned)sizeof(th_person),
           (unsigned)(sizeof mapa / 1024), (unsigned)(sizeof buscador / 1024), (unsigned)(sizeof elementos / 1024));
    int pacientes = 0;
    while (pacientes < pacientes_objetivo && (int)sim.hours < horas_total)
        pacientes = tick_medido();
    int cuantas;
    const th_view v = vista_con_mas_gente(&cuantas);
    debugf("TH64 hito5: hora %lu: %d pacientes en el mapa, %d personas; la camara (%d,%d) ve a %d\n",
           (unsigned long)sim.hours, pacientes, sim.n_order, v.x, v.y, cuantas);
    mapa_n64_medir_cargas(&sprites, paletas.colores[PALETA_NORMAL]);
    comparar_cargas(v);
    medir_lista(v);
    musica_empezar();
    PASO("con la música sonando", true);
    // Cada velocidad sin repartir y repartida (simulacion.h), con las mismas
    // horas; la Normal, también con las otras dos cargas de textura.
    par_de_recorridos("normal", v, 900, 185, true);
    par_de_recorridos("max", v, 300, 278, false);
    par_de_recorridos("mas", v, 300, 556, false);
    mixer_close(); // la música para: el resto no la necesita
    while ((int)sim.hours < horas_total)
        tick_medido();
    debugf("TH64 hito5: %lu horas; %u pacientes; huella %08lx\n", (unsigned long)sim.hours,
           (unsigned)sim.patients_spawned, (unsigned long)th_sim_hash(&sim));
    debugf("TH64 hito5: fin\n");
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

    relleno_cache();
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    joypad_init();
    asset_init_compression(2);
    const bool dfs_ok = dfs_init(DFS_DEFAULT_LOCATION) == DFS_ESUCCESS;
    rdpq_text_register_font(FUENTE_DEPURACION, rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));
    // El contador de dinero, con FONT100M (o la de depuración si no está).
    rdpq_text_register_font(FUENTE_DINERO, dfs_ok && existe("rom:/fuentes/font100m.font64")
                                               ? rdpq_font_load("rom:/fuentes/font100m.font64")
                                               : rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));
    th_rng_seed(&azar, 64);

    int nivel = 1;
#ifdef TH_MEDIR
    paso("arranque (vídeo, RDP, mandos, fuentes)", true);
#endif
    const bool assets = dfs_ok && PASO("paleta", paletas_cargar(&paletas, "rom:/paleta.tpl")) &&
                        PASO("MSPR-0", hoja_cargar(&sprites, "rom:/datam/mspr-0.tsh")) &&
                        PASO("MBLK-0", hoja_cargar(&bloques, "rom:/datam/mblk-0.tsh")) &&
                        th_sheet_init(&sh_bloques, bloques.bloque, (size_t)bloques.tam) == 0 &&
                        th_sheet_init(&sh_sprites, sprites.bloque, (size_t)sprites.tam) == 0 &&
                        PASO("animaciones", cargar_anims("rom:/datam/animaciones.tan")) &&
                        PASO("nivel", cargar_nivel(nivel));
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
    if (assets && !comprobar_sonido())
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
    if (assets && cargar_hospital(1)) {
        // Hito 4: 3.000 horas del escenario tan deprisa como se pueda (la
        // huella tiene que ser la de th64sim hito4 con la semilla 1), la vista
        // del hospital contra la referencia y un recorrido con el hospital en marcha.
        const int horas = 3000;
        uint32_t total = 0, maximo = 0;
        int hora_max = 0, n_max = 0;
        // Para separar la temperatura del resto: casillas que recalculará
        // (las que cambiaron y sus vecinas) y regresión lineal del tiempo.
        double sn = 0, st = 0, snn = 0, snt = 0;
        for (int i = 0; i < horas; i++) {
            int n = 0;
            for (int w = 0; w < TH_MAP_WIDTH * TH_MAP_HEIGHT / 32; w++) {
                const uint32_t *c = sim.temp_changed;
                uint32_t m = c[w] | c[w] << 1 | c[w] >> 1;
                if (w > 0)
                    m |= c[w - 1] >> 31;
                if (w + 1 < TH_MAP_WIDTH * TH_MAP_HEIGHT / 32)
                    m |= c[w + 1] << 31;
                if (w >= 4)
                    m |= c[w - 4];
                if (w + 4 < TH_MAP_WIDTH * TH_MAP_HEIGHT / 32)
                    m |= c[w + 4];
                n += __builtin_popcount(m);
            }
            const uint32_t t0 = get_ticks();
            th_sim_tick(&sim);
            const uint32_t d = get_ticks() - t0;
            total += d;
            if (d > maximo) {
                maximo = d;
                hora_max = i + 1;
                n_max = n;
            }
            const double us = d * 1e6 / TICKS_PER_SECOND;
            sn += n;
            st += us;
            snn += (double)n * n;
            snt += n * us;
        }
        const float ms = 1000.0f / TICKS_PER_SECOND;
        debugf("TH64 hito4: %d horas: %u pacientes, saldo %ld, reputacion %ld, huella %08lx; %.3f ms por tick de "
               "media, %.3f ms el mas largo; simulacion %u KiB\n",
               horas, (unsigned)sim.patients_spawned, (long)sim.hosp.balance, (long)sim.hosp.reputation,
               (unsigned long)th_sim_hash(&sim), total * ms / horas, maximo * ms, (unsigned)(sizeof sim / 1024));
        const double pend = (horas * snt - sn * st) / (horas * snn - sn * sn);
        debugf("TH64 hito4: temperatura: %.0f casillas por tick de media; %.2f us por casilla, %.0f us el resto del "
               "tick; el mas largo, la hora %d (%d casillas)\n",
               sn / horas, pend, (st - pend * sn) / horas, hora_max, n_max);
        th_view c = vista_hospital();
        const int distintos = comprobar_vista("hito4", &c);
        debugf("TH64 hito4: comparacion con la referencia: %d pixeles distintos\n", distintos);
        if (distintos)
            fallos++;
        recorrido_hospital(c, 450);
    }
    debugf("TH64 pruebas: fin, %d fallos\n", fallos);
    if (emux_detect(0))
        emux_ioctl_exit();
#endif

#ifdef TH_MEDIR
    // La semilla 5: con las necesidades de los pacientes, la 1 ya no junta 50
    // pacientes antes de la hora 9.000 (tools/hito5/medir.sh usa la misma).
    if (assets && cargar_hospital(5)) {
        PASO("hospital cargado", true);
        medir_hito5();
    }
    if (emux_detect(0))
        emux_ioctl_exit();
#endif

    // Con el escenario del hito 4, el hospital; si no, el visor del hito 3.
    if (assets && cargar_hospital(1)) {
        for (int i = 0; i < TH_HOSPITAL_HORAS; i++)
            th_sim_tick(&sim);
        // La simulación, repartida entre fotogramas (simulacion.h).
        simulacion_iniciar(&sim, foto, (int)(sizeof foto / sizeof foto[0]));
        repartida = true;
        v = vista_hospital();
        int plantilla = 0;
        for (int k = 0; k < sim.n_order; k++)
            plantilla += sim.hum[sim.order[k]].kind != TH_HUM_PATIENT;
        heap_stats_t hs;
        sys_get_heap_stats(&hs);
        debugf("TH64 hito4: hospital del escenario cargado: %d objetos, %d salas, %d en plantilla; %d horas "
               "adelantado, %d pacientes en el mapa; heap usado %d KiB; simulacion %u KiB\n",
               sim.n_obj, sim.n_rooms, plantilla, TH_HOSPITAL_HORAS, sim.n_order - plantilla, hs.used / 1024,
               (unsigned)(sizeof sim / 1024));
    } else if (assets) {
        cargar_nivel(nivel);
        n_humanoides = 0;
        nuevo_humanoide(mapa.camera_x, mapa.camera_y);
    }
    int velocidad = 1;
    bool datos = true, medir_rdp = false, seguir = !hospital;
    float cpu_ms = 0, lista_ms = 0, sim_ms = 0, rdp_buf_ms = 0, rdp_pipe_ms = 0;
    mapa_n64_stats_t st = {0};
    sim_t s = {0};
    uint32_t reloj = get_ticks(), acumulado = 0, t_rep = 0;
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
        if (assets && pulsado.b && !hospital) {
            for (int i = 0; i < 10; i++) {
                int x, y;
                casilla_al_azar(&x, &y);
                th_humanoid *h = nuevo_humanoide(x, y);
                if (h)
                    a_pasear(h, &s);
            }
        }
        if (hospital && pulsado.z)
            velocidad = velocidad == 1 ? 8 : 1;
        if (assets && !hospital && pulsado.z) {
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
        // El mundo avanza un tick cada 54 ms de reloj (como mucho 4 por
        // fotograma). El hospital pide los ticks a la simulación repartida, que
        // los hace después de dibujar; si va atrasada más de dos fotogramas de
        // ticks, no se piden más: el juego va más despacio, sin saltar fotogramas.
        acumulado += t0 - reloj;
        reloj = t0;
        uint32_t sim0 = s.cpu_ticks + s.cpu_caminos;
        for (int k = 0; assets && acumulado * velocidad >= por_tick && k < 4 * velocidad; k++) {
            acumulado -= por_tick / velocidad;
            if (hospital) {
                if (simulacion_pendientes() <= 2 * velocidad)
                    simulacion_pedir(1);
            } else {
                tick_mundo(&s);
            }
        }
        if (acumulado >= por_tick)
            acumulado = 0;
        uint32_t sim_cpu = s.cpu_ticks + s.cpu_caminos - sim0 + t_rep;
        if (hospital && seguir) {
            const int p = ultimo_paciente();
            if (p >= 0)
                v = vista_de(&sim.hum[p].h);
        } else if (assets && seguir && n_humanoides > 0) {
            v = vista_de(&humanoides[0]);
        }
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
        sim_ms += (sim_cpu * ms - sim_ms) / 30;
        st = m.st;
        if (datos) {
            rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 12, "TH64 %s  nivel %d  (%d,%d)", TH64_VERSION, nivel, v.x,
                             v.y);
            if (assets) {
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 24, "CPU %.1f ms (mundo %.1f, lista %.1f) %.1f fps",
                                 cpu_ms, sim_ms, lista_ms, display_get_fps());
                if (hospital)
                    rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 36, "%d personas  rep %ld  x%d  %d sprites",
                                     sim.n_order, (long)sim.hosp.reputation, velocidad, st.sprites);
                else
                    rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 36, "%d humanoides  %d sprites  %d cargas",
                                     n_humanoides, st.sprites, st.cargas);
                if (medir_rdp)
                    rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 48, "RDP ocupado %.2f ms (pipe %.2f ms)", rdp_buf_ms,
                                     rdp_pipe_ms);
            } else {
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 24, "Sin assets: ejecuta tools/conv/conv.py");
            }
            if (hospital) {
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 210, "A: seguir al ultimo paciente  Z: x1/x8  R: datos");
            } else {
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 220, "RAM %d KiB  heap %d/%d KiB", ram_kib,
                                 heap.used / 1024, heap.total / 1024);
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 232, "A: seguir  B: +10  Z: nivel  R: datos  L: RDP");
            }
        }
        if (hospital)
            dibujar_marcador();
        rdpq_detach_show();
        if (hospital) {
            const uint32_t cpu0 = simulacion_stats()->cpu;
            simulacion_avanzar(t0 + TICKS_FROM_MS(28));
            t_rep = simulacion_stats()->cpu - cpu0;
        }
    }
}
