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
#include <string.h>

#include "graficos.h"
#include "guion.h"
#include "interfaz.h"
#include "mapa_n64.h"
#include "partida.h"
#include "raster.h"
#include "simulacion.h"
#include "vigilante.h"
#include "th/anim.h"
#include "th/build.h"
#include "th/humanoid.h"
#include "th/map.h"
#include "th/path.h"
#include "th/rng.h"
#include "objetos.h"
#include "th/rules.h"
#include "th/scenario.h"
#include "th/sim.h"
#include "th/version.h"
#include "ventanas.h"
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

// Las cuatro vistas (hito 6.2): la rotación de la cámara (girar.h) y el mapa
// girado, que solo ocupa memoria (96 KiB) mientras la vista está girada.
static int giro;
static th_rot_map *mapa_girado;
// Lo que la partida añade a la escena (las ventanas, ventanas.h; o la casilla
// bajo el puntero con los datos a la vista): las superposiciones por casilla
// (th_scene.overlays), las entidades que van con las demás (el plano: paredes,
// puerta y ventanas, en casillas de la vista) y las que van encima de todo (el
// objeto o la persona en la mano, th_view_entity_on_top). Solo mientras se
// juega; las pruebas dibujan sin nada de esto.
static const th_view_overlays *extra_superposiciones;
static const th_view_entity *extra_entidades, *extra_encima;
static int n_extra_entidades, n_extra_encima;
// La instantánea es de la simulación (const): para añadirle las entidades del
// plano se copia aquí.
static th_view_entity con_extra[TH_VISTA_SIM_MAX + TH_GAME_UI_ENTITIES];

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

// Lee unas reglas (TRG1). Si datos no es NULL, deja ahí el bloque leído
// (para liberarlo al cambiar de reglas); si no, quedan cargadas para siempre.
static bool cargar_reglas_rom_en(const char *ruta, th_rules *r, void **datos)
{
    if (!existe(ruta))
        return false;
    int tam = 0;
    uint8_t *d = asset_load(ruta, &tam);
    if (datos)
        *datos = d;
    return d && th_rules_init(r, d, (size_t)tam) == 0;
}

static bool cargar_reglas_rom(const char *ruta, th_rules *r)
{
    return cargar_reglas_rom_en(ruta, r, NULL);
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
// El escenario de las pruebas y de la ROM de medida: el del hito 4 o, en la
// ROM de medida, el del hito 5 (el del hito 4 con 18 llegadas al mes).
#ifndef TH_ESCENARIO
#ifdef TH_MEDIR
#define TH_ESCENARIO "hito5"
#else
#define TH_ESCENARIO "hito4"
#endif
#endif

// Hito 4: el nivel del escenario rom:/escenarios/<nombre>.trg con sus
// casillas y el hospital listo para empezar (th_sim_init), con la semilla
// dada. Las reglas del escenario y del nivel se cambian solo si cambian.
static bool cargar_hospital_de(const char *nombre, uint32_t semilla)
{
    static th_rules defs, nivel_cfg, esc;
    static bool hay_defs;
    static void *datos_esc, *datos_nivel;
    static char esc_cargado[32], nivel_cargado[64];
    char ruta[64];
    hospital = false;
    if (strcmp(esc_cargado, nombre) != 0) {
        free(datos_esc);
        datos_esc = NULL;
        esc_cargado[0] = 0;
        snprintf(ruta, sizeof ruta, "rom:/escenarios/%s.trg", nombre);
        if (!PASO("escenario", cargar_reglas_rom_en(ruta, &esc, &datos_esc)))
            return false;
        snprintf(esc_cargado, sizeof esc_cargado, "%s", nombre);
    }
    if (!hay_defs && !PASO("definiciones", cargar_reglas_rom("rom:/reglas/definiciones.trg", &defs)))
        return false;
    hay_defs = true;
    const char *dif = th_rules_str(&esc, th_rules_get(&esc, esc.root, "dificultad"), NULL);
    const int n = th_rules_int(&esc, th_rules_get(&esc, esc.root, "nivel"), 1);
    snprintf(ruta, sizeof ruta, "rom:/reglas/%s%02d.trg", dif ? dif : "full", n);
    if (strcmp(nivel_cargado, ruta) != 0) {
        free(datos_nivel);
        datos_nivel = NULL;
        nivel_cargado[0] = 0;
        if (!PASO("reglas del nivel", cargar_reglas_rom_en(ruta, &nivel_cfg, &datos_nivel)))
            return false;
        snprintf(nivel_cargado, sizeof nivel_cargado, "%s", ruta);
    }
    if (!cargar_nivel(n) || th_scenario_apply_cells(&mapa, &esc, th_rules_get(&esc, esc.root, "celdas")) < 0)
        return false;
    const int e = th_sim_init(&sim, &mapa, &buscador, &anims, &defs, &nivel_cfg, &esc, semilla);
    if (e)
        debugf("TH64 hito4: el escenario %s no se puede cargar (%d)\n", nombre, e);
    hospital = e == 0;
    return hospital;
}

#if defined(TH_PRUEBAS) || defined(TH_MEDIR)
static bool cargar_hospital(uint32_t semilla)
{
    return cargar_hospital_de(TH_ESCENARIO, semilla);
}
#endif

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

#if defined(TH_PRUEBAS) || defined(TH_MEDIR)
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

// Cámara con la casilla (x, y) del mundo en el centro de la pantalla.
static th_view vista_en(int x, int y)
{
    th_view v = {.unit = UNIDAD, .w = ANCHO, .h = ALTO};
    th_rot_tile(giro, x, y, &x, &y);
    th_view_tile(&v, x, y, &v.x, &v.y);
    v.x -= ANCHO / 2;
    v.y += UNIDAD / 2 - ALTO / 2;
    th_view_clamp(&v);
    return v;
}

// La entidad de un humanoide sin simulación, con la cámara girada.
static th_view_entity entidad_de(const th_humanoid *h)
{
    th_view_entity e = {.tile_x = h->tile_x, .tile_y = h->tile_y, .px = h->px, .py = h->py, .frame = h->frame,
                        .anim = h->anim, .flags = h->anim_flags, .drawing_layer = h->drawing_layer,
                        .attach_seq = h->attach_seq, .layers = h->layers, .crop = TH_VIEW_NO_CROP};
    if (giro)
        th_vista_humanoide_rot(h, &anims, giro, &e);
    return e;
}

// Cámara centrada en un humanoide (en su posición en píxeles, no en su casilla).
static th_view vista_de(const th_humanoid *h)
{
    const th_view_entity e = entidad_de(h);
    th_view v = {.unit = UNIDAD, .w = ANCHO, .h = ALTO};
    th_view_tile(&v, e.tile_x, e.tile_y, &v.x, &v.y);
    v.x += e.px * UNIDAD / 32 - ANCHO / 2;
    v.y += e.py * UNIDAD / 32 + UNIDAD / 2 - ALTO / 2 - 16;
    th_view_clamp(&v);
    return v;
}

// Gira la cámara a la rotación nueva sin mover lo que hay en el centro de la
// pantalla, con el mapa girado al día. Devuelve false si no hay memoria para
// el mapa girado (entonces no gira).
static bool girar_camara(th_view *v, int nuevo)
{
    nuevo &= 3;
    if (nuevo && !mapa_girado) {
        mapa_girado = malloc(sizeof *mapa_girado);
        if (!mapa_girado)
            return false;
        mapa_girado->map = NULL;
    }
    if (nuevo)
        th_rot_map_update(mapa_girado, &mapa, nuevo);
    th_view_rotate(v, giro, nuevo);
    th_view_clamp(v);
    giro = nuevo;
    if (!giro) {
        free(mapa_girado);
        mapa_girado = NULL;
    }
    return true;
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
        n = th_vista_sim_rot(&sim, giro, entidades, (int)(sizeof entidades / sizeof entidades[0]));
#ifdef TH_MEDIR
        m->vista_sim = get_ticks() - t0;
#endif
    } else {
        for (int i = 0; i < n_humanoides; i++)
            entidades[n++] = entidad_de(&humanoides[i]);
    }
    // Con la cámara girada, el mapa girado (al día: el mapa puede cambiar).
    const th_rot_map *giradas = NULL;
    if (giro && mapa_girado) {
        th_rot_map_update(mapa_girado, &mapa, giro);
        giradas = mapa_girado;
    }
    if (n_extra_entidades > 0) {
        const int cabe = (int)(sizeof con_extra / sizeof con_extra[0]) - n;
        int k = n_extra_entidades < cabe ? n_extra_entidades : cabe;
        if (k > TH_GAME_UI_ENTITIES) // el contrato (ventanas.h); ventanas_fotograma ya lo comprueba
            k = TH_GAME_UI_ENTITIES;
        memcpy(con_extra, ents, (size_t)n * sizeof *ents);
        memcpy(con_extra + n, extra_entidades, (size_t)k * sizeof *ents);
        ents = con_extra;
        n += k;
    }
    th_scene sc = {.map = &mapa, .rot = giradas, .sheets = {&sh_bloques, &sh_sprites}, .anims = &anims,
                   .entities = ents, .entity_count = n, .overlays = extra_superposiciones};
    n = th_view_scene(v, &sc, &lista);
    assertf(n >= 0, "la lista de dibujo no cabe");
    // Lo que sigue al puntero, encima de todo (UIPlaceObjects:draw y
    // UIPlaceStaff:draw dibujan fuera del mapa, place_objects.lua:596-612 y
    // place_staff.lua:127-140).
    for (int i = 0; i < n_extra_encima && i < TH_GAME_UI_ENTITIES; i++)
        if (th_view_entity_on_top(v, &sc, &extra_encima[i], &lista) != 0)
            break;
    uint32_t t1 = get_ticks();
    // Con las tres paletas: los fantasmas en gris (TH_ANIM_PAL_GREY) y el plano
    // que no vale en rojo (TH_ANIM_PAL_RED). Sin ellos, igual que con la normal.
    const uint16_t *const pals[PALETAS_MAX] = {paletas.colores[PALETA_NORMAL], paletas.colores[PALETA_FANTASMA_GRIS],
                                               paletas.colores[PALETA_FANTASMA_ROJO]};
    mapa_n64_dibujar_paletas(&lista, hojas, pals, ANCHO, ALTO, &m->st);
    m->lista = t1 - t0;
    m->rdp = get_ticks() - t1;
}

#ifdef TH_PRUEBAS
// Dibuja con el RDP la vista, la dibuja también con el rasterizador de
// referencia (en el orden de CorsixTH, sprite a sprite) y compara los dos
// píxel a píxel. En las mezclas el RDP redondea distinto: se cuentan aparte
// las que difieren en 1 (de 31) por componente.
typedef struct {
    int distintos, mezcla_1, mezclas, de_entidades;
} diferencias_t;

// Lo que dibujó el RDP en la pantalla frente al rasterizador de referencia con
// la misma lista y las mismas hojas.
static diferencias_t comparar_con_referencia(const char *hito, surface_t *pantalla, const th_drawlist *l,
                                             const th_sheet *const sheets[TH_SHEETS])
{
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
    // Por franjas de FRANJA filas: con las ventanas (comprobar_ventanas) no
    // quedan en el heap los 375 KiB de la pantalla entera a 24 bits más la
    // mezcla y la hoja. Cada elemento se pinta desplazado (y, y las filas del
    // recorte) en la franja; el orden y los píxeles son los mismos.
    enum { FRANJA = 48 };
    th_raster r = {.rgb = malloc(ANCHO * FRANJA * 3), .mezcla = malloc(ANCHO * FRANJA), .hoja = malloc(ANCHO * FRANJA),
                   .w = ANCHO, .h = FRANJA, .paleta = (const uint8_t(*)[3])pal8};
    // Las paletas fantasma gris y roja (pieza 3 de la demo), si las hay: con
    // ellas pinta la referencia los elementos con TH_DRAW_PAL_GREY o _RED.
    static uint8_t pal_fantasma[2][256][3];
    for (int f = 0; f < 2; f++) {
        const uint16_t *pf = paletas.colores[PALETA_FANTASMA_GRIS + f];
        if (!pf)
            continue;
        for (int i = 0; i < 256; i++) {
            unsigned c = pf[i], cr = c >> 11 & 31, cg = c >> 6 & 31, cb = c >> 1 & 31;
            pal_fantasma[f][i][0] = (uint8_t)(cr << 3 | cr >> 2);
            pal_fantasma[f][i][1] = (uint8_t)(cg << 3 | cg >> 2);
            pal_fantasma[f][i][2] = (uint8_t)(cb << 3 | cb >> 2);
        }
        if (f == 0)
            r.paleta_gris = (const uint8_t(*)[3])pal_fantasma[0];
        else
            r.paleta_roja = (const uint8_t(*)[3])pal_fantasma[1];
    }
    assertf(r.rgb && r.mezcla && r.hoja, "sin memoria para la referencia");

    const uint16_t *fb = pantalla->buffer;
    int distintos = 0, mezcla_1 = 0, mezclas = 0, de_entidades = 0;
    for (int y0 = 0; y0 < ALTO; y0 += FRANJA) {
        th_raster_clear(&r, 0, 0, 0);
        for (int e = 0; e < l->count; e++) {
            th_draw d = l->items[e];
            d.y = (int16_t)(d.y - y0);
            if (d.sheet != TH_SHEET_FILL && (d.flags & TH_DRAW_CLIP)) {
                d.clip_y0 = (int16_t)(d.clip_y0 - y0);
                d.clip_y1 = (int16_t)(d.clip_y1 - y0);
            }
            const th_drawlist uno = {.items = &d, .count = 1, .capacity = 1};
            th_raster_draw(&r, sheets, &uno);
        }
        for (int fy = 0; fy < FRANJA && y0 + fy < ALTO; fy++) {
            const int y = y0 + fy;
            for (int x = 0; x < ANCHO; x++) {
                int i = fy * ANCHO + x;
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
                        debugf("TH64 %s:   (%d,%d): RDP %04x, referencia %02x%02x%02x%s\n", hito, x, y, c,
                               r.rgb[3 * i], r.rgb[3 * i + 1], r.rgb[3 * i + 2], r.mezcla[i] ? " (mezcla)" : "");
                }
            }
        }
    }
    free(r.rgb);
    free(r.mezcla);
    free(r.hoja);
    return (diferencias_t){distintos, mezcla_1, mezclas, de_entidades};
}

static int comprobar_vista(const char *hito, const th_view *v)
{
    surface_t *pantalla = display_get();
    rdpq_attach(pantalla, NULL);
    rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
    rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
    medida_t m;
    dibujar_escena(v, &m);
    const th_sheet *const sheets[TH_SHEETS] = {&sh_bloques, &sh_sprites};
    const diferencias_t d = comparar_con_referencia(hito, pantalla, &lista, sheets);
    debugf("TH64 %s: vista (%d,%d): %d pixeles comparados, %d distintos; %d de %d mezclados a 1/31; "
           "%d de entidades; %d sprites, %d cargas, %d cortes, %d casillas redibujadas\n",
           hito, v->x, v->y, ANCHO * ALTO, d.distintos, d.mezcla_1, d.mezclas, d.de_entidades, m.st.sprites,
           m.st.cargas, m.st.cortes, lista.redraws);
    display_show(pantalla);
    return d.distintos;
}

// Pieza 3 de la demo (06_plan.md): el plano de una sala de 5 × 4 en (64, 66)
// del nivel 1, con una casilla mala (suelo 67 y paredes rojas), una puerta
// doble en su pared norte (marcas 25, 24 y 26) y una persona fantasma gris
// encima de todo (th_view_entity_on_top), dibujados por el RDP con las
// paletas fantasma (mapa_n64_dibujar_paletas) frente a la referencia, sin
// girar y con la cámara girada una vez. Mide además lo que cuesta la lista
// con y sin las superposiciones. Es un apaño en el fichero de la pieza 6:
// solo existe en la ROM de pruebas.
static int comprobar_fantasmas(void)
{
    static th_view_overlays ov;
    static th_view_entity paredes[32];
    static const uint8_t malas[5 * 4] = {[3 * 5 + 4] = 1}; // (68, 69)
    const th_walk_anims *doctor = th_walk_anims_find("Doctor");
    int distintos = 0;
    for (int g = 0; g < 2; g++) {
        th_view v = vista_en(66, 68);
        if (!girar_camara(&v, g))
            return 1;
        v = vista_en(66, 68);
        th_view_overlays_clear(&ov);
        const int n_paredes = th_view_blueprint(&anims, 64, 66, 5, 4, giro, malas, 0, &ov, paredes, 32);
        assertf(n_paredes == 17, "el plano: %d paredes", n_paredes);
        th_view_overlays_set(&ov, 65, 66, 25, 0);
        th_view_overlays_set(&ov, 66, 66, 24, 0);
        th_view_overlays_set(&ov, 67, 66, 26, 0);
        th_view_entity fantasma;
        int fx, fy;
        th_rot_tile(giro, 66, 68, &fx, &fy);
        th_vista_persona_fantasma(doctor, &anims, 66, 68, TH_DIR_EAST, giro, TH_ANIM_PAL_GREY, NULL, &fantasma);
        const th_rot_map *giradas = NULL;
        if (giro) {
            th_rot_map_update(mapa_girado, &mapa, giro);
            giradas = mapa_girado;
        }
        th_scene sc = {.map = &mapa, .rot = giradas, .sheets = {&sh_bloques, &sh_sprites}, .anims = &anims,
                       .entities = paredes, .entity_count = n_paredes};
        // La lista sin y con las superposiciones (la media de 8).
        uint32_t t0 = get_ticks();
        for (int k = 0; k < 8; k++) {
            sc.overlays = NULL;
            th_view_scene(&v, &sc, &lista);
        }
        uint32_t t1 = get_ticks();
        for (int k = 0; k < 8; k++) {
            sc.overlays = &ov;
            th_view_scene(&v, &sc, &lista);
        }
        uint32_t t2 = get_ticks();
        assertf(th_view_entity_on_top(&v, &sc, &fantasma, &lista) == 0, "el fantasma no cabe");
        int marcas = 0, rojos = 0, grises = 0;
        for (int i = 0; i < lista.count; i++) {
            const th_draw *d = &lista.items[i];
            marcas += d->sheet == TH_SHEET_BLOCKS && d->sprite >= 25 && d->sprite <= 32; // la puerta doble
            rojos += (d->flags & TH_DRAW_PAL_RED) != 0;
            grises += (d->flags & TH_DRAW_PAL_GREY) != 0;
        }

        surface_t *pantalla = display_get();
        rdpq_attach(pantalla, NULL);
        rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
        rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
        mapa_n64_stats_t st;
        const uint16_t *const pals[PALETAS_MAX] = {paletas.colores[PALETA_NORMAL],
                                                   paletas.colores[PALETA_FANTASMA_GRIS],
                                                   paletas.colores[PALETA_FANTASMA_ROJO]};
        uint32_t t3 = get_ticks();
        mapa_n64_dibujar_paletas(&lista, hojas, pals, ANCHO, ALTO, &st);
        uint32_t t4 = get_ticks();
        const th_sheet *const sheets[TH_SHEETS] = {&sh_bloques, &sh_sprites};
        const diferencias_t d = comparar_con_referencia("demo3", pantalla, &lista, sheets);
        debugf("TH64 demo3: giro %d: %d pixeles comparados, %d distintos; %d de %d mezclados a 1/31; %d sprites "
               "(%d marcas, %d rojos, %d grises), %d cargas, %d TLUT; lista %lu us sin superposiciones, %lu us con "
               "(%d); RDP %lu us\n",
               giro, ANCHO * ALTO, d.distintos, d.mezcla_1, d.mezclas, lista.count, marcas, rojos, grises, st.cargas,
               st.tluts, (unsigned long)TICKS_TO_US((t1 - t0) / 8), (unsigned long)TICKS_TO_US((t2 - t1) / 8),
               ov.count, (unsigned long)TICKS_TO_US(t4 - t3));
        display_show(pantalla);
        distintos += d.distintos;
        if (marcas != 2 || !rojos || !grises || !st.tluts)
            distintos++; // no se probó lo que se quería probar
    }
    th_view v = vista_en(66, 68);
    girar_camara(&v, 0);
    return distintos;
}

// Hito 6.1: el panel inferior, su descripción y el puntero, dibujados por el
// RDP y por la referencia, en varios estados: en pausa, con los botones de
// gestión a la vista, con un botón pulsado y con dos descripciones.
static int comprobar_interfaz(void)
{
    if (!interfaz_cargar(ANCHO, ALTO, false))
        return 1;
    static const struct {
        int x, y;
        bool pulsado, descripcion, pausa;
        int32_t dinero, reputacion;
        int dia, mes;
    } estados[] = {
        {160, 120, false, false, true, 40000, 500, 1, 1},  {200, 225, false, false, false, -1234, 0, 28, 9},
        {120, 227, true, false, false, 1234567, 1000, 31, 12}, {10, 225, false, true, false, 987, 333, 15, 4},
        {40, 233, false, true, false, 40000, 777, 3, 3},
    };
    static th_draw elementos_ui[512];
    int distintos = 0;
    for (unsigned i = 0; i < sizeof estados / sizeof estados[0]; i++) {
        interfaz_estado(estados[i].x, estados[i].y, estados[i].pulsado, estados[i].descripcion);
        const th_bottom_info info = {.balance = estados[i].dinero, .day = estados[i].dia, .month = estados[i].mes,
                                     .reputation = estados[i].reputacion, .rep_min = 0, .rep_max = 1000,
                                     .paused = estados[i].pausa};
        th_drawlist l = {.items = elementos_ui, .capacity = 512};
        interfaz_lista(&info, &l);
        surface_t *pantalla = display_get();
        rdpq_attach(pantalla, NULL);
        rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
        rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
        mapa_n64_stats_t st;
        const uint32_t t0 = get_ticks();
        interfaz_dibujar(&info, paletas.colores[PALETA_NORMAL], &st);
        const uint32_t t1 = get_ticks();
        const diferencias_t d = comparar_con_referencia("hito6.1", pantalla, &l, interfaz_hojas());
        debugf("TH64 hito6.1: interfaz %u (puntero %d,%d): %d elementos, %d pixeles distintos; %d cargas; CPU del "
               "dibujo %.3f ms\n",
               i + 1, estados[i].x, estados[i].y, l.count, d.distintos, st.cargas, (t1 - t0) * 1000.0f / TICKS_PER_SECOND);
        display_show(pantalla);
        distintos += d.distintos;
    }
    debugf("TH64 hito6.1: comparacion de la interfaz con la referencia: %d pixeles distintos\n", distintos);
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
    // Hito 6.2: lo mismo en las cuatro vistas, con lo que cuesta pasar la
    // simulación a entidades de cada una y hacer el mapa girado.
    for (int r = 0; r < 4; r++) {
        th_view g = v;
        const uint32_t m0 = get_ticks();
        girar_camara(&g, r);
        const uint32_t m1 = get_ticks();
        int ne = 0;
        for (int k = 0; k < VECES; k++)
            ne = th_vista_sim_rot(&sim, r, entidades, (int)(sizeof entidades / sizeof entidades[0]));
        const uint32_t m2 = get_ticks();
        th_scene sg = {.map = &mapa, .rot = r ? mapa_girado : NULL,
                       .sheets = {&sh_bloques, &sh_sprites}, .anims = &anims, .entities = entidades,
                       .entity_count = ne};
        for (int k = 0; k < VECES; k++)
            th_view_scene(&g, &sg, &lista);
        const uint32_t m3 = get_ticks();
        debugf("TH64 hito6.2: vista %d: lista de dibujo %lu us (%d sprites, %d entidades), entidades de la "
               "simulacion %lu us, mapa girado %lu us\n",
               r, (unsigned long)TICKS_TO_US((m3 - m2) / VECES), lista.count, ne,
               (unsigned long)TICKS_TO_US((m2 - m1) / VECES), (unsigned long)TICKS_TO_US(m1 - m0));
        girar_camara(&g, 0);
    }
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

// El giro con el que abre el primer modo (la ROM de capturas).
static int giro_inicial;
#ifndef TH_GIRO_INICIAL
#define TH_GIRO_INICIAL 0
#endif

// La ROM de prueba (hito 6.2): qué se puede abrir desde el menú. La partida
// nueva (la demo jugable, docs/06_plan.md) va al final para no cambiar los
// números de make MODO=m, pero el menú empieza en ella.
enum { MODO_HITO4, MODO_HITO5, MODO_VISTAS, MODO_VISOR, MODO_PARTIDA, MODOS };
static const struct {
    const char *nombre, *escenario; // escenario: rom:/escenarios/<escenario>.trg, o NULL (el visor)
    uint32_t semilla;
    bool nueva; // la partida empieza en la hora 0 aunque la ROM adelante el hospital (make HORAS=N)
} modos[MODOS] = {
    {"Hospital del hito 4 (recepcion y consulta)", "hito4", 1, false},
    {"Hospital del hito 5 (18 llegadas al mes)", "hito5", 5, false},
    {"Hospital de las vistas (8 salas)", "vistas", 1, false},
    {"Visor de niveles", NULL, 0, false},
    // El nivel 1 recién empezado: 40.000 $, abierto y sin personal
    // (tools/hito6/demo.sh).
    {"Partida nueva (nivel 1)", "demo", 1, true},
};

static bool modo_disponible(int m)
{
    char ruta[64];
    if (!modos[m].escenario)
        return true;
    snprintf(ruta, sizeof ruta, "rom:/escenarios/%s.trg", modos[m].escenario);
    return existe(ruta);
}

// Los botones que se acaban de pulsar (los mandos se leen en cada barrido:
// interfaz_mandos_iniciar).
static joypad_buttons_t recien_pulsados(joypad_buttons_t ahora)
{
    static joypad_buttons_t antes;
    joypad_buttons_t p = {.raw = ahora.raw & ~antes.raw};
    antes = ahora;
    return p;
}

// Las ventanas de la partida (ventanas.h) en un fotograma del hospital: lo que
// llegó del ratón (interfaz_mandos) va a th_game_ui_frame con la cámara v con
// la que se va a dibujar, y lo que devuelve se aplica: las órdenes a la cola
// (partida_mandar_orden), el texto a la barra y lo que hay que dibujar a la
// escena (extra_*). Con los datos a la vista y sin superposiciones de las
// ventanas, la casilla bajo el puntero (*cx, *cy, del mundo) se resalta con el
// rombo azul de los planos (bloque 24 al 50 %) por las superposiciones, debajo
// de las paredes y las personas como la capa ui de CorsixTH. El aviso lo
// decide la ventana (ventanas.h): el sonido wrong2 (out->reject_sound) solo
// donde CorsixTH lo toca, y el texto también con los rechazos del núcleo que
// vuelven por th_game_ui_order_result (aviso_de en ventanas.c: la pieza 6 del
// plan), donde CorsixTH casi siempre calla; con alguien encima del objeto,
// nada (place_objects.lua:516-520). El rechazo queda además en el registro
// («TH64 orden», con su motivo) y en debugf.
static void ventanas_fotograma(th_game_ui *ui, const interfaz_orden_t *orden, bool escape, bool pausa, bool datos,
                               const th_view *v, th_game_ui_output *out, int *cx, int *cy)
{
    static th_view_overlays resaltado;
    int px, py;
    interfaz_puntero(&px, &py);
    partida_vaciar(&sim);
    const th_game_ui_input in = {.px = px, .py = py, .world_down = orden->mundo_abajo,
                                 .world_up = orden->mundo_arriba, .win = orden->ventana, .id = orden->id,
                                 .mouse = orden->raton, .panel_button = orden->boton, .escape = escape,
                                 .paused = pausa, .sim_idle = !simulacion_en_tick()};
    th_game_ui_frame(ui, &in, &sim, v, giro, out);
    assertf(out->n_orders >= 0 && out->n_orders <= TH_GAME_UI_ORDERS, "ventanas: %d ordenes", out->n_orders);
    assertf(!out->entities || (out->n_entities >= 0 && out->n_entities <= TH_GAME_UI_ENTITIES),
            "ventanas: %d entidades", out->n_entities);
    assertf(!out->on_top || (out->n_on_top >= 0 && out->n_on_top <= TH_GAME_UI_ENTITIES), "ventanas: %d encima",
            out->n_on_top);
    for (int i = 0; i < out->n_orders; i++)
        if (partida_mandar_orden(&sim, &out->orders[i]) != 0)
            th_game_ui_order_dropped(ui, &out->orders[i]); // la ventana no se queda esperando su respuesta
    if (out->reject_sound)
        partida_aviso(out->message);
    else if (out->message)
        partida_texto(out->message);
    int motivo;
    if (partida_rechazo(&motivo))
        debugf("TH64 partida: orden rechazada por el nucleo (%s)\n", th_build_reason_name(motivo));
    *cx = *cy = -1;
    if (datos) {
        int vx, vy;
        th_view_pick(v, px, py, &vx, &vy);
        if (vx >= 0 && vy >= 0 && vx < TH_MAP_WIDTH && vy < TH_MAP_HEIGHT)
            th_rot_tile_inv(giro, vx, vy, cx, cy);
    }
    extra_superposiciones = out->overlays;
    if (!out->overlays && *cx >= 0) {
        th_view_overlays_clear(&resaltado);
        th_view_overlays_set(&resaltado, *cx, *cy, 24, TH_DRAW_ALPHA50);
        extra_superposiciones = &resaltado;
    }
    extra_entidades = out->entities;
    n_extra_entidades = out->entities ? out->n_entities : 0;
    extra_encima = out->on_top;
    n_extra_encima = out->on_top ? out->n_on_top : 0;
}

// Lo que añadió ventanas_fotograma, fuera de la escena.
static void sin_extras(void)
{
    extra_superposiciones = NULL;
    extra_entidades = extra_encima = NULL;
    n_extra_entidades = n_extra_encima = 0;
}

// Las ventanas, con lo que les da la ROM: la pila con el panel, las fuentes y
// los textos de la interfaz, las animaciones y las de caminar. env.walk es el
// PRINCIPIO de la tabla (th_walk_anims_at(0), la primera clase), no la de una
// clase concreta: la de cada persona se busca con th_walk_anims_find (el médico,
// "Doctor"). Lo decide quien integre (ventanas.h no lo aclara).
static th_game_ui *ventanas_iniciar(void)
{
    th_game_ui *ui = th_game_ui_instance();
    const th_game_ui_env env = {.stack = interfaz_pila(), .fonts = interfaz_fuentes(), .texts = interfaz_textos(),
                                .anims = &anims, .walk = th_walk_anims_at(0), .screen_w = ANCHO,
                                .screen_h = ALTO};
    th_game_ui_init(ui, &env);
    return ui;
}

// Sin Expansion Pak (4 MiB) la ROM normal no cabe: las hojas y las
// animaciones dejan unos 250 KiB de heap y asset_load se paraba en el nivel 1
// con la pantalla del fallo («Out of memory», cargar_nivel). La demo con 4 MiB
// es el hito 6.10 (docs/06_plan.md); hasta entonces, un aviso. Las ROM de
// pruebas y de medida siguen adelante: la de medida mide justo eso.
#if !defined(TH_PRUEBAS) && !defined(TH_MEDIR)
static void sin_expansion(int ram_kib)
{
    debugf("TH64 motor: sin Expansion Pak (%d KiB de RDRAM): no cabe, se para\n", ram_kib);
    for (;;) {
        surface_t *pantalla = display_get();
        rdpq_attach(pantalla, NULL);
        rdpq_set_mode_fill(RGBA32(0, 0, 40, 255));
        rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 100, "TH64 %s", TH64_VERSION);
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 124, "Hace falta el Expansion Pak (8 MiB).");
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 136, "Esta consola tiene %d KiB de RAM.", ram_kib);
        rdpq_detach_show();
    }
}
#endif

// El menú de la ROM de prueba: la cruceta o el stick eligen y A abre.
static int menu(int actual, bool assets, int ram_kib, bool expansion)
{
    debugf("TH64 menu: ROM de prueba lista\n");
    joypad_inputs_t in;
    interfaz_leer(&in);
    recien_pulsados(in.btn);
    int bajo_stick = 0; // el stick, como la cruceta: un paso al pasar del centro
    for (;;) {
        interfaz_leer(&in);
        const joypad_buttons_t p = recien_pulsados(in.btn);
        int paso = p.d_down || p.c_down ? 1 : p.d_up || p.c_up ? -1 : 0;
        const int st = in.stick_y < -40 ? 1 : in.stick_y > 40 ? -1 : 0;
        if (st != bajo_stick)
            paso += st;
        bajo_stick = st;
        for (int k = 0; paso && k < MODOS; k++) {
            actual = (actual + (paso > 0 ? 1 : MODOS - 1)) % MODOS;
            if (modo_disponible(actual))
                break;
        }
        if ((p.a || p.start) && assets && modo_disponible(actual)) {
            vigilante_fase(NULL, 0); // la carga de la partida no tiene límite
            return actual;
        }
        vigilante_fase("menu display_get", 160);
        surface_t *pantalla = display_get();
        vigilante_fase("menu dibujar", 160);
        rdpq_attach(pantalla, NULL);
        rdpq_set_mode_fill(RGBA32(0, 0, 40, 255));
        rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 24, "TH64 %s - ROM de prueba", TH64_VERSION);
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 36, "RAM %d KiB, Expansion Pak %s", ram_kib,
                         expansion ? "si" : "no");
        if (!assets)
            rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 60, "Sin assets: ejecuta tools/conv/conv.py");
        for (int m = 0; m < MODOS; m++)
            rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 72 + 16 * m, "%s %s%s", m == actual ? ">" : " ",
                             modos[m].nombre, modo_disponible(m) ? "" : " (no esta)");
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 168, "Cruceta: elegir   A: abrir");
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 184, "En el hospital: Start pausa, R Escape;");
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 196, "en pausa, Z 1 s: menu. Arriba: datos.");
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 208, "En el visor: Start vuelve aqui.");
        rdpq_text_printf(NULL, FUENTE_DEPURACION, 16, 224, "Cruceta izquierda y derecha: girar");
        rdpq_detach_show();
    }
}

// Con la interfaz, salir al menú es Z mantenido SALIR_MS en pausa, sin ninguna
// ventana abierta (th_game_ui_active) y con el stick quieto. Z también acelera
// el puntero (decisión 3; interfaz_mandos: th_pointer_stick con btn.z), y en
// pausa se puede mover el puntero y abrir ventanas (decisión 4): con solo
// «Z en pausa», el gesto de acelerar el puntero abandonaba la partida (y
// partida_terminar descarta las órdenes que esperan). Mover el stick, soltar Z
// o abrir una ventana vuelve a empezar la cuenta. El mismo umbral que la zona
// muerta del puntero (puntero.c, eje: 8).
#define SALIR_MS 1000
static bool salir_con_z(const joypad_inputs_t *in, bool pausa, const th_game_ui *ui)
{
    static uint32_t desde; // ms en que empezó Z quieto; 0: no está
    const bool quieto = abs(in->stick_x) <= 8 && abs(in->stick_y) <= 8;
    if (!pausa || !in->btn.z || !quieto || (ui && th_game_ui_active(ui) != TH_GW_NONE)) {
        desde = 0;
        return false;
    }
    const uint32_t ahora = get_ticks_ms() | 1;
    if (!desde)
        desde = ahora;
    if (ahora - desde < SALIR_MS)
        return false;
    desde = 0;
    return true;
}

// Abre lo elegido en el menú y lo juega hasta que se pide volver.
//   - El hospital (hito 6.1 y la demo jugable, decisión 3 de docs/06_plan.md):
//     el stick (o el ratón de N64) mueve el puntero y Z lo acelera; A y B son
//     los botones izquierdo y derecho del ratón; R es Escape (cierra la
//     ventana de arriba o vuelve una fase atrás); los botones C y la cruceta
//     abajo desplazan la vista; la cruceta izquierda y derecha la giran (hito
//     6.2); la cruceta arriba enseña los datos y la casilla bajo el puntero;
//     L recorre x1, x2, x4 y x8; Start pausa y, en pausa, Z mantenido 1 s
//     sin ventana abierta y con el stick quieto vuelve al menú (salir_con_z).
//     Las órdenes de las ventanas van a la cola del núcleo y se graban por
//     debugf (partida.h); al volver al menú, la línea final con la huella.
//   - El visor de niveles (hito 3): el stick y la cruceta arriba y abajo
//     mueven la cámara; la cruceta izquierda y derecha la giran; A hace que
//     siga al primer humanoide; B añade 10; Z pasa al nivel siguiente; R
//     oculta los datos; L mide el RDP; Start vuelve al menú.
static void jugar(int modo, bool assets, int ram_kib, const heap_stats_t *heap)
{
    int nivel = 1;
    th_view v = {0};
    girar_camara(&v, 0);
    repartida = false;
    hospital = false;
    if (modos[modo].escenario) {
        if (!cargar_hospital_de(modos[modo].escenario, modos[modo].semilla))
            return;
        for (int i = 0; !modos[modo].nueva && i < TH_HOSPITAL_HORAS; i++)
            th_sim_tick(&sim);
        // La simulación, repartida entre fotogramas (simulacion.h).
        simulacion_iniciar(&sim, foto, (int)(sizeof foto / sizeof foto[0]));
        simulacion_girar(0);
        repartida = true;
        v = vista_hospital();
        int plantilla = 0;
        for (int k = 0; k < sim.n_order; k++)
            plantilla += sim.hum[sim.order[k]].kind != TH_HUM_PATIENT;
        heap_stats_t hs;
        sys_get_heap_stats(&hs);
        debugf("TH64 hito4: hospital del escenario %s cargado: %d objetos, %d salas, %d en plantilla; %d horas "
               "adelantado, %d pacientes en el mapa; heap usado %d KiB; simulacion %u KiB\n",
               modos[modo].escenario, sim.n_obj, sim.n_rooms, plantilla, modos[modo].nueva ? 0 : TH_HOSPITAL_HORAS,
               sim.n_order - plantilla, hs.used / 1024, (unsigned)(sizeof sim / 1024));
    } else {
        if (!cargar_nivel(nivel))
            return;
        n_humanoides = 0;
        nuevo_humanoide(mapa.camera_x, mapa.camera_y);
        v = vista_en(mapa.camera_x, mapa.camera_y);
        debugf("TH64 hito3: visor de niveles, nivel %d\n", nivel);
    }
    int velocidad = 1, giro_pedido = giro_inicial;
    if (giro_inicial) {
        if (hospital)
            simulacion_girar(giro_inicial);
        else
            girar_camara(&v, giro_inicial);
        giro_inicial = 0;
    }
    bool datos = !hospital, medir_rdp = false, seguir = !hospital;
    const bool interfaz = hospital && interfaz_cargar(ANCHO, ALTO, true);
    // Las ventanas de la partida y la grabación de las órdenes. La partida se
    // puede repetir en el PC (tools/hito6/repetir.sh) si empieza en la hora 0
    // del escenario: la partida nueva, o cualquier hospital sin make HORAS=N.
    th_game_ui *ui = NULL;
    th_game_ui_output salida = {0};
    if (hospital) {
        if (interfaz) {
            ui = ventanas_iniciar();
            if (!partida_sonido_iniciar())
                debugf("TH64 partida: sin el sonido de los rechazos (rom:/sonido/comun/wrong2.wav64)\n");
        }
        const bool desde_cero = modos[modo].nueva || TH_HOSPITAL_HORAS == 0;
        partida_empezar(&sim, ui, desde_cero ? modos[modo].escenario : NULL, modos[modo].semilla);
    }
    bool pausa = false;
    // La velocidad de verdad (decimas: ticks por segundo · 54 ms / 100), medida
    // cada segundo: la simulación no da para x8 (ni casi para x4) y va más
    // despacio sin saltar fotogramas (docs/06_juego_base.md, «Las velocidades»).
    uint32_t vel_ms = get_ticks_ms(), vel_ticks = hospital ? simulacion_stats()->ticks : 0;
    int vel_real = 0;
    float cpu_ms = 0, lista_ms = 0, sim_ms = 0, rdp_buf_ms = 0, rdp_pipe_ms = 0;
    mapa_n64_stats_t st = {0};
    sim_t s = {0};
    uint32_t reloj = get_ticks(), acumulado = 0, t_rep = 0;
    const uint32_t por_tick = TICKS_PER_SECOND / 1000 * MS_POR_TICK;
    joypad_inputs_t in;
    interfaz_leer(&in);
    recien_pulsados(in.btn);
    uint32_t fotograma = 0;
    for (;;) {
        vigilante_fase("mandos", 160);
        interfaz_orden_t orden = {0};
        if (interfaz)
            orden = interfaz_mandos(&in);
        else
            interfaz_leer(&in);
        const joypad_buttons_t pulsado = recien_pulsados(in.btn);
        if (interfaz ? salir_con_z(&in, pausa, ui) : pulsado.start)
            break; // al menú
        // R es Escape con la interfaz (in->escape); los datos, la cruceta arriba.
        if (interfaz ? pulsado.d_up : pulsado.r)
            datos = !datos;
        if (pulsado.l && !interfaz)
            medir_rdp = !medir_rdp;
        if (pulsado.a && !interfaz)
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
        if (interfaz && pulsado.l)
            velocidad = velocidad == 8 ? 1 : velocidad * 2;
        if (interfaz && pulsado.start) {
            pausa = !pausa;
            // World:setSpeed("Pause") para el reloj al momento: los ticks ya
            // pedidos y sin empezar no corren en pausa (simulacion_soltar).
            if (pausa && hospital)
                simulacion_soltar();
        }
        if (assets && !hospital && pulsado.z) {
            nivel = nivel % NIVELES + 1;
            if (cargar_nivel(nivel)) {
                nuevo_humanoide(mapa.camera_x, mapa.camera_y);
                v = vista_en(mapa.camera_x, mapa.camera_y);
            }
        }
        // La cruceta izquierda y derecha gira la cámara. En el hospital, la
        // cámara gira cuando lo hacen las entidades de la simulación
        // (simulacion_girar), para no dibujar un fotograma con las dos a
        // destiempo.
        if (pulsado.d_left || pulsado.d_right) {
            giro_pedido = (giro_pedido + (pulsado.d_right ? 1 : 3)) & 3;
            if (hospital)
                simulacion_girar(giro_pedido);
            else if (!girar_camara(&v, giro_pedido))
                giro_pedido = giro;
        }
        // La vista: en el hospital, los botones C, la cruceta abajo y el borde
        // de la pantalla; en el visor, el stick y la cruceta. Mover la cámara
        // deja de seguir al humanoide.
        int sx = 0, sy = 0;
        if (interfaz) {
            sx = orden.vista_dx + (in.btn.c_right ? 8 : in.btn.c_left ? -8 : 0);
            sy = orden.vista_dy + (in.btn.c_down || in.btn.d_down ? 8 : in.btn.c_up ? -8 : 0);
        } else {
            sx = abs(in.stick_x) > 8 ? in.stick_x / 6 : 0;
            sy = abs(in.stick_y) > 8 ? -in.stick_y / 6 : 0;
            sy += in.btn.d_down ? 8 : in.btn.d_up ? -8 : 0;
        }
        if (sx || sy)
            seguir = false;
        v.x += sx;
        v.y += sy;
        th_view_clamp(&v);

        vigilante_fase("display_get", 160);
        surface_t *pantalla = display_get();
        vigilante_fase("ticks", 160);
        vigilante_contexto(++fotograma, hospital ? sim.hours : 0, velocidad, pausa, hospital ? sim.n_order : 0,
                           ui ? th_game_ui_active(ui) : -1);
#ifdef TH_VIGILANTE_PRUEBA
        if (fotograma == 150) { // la prueba del vigilante: un atasco a propósito
            vigilante_fase("prueba_del_vigilante", 160);
            wait_ms(300);
        }
#endif
        uint32_t t0 = get_ticks();
        // El mundo avanza un tick cada 54 ms de reloj (como mucho 4 por
        // fotograma). El hospital pide los ticks a la simulación repartida, que
        // los hace después de dibujar; si va atrasada más de dos fotogramas de
        // ticks, no se piden más: el juego va más despacio, sin saltar fotogramas.
        acumulado += t0 - reloj;
        reloj = t0;
        uint32_t sim0 = s.cpu_ticks + s.cpu_caminos;
        for (int k = 0; assets && !pausa && acumulado * velocidad >= por_tick && k < 4 * velocidad; k++) {
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
        if (hospital) {
            const uint32_t ahora = get_ticks_ms();
            if (pausa || ahora - vel_ms >= 1000) {
                const uint32_t t = simulacion_stats()->ticks;
                vel_real = pausa ? 0 : (int)((t - vel_ticks) * MS_POR_TICK * 10 / (ahora - vel_ms));
                vel_ms = ahora;
                vel_ticks = t;
            }
        }
        // La cámara, con la rotación de las entidades que se van a dibujar.
        if (hospital && simulacion_instantanea()->giro != giro &&
            !girar_camara(&v, simulacion_instantanea()->giro))
            simulacion_girar(giro_pedido = giro); // sin memoria para girar
        if (hospital && seguir) {
            const int p = ultimo_paciente();
            if (p >= 0)
                v = vista_de(&sim.hum[p].h);
        } else if (assets && seguir && n_humanoides > 0) {
            v = vista_de(&humanoides[0]);
        }
        // Las ventanas, con la cámara con la que se va a dibujar, y la casilla
        // bajo el puntero (con los datos a la vista).
        int casilla_x = -1, casilla_y = -1;
        vigilante_fase("ventanas", 160);
        if (ui)
            ventanas_fotograma(ui, &orden, pulsado.r, pausa, datos, &v, &salida, &casilla_x, &casilla_y);
        vigilante_fase("dibujar", 160);
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
            rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 12, "TH64 %s  nivel %d  vista %d  (%d,%d)", TH64_VERSION,
                             nivel, giro, v.x, v.y);
            if (assets) {
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 24, "CPU %.1f ms (mundo %.1f, lista %.1f) %.1f fps",
                                 cpu_ms, sim_ms, lista_ms, display_get_fps());
                if (hospital)
                    rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 36,
                                     "%d personas  rep %ld  x%d (real x%d.%d)  %d sprites", sim.n_order, (long)sim.hosp.reputation, velocidad, vel_real / 10, vel_real % 10,
                                     st.sprites);
                else
                    rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 36, "%d humanoides  %d sprites  %d cargas",
                                     n_humanoides, st.sprites, st.cargas);
                if (casilla_x >= 0)
                    rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 48, "casilla (%d,%d)", casilla_x, casilla_y);
                if (medir_rdp)
                    rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 48, "RDP ocupado %.2f ms (pipe %.2f ms)", rdp_buf_ms,
                                     rdp_pipe_ms);
            }
            if (hospital) {
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 200,
                                 pausa ? "Z 1 s: menu  Start: seguir" : "Start: pausa  L: x1/2/4/8  R: Escape");
            } else {
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 220, "RAM %d KiB  heap %d/%d KiB", ram_kib,
                                 heap->used / 1024, heap->total / 1024);
                rdpq_text_printf(NULL, FUENTE_DEPURACION, 8, 232, "A: seguir B: +10 Z: nivel <>: girar Start: menu");
            }
        }
        if (interfaz) {
            const instantanea_t *f = simulacion_instantanea();
            const th_bottom_info info = {.balance = f->saldo, .day = f->fecha.day, .month = f->fecha.month,
                                         .reputation = f->reputacion, .rep_min = 0, .rep_max = 1000,
                                         .paused = pausa, .message = partida_mensaje()};
            mapa_n64_stats_t st_ui;
            interfaz_dibujar(&info, paletas.colores[PALETA_NORMAL], &st_ui);
        }
        vigilante_fase("detach_show", 160);
        rdpq_detach_show();
        sin_extras();
        vigilante_fase("sonido", 160);
        partida_sonido_fotograma();
        if (hospital) {
            vigilante_fase("simulacion", 600);
            const uint32_t cpu0 = simulacion_stats()->cpu;
            simulacion_avanzar(t0 + TICKS_FROM_MS(28));
            t_rep = simulacion_stats()->cpu - cpu0;
        }
    }
    // Al menú: sin órdenes nuevas, la simulación sin un tick a medias, las
    // órdenes que no llegaron a aplicarse descartadas, la línea final de la
    // partida (partida_terminar) y la cámara sin girar.
    vigilante_fase(NULL, 0);
    if (hospital) {
        simulacion_terminar();
        partida_terminar(&sim);
    }
    if (ui)
        th_game_ui_close_all(ui);
    partida_sonido_cerrar();
    sin_extras();
    girar_camara(&v, 0);
    simulacion_girar(0);
}

#ifdef TH_PRUEBAS
// La partida de prueba de la demo jugable (pieza 6 de docs/06_plan.md): la
// tabla copia tools/hito6/ordenes/demo.tsv (las 13 órdenes, con los campos de
// th_order que deja th64sim_leer_orden), y los resúmenes del perfil de las
// dos contrataciones son los que pone th64sim partida con la semilla 1 en
// ordenes_rom.log (577e31bf, la recepcionista 0 en el tick 1, y cf501390, el
// médico 0 en el tick 100). Si demo.tsv cambia, hay que cambiarla a mano.
static const struct {
    uint32_t tick;
    uint8_t kind, x, y, dir, qty, w, h;
    const char *def;  // «objetos.<id>» o «salas.<id>» de las definiciones, o NULL
    uint32_t resumen; // contratar: th_staff_profile_digest del candidato
} ordenes_demo[] = {
    {0, TH_ORD_COMPRAR, 0, 0, 0, 1, 0, 0, "objetos.reception_desk", 0},
    {0, TH_ORD_COLOCAR, 62, 74, TH_DIR_NORTH, 0, 0, 0, "objetos.reception_desk", 0},
    {1, TH_ORD_CONTRATAR, 62, 73, TH_HUM_RECEPTIONIST, 0, 0, 0, NULL, 0x577e31bfu},
    {50, TH_ORD_SALA, 57, 50, 0, 0, 4, 4, "salas.gp", 0},
    {50, TH_ORD_PUERTA, 58, 53, TH_DIR_SOUTH, 0, 0, 0, NULL, 0},
    {50, TH_ORD_VENTANA, 60, 51, TH_DIR_EAST, 0, 0, 0, NULL, 0},
    {50, TH_ORD_VENTANA, 60, 52, TH_DIR_EAST, 0, 0, 0, NULL, 0},
    {50, TH_ORD_FIN_VENTANAS, 0, 0, 0, 0, 0, 0, NULL, 0},
    {51, TH_ORD_COLOCAR, 58, 52, TH_DIR_SOUTH, 0, 0, 0, "objetos.chair", 0},
    {51, TH_ORD_COLOCAR, 58, 51, TH_DIR_NORTH, 0, 0, 0, "objetos.desk", 0},
    {51, TH_ORD_COLOCAR, 60, 50, TH_DIR_NORTH, 0, 0, 0, "objetos.cabinet", 0},
    {51, TH_ORD_FIN_SALA, 0, 0, 0, 0, 0, 0, NULL, 0},
    {100, TH_ORD_CONTRATAR, 59, 56, TH_HUM_DOCTOR, 0, 0, 0, NULL, 0xcf501390u},
};

// «grupo.id» en las definiciones de la simulación (0 si no está).
static th_rnode definicion(const char *ruta)
{
    char grupo[16];
    const char *punto = strchr(ruta, '.');
    if (!punto || punto - ruta >= (int)sizeof grupo)
        return 0;
    memcpy(grupo, ruta, (size_t)(punto - ruta));
    grupo[punto - ruta] = 0;
    return th_rules_get(sim.defs, th_rules_get(sim.defs, sim.defs->root, grupo), punto + 1);
}

// La partida en marcha como en jugar(), sin tocar los mandos: el reloj pide
// los ticks a la velocidad dada, las ventanas reciben el fotograma (sin
// ninguna abierta: el esqueleto de ventanas.c no abre ninguna), se dibujan el
// hospital con las tres paletas y la interfaz, y la simulación sigue con lo
// que sobra hasta 28 ms. Mide la CPU del fotograma entero (con los ticks).
static void recorrido_partida(th_game_ui *ui, const char *nombre, int velocidad, int fotogramas, const th_view *vista)
{
    th_view v = vista ? *vista : vista_hospital();
    cuentas_t c = {0};
    uint32_t reloj = get_ticks(), acumulado = 0, cpu_ui = 0, cpu_interfaz = 0, max_ui = 0;
    const uint32_t por_tick = TICKS_PER_SECOND / 1000 * MS_POR_TICK, ticks0 = simulacion_stats()->ticks;
    const uint32_t empieza = get_ticks();
    for (int f = 0; f < fotogramas; f++) {
        surface_t *pantalla = display_get();
        const uint32_t t0 = get_ticks();
        joypad_inputs_t in;
        const interfaz_orden_t orden = interfaz_mandos(&in);
        acumulado += t0 - reloj;
        reloj = t0;
        for (int k = 0; acumulado * velocidad >= por_tick && k < 4 * velocidad; k++) {
            acumulado -= por_tick / velocidad;
            if (simulacion_pendientes() <= 2 * velocidad)
                simulacion_pedir(1);
        }
        if (acumulado >= por_tick)
            acumulado = 0;
        th_game_ui_output out;
        int cx, cy;
        const uint32_t u0 = get_ticks();
        ventanas_fotograma(ui, &orden, false, false, false, &v, &out, &cx, &cy);
        const uint32_t u1 = get_ticks();
        rdpq_attach(pantalla, NULL);
        rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
        rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
        medida_t m = {0};
        dibujar_escena(&v, &m);
        const instantanea_t *fi = simulacion_instantanea();
        const th_bottom_info info = {.balance = fi->saldo, .day = fi->fecha.day, .month = fi->fecha.month,
                                     .reputation = fi->reputacion, .rep_min = 0, .rep_max = 1000,
                                     .message = partida_mensaje()};
        mapa_n64_stats_t st_ui;
        const uint32_t i0 = get_ticks();
        interfaz_dibujar(&info, paletas.colores[PALETA_NORMAL], &st_ui);
        const uint32_t i1 = get_ticks();
        rdpq_detach_show();
        sin_extras();
        simulacion_avanzar(t0 + TICKS_FROM_MS(28));
        contar(&c, t0, get_ticks(), &m);
        cpu_ui += u1 - u0;
        cpu_interfaz += i1 - i0;
        if (u1 - u0 > max_ui)
            max_ui = u1 - u0;
    }
    const uint32_t dura = get_ticks() - empieza;
    simulacion_terminar();
    informar("partida", nombre, &c);
    const float ms = 1000.0f / TICKS_PER_SECOND;
    // La velocidad de verdad: los ticks hechos por los que tocaban a x1 (uno
    // cada 54 ms del reloj) en lo que duró el recorrido.
    const uint32_t hechos = simulacion_stats()->ticks - ticks0;
    debugf("TH64 partida: %s: %lu ticks en %.0f ms (%.1f ticks/s: velocidad real x%.2f); ventanas %.3f ms de media "
           "(%.3f como mucho); interfaz %.2f ms; %d personas\n",
           nombre, (unsigned long)hechos, dura * ms, hechos * 1000.0f / (dura * ms),
           hechos * (float)MS_POR_TICK / (dura * ms), cpu_ui * ms / fotogramas, max_ui * ms,
           cpu_interfaz * ms / fotogramas, sim.n_order);
}

// La partida de prueba: demo.trg con la semilla 1 y las órdenes de
// ordenes_demo por el mismo camino que la partida real (partida_mandar_orden
// en el fotograma cuyo tick toca y la simulación repartida, simulacion.h,
// corriendo sin límite de tiempo), durante 3.000 horas. Escribe las líneas del
// contrato del registro (tools/hito6/README.md; tools/motor_check.sh las pasa
// por tools/hito6/repetir.sh, que repite la partida en el PC) y la CPU de cada
// tick que aplica órdenes (el bloqueo de zonas y la temperatura: el riesgo 1
// de la demo). Después, la CPU del fotograma con la partida en marcha a x1 y a
// x8. Devuelve los fallos.
static int comprobar_partida(void)
{
    const uint32_t horas = 3000;
    if (!existe("rom:/escenarios/demo.trg")) {
        debugf("TH64 partida: no esta rom:/escenarios/demo.trg (tools/hito6/demo.sh): no se prueba\n");
        return 0;
    }
    if (!interfaz_cargar(ANCHO, ALTO, false) || !cargar_hospital_de("demo", 1))
        return 1;
    heap_stats_t hs;
    sys_get_heap_stats(&hs);
    debugf("TH64 partida: demo.trg cargado: heap usado %d KiB de %d KiB (libres %d KiB); simulacion %u KiB\n",
           hs.used / 1024, hs.total / 1024, (hs.total - hs.used) / 1024, (unsigned)(sizeof sim / 1024));
    simulacion_iniciar(&sim, foto, (int)(sizeof foto / sizeof foto[0]));
    simulacion_girar(0);
    repartida = true;
    th_game_ui *ui = ventanas_iniciar();
    partida_empezar(&sim, ui, "demo", 1);
    const int n = (int)(sizeof ordenes_demo / sizeof ordenes_demo[0]);
    const float ms = 1000.0f / TICKS_PER_SECOND;
    int i = 0;
    const uint32_t t0 = get_ticks();
    while (sim.hours < horas) {
        // Aquí la simulación no tiene nada pedido: sim.hours es la hora del
        // tick siguiente.
        int con_orden = 0;
        for (; i < n && ordenes_demo[i].tick <= sim.hours; i++, con_orden++) {
            const th_order o = {.kind = ordenes_demo[i].kind, .x = ordenes_demo[i].x, .y = ordenes_demo[i].y,
                                .dir = ordenes_demo[i].dir, .qty = ordenes_demo[i].qty, .w = ordenes_demo[i].w,
                                .h = ordenes_demo[i].h,
                                .def = ordenes_demo[i].def ? definicion(ordenes_demo[i].def)
                                                           : (th_rnode)ordenes_demo[i].resumen};
            partida_mandar_orden(&sim, &o);
        }
        uint32_t hasta = con_orden ? sim.hours + 1 : i < n ? ordenes_demo[i].tick : horas;
        if (hasta > horas)
            hasta = horas;
        simulacion_pedir((int)(hasta - sim.hours));
        simulacion_terminar();
        if (con_orden) {
            const simulacion_stats_t *st = simulacion_stats();
            uint32_t hora;
            int cuantas;
            const char *nombres = partida_ultimo_tick(&hora, &cuantas);
            debugf("TH64 partida: orden %s en el tick %lu: %.2f ms de tick (y %.2f ms de su pasada de temperatura; "
                   "%d ordenes, tick medido %lu)\n",
                   nombres, (unsigned long)hora, st->ultimo_tick * ms, st->ultima_temperatura * ms, cuantas,
                   (unsigned long)st->hora_ultimo_tick);
        }
    }
    const uint32_t t1 = get_ticks();
    int aceptadas;
    const int aplicadas = partida_aplicadas(&aceptadas);
    const uint32_t final_horas = sim.hours;
    debugf("TH64 partida: %d ordenes de %d aplicadas, %d aceptadas; %lu horas en %.0f ms (%.3f ms por tick, %.3f el "
           "mas largo); %u pacientes\n",
           aplicadas, n, aceptadas, (unsigned long)sim.hours, (t1 - t0) * ms, (t1 - t0) * ms / horas,
           simulacion_stats()->tick_mas_largo * ms, (unsigned)sim.patients_spawned);
    // La partida de referencia del PC a 3.000 horas: th64sim partida motor/assets
    // motor/assets/escenarios/demo.trg tools/hito6/ordenes/demo.tsv 1 3000 <dir>
    // da «13 órdenes, 13 aceptadas; huella acd9c19a; saldo 36826». Si cambian
    // demo.trg, demo.tsv o la simulación, se recalculan con esa orden.
    const uint32_t huella = th_sim_hash(&sim);
    const bool huella_ok = huella == 0xacd9c19au && sim.hosp.balance == 36826;
    debugf("TH64 partida: huella %08lx y saldo %ld a %lu horas: %s (PC: acd9c19a, 36826)\n", (unsigned long)huella,
           (long)sim.hosp.balance, (unsigned long)sim.hours, huella_ok ? "iguales" : "DISTINTOS");
    partida_terminar(&sim);
    // El fotograma con la partida en marcha (sin grabar: la línea final ya está).
    // Cada velocidad con su velocidad real (ticks hechos frente a los pedidos):
    // depende de lo que cueste el tick, sobre todo la pasada de temperatura
    // (th_sim_temperature_step, que se para cuando nada cambia), así que x8 se
    // mide dos veces, justo después de x1 (como en la versión anterior) y al
    // final.
    recorrido_partida(ui, "x1", 1, 300, NULL);
    recorrido_partida(ui, "x8", 8, 150, NULL);
    recorrido_partida(ui, "x2", 2, 150, NULL);
    recorrido_partida(ui, "x4", 4, 150, NULL);
    recorrido_partida(ui, "x8 otra vez", 8, 150, NULL);
    th_game_ui_close_all(ui);
    repartida = false;
    // Además de las 13 aplicadas, las 13 aceptadas (si el núcleo las rechazase
    // todas, la partida seguiría «aplicándolas»). La huella y el saldo los
    // compara motor_check con la partida repetida en el PC (paso 3b,
    // tools/hito6/repetir.sh), y aquí con los de th64sim partida a 3.000 h.
    return aplicadas != n || aceptadas != n || final_horas != horas || !huella_ok;
}

// La demo jugable, las ventanas en la ROM (docs/06_plan.md): los estados de
// las ventanas por el camino de la partida real, en la pantalla entera,
// dibujados por el RDP y por el rasterizador de referencia, y una partida
// jugada desde las ventanas. El guion (los pasos, sus fotogramas y su orden)
// es el de ui/guion.h, el mismo que ejecuta th64sim ventanas baja en el PC.
//
// estados_ventanas copia las líneas de los estados de tools/hito6/ventanas.txt
// (sin los comentarios), y partida_ventanas la de
// tools/hito6/ventanas_partida.txt: si los ficheros cambian, hay que cambiar
// las tablas a mano (tools/motor_check.sh, paso 3b, lo comprueba).
static const char *const estados_ventanas[] = {
    "construir 60 62 0 panel:salas boton:1 px:191,57",
    "obra_plano 58 52 0 panel:salas boton:1 boton:11 baja:57,50 mueve:60,53 puntero:60,53",
    "obra_plano_malo 62 76 0 panel:salas boton:1 boton:11 baja:61,77 mueve:64,80 puntero:64,80",
    "obra_puerta 58 52 0 panel:salas boton:1 boton:11 baja:57,50 mueve:60,53 sube:60,53 boton:203 puntero:58,54",
    "obra_ventanas 58 52 0 panel:salas boton:1 boton:11 baja:57,50 mueve:60,53 sube:60,53 boton:203 clic:58,54 clic:61,51 puntero:61,52",
    "obra_objetos 58 52 0 panel:salas boton:1 boton:11 baja:57,50 mueve:60,53 sube:60,53 boton:203 clic:58,54 boton:203 tick:2 puntero:58,51",
    "amueblar 60 66 0 panel:objetos boton:160 boton:160 px:180,74",
    "colocar_valido 62 72 0 panel:objetos boton:160 boton:181 puntero:62,74",
    "colocar_invalido 62 72 0 panel:objetos boton:160 boton:181 puntero:63,79",
    "colocar_girado 62 72 1 panel:objetos boton:160 boton:181 clicd:62,74 puntero:62,74",
    "contratar_sin 60 66 0 panel:contratar px:100,60",
    "contratar 60 66 0 panel:contratar boton:300 px:100,60",
    "colocar_personal 60 58 0 panel:contratar boton:300 boton:313 puntero:59,56",
    "colocar_personal_mal 62 76 0 panel:contratar boton:300 boton:313 px:60,150",
    "pausa_aviso 60 66 0 pausa:1 panel:objetos boton:160 boton:181 px:140,190",
};
static const char *const partida_ventanas[] = {
    "partida 62 72 0 panel:objetos boton:160 boton:181 clic:62,74 panel:contratar boton:303 boton:313 clic:62,73 tick:47 camara:62,52 panel:salas boton:1 boton:11 baja:57,50 mueve:60,53 sube:60,53 boton:203 clic:58,54 clic:61,51 clic:61,52 boton:203 tick:1 boton:211 clic:60,50 boton:211 clicd:58,52 clicd:58,52 clic:58,52 clic:58,51 boton:203 tick:40 camara:60,60 panel:contratar boton:300 boton:313 clic:59,56 tick:2899 puntero:59,56",
};

// La colocación del recorrido «x1 con colocar» (comprobar_ventanas_partida),
// sobre la partida a las 3.000 horas: otro mostrador (el botón 181 de
// amueblar) en la mano y el puntero en una casilla libre del pasillo, abajo a
// la derecha de la recepción, donde cabe (el fantasma sale azul; en th64sim
// ventanas, la línea de ventanas_partida.txt seguida de «camara:62,72» y
// estos pasos).
static const char colocar_x1[] = "colocar_x1 62 72 0 panel:objetos boton:160 boton:181 puntero:65,76";

// El anfitrión del guion en la ROM: cada fotograma pasa por ventanas_fotograma
// (th_game_ui_frame con el puntero puesto donde dice el guion, las órdenes a
// partida_mandar_orden, el texto a la barra y lo que hay que dibujar a la
// escena), y cada hora es un tick de la simulación repartida (simulacion.h)
// acabado: el resultado de las órdenes vuelve a las ventanas desde el gancho
// (partida.c, al_aplicar → th_game_ui_order_result), dentro de la corrutina.
//
// Con mando, los clics no los lleva el guion: bajan y suben en el mando
// (interfaz_inyectar_boton, en fotogramas distintos) y pasan por la pila en
// interfaz_mandos, como los de un jugador. Con trocear, la hora no se acaba
// entera: se empieza y se avanza un poco (simulacion_avanzar), así que el
// fotograma siguiente puede caer a medio tick (sim_idle = 0, la instantánea y
// las órdenes retenidas por partida_vaciar); la acaba la hora siguiente, y
// entera si hay órdenes que aplicar (el guion espera su resultado antes del
// paso siguiente, como esperaría un jugador).
typedef struct {
    th_game_ui *ui;
    uint32_t ultimo_ui, max_ui; // CPU de ventanas_fotograma: el último fotograma y el más caro
    bool mando, trocear;
    int a_medias, retenidas; // fotogramas a medio tick y órdenes mandadas en ellos
} guion_rom_t;

static int guion_fotograma(void *ctx, const th_game_ui_input *in, const th_view *v, int rot, th_game_ui_output *out)
{
    guion_rom_t *a = ctx;
    assertf(rot == giro, "el guion gira %d y la camara %d", rot, giro);
    interfaz_poner_puntero(in->px, in->py);
    interfaz_orden_t orden = {.boton = in->panel_button, .ventana = in->win, .id = in->id,
                              .raton = in->mouse, .mundo_abajo = in->world_down,
                              .mundo_arriba = in->world_up};
    if (a->mando) { // los clics, de la pila por interfaz_mandos; el panel del paso, tal cual
        joypad_inputs_t j;
        orden = interfaz_mandos(&j);
        if (in->panel_button)
            orden.boton = in->panel_button;
    }
    const bool a_medias = simulacion_en_tick();
    int cx, cy;
    const uint32_t t0 = get_ticks();
    ventanas_fotograma(a->ui, &orden, in->escape, in->paused, false, v, out, &cx, &cy);
    a->a_medias += a_medias;
    if (a_medias)
        a->retenidas += out->n_orders;
    a->ultimo_ui = get_ticks() - t0;
    if (a->ultimo_ui > a->max_ui)
        a->max_ui = a->ultimo_ui;
    return out->n_orders;
}

static void guion_hora(void *ctx)
{
    const guion_rom_t *a = ctx;
    simulacion_terminar(); // la hora anterior, si quedó a medias
    simulacion_pedir(1);
    if (!a->trocear) {
        simulacion_terminar();
        return;
    }
    partida_vaciar(&sim); // las retenidas a medio tick, con la cola del núcleo libre
    if (partida_pendientes(&sim))
        simulacion_terminar();
    else
        simulacion_avanzar(get_ticks() + TICKS_FROM_US(300));
}

static void guion_mando(void *ctx, int raton, int abajo)
{
    (void)ctx;
    interfaz_inyectar_boton(raton, abajo != 0);
}

// Un estado del guion desde demo.trg recién cargado (hora 0, semilla 1, como la
// partida nueva y th64sim ventanas): la pila y las ventanas como nuevas, la
// cámara girada como dice la línea, la partida grabando (partida_empezar; con
// nueva, con la línea «TH64 partida nueva») y los pasos hasta el fotograma del
// final. Deja en out lo de ese fotograma (lo que se dibuja). false si la línea
// no se pudo recorrer.
static bool guion_estado(th_guion *gs, guion_rom_t *a, const char *linea, bool nueva, th_game_ui_output *out)
{
    if (th_guion_linea(gs, linea))
        return false;
    // La marca del principio: motor_check (paso 3b) toma de aquí hasta la
    // línea del estado las del registro de su partida.
    debugf("TH64 demo5: estado %s: empieza%s%s\n", gs->nombre, a->mando ? ", con mando" : "",
           a->trocear ? ", con la hora troceada" : "");
    // El mapa girado del estado anterior (96 KiB) se suelta antes de cargar el
    // nivel: asset_load pide los 160 KiB del nivel de una vez y, con el giro
    // de colocar_girado aún vivo y el heap de la ROM de pruebas ya partido, el
    // estado siguiente se quedaba sin memoria.
    th_view g = gs->v;
    if (giro && !girar_camara(&g, 0))
        return false;
    if (!cargar_hospital_de("demo", 1))
        return false;
    simulacion_iniciar(&sim, foto, (int)(sizeof foto / sizeof foto[0]));
    repartida = true;
    g = gs->v;
    if (!girar_camara(&g, gs->rot))
        return false;
    simulacion_girar(gs->rot);
    if (a->ui)
        th_game_ui_close_all(a->ui);
    interfaz_reiniciar();
    a->ui = ventanas_iniciar();
    gs->g = a->ui;
    a->max_ui = a->ultimo_ui = 0;
    a->a_medias = a->retenidas = 0;
    partida_empezar(&sim, a->ui, nueva ? "demo" : NULL, 1);
    const th_guion_host host = {.ctx = a, .frame = guion_fotograma, .tick = guion_hora,
                                .mando = a->mando ? guion_mando : NULL};
    int r;
    while ((r = th_guion_paso(gs, &host, out)) > 0)
        ;
    if (r < 0)
        debugf("TH64 demo5: estado %s: %s\n", gs->nombre, gs->error);
    return r == 0;
}

// La pantalla entera del último fotograma del estado: el mapa con sus
// superposiciones y entidades y los fantasmas encima (dibujar_escena, con lo que
// dejó ventanas_fotograma en extra_*, y las tres paletas), la pila con el panel y
// la ventana y el puntero (interfaz_lista), por el RDP y por la referencia con
// las hojas del mapa y de la interfaz. El texto de la barra es el del último
// fotograma (out->message), como en th64sim ventanas: no el de
// partida_mensaje, que partida_terminar ya borró y que en la partida dura 3 s
// de reloj. Escribe la línea del estado con la huella de la lista entera
// (th_drawlist_hash; motor_check la compara con la de th64sim ventanas) y
// devuelve los píxeles distintos.
static int guion_comparar(const th_guion *gs, const guion_rom_t *a, const th_game_ui_output *out)
{
    static th_draw elementos_ui[1024], todos[4096 + 1024];
    surface_t *pantalla = display_get();
    rdpq_attach(pantalla, NULL);
    rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
    rdpq_fill_rectangle(0, 0, ANCHO, ALTO);
    medida_t m = {0};
    dibujar_escena(&gs->v, &m);
    const th_bottom_info info = {.balance = sim.hosp.balance, .day = sim.date.day, .month = sim.date.month,
                                 .reputation = sim.hosp.reputation, .rep_min = 0, .rep_max = 1000,
                                 .paused = gs->pausa, .message = out->message};
    th_drawlist lu = {.items = elementos_ui, .capacity = (int)(sizeof elementos_ui / sizeof elementos_ui[0])};
    const uint32_t t0 = get_ticks();
    const int lista_ui = interfaz_lista(&info, &lu);
    const uint32_t t1 = get_ticks();
    mapa_n64_stats_t st_ui = {0};
    interfaz_dibujar_lista(&lu, paletas.colores[PALETA_NORMAL], &st_ui);
    const uint32_t t2 = get_ticks();
    // La referencia: las dos listas seguidas, como la de th64sim ventanas.
    th_drawlist l = {.items = todos, .capacity = (int)(sizeof todos / sizeof todos[0])};
    memcpy(todos, lista.items, (size_t)lista.count * sizeof todos[0]);
    memcpy(todos + lista.count, lu.items, (size_t)lu.count * sizeof todos[0]);
    l.count = lista.count + lu.count;
    l.floor_count = lista.floor_count; // los suelos son los del mapa (th_drawlist_hash los cuenta)
    l.redraws = lista.redraws;
    const th_sheet *sheets[TH_SHEETS];
    const th_sheet *const *de_ui = interfaz_hojas();
    for (int k = 0; k < TH_SHEETS; k++)
        sheets[k] = de_ui[k];
    sheets[TH_SHEET_BLOCKS] = &sh_bloques;
    sheets[TH_SHEET_SPRITES] = &sh_sprites;
    const diferencias_t d = comparar_con_referencia("demo5", pantalla, &l, sheets);
    display_show(pantalla);
    const float ms = 1000.0f / TICKS_PER_SECOND;
    debugf("TH64 demo5: estado %s: %d elementos, lista %08lx, %d pixeles distintos, %d cargas, %d TLUT; CPU: ventanas %.3f ms, "
           "lista %.3f ms, RDP %.3f ms (ventana %d; %d del mapa y %d de interfaz; %d de %d mezclados a 1/31; "
           "ventanas %.3f ms el fotograma mas caro; %d fotogramas, %lu horas)\n",
           gs->nombre, l.count, (unsigned long)th_drawlist_hash(&l), d.distintos, m.st.cargas + st_ui.cargas, m.st.tluts + st_ui.tluts,
           a->ultimo_ui * ms, (m.lista + (t1 - t0)) * ms, (m.rdp + (t2 - t1)) * ms, th_game_ui_active(a->ui),
           lista.count, lu.count, d.mezcla_1, d.mezclas, a->max_ui * ms, gs->fotogramas, (unsigned long)gs->horas);
    return d.distintos + (lista_ui < 0); // la lista de la interfaz, entera
}

// Los 15 estados de tools/hito6/ventanas.txt contra la referencia. Devuelve los
// píxeles distintos (y los estados que no se pudieron recorrer).
static int comprobar_ventanas(th_game_ui **ui)
{
    static th_guion gs;
    guion_rom_t a = {.ui = *ui};
    th_guion_init(&gs, interfaz_pila(), interfaz_panel(), NULL, ANCHO, ALTO);
    int distintos = 0;
    const int n = (int)(sizeof estados_ventanas / sizeof estados_ventanas[0]);
    for (int i = 0; i < n; i++) {
        th_game_ui_output out;
        if (!guion_estado(&gs, &a, estados_ventanas[i], false, &out)) {
            distintos++;
            continue;
        }
        if (i == 0) {
            heap_stats_t hs;
            sys_get_heap_stats(&hs);
            debugf("TH64 demo5: demo.trg cargado con la interfaz y las ventanas: heap usado %d KiB de %d KiB "
                   "(libres %d KiB); simulacion %u KiB\n",
                   hs.used / 1024, hs.total / 1024, (hs.total - hs.used) / 1024, (unsigned)(sizeof sim / 1024));
        }
        simulacion_terminar();
        partida_terminar(&sim);
        distintos += guion_comparar(&gs, &a, &out);
        sin_extras();
    }
    *ui = a.ui;
    debugf("TH64 demo5: comparacion de las ventanas con la referencia: %d pixeles distintos en total\n", distintos);
    // El heap tras los 15 estados, para ver si algún estado deja memoria sin
    // soltar (la línea del primero está arriba).
    heap_stats_t hs;
    sys_get_heap_stats(&hs);
    debugf("TH64 demo5: tras los %d estados: heap usado %d KiB de %d KiB (libres %d KiB)\n", n, hs.used / 1024,
           hs.total / 1024, (hs.total - hs.used) / 1024);
    th_view v = vista_hospital();
    girar_camara(&v, 0);
    simulacion_girar(0);
    return distintos;
}

// La partida de tools/hito6/ventanas_partida.txt: lo de demo.tsv jugado con el
// puntero y los clics del guion, por el mismo camino que la partida real, y
// hasta las 3.000 horas. Escribe las tres líneas del contrato del registro
// (tools/motor_check.sh, paso 3b, las pasa por tools/hito6/repetir.sh) y la
// pantalla del final contra la referencia. Después, con la partida a las 3.000
// horas, mide el fotograma entero a x1 con las ventanas más caras abiertas.
// Devuelve los fallos.
static int comprobar_ventanas_partida(th_game_ui **ui)
{
    static th_guion gs;
    guion_rom_t a = {.ui = *ui};
    th_guion_init(&gs, interfaz_pila(), interfaz_panel(), NULL, ANCHO, ALTO);
    th_game_ui_output out;
    const uint32_t t0 = get_ticks();
    const bool ok = guion_estado(&gs, &a, partida_ventanas[0], true, &out);
    const uint32_t t1 = get_ticks();
    *ui = a.ui;
    if (!ok)
        return 1;
    simulacion_terminar();
    int aceptadas;
    const int aplicadas = partida_aplicadas(&aceptadas);
    const uint32_t huella = th_sim_hash(&sim);
    // La misma partida en el PC: th64sim ventanas baja motor/assets
    // motor/assets/escenarios/demo.trg tools/hito6/ventanas_partida.txt <dir>
    // da «13 órdenes, 13 aceptadas; 3000 horas; huella cc315406; saldo 36826».
    // Si cambian demo.trg, el guion, las ventanas o la simulación, se
    // recalculan con esa orden.
    const bool iguales = huella == 0xcc315406u && sim.hosp.balance == 36826 && sim.hours == 3000;
    debugf("TH64 demo5: la partida desde las ventanas: %d ordenes aplicadas, %d aceptadas; %d fotogramas, %lu horas "
           "en %.0f ms; huella %08lx, saldo %ld: %s (PC: cc315406, 36826)\n",
           aplicadas, aceptadas, gs.fotogramas, (unsigned long)sim.hours, (t1 - t0) * 1000.0f / TICKS_PER_SECOND,
           (unsigned long)huella, (long)sim.hosp.balance, iguales ? "iguales" : "DISTINTOS");
    partida_terminar(&sim);
    const int distintos = guion_comparar(&gs, &a, &out);
    sin_extras();
    debugf("TH64 demo5: la partida desde las ventanas contra la referencia: %d pixeles distintos\n", distintos);
    // Otra vez, con los clics por el mando (A o B abajo en un fotograma y
    // arriba en el siguiente, por interfaz_mandos) y la hora troceada (los
    // fotogramas a medio tick y las órdenes que partida_vaciar retiene): el
    // camino de un jugador, que tiene que dar las mismas órdenes en las
    // mismas horas (motor_check compara su registro con el de th64sim
    // ventanas, como el de la primera).
    a.mando = a.trocear = true;
    const uint32_t t2 = get_ticks();
    const bool ok_mando = guion_estado(&gs, &a, partida_ventanas[0], true, &out);
    simulacion_terminar();
    const uint32_t t3 = get_ticks();
    a.mando = a.trocear = false;
    *ui = a.ui;
    int aceptadas_mando;
    const int aplicadas_mando = partida_aplicadas(&aceptadas_mando);
    const uint32_t huella_mando = th_sim_hash(&sim);
    const bool iguales_mando = ok_mando && huella_mando == huella && sim.hosp.balance == 36826 && sim.hours == 3000;
    debugf("TH64 demo5: la partida desde las ventanas con mando y la hora troceada: %d ordenes aplicadas, %d "
           "aceptadas; %d fotogramas (%d a medio tick, con %d ordenes retenidas), %lu horas en %.0f ms; huella "
           "%08lx, saldo %ld: %s\n",
           aplicadas_mando, aceptadas_mando, gs.fotogramas, a.a_medias, a.retenidas, (unsigned long)sim.hours,
           (t3 - t2) * 1000.0f / TICKS_PER_SECOND, (unsigned long)huella_mando, (long)sim.hosp.balance,
           iguales_mando ? "iguales" : "DISTINTOS");
    partida_terminar(&sim);
    sin_extras();
    girar_camara(&gs.v, 0);
    simulacion_girar(0);
    // El fotograma entero a x1 con la partida en marcha (3 pacientes, la
    // plantilla y la consulta) y cada ventana abierta como la abriría su
    // botón del panel (th_game_ui_open, entre fotogramas: no hay obra ni lista).
    static const struct {
        int cual;
        const char *nombre;
    } abiertas[] = {{TH_GW_AMUEBLAR, "x1 con amueblar"},
                    {TH_GW_CONSTRUIR, "x1 con construir"},
                    {TH_GW_CONTRATAR, "x1 con contratar"}};
    for (unsigned i = 0; i < sizeof abiertas / sizeof abiertas[0]; i++) {
        simulacion_iniciar(&sim, foto, (int)(sizeof foto / sizeof foto[0]));
        if (th_game_ui_open(a.ui, abiertas[i].cual) != 0) {
            debugf("TH64 demo5: %s: no se abre\n", abiertas[i].nombre);
            continue;
        }
        recorrido_partida(a.ui, abiertas[i].nombre, 1, 300, NULL);
        th_game_ui_close_all(a.ui);
    }
    // Y con colocar y un objeto en la mano (la vista previa,
    // th_build_check_cell, la más cara): el mostrador comprado por el guion
    // (colocar_x1), con la cámara del guion y el puntero en el pasillo; la
    // partida graba otra vez para que la compra vuelva
    // a la ventana (partida_empezar sin la línea de partida nueva).
    simulacion_iniciar(&sim, foto, (int)(sizeof foto / sizeof foto[0]));
    partida_empezar(&sim, a.ui, NULL, 1);
    const th_guion_host host = {.ctx = &a, .frame = guion_fotograma, .tick = guion_hora};
    int r = th_guion_linea(&gs, colocar_x1);
    if (r == 0) {
        th_game_ui_close_all(a.ui);
        while ((r = th_guion_paso(&gs, &host, &out)) > 0)
            ;
    }
    simulacion_terminar();
    if (r == 0 && th_game_ui_active(a.ui) == TH_GW_COLOCAR)
        recorrido_partida(a.ui, "x1 con colocar", 1, 300, &gs.v);
    else
        debugf("TH64 demo5: x1 con colocar: no se abre (%s, ventana %d)\n", gs.error, th_game_ui_active(a.ui));
    th_game_ui_close_all(a.ui);
    partida_terminar(&sim);
    sin_extras();
    repartida = false;
    return aplicadas != 13 || aceptadas != 13 || !iguales || distintos != 0 || aplicadas_mando != 13 ||
           aceptadas_mando != 13 || !iguales_mando;
}
#endif

int main(void)
{
#ifdef TH_SIN_USB
    debug_init(DEBUG_FEATURE_LOG_EMU); // la variante de diagnóstico sin el registro por USB (vigilante.h)
#else
    debug_init(DEBUG_FEATURE_LOG_USB | DEBUG_FEATURE_LOG_EMU);
#endif
    vigilante_iniciar();
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
    // El contador de dinero, con FONT100M o, si no está, la misma fuente de
    // depuración (cargarla otra vez aborta: «Trying to load already loaded
    // font data», rdpq_font.c:170).
    rdpq_font_t *depuracion = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_text_register_font(FUENTE_DEPURACION, depuracion);
    rdpq_text_register_font(FUENTE_DINERO, dfs_ok && existe("rom:/fuentes/font100m.font64")
                                               ? rdpq_font_load("rom:/fuentes/font100m.font64")
                                               : depuracion);
#if !defined(TH_PRUEBAS) && !defined(TH_MEDIR)
    if (!expansion)
        sin_expansion(ram_kib);
#endif
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
    (void)v; // solo lo usan las pruebas
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
        distintos = comprobar_fantasmas();
        debugf("TH64 demo3: comparacion con la referencia: %d pixeles distintos en total\n", distintos);
        if (distintos)
            fallos++;
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
        // Hito 6.2: el mismo hospital en las otras tres vistas, contra la
        // referencia, y la huella de la lista de dibujo de cada una (la de
        // th64sim lista --giro=r con la misma cámara: tools/motor_check.sh).
        int distintos_giro = 0;
        for (int r = 1; r <= 3; r++) {
            th_view g = c;
            const uint32_t t0 = get_ticks();
            girar_camara(&g, r);
            const uint32_t t1 = get_ticks();
            distintos_giro += comprobar_vista("hito6.2", &g);
            debugf("TH64 hito6.2: vista %d, camara (%d,%d): lista de %d elementos, huella %08lx; mapa girado en "
                   "%.1f ms\n",
                   r, g.x, g.y, lista.count, (unsigned long)th_drawlist_hash(&lista),
                   (t1 - t0) * 1000.0f / TICKS_PER_SECOND);
            girar_camara(&g, 0);
        }
        debugf("TH64 hito6.2: comparacion de las vistas giradas con la referencia: %d pixeles distintos\n",
               distintos_giro);
        if (distintos_giro)
            fallos++;
        recorrido_hospital(c, 450);
    }
    if (assets && comprobar_interfaz())
        fallos++;
    if (assets && comprobar_partida())
        fallos++;
    // Las ventanas de la partida en la ROM: los 15 estados contra la referencia
    // y la partida jugada desde ellas (repetida en el PC por motor_check).
    if (assets && existe("rom:/escenarios/demo.trg") && interfaz_cargar(ANCHO, ALTO, false)) {
        th_game_ui *ui = NULL;
        if (comprobar_ventanas(&ui))
            fallos++;
        if (comprobar_ventanas_partida(&ui))
            fallos++;
        if (ui)
            th_game_ui_close_all(ui);
        interfaz_reiniciar();
    }
    // La pila de la corrutina de la simulación después de todo lo de arriba
    // (la partida de prueba, los estados de las ventanas y las dos partidas
    // desde ellas): motor_check avisa si pasa de la mitad.
    debugf("TH64 pruebas: pila de la simulacion %d B de %d\n", simulacion_pila_usada(), simulacion_pila());
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

    // La ROM de prueba: el menú y, desde él, lo que se elija (hito 6.2). La
    // de capturas (make HORAS=N MODO=m GIRO=r) abre directamente el modo m
    // con la cámara girada r veces.
    interfaz_mandos_iniciar();
    // La partida nueva, o sin demo.trg en la DFS el primer modo que haya.
    int modo = MODO_PARTIDA;
    for (int k = 0; k < MODOS && !modo_disponible(modo); k++)
        modo = (modo + 1) % MODOS;
#ifdef TH_MODO_INICIAL
    giro_inicial = TH_GIRO_INICIAL;
    jugar(TH_MODO_INICIAL, assets, ram_kib, &heap);
    modo = TH_MODO_INICIAL;
#endif
    for (;;) {
        modo = menu(modo, assets, ram_kib, expansion);
        jugar(modo, assets, ram_kib, &heap);
    }
}
