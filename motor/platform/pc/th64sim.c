// th64sim: el núcleo del motor en el PC, sin pantalla.
// - huella: la de la simulación de referencia, para compararla con la de la
//   ROM (tools/motor_check.sh);
// - mapa: carga un nivel convertido (TMP1), escribe su huella y, si se le da
//   el volcado del oráculo de CorsixTH (bench/scenarios/volcar_mapa.lua, o
//   celdas_nivel.lua con --comparar), lo compara casilla a casilla;
// - caminos: repite con el A* del núcleo los caminos que pidió el oráculo de
//   CorsixTH (bench/scenarios/caminos.lua) y los compara casilla a casilla;
// - caminata: repite la caminata que volcó el oráculo de CorsixTH
//   (bench/scenarios/caminata.lua) y la compara tick a tick;
// - vista: dibuja con el rasterizador de referencia (vista/raster.c) lo que se
//   ve del mapa desde una posición de cámara y lo guarda en PPM, para
//   compararlo con una captura de CorsixTH (tools/oraculo_mapa);
// - reglas: vuelca un fichero de reglas (TRG1) en el JSON canónico del
//   conversor, para comprobar el lector del núcleo (tools/conv/reglas.py);
// - escenario: carga un nivel con un escenario exportado de CorsixTH
//   (tools/oraculo_hito4) y compara sus casillas con las de CorsixTH;
// - banderas: lo mismo, pero con las banderas calculadas por el núcleo a
//   partir de las salas y los objetos (th/objects.h);
// - salas: lo mismo, pero con las salas levantadas por el núcleo como
//   UIEditRoom:finishRoom (th/build.h), capas incluidas: el oráculo de las
//   paredes del hito 6.3;
// - hito4: corre ese escenario con la simulación y escribe la traza de
//   bench/scenarios/traza_hito4.lua (hito4.c);
// - partida: lo mismo, con una lista de órdenes con tick, como las graba la
//   ROM (hito4.c; tools/hito6/repetir.sh);
// - aparicion: los puntos de aparición de un nivel recién cargado, como
//   World:calculateSpawnTiles (tools/hito6/demo.sh).
// Más adelante cargará un escenario y escribirá el mismo volcado que el
// oráculo de la simulación (docs/04_construccion.md).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "raster.h"
#include "th/anim.h"
#include "th/build.h"
#include "th/humanoid.h"
#include "th/map.h"
#include "th/objects.h"
#include "th/path.h"
#include "th/rules.h"
#include "th/scenario.h"
#include "th/sim.h"
#include "th/version.h"
#include "th_test.h"
#include "th64sim.h"
#include "objetos.h"
#include "panel_inferior.h"
#include "raster.h"
#include "vista.h"

void (*th_test_log)(const char *fmt, ...) = NULL;

static th_map mapa;
static th_pathfinder buscador;

static int uso(void)
{
    fprintf(stderr,
            "th64sim %s\n"
            "Uso: th64sim huella <semilla> <ticks>\n"
            "     th64sim mapa <nivel.tmp> <número de nivel> [mapa_nivelN.tsv | --volcar celdas.tsv | --comparar "
            "celdas.tsv]\n"
            "     th64sim escenario <nivel.tmp> <escenario.trg> <celdas.tsv>\n"
            "     th64sim banderas <assets> <escenario.trg> <celdas.tsv>\n"
            "     th64sim salas <assets> <vacio.trg> <escenario.trg> <celdas.tsv> [--volcar celdas.tsv]\n"
            "     th64sim hito4 <assets> <escenario.trg> <semilla> <horas> <dir_salida>\n"
            "     th64sim carga <assets> <escenario.trg> <semilla> <horas>\n"
            "     th64sim partida <assets> <escenario.trg> <ordenes.tsv> <semilla> <horas> <dir_salida>\n"
            "     th64sim aparicion <nivel.tmp> <número de nivel>\n"
            "     th64sim escena4 <assets> <escenario.trg> <bloques.tsh> <sprites.tsh> <animaciones.tan> <paleta> "
            "<unidad> <x> <y> <ancho> <alto> <horas> <personas 0|1> <salida.ppm> [mezcla.pgm [entidades.pgm]]\n"
            "     th64sim lista <assets> <escenario.trg> <bloques.tsh> <sprites.tsh> <animaciones.tan> <unidad> <x> <y> "
            "<ancho> <alto> <horas> <veces> [semilla]\n"
            "     th64sim caminos <nivel.tmp> <número de nivel> <caminos_nivelN.tsv>\n"
            "     th64sim caminata <nivel.tmp> <animaciones.tan> <caminata.tsv>\n"
            "     th64sim escena <nivel.tmp> <bloques.tsh> <sprites.tsh> <animaciones.tan> <paleta> <unidad> <x> <y> "
            "<ancho> <alto> <caminata.tsv> <tick> <salida.ppm> [mezcla.pgm [entidades.pgm]]\n"
            "     th64sim reglas <reglas.trg> [ruta]\n"
            "     th64sim vista <nivel.tmp> <número de nivel> <bloques.tsh> <paleta> <unidad> <x> <y> <ancho> <alto> "
            "<salida.ppm> [mezcla.pgm]\n"
            "     th64sim panel <alta|baja> <nivel.tmp> <número de nivel> <dir_hojas> <textos.trg> <capturas.txt> "
            "<dir_salida>\n"
            "     th64sim tabla_giro <salida.tsv>\n"
            "       paleta: .tpl del conversor o MPALETTE.DAT del juego (6 bits por componente)\n"
            "       mezcla.pgm: 255 donde el último sprite pintado era semitransparente (sombras)\n"
            "       --giro=r (vista, escena4 y lista): la cámara girada r × 90°, con el mismo centro; con\n"
            "         --camara-girada, x e y son ya las de la vista girada\n"
            "       --sin-sombras (vista, escena4 y lista): sin las sombras del mapa\n",
            TH64_VERSION);
    return 2;
}

static uint8_t *leer_fichero(const char *ruta, size_t *len)
{
    FILE *f = fopen(ruta, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = n > 0 ? malloc((size_t)n) : NULL;
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

// Columnas del volcado del oráculo, en su orden.
enum {
    C_X, C_Y, C_SUELO, C_NORTE, C_OESTE, C_PASSABLE, C_HOSPITAL, C_BUILDABLE, C_BN, C_BE, C_BS, C_BW,
    C_TN, C_TE, C_TS, C_TW, C_TALLN, C_TALLW, C_PARCELA, C_THOB, C_NUM
};

static const char *const nombres[C_NUM] = {
    "x", "y", "suelo", "norte", "oeste", "passable", "hospital", "buildable", "buildableNorth",
    "buildableEast", "buildableSouth", "buildableWest", "travelNorth", "travelEast", "travelSouth",
    "travelWest", "tallNorth", "tallWest", "parcela", "thob"};

static void valores(const th_tile *t, int x, int y, long v[C_NUM])
{
    static const uint32_t bits[] = {
        TH_TILE_PASSABLE, TH_TILE_HOSPITAL, TH_TILE_BUILDABLE, TH_TILE_BUILDABLE_N, TH_TILE_BUILDABLE_E,
        TH_TILE_BUILDABLE_S, TH_TILE_BUILDABLE_W, TH_TILE_TRAVEL_N, TH_TILE_TRAVEL_E, TH_TILE_TRAVEL_S,
        TH_TILE_TRAVEL_W, TH_TILE_TALL_N, TH_TILE_TALL_W};
    v[C_X] = x;
    v[C_Y] = y;
    v[C_SUELO] = t->floor;
    v[C_NORTE] = t->wall_n;
    v[C_OESTE] = t->wall_w;
    for (int i = 0; i < 13; i++)
        v[C_PASSABLE + i] = (t->flags & bits[i]) != 0;
    v[C_PARCELA] = t->parcel;
    // El primero de la lista, como getCellFlags().thob de CorsixTH.
    v[C_THOB] = t->object;
}

static int comparar(const char *ruta)
{
    FILE *f = fopen(ruta, "r");
    if (!f) {
        fprintf(stderr, "no se puede abrir %s\n", ruta);
        return 1;
    }
    char linea[512];
    if (!fgets(linea, sizeof(linea), f)) {
        fclose(f);
        return 1;
    }
    long dif[C_NUM] = {0};
    int filas = 0, casillas_dif = 0, mostradas = 0;
    while (fgets(linea, sizeof(linea), f)) {
        long o[C_NUM], v[C_NUM];
        char *p = linea;
        for (int i = 0; i < C_NUM; i++)
            o[i] = strtol(p, &p, 10);
        if (o[C_X] < 0 || o[C_X] >= TH_MAP_WIDTH || o[C_Y] < 0 || o[C_Y] >= TH_MAP_HEIGHT)
            continue;
        filas++;
        valores(th_map_tile_c(&mapa, (int)o[C_X], (int)o[C_Y]), (int)o[C_X], (int)o[C_Y], v);
        int distinta = 0;
        for (int i = C_SUELO; i < C_NUM; i++) {
            if (o[i] == v[i])
                continue;
            distinta = 1;
            dif[i]++;
            if (mostradas++ < 10)
                printf("  (%ld,%ld) %s: th64 %ld, CorsixTH %ld\n", o[C_X], o[C_Y], nombres[i], v[i], o[i]);
        }
        casillas_dif += distinta;
    }
    fclose(f);
    printf("oráculo: %d casillas; %d distintas\n", filas, casillas_dif);
    for (int i = C_SUELO; i < C_NUM; i++)
        if (dif[i])
            printf("  %-15s %5ld\n", nombres[i], dif[i]);
    return filas == TH_MAP_WIDTH * TH_MAP_HEIGHT && casillas_dif == 0 ? 0 : 1;
}

// Columnas de celdas.tsv (bench/scenarios/exportar_escenario.lua): todas las
// banderas de getCellFlags de CorsixTH, la sala, la parcela y el objeto.
static const struct {
    const char *nombre;
    uint32_t bit;
} banderas_celdas[] = {
    {"passable", TH_TILE_PASSABLE},         {"hospital", TH_TILE_HOSPITAL},
    {"buildable", TH_TILE_BUILDABLE},       {"room", TH_TILE_ROOM},
    {"doorWest", TH_TILE_DOOR_W},           {"doorNorth", TH_TILE_DOOR_N},
    {"tallWest", TH_TILE_TALL_W},           {"tallNorth", TH_TILE_TALL_N},
    {"travelNorth", TH_TILE_TRAVEL_N},      {"travelEast", TH_TILE_TRAVEL_E},
    {"travelSouth", TH_TILE_TRAVEL_S},      {"travelWest", TH_TILE_TRAVEL_W},
    {"doNotIdle", TH_TILE_DO_NOT_IDLE},     {"buildableNorth", TH_TILE_BUILDABLE_N},
    {"buildableEast", TH_TILE_BUILDABLE_E}, {"buildableSouth", TH_TILE_BUILDABLE_S},
    {"buildableWest", TH_TILE_BUILDABLE_W},
};
#define N_BANDERAS ((int)(sizeof banderas_celdas / sizeof banderas_celdas[0]))

// Valor de la columna col de la casilla (x, y); -1 si la columna no existe.
static long valor_celda(int x, int y, const char *col)
{
    const th_tile *t = th_map_tile_c(&mapa, x, y);
    if (!strcmp(col, "x"))
        return x;
    if (!strcmp(col, "y"))
        return y;
    if (!strcmp(col, "suelo"))
        return t->floor;
    if (!strcmp(col, "norte"))
        return t->wall_n;
    if (!strcmp(col, "oeste"))
        return t->wall_w;
    if (!strcmp(col, "sala"))
        return t->room;
    if (!strcmp(col, "parcela"))
        return t->parcel;
    if (!strcmp(col, "thob"))
        return t->object;
    if (!strcmp(col, "owner"))
        return th_map_parcel_owner(&mapa, t->parcel);
    for (int i = 0; i < N_BANDERAS; i++)
        if (!strcmp(col, banderas_celdas[i].nombre))
            return (t->flags & banderas_celdas[i].bit) != 0;
    return -1;
}

// Vuelca el mapa del núcleo en el formato de celdas.tsv.
static int volcar_celdas(const char *ruta)
{
    FILE *f = fopen(ruta, "w");
    if (!f)
        return 1;
    fputs("x\ty\tsuelo\tnorte\toeste", f);
    for (int i = 0; i < N_BANDERAS; i++)
        fprintf(f, "\t%s", banderas_celdas[i].nombre);
    fputs("\tsala\tparcela\tthob\n", f);
    static const char *const fijas[] = {"suelo", "norte", "oeste"}, *const finales[] = {"sala", "parcela", "thob"};
    for (int y = 0; y < TH_MAP_HEIGHT; y++)
        for (int x = 0; x < TH_MAP_WIDTH; x++) {
            fprintf(f, "%d\t%d", x, y);
            for (int i = 0; i < 3; i++)
                fprintf(f, "\t%ld", valor_celda(x, y, fijas[i]));
            for (int i = 0; i < N_BANDERAS; i++)
                fprintf(f, "\t%ld", valor_celda(x, y, banderas_celdas[i].nombre));
            for (int i = 0; i < 3; i++)
                fprintf(f, "\t%ld", valor_celda(x, y, finales[i]));
            fputc('\n', f);
        }
    fclose(f);
    return 0;
}

// Compara el mapa del núcleo con un celdas.tsv, columna a columna por nombre.
static int comparar_celdas(const char *ruta)
{
    FILE *f = fopen(ruta, "r");
    if (!f) {
        fprintf(stderr, "no se puede abrir %s\n", ruta);
        return 1;
    }
    static char linea[1024];
    char cols[32][24];
    int ncols = 0;
    if (!fgets(linea, sizeof linea, f)) {
        fclose(f);
        return 1;
    }
    for (char *tok = strtok(linea, "\t\r\n"); tok && ncols < 32; tok = strtok(NULL, "\t\r\n"))
        snprintf(cols[ncols++], sizeof cols[0], "%s", tok);
    if (ncols < 3 || strcmp(cols[0], "x") || strcmp(cols[1], "y")) {
        fclose(f);
        fprintf(stderr, "%s no empieza por x, y\n", ruta);
        return 1;
    }
    // Las columnas que el núcleo no tiene (la capa de la interfaz, «ui») no
    // se comparan; se dice cuáles son.
    int usar[32] = {0};
    for (int i = 2; i < ncols; i++) {
        usar[i] = valor_celda(0, 0, cols[i]) != -1;
        if (!usar[i])
            printf("  sin comparar: %s\n", cols[i]);
    }
    long dif[32] = {0};
    int filas = 0, distintas = 0, mostradas = 0;
    while (fgets(linea, sizeof linea, f)) {
        long v[32] = {0};
        char *p = linea;
        for (int i = 0; i < ncols; i++)
            v[i] = strtol(p, &p, 10);
        if (v[0] < 0 || v[0] >= TH_MAP_WIDTH || v[1] < 0 || v[1] >= TH_MAP_HEIGHT)
            continue;
        filas++;
        int distinta = 0;
        for (int i = 2; i < ncols; i++) {
            if (!usar[i])
                continue;
            const long nuestro = valor_celda((int)v[0], (int)v[1], cols[i]);
            if (nuestro == v[i])
                continue;
            distinta = 1;
            dif[i]++;
            if (mostradas++ < 10)
                printf("  (%ld,%ld) %s: th64 %ld, CorsixTH %ld\n", v[0], v[1], cols[i], nuestro, v[i]);
        }
        distintas += distinta;
    }
    fclose(f);
    printf("casillas: %d comparadas, %d distintas\n", filas, distintas);
    for (int i = 2; i < ncols; i++)
        if (dif[i])
            printf("  %-15s %5ld\n", cols[i], dif[i]);
    return filas == TH_MAP_WIDTH * TH_MAP_HEIGHT && distintas == 0 ? 0 : 1;
}

static int cargar_mapa(const char *ruta, int nivel)
{
    size_t len;
    uint8_t *datos = leer_fichero(ruta, &len);
    if (!datos) {
        fprintf(stderr, "no se puede leer %s\n", ruta);
        return 1;
    }
    int r = th_map_load(&mapa, datos, len, nivel);
    free(datos);
    if (r != TH_MAP_OK) {
        fprintf(stderr, "th_map_load: error %d\n", r);
        return 1;
    }
    return 0;
}

// Paleta de 8 bits desde el .tpl del conversor (RGBA5551, la de la N64) o
// desde MPALETTE.DAT (6 bits, convertida como CorsixTH: redondeo de c·255/63).
static int cargar_paleta(const char *ruta, uint8_t pal[256][3])
{
    size_t len;
    uint8_t *d = leer_fichero(ruta, &len);
    if (!d)
        return 1;
    if (len >= 8 + 512 && memcmp(d, "TPL1", 4) == 0) {
        for (int i = 0; i < 256; i++) {
            unsigned c = (unsigned)(d[8 + 2 * i] << 8 | d[9 + 2 * i]);
            unsigned r = c >> 11 & 31, g = c >> 6 & 31, b = c >> 1 & 31;
            pal[i][0] = (uint8_t)(r << 3 | r >> 2);
            pal[i][1] = (uint8_t)(g << 3 | g >> 2);
            pal[i][2] = (uint8_t)(b << 3 | b >> 2);
        }
    } else if (len >= 768) {
        for (int i = 0; i < 768; i++)
            pal[i / 3][i % 3] = (uint8_t)(((d[i] & 63) * 510 + 63) / 126);
    } else {
        free(d);
        return 1;
    }
    free(d);
    return 0;
}

// Dibuja la escena con el rasterizador de referencia y la guarda en PPM (y la
// máscara de píxeles semitransparentes en PGM, si se pide).
static int dibujar_escena(const th_view *v, const th_scene *sc, uint8_t paleta[256][3], const char *ppm,
                          const char *pgm, const char *pgm_entidades)
{
    static th_draw elementos[8192];
    th_drawlist lista = {.items = elementos, .capacity = 8192};
    if (th_view_scene(v, sc, &lista) < 0) {
        fprintf(stderr, "la lista de dibujo no cabe\n");
        return 1;
    }
    uint8_t *rgb = malloc((size_t)v->w * v->h * 3);
    uint8_t *mezcla = malloc((size_t)v->w * v->h), *hoja = malloc((size_t)v->w * v->h);
    th_raster r = {.rgb = rgb, .mezcla = mezcla, .hoja = hoja, .w = v->w, .h = v->h,
                   .paleta = (const uint8_t(*)[3])paleta};
    th_raster_clear(&r, 0, 0, 0);
    th_raster_draw(&r, sc->sheets, &lista);
    FILE *f = fopen(ppm, "wb");
    if (!f)
        return 1;
    fprintf(f, "P6\n%d %d\n255\n", v->w, v->h);
    fwrite(rgb, 3, (size_t)v->w * v->h, f);
    fclose(f);
    if (pgm && (f = fopen(pgm, "wb")) != NULL) {
        for (int i = 0; i < v->w * v->h; i++)
            mezcla[i] = mezcla[i] ? 255 : 0;
        fprintf(f, "P5\n%d %d\n255\n", v->w, v->h);
        fwrite(mezcla, 1, (size_t)v->w * v->h, f);
        fclose(f);
    }
    if (pgm_entidades && (f = fopen(pgm_entidades, "wb")) != NULL) {
        for (int i = 0; i < v->w * v->h; i++)
            hoja[i] = hoja[i] == 1 + TH_SHEET_SPRITES ? 255 : 0;
        fprintf(f, "P5\n%d %d\n255\n", v->w, v->h);
        fwrite(hoja, 1, (size_t)v->w * v->h, f);
        fclose(f);
    }
    int sombras = 0, entidades = 0;
    for (int i = 0; i < lista.count; i++) {
        sombras += (lista.items[i].flags & TH_DRAW_ALPHA25) != 0;
        entidades += lista.items[i].sheet == TH_SHEET_SPRITES;
    }
    printf("vista (%d, %d) %dx%d, unidad %d: %d sprites (%d de suelo y sombras, %d sombras, %d de entidades); "
           "%d casillas redibujadas\n",
           v->x, v->y, v->w, v->h, v->unit, lista.count, lista.floor_count, sombras, entidades, lista.redraws);
    free(rgb);
    free(mezcla);
    free(hoja);
    return 0;
}

static int cargar_hoja(const char *ruta, th_sheet *h, uint8_t **datos)
{
    size_t len;
    *datos = leer_fichero(ruta, &len);
    if (!*datos || th_sheet_init(h, *datos, len) != 0) {
        fprintf(stderr, "no se puede leer la hoja %s\n", ruta);
        return 1;
    }
    return 0;
}

// --giro=r: la cámara girada r veces 90° (hito 6.2), con el mismo punto
// del mundo en el centro de la pantalla. --sin-sombras: sin las sombras del
// mapa, para comparar las vistas giradas con CorsixTH, que las dibuja con la
// luz girada (hito 6.2b).
static int giro, camara_girada, sin_sombras;
static th_rot_map mapa_girado;

static void aplicar_giro(th_view *v, th_scene *sc)
{
    sc->no_shadows = sin_sombras;
    if (!giro)
        return;
    if (!camara_girada) { // --camara-girada: x e y ya son de la vista girada
        th_view_rotate(v, 0, giro);
        th_view_clamp(v); // como la ROM (girar_camara)
    }
    th_rot_map_update(&mapa_girado, sc->map, giro);
    sc->rot = &mapa_girado;
}

static int orden_vista(int argc, char **argv)
{
    if (cargar_mapa(argv[2], atoi(argv[3])))
        return 1;
    uint8_t *hoja;
    th_sheet bloques;
    static uint8_t paleta[256][3];
    if (cargar_hoja(argv[4], &bloques, &hoja) || cargar_paleta(argv[5], paleta) != 0)
        return 1;
    th_view v = {.unit = atoi(argv[6]), .x = atoi(argv[7]), .y = atoi(argv[8]), .w = atoi(argv[9]),
                 .h = atoi(argv[10])};
    th_scene sc = {.map = &mapa, .sheets = {&bloques, NULL}};
    aplicar_giro(&v, &sc);
    int r = dibujar_escena(&v, &sc, paleta, argv[11], argc > 12 ? argv[12] : NULL, NULL);
    free(hoja);
    return r;
}

// Hito 6.1: el mapa y el panel inferior en cada estado de las capturas de
// bench/scenarios/captura_panel.lua. Con «alta», a 640 × 480 con los gráficos
// de CorsixTH (dir_hojas: el de tools/oraculo_mapa/alta.py), para compararlo
// con sus capturas; con «baja», a 320 × 240 con los de la N64 (dir_hojas:
// motor/assets), la cámara y el puntero a la mitad y el puntero dibujado.
// Escribe panel_N.ppm y panel_N_ui.pgm (donde pintó la interfaz: 1 + la hoja,
// o 255 en los rectángulos) en dir_salida.
static int orden_panel(char **argv)
{
    const int alta = strcmp(argv[2], "alta") == 0;
    if (!alta && strcmp(argv[2], "baja") != 0)
        return uso();
    if (cargar_mapa(argv[3], atoi(argv[4])))
        return 1;
    static const char *de_alta[TH_SHEETS] = {"vblk-0", NULL, "panel02v", "mpointer", "font05v",
                                             "font16v", "font01v", "font124v", "font00v", NULL};
    static const char *de_baja[TH_SHEETS] = {"datam/mblk-0", NULL, "datam/panel02m", "datam/mpointer",
                                             "fuentes/font100m", "fuentes/font101m", "fuentes/font132m",
                                             "fuentes/font131m", "fuentes/font130m", "datam/mutton01"};
    const char *const *nombres = alta ? de_alta : de_baja;
    th_sheet hojas[TH_SHEETS];
    const th_sheet *sheets[TH_SHEETS] = {NULL};
    uint8_t *datos[TH_SHEETS] = {NULL};
    char ruta[1024];
    for (int i = 0; i < TH_SHEETS; i++) {
        if (!nombres[i])
            continue;
        snprintf(ruta, sizeof ruta, "%s/%s.tsh", argv[5], nombres[i]);
        if (cargar_hoja(ruta, &hojas[i], &datos[i]))
            return 1;
        sheets[i] = &hojas[i];
    }
    // En alta, la paleta de DATA y la de las descripciones de CorsixTH
    // (Pref01V.pal); en baja, la de la N64 para todo.
    static uint8_t paleta[256][3], pal_tooltip[256][3];
    snprintf(ruta, sizeof ruta, alta ? "%s/mpalette.dat" : "%s/paleta.tpl", argv[5]);
    if (cargar_paleta(ruta, paleta))
        return 1;
    if (alta) {
        snprintf(ruta, sizeof ruta, "%s/pref01v.pal", argv[5]);
        if (cargar_paleta(ruta, pal_tooltip))
            return 1;
    }
    size_t len;
    uint8_t *t_datos = leer_fichero(argv[6], &len);
    th_rules textos;
    if (!t_datos || th_rules_init(&textos, t_datos, len) != 0) {
        fprintf(stderr, "no se pueden leer los textos %s\n", argv[6]);
        return 1;
    }
    // Las de UIBottomPanel:_initFonts y UI; en baja, como en la ROM (interfaz.c).
    const th_ui_fonts fuentes = {
        .money = {sheets[TH_SHEET_FONT_MONEY], TH_SHEET_FONT_MONEY, 0, 0, 0},
        .date = {sheets[TH_SHEET_FONT_DATE], TH_SHEET_FONT_DATE, 0, 0, alta ? 0 : 2},
        .white = {sheets[TH_SHEET_FONT_WHITE], TH_SHEET_FONT_WHITE, 0, alta ? -2 : -1, 0},
        .pause = {sheets[TH_SHEET_FONT_PAUSE], TH_SHEET_FONT_PAUSE, 0, 0, 0},
        .tooltip = {sheets[TH_SHEET_FONT_TOOLTIP], TH_SHEET_FONT_TOOLTIP, 0, 0, 0},
    };
    FILE *f = fopen(argv[7], "r");
    if (!f)
        return 1;
    const int W = alta ? 640 : 320, H = alta ? 480 : 240, k = alta ? 1 : 2;
    static th_draw elementos[8192];
    static uint8_t rgb[640 * 480 * 3], hoja[640 * 480];
    th_raster r = {.rgb = rgb, .hoja = hoja, .w = W, .h = H, .paleta = (const uint8_t(*)[3])paleta};
    if (alta)
        r.paletas[TH_SHEET_FONT_TOOLTIP] = (const uint8_t(*)[3])pal_tooltip;
    char linea[1024], png[512];
    int n = 0;
    while (fgets(linea, sizeof linea, f)) {
        int vx, vy, dia, mes, rep, pausa, px, py, pulsado, descripcion;
        long dinero;
        if (sscanf(linea, "%511s %d %d %ld %d %d %d %d %d %d %d %d", png, &vx, &vy, &dinero, &dia, &mes, &rep, &pausa,
                   &px, &py, &pulsado, &descripcion) != 12)
            continue;
        n++;
        if (px >= 0) {
            px /= k;
            py /= k;
        }
        th_drawlist l = {.items = elementos, .capacity = 8192};
        th_view v = {.unit = 32 / k, .x = vx / k, .y = vy / k, .w = W, .h = H};
        if (th_view_map(&v, &mapa, sheets[TH_SHEET_BLOCKS], &l) < 0)
            return 1;
        th_bottom_panel bp;
        th_bottom_panel_init(&bp, alta, W, H);
        th_bottom_panel_mouse_move(&bp, px, py);
        if (pulsado)
            th_ui_mouse_down(&bp.win, TH_UI_LEFT, px, py);
        const th_bottom_info info = {.balance = (int32_t)dinero, .day = dia, .month = mes, .reputation = rep,
                                     .rep_min = 0, .rep_max = 1000, .paused = pausa};
        if (th_bottom_panel_draw(&bp, &fuentes, &info, &textos, &l) < 0)
            return 1;
        char texto[512];
        int tx, ty;
        if (descripcion && th_bottom_panel_tooltip(&bp, px, py, &info, &textos, texto, sizeof texto, &tx, &ty))
            th_font_tooltip(&fuentes.tooltip, texto, tx, ty, th_bottom_panel_tooltip_width(&bp), W, &l);
        if (!alta && px >= 0 && l.count < l.capacity)
            l.items[l.count++] = (th_draw){.sprite = 1, .sheet = TH_SHEET_POINTER, .x = (int16_t)px, .y = (int16_t)py};
        th_raster_clear(&r, 0, 0, 0);
        th_raster_draw(&r, sheets, &l);
        snprintf(ruta, sizeof ruta, "%s/panel_%d.ppm", argv[8], n);
        FILE *o = fopen(ruta, "wb");
        if (!o)
            return 1;
        fprintf(o, "P6\n%d %d\n255\n", W, H);
        fwrite(rgb, 3, (size_t)(W * H), o);
        fclose(o);
        snprintf(ruta, sizeof ruta, "%s/panel_%d_ui.pgm", argv[8], n);
        if (!(o = fopen(ruta, "wb")))
            return 1;
        int ui = 0;
        for (int i = 0; i < W * H; i++) {
            if (hoja[i] < 1 + TH_SHEET_PANEL)
                hoja[i] = 0;
            ui += hoja[i] != 0;
        }
        fprintf(o, "P5\n%d %d\n255\n", W, H);
        fwrite(hoja, 1, (size_t)(W * H), o);
        fclose(o);
        printf("panel %d: %d elementos, %d píxeles de interfaz\n", n, l.count, ui);
    }
    fclose(f);
    for (int i = 0; i < TH_SHEETS; i++)
        free(datos[i]);
    free(t_datos);
    return n ? 0 : 1;
}

static int orden_caminos(char **argv)
{
    if (cargar_mapa(argv[2], atoi(argv[3])))
        return 1;
    FILE *f = fopen(argv[4], "r");
    if (!f) {
        fprintf(stderr, "no se puede abrir %s\n", argv[4]);
        return 1;
    }
    static char linea[65536];
    static uint8_t xs[TH_PATH_NODES], ys[TH_PATH_NODES];
    int pares = 0, misma_longitud = 0, mismo_camino = 0, sin_camino = 0, max_long = 0;
    long long expandidos = 0;
    uint32_t max_expandidos = 0;
    while (fgets(linea, sizeof linea, f)) {
        char *p = linea;
        int x1 = (int)strtol(p, &p, 10), y1 = (int)strtol(p, &p, 10);
        int x2 = (int)strtol(p, &p, 10), y2 = (int)strtol(p, &p, 10);
        int esperada = (int)strtol(p, &p, 10);
        pares++;
        int l = th_path_find(&buscador, &mapa, x1, y1, x2, y2);
        expandidos += buscador.expanded;
        if (buscador.expanded > max_expandidos)
            max_expandidos = buscador.expanded;
        if (l != esperada) {
            if (pares - misma_longitud <= 5)
                printf("  (%d,%d)->(%d,%d): th64 %d pasos, CorsixTH %d\n", x1, y1, x2, y2, l, esperada);
            continue;
        }
        misma_longitud++;
        if (l < 0) {
            sin_camino++;
            mismo_camino++;
            continue;
        }
        if (l > max_long)
            max_long = l;
        int n = th_path_get(&buscador, xs, ys, TH_PATH_NODES);
        int igual = n == l + 1;
        for (int i = 0; igual && i < n; i++) {
            int cx = (int)strtol(p, &p, 10);
            p++; // la coma
            int cy = (int)strtol(p, &p, 10);
            igual = cx == xs[i] && cy == ys[i];
        }
        mismo_camino += igual;
        if (!igual && pares - mismo_camino <= 5)
            printf("  (%d,%d)->(%d,%d): misma longitud (%d), otro camino\n", x1, y1, x2, y2, l);
    }
    fclose(f);
    printf("caminos: %d pares; %d con la misma longitud y %d con el mismo camino que CorsixTH (%d sin camino); "
           "el más largo, %d pasos; %.1f nodos expandidos de media, %lu como máximo\n",
           pares, misma_longitud, mismo_camino, sin_camino, max_long, pares ? (double)expandidos / pares : 0.0,
           (unsigned long)max_expandidos);
    return pares > 0 && mismo_camino == pares ? 0 : 1;
}

static th_humanoid humanoide;

// Lee la cabecera de un volcado de bench/scenarios/caminata.lua, carga su
// nivel y pone al humanoide como lo puso el oráculo: en la casilla de salida,
// con las mismas capas y la orden de caminar.
static int preparar_caminata(FILE *f, const char *ruta_tmp, th_world *w, th_humanoid *h, int ruta[4])
{
    int nivel, c0, c1, c2;
    char linea[256];
    if (fscanf(f, "# nivel %d ruta %d,%d;%d,%d capas 0=%d 1=%d 2=%d\n", &nivel, &ruta[0], &ruta[1], &ruta[2],
               &ruta[3], &c0, &c1, &c2) != 8 ||
        !fgets(linea, sizeof linea, f)) {
        fprintf(stderr, "cabecera del volcado no reconocida\n");
        return -1;
    }
    if (cargar_mapa(ruta_tmp, nivel))
        return -1;
    th_humanoid_init(w, h, th_walk_anims_find("Standard Male Patient"));
    th_humanoid_set_layer(h, 0, c0);
    th_humanoid_set_layer(h, 1, c1);
    th_humanoid_set_layer(h, 2, c2);
    th_humanoid_set_tile(w, h, ruta[0], ruta[1]);
    th_humanoid_set_next_action(w, h, th_action_walk(ruta[2], ruta[3]));
    th_humanoid_queue_action(h, th_action_idle());
    return nivel;
}

static int cargar_anims(const char *ruta, th_anims *a, uint8_t **datos)
{
    size_t len;
    *datos = leer_fichero(ruta, &len);
    if (!*datos || th_anims_init(a, *datos, len) != 0) {
        fprintf(stderr, "no se pueden leer las animaciones %s\n", ruta);
        return 1;
    }
    return 0;
}

static int orden_caminata(char **argv)
{
    FILE *f = fopen(argv[4], "r");
    if (!f) {
        fprintf(stderr, "no se puede abrir %s\n", argv[4]);
        return 1;
    }
    uint8_t *tan;
    th_anims anims;
    if (cargar_anims(argv[3], &anims, &tan)) {
        fclose(f);
        return 1;
    }
    th_world w = {.map = &mapa, .pf = &buscador, .anims = &anims};
    th_humanoid *h = &humanoide;
    int ruta[4];
    int nivel = preparar_caminata(f, argv[2], &w, h, ruta);
    if (nivel < 0) {
        fclose(f);
        return 1;
    }
    char linea[256];
    int ticks = 0, iguales = 0, mostradas = 0;
    long o[8];
    while (fgets(linea, sizeof linea, f)) {
        char *p = linea;
        for (int i = 0; i < 8; i++)
            o[i] = strtol(p, &p, 10);
        for (; ticks < o[0]; ticks++)
            th_humanoid_tick(&w, h);
        long v[8] = {ticks, h->tile_x, h->tile_y, h->px, h->py, h->anim, (long)h->frame, h->anim_flags};
        int igual = memcmp(o, v, sizeof o) == 0;
        iguales += igual;
        if (!igual && mostradas++ < 5)
            printf("  tick %ld: th64 (%ld,%ld) %+ld%+ld anim %ld fot %ld band %ld; CorsixTH (%ld,%ld) %+ld%+ld anim %ld "
                   "fot %ld band %ld\n",
                   o[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], o[1], o[2], o[3], o[4], o[5], o[6], o[7]);
    }
    fclose(f);
    free(tan);
    printf("caminata del nivel %d de (%d,%d) a (%d,%d): %d de %d ticks iguales a CorsixTH\n", nivel, ruta[0], ruta[1],
           ruta[2], ruta[3], iguales, ticks + 1);
    return iguales == ticks + 1 ? 0 : 1;
}

// Repite la caminata de un volcado del oráculo hasta un tick y dibuja la
// escena (mapa y humanoide) con las hojas y animaciones que se le den.
static int orden_escena(int argc, char **argv)
{
    // escena <nivel.tmp> <bloques.tsh> <sprites.tsh> <anim.tan> <paleta> <unidad> <x> <y> <w> <h>
    //        <caminata.tsv> <tick> <salida.ppm> [mezcla.pgm]
    uint8_t *b_datos, *s_datos, *a_datos;
    th_sheet bloques, sprites;
    th_anims anims;
    static uint8_t paleta[256][3];
    if (cargar_hoja(argv[3], &bloques, &b_datos) || cargar_hoja(argv[4], &sprites, &s_datos) ||
        cargar_anims(argv[5], &anims, &a_datos) || cargar_paleta(argv[6], paleta) != 0)
        return 1;
    FILE *f = fopen(argv[12], "r");
    if (!f)
        return 1;
    th_world w = {.map = &mapa, .pf = &buscador, .anims = &anims};
    th_humanoid *h = &humanoide;
    int ruta[4];
    int nivel = preparar_caminata(f, argv[2], &w, h, ruta);
    fclose(f);
    if (nivel < 0)
        return 1;
    const int tick = atoi(argv[13]);
    for (int i = 0; i < tick; i++)
        th_humanoid_tick(&w, h);
    th_view_entity e = {.tile_x = h->tile_x, .tile_y = h->tile_y, .px = h->px, .py = h->py, .frame = h->frame,
                        .anim = h->anim, .flags = h->anim_flags, .drawing_layer = h->drawing_layer,
                        .attach_seq = h->attach_seq, .layers = h->layers, .crop = TH_VIEW_NO_CROP};
    printf("tick %d: humanoide en (%d,%d) %+d%+d, animación %u, fotograma %lu\n", tick, h->tile_x, h->tile_y, h->px,
           h->py, h->anim, (unsigned long)h->frame);
    th_view v = {.unit = atoi(argv[7]), .x = atoi(argv[8]), .y = atoi(argv[9]), .w = atoi(argv[10]),
                 .h = atoi(argv[11])};
    th_scene sc = {.map = &mapa, .sheets = {&bloques, &sprites}, .anims = &anims, .entities = &e, .entity_count = 1};
    int r = dibujar_escena(&v, &sc, paleta, argv[14], argc > 15 ? argv[15] : NULL, argc > 16 ? argv[16] : NULL);
    free(b_datos);
    free(s_datos);
    free(a_datos);
    return r;
}

static int orden_mapa(int argc, char **argv)
{
    if (cargar_mapa(argv[2], atoi(argv[3])))
        return 1;
    int sombras[3] = {0};
    for (int i = 0; i < TH_MAP_WIDTH * TH_MAP_HEIGHT; i++) {
        sombras[0] += (mapa.tiles[i].flags & TH_TILE_SHADOW_HALF) != 0;
        sombras[1] += (mapa.tiles[i].flags & TH_TILE_SHADOW_FULL) != 0;
        sombras[2] += (mapa.tiles[i].flags & TH_TILE_SHADOW_WALL) != 0;
    }
    printf("huella %08lx; %d parcelas; sombras: %d medias, %d completas, %d de pared\n",
           (unsigned long)th_map_hash(&mapa), mapa.parcel_count, sombras[0], sombras[1], sombras[2]);
    return argc > 4 ? comparar(argv[4]) : 0;
}

// th64sim aparicion <nivel.tmp> <n>: World:calculateSpawnTiles (world.lua:326-357)
// sobre el nivel recién cargado, con el buscador del núcleo: por cada borde
// (norte, este, sur y oeste, recorridos desde la esquina de World), las
// casillas desde las que se llega al hospital (isReachableFromHospital,
// th_path_reachable_from_hospital) y, de ellas, como mucho 8 repartidas:
// la de índice floor((i − 0,5) / num × #xs + 1), en enteros. Escribe la
// lista «puntos_aparicion» del escenario (JSON, casillas desde 0).
static int orden_aparicion(char **argv)
{
    if (cargar_mapa(argv[2], atoi(argv[3])))
        return 1;
    const int w = TH_MAP_WIDTH, h = TH_MAP_HEIGHT;
    static const struct {
        const char *dir;
        int ox, oy, dx, dy;
    } bordes[4] = {{"north", 0, 0, 1, 0},
                   {"east", w - 1, 0, 0, 1},
                   {"south", w - 1, h - 1, -1, 0},
                   {"west", 0, h - 1, 0, -1}};
    int primero = 1;
    putchar('[');
    for (int b = 0; b < 4; b++) {
        int xs[TH_MAP_WIDTH + TH_MAP_HEIGHT], ys[TH_MAP_WIDTH + TH_MAP_HEIGHT], n = 0;
        for (int x = bordes[b].ox, y = bordes[b].oy; x >= 0 && x < w && y >= 0 && y < h;
             x += bordes[b].dx, y += bordes[b].dy)
            if (th_path_reachable_from_hospital(&buscador, &mapa, x, y)) {
                xs[n] = x;
                ys[n] = y;
                n++;
            }
        const int num = n < 8 ? n : 8;
        for (int i = 1; i <= num; i++) {
            // floor((i − 0,5) / num × n + 1) − 1 (desde 0) = floor((2i − 1) n / 2num)
            const int k = (2 * i - 1) * n / (2 * num);
            printf("%s{\"direccion\":\"%s\",\"x\":%d,\"y\":%d}", primero ? "" : ",", bordes[b].dir, xs[k], ys[k]);
            primero = 0;
        }
    }
    puts("]");
    return 0;
}

// JSON canónico de tools/conv/reglas.lua: claves enteras como "#n", reales
// con punto o exponente.
static void json_cadena(const char *s, uint32_t n)
{
    putchar('"');
    for (uint32_t i = 0; i < n; i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\')
            printf("\\%c", c);
        else if (c == '\n')
            fputs("\\n", stdout);
        else if (c == '\t')
            fputs("\\t", stdout);
        else if (c < 0x20 || c == 0x7F)
            printf("\\u%04x", c);
        else
            putchar(c);
    }
    putchar('"');
}

static void json_valor(const th_rules *r, th_rnode n)
{
    uint32_t largo;
    switch (th_rules_type(r, n)) {
    case TH_RULES_NIL: fputs("null", stdout); break;
    case TH_RULES_FALSE: fputs("false", stdout); break;
    case TH_RULES_TRUE: fputs("true", stdout); break;
    case TH_RULES_INT: printf("%ld", (long)th_rules_int(r, n, 0)); break;
    case TH_RULES_REAL: {
        const uint64_t bits = th_rules_real_bits(r, n);
        double d;
        memcpy(&d, &bits, sizeof d);
        char txt[40];
        snprintf(txt, sizeof txt, "%.17g", d);
        fputs(txt, stdout);
        if (!strpbrk(txt, ".eE"))
            fputs(".0", stdout);
        break;
    }
    case TH_RULES_STRING: {
        const char *s = th_rules_str(r, n, &largo);
        json_cadena(s, largo);
        break;
    }
    case TH_RULES_LIST:
        putchar('[');
        for (uint32_t i = 0; i < th_rules_len(r, n); i++) {
            if (i)
                putchar(',');
            json_valor(r, th_rules_at(r, n, i));
        }
        putchar(']');
        break;
    case TH_RULES_MAP:
        putchar('{');
        for (uint32_t i = 0; i < th_rules_len(r, n); i++) {
            if (i)
                putchar(',');
            const th_rnode k = th_rules_entry_key(r, n, i);
            if (th_rules_type(r, k) == TH_RULES_INT) {
                printf("\"#%ld\"", (long)th_rules_int(r, k, 0));
            } else {
                const char *s = th_rules_str(r, k, &largo);
                json_cadena(s, largo);
            }
            putchar(':');
            json_valor(r, th_rules_entry_value(r, n, i));
        }
        putchar('}');
        break;
    }
}

// Carga un nivel y le aplica un escenario (th/scenario.h); si se da el
// celdas.tsv que exportó CorsixTH, compara las 16.384 casillas.
static int orden_escenario(char **argv)
{
    size_t len;
    uint8_t *datos = leer_fichero(argv[3], &len);
    th_rules r;
    if (!datos || th_rules_init(&r, datos, len) != 0) {
        fprintf(stderr, "%s no es un escenario válido\n", argv[3]);
        return 1;
    }
    const int nivel = th_rules_int(&r, th_rules_get(&r, r.root, "nivel"), 0);
    if (cargar_mapa(argv[2], nivel))
        return 1;
    const int n = th_scenario_apply_cells(&mapa, &r, th_rules_get(&r, r.root, "celdas"));
    printf("escenario: nivel %d, %d casillas cambiadas, %u objetos, %u salas, %u en plantilla\n", nivel, n,
           th_rules_len(&r, th_rules_get(&r, r.root, "objetos")), th_rules_len(&r, th_rules_get(&r, r.root, "salas")),
           th_rules_len(&r, th_rules_get(&r, r.root, "personal")));
    free(datos);
    return n < 0 ? 1 : comparar_celdas(argv[4]);
}

// Las capas, la bandera room y la sala de cada casilla de un celdas.tsv: lo
// que dejan en el mapa las paredes, las ventanas y markRoom. Devuelve cuántas
// casillas son de una sala, o -1.
static int aplicar_salas(const char *ruta)
{
    FILE *f = fopen(ruta, "r");
    if (!f) {
        fprintf(stderr, "no se puede abrir %s\n", ruta);
        return -1;
    }
    static char linea[1024];
    static const char *const quiero[] = {"x", "y", "suelo", "norte", "oeste", "room", "sala"};
    int col[7], ncols = 0;
    for (int k = 0; k < 7; k++)
        col[k] = -1;
    if (fgets(linea, sizeof linea, f))
        for (char *tok = strtok(linea, "\t\r\n"); tok; tok = strtok(NULL, "\t\r\n"), ncols++)
            for (int k = 0; k < 7; k++)
                if (!strcmp(tok, quiero[k]))
                    col[k] = ncols;
    for (int k = 0; k < 7; k++)
        if (col[k] < 0 || col[k] >= 32) {
            fclose(f);
            fprintf(stderr, "%s: falta la columna %s\n", ruta, quiero[k]);
            return -1;
        }
    int en_sala = 0;
    while (fgets(linea, sizeof linea, f)) {
        long v[32] = {0};
        char *p = linea;
        for (int i = 0; i < ncols && i < 32; i++)
            v[i] = strtol(p, &p, 10);
        const long x = v[col[0]], y = v[col[1]];
        if (x < 0 || x >= TH_MAP_WIDTH || y < 0 || y >= TH_MAP_HEIGHT)
            continue;
        th_tile *t = th_map_tile(&mapa, (int)x, (int)y);
        t->floor = (uint8_t)v[col[2]];
        t->wall_n = (uint8_t)v[col[3]];
        t->wall_w = (uint8_t)v[col[4]];
        t->flags = v[col[5]] ? t->flags | TH_TILE_ROOM : t->flags & ~(uint32_t)TH_TILE_ROOM;
        t->room = (uint16_t)v[col[6]];
        en_sala += v[col[5]] != 0;
    }
    fclose(f);
    return en_sala;
}

static int es_puerta(const char *id)
{
    return id && (!strcmp(id, "door") || !strcmp(id, "swing_door_left") || !strcmp(id, "swing_door_right"));
}

// Las banderas de un hospital calculadas por el núcleo (hito 6.3): carga el
// nivel, le pone las capas y las salas del celdas.tsv de CorsixTH, rehace el
// tránsito y coloca los objetos del escenario en el orden de world.entities.
// Las puertas de la entrada ya las puso th_map_load. Tras las puertas de una
// sala (finishRoom) van update_pathfinding y resetSideObjects, como al
// terminarla. Después compara las 16.384 casillas con el mismo celdas.tsv.
static int orden_banderas(char **argv)
{
    char ruta[1024];
    size_t len;
    snprintf(ruta, sizeof ruta, "%s/reglas/definiciones.trg", argv[2]);
    uint8_t *dd = leer_fichero(ruta, &len);
    th_rules defs, esc;
    if (!dd || th_rules_init(&defs, dd, len) != 0) {
        fprintf(stderr, "%s no vale\n", ruta);
        return 1;
    }
    uint8_t *de = leer_fichero(argv[3], &len);
    if (!de || th_rules_init(&esc, de, len) != 0) {
        fprintf(stderr, "%s no es un escenario válido\n", argv[3]);
        return 1;
    }
    const int nivel = th_rules_int(&esc, th_rules_get(&esc, esc.root, "nivel"), 0);
    snprintf(ruta, sizeof ruta, "%s/niveles/nivel%02d.tmp", argv[2], nivel);
    if (cargar_mapa(ruta, nivel))
        return 1;
    const int en_sala = aplicar_salas(argv[4]);
    if (en_sala < 0)
        return 1;
    th_map_update_pathfinding(&mapa);

    static th_object objs[256];
    uint32_t seq = 0;
    th_objects w = {&mapa, &buscador, &defs, objs, 0, &seq, NULL, 0};
    const th_rnode lista = th_rules_get(&esc, esc.root, "objetos");
    const th_rnode defs_obj = th_rules_get(&defs, defs.root, "objetos");
    static const char *const dirs[4] = {"north", "east", "south", "west"};
    const uint32_t n = th_rules_len(&esc, lista);
    int adoptados = 0, colocados = 0, de_borde = 0, salas = 0, errores = 0;
    for (uint32_t i = 0; i < n && w.n_obj < (int)(sizeof objs / sizeof objs[0]); i++) {
        const th_rnode o = th_rules_at(&esc, lista, i);
        const char *id = th_rules_str(&esc, th_rules_get(&esc, o, "id"), NULL);
        const char *dn = th_rules_str(&esc, th_rules_get(&esc, o, "direccion"), NULL);
        int dir = 0;
        while (dn && dir < 3 && strcmp(dn, dirs[dir]))
            dir++;
        const int x = th_rules_int(&esc, th_rules_get(&esc, o, "x"), -1);
        const int y = th_rules_int(&esc, th_rules_get(&esc, o, "y"), -1);
        th_object *ob = &objs[w.n_obj];
        memset(ob, 0, sizeof *ob);
        if (th_object_init(ob, &defs, th_rules_get(&defs, defs_obj, id ? id : ""), dir)) {
            fprintf(stderr, "objeto %s sin definición\n", id ? id : "?");
            return 1;
        }
        const int k = w.n_obj++;
        if (ob->oclass == TH_OCLASS_ENTRANCE_DOOR) {
            if (!th_map_has_thob(&mapa, x, y, ob->thob)) {
                printf("  la puerta de la entrada %s de (%d,%d) no está en el mapa\n", id, x, y);
                errores++;
            }
            th_object_adopt(&w, k, x, y);
            adoptados++;
            continue;
        }
        th_object_set_tile(&w, k, x, y);
        colocados++;
        de_borde += ob->oclass == TH_OCLASS_SIDE;
        const th_rnode sig = i + 1 < n ? th_rules_at(&esc, lista, i + 1) : 0;
        if (es_puerta(id) && !es_puerta(th_rules_str(&esc, th_rules_get(&esc, sig, "id"), NULL))) {
            th_map_update_pathfinding(&mapa);
            th_objects_reset_side(&w);
            salas++;
        }
    }
    if (w.n_obj < (int)n) {
        fprintf(stderr, "demasiados objetos: %u\n", n);
        return 1;
    }
    th_map_update_shadows(&mapa);
    printf("banderas: nivel %d; %d casillas de sala, %d salas terminadas; %u objetos: %d de la entrada, %d colocados "
           "(%d de borde); huella del mapa %08lx\n",
           nivel, en_sala, salas, n, adoptados, colocados, de_borde, (unsigned long)th_map_hash(&mapa));
    if (mapa.thob_lost) {
        printf("  %u objetos no cupieron en la lista de su casilla\n", (unsigned)mapa.thob_lost);
        errores++;
    }
    free(dd);
    free(de);
    const int r = comparar_celdas(argv[4]);
    return r || errores;
}

// --- th64sim salas: el oráculo de las paredes (hito 6.3, paso 3)
//
// salas <assets> <vacio.trg> <escenario.trg> <celdas.tsv> [--volcar fichero]
//
// El núcleo levanta las salas de un hospital exportado de CorsixTH igual que
// UIEditRoom:finishRoom (th_build_rebuild_room: las paredes por tramos con
// los juegos de paredes de las reglas, check_external_window, las puertas
// como objetos, markRoom y resetSideObjects) y las 16.384 casillas se comparan
// con TODAS las columnas de celdas.tsv, las capas (suelo, norte y oeste)
// incluidas; la orden «banderas» las copiaba del volcado.
//   - vacio.trg es el mismo escenario sin «salas», sin «listas», sin personal
//     y solo con los objetos que se crean desde el mapa (las puertas de la
//     entrada, supports_creation_for_map): th64sim_cargar_escenario deja la
//     simulación con el nivel recién cargado, los dueños de las parcelas y
//     nada más. Se comprueba que el inicio es eso: el nivel con esas parcelas
//     (todavía no tiene que coincidir con celdas.tsv).
//   - Después se recorren los objetos de escenario.trg en su orden (el de
//     world.entities, que es el de construcción). Una puerta (door o
//     swing_door_right; la hoja esclava se salta, la crea la sala) es la sala
//     cuya «puerta» está en su casilla: de la puerta sale el borde (la casilla
//     de dentro y el lado) y de las capas de celdas.tsv las ventanas, y
//     th_build_rebuild_room la levanta. Los demás objetos entran con
//     th_sim_new_object (World:newObject → Object:setTile), como los colocó
//     el jugador, y así resetSideObjects cae donde en CorsixTH: al terminar
//     cada sala, antes de sus objetos.

// Las capas de pared de celdas.tsv (0 = norte, 1 = oeste), para las ventanas.
static uint8_t capas_tsv[2][TH_MAP_WIDTH * TH_MAP_HEIGHT];

static int leer_capas(const char *ruta)
{
    FILE *f = fopen(ruta, "r");
    if (!f) {
        fprintf(stderr, "no se puede abrir %s\n", ruta);
        return 1;
    }
    static char linea[1024];
    static const char *const quiero[4] = {"x", "y", "norte", "oeste"};
    int col[4] = {-1, -1, -1, -1}, ncols = 0;
    if (fgets(linea, sizeof linea, f))
        for (char *tok = strtok(linea, "\t\r\n"); tok; tok = strtok(NULL, "\t\r\n"), ncols++)
            for (int k = 0; k < 4; k++)
                if (!strcmp(tok, quiero[k]))
                    col[k] = ncols;
    for (int k = 0; k < 4; k++)
        if (col[k] < 0 || col[k] >= 32) {
            fclose(f);
            fprintf(stderr, "%s: falta la columna %s\n", ruta, quiero[k]);
            return 1;
        }
    memset(capas_tsv, 0, sizeof capas_tsv);
    int filas = 0;
    while (fgets(linea, sizeof linea, f)) {
        long v[32] = {0};
        char *p = linea;
        for (int i = 0; i < ncols && i < 32; i++)
            v[i] = strtol(p, &p, 10);
        const long x = v[col[0]], y = v[col[1]];
        if (x < 0 || x >= TH_MAP_WIDTH || y < 0 || y >= TH_MAP_HEIGHT)
            continue;
        // Las capas van sin las banderas de dibujo del byte alto (escenario.py
        // rechaza los volcados que las llevan).
        capas_tsv[0][y * TH_MAP_WIDTH + x] = (uint8_t)v[col[2]];
        capas_tsv[1][y * TH_MAP_WIDTH + x] = (uint8_t)v[col[3]];
        filas++;
    }
    fclose(f);
    if (filas != TH_MAP_WIDTH * TH_MAP_HEIGHT) {
        fprintf(stderr, "%s: %d casillas, no %d\n", ruta, filas, TH_MAP_WIDTH * TH_MAP_HEIGHT);
        return 1;
    }
    return 0;
}

// Un bloque de pared es ventana si está en window_tiles de algún juego de
// paredes (defs.paredes): el de la sala o el de una vecina con otro juego
// (finishRoom respeta la ventana del vecino: existing == "window_tiles").
static int es_ventana(const th_rules *defs, int block)
{
    if (!block)
        return 0;
    const th_rnode sets = th_rules_get(defs, defs->root, "paredes");
    for (uint32_t i = 0; i < th_rules_len(defs, sets); i++) {
        const th_rnode grp = th_rules_get(defs, th_rules_entry_value(defs, sets, i), "window_tiles");
        for (uint32_t k = 0; k < th_rules_len(defs, grp); k++)
            if (th_rules_int(defs, th_rules_entry_value(defs, grp, k), -1) == block)
                return 1;
    }
    return 0;
}

// Las ventanas de la sala (rx, ry, w, h) deducidas de las capas finales: los
// tramos del borde que admite er:screenToWall (sin las esquinas: x en
// rx+1..rx+w-2 al norte y al sur, y en ry+1..ry+h-2 al oeste y al este) cuya
// pared es una ventana. El tramo de cada lado es el de
// doorWallOffsetCalculations: al norte, la capa norte de la casilla de
// dentro; al sur, la capa norte de la casilla de abajo (ry + h); al oeste, la
// capa oeste de la de dentro; al este, la capa oeste de la casilla de la
// derecha (rx + w). Devuelve cuántas hay, aunque solo guarde las cap primeras.
static int ventanas_de(const th_rules *defs, int rx, int ry, int w, int h, th_edge *out, int cap)
{
    int n = 0;
#define VENTANA(X, Y, LADO)                                                                                            \
    do {                                                                                                               \
        if (n < cap)                                                                                                   \
            out[n] = (th_edge){(uint8_t)(X), (uint8_t)(Y), LADO};                                                      \
        n++;                                                                                                           \
    } while (0)
    for (int i = 1; i <= w - 2; i++) {
        const int x = rx + i;
        if (es_ventana(defs, capas_tsv[0][ry * TH_MAP_WIDTH + x]))
            VENTANA(x, ry, TH_DIR_NORTH);
        if (ry + h < TH_MAP_HEIGHT && es_ventana(defs, capas_tsv[0][(ry + h) * TH_MAP_WIDTH + x]))
            VENTANA(x, ry + h - 1, TH_DIR_SOUTH);
    }
    for (int i = 1; i <= h - 2; i++) {
        const int y = ry + i;
        if (es_ventana(defs, capas_tsv[1][y * TH_MAP_WIDTH + rx]))
            VENTANA(rx, y, TH_DIR_WEST);
        if (rx + w < TH_MAP_WIDTH && es_ventana(defs, capas_tsv[1][y * TH_MAP_WIDTH + rx + w]))
            VENTANA(rx + w - 1, y, TH_DIR_EAST);
    }
#undef VENTANA
    return n;
}

// El borde de la puerta (la casilla de dentro y su lado) desde el objeto
// puerta de CorsixTH, que en la doble es la hoja maestra (room.door): una
// puerta «north» cuya casilla es de la sala está en el lado norte y, si está
// en la fila de abajo (ry + h), en el sur; una «west», en el oeste o, en la
// columna rx + w, en el este (doorWallOffsetCalculations al revés). Devuelve
// 0 si la casilla no es un tramo del borde de esa sala.
static int borde_puerta(int rx, int ry, int w, int h, int dx, int dy, int west, th_edge *e)
{
    if (!west && dy == ry && rx <= dx && dx < rx + w)
        *e = (th_edge){(uint8_t)dx, (uint8_t)dy, TH_DIR_NORTH};
    else if (!west && dy == ry + h && rx <= dx && dx < rx + w)
        *e = (th_edge){(uint8_t)dx, (uint8_t)(dy - 1), TH_DIR_SOUTH};
    else if (west && dx == rx && ry <= dy && dy < ry + h)
        *e = (th_edge){(uint8_t)dx, (uint8_t)dy, TH_DIR_WEST};
    else if (west && dx == rx + w && ry <= dy && dy < ry + h)
        *e = (th_edge){(uint8_t)(dx - 1), (uint8_t)dy, TH_DIR_EAST};
    else
        return 0;
    return 1;
}

// El inicio del oráculo: la simulación cargada con vacio.trg tiene que ser el
// nivel recién cargado con los dueños de las parcelas que trae el escenario,
// y nada más. Devuelve cuántas casillas difieren de eso (capas, banderas,
// parcela, primer objeto y sala), o -1 si el nivel no se puede cargar.
static int comprobar_inicio(const th_sim *s, const char *assets, int nivel)
{
    char ruta[1024];
    snprintf(ruta, sizeof ruta, "%s/niveles/nivel%02d.tmp", assets, nivel);
    if (cargar_mapa(ruta, nivel))
        return -1;
    const th_map *m = s->w.map;
    for (int p = 1; p < mapa.parcel_count && p < m->parcel_count; p++)
        if (m->parcel_owner[p] != mapa.parcel_owner[p])
            th_map_set_parcel_owner(&mapa, p, m->parcel_owner[p], NULL);
    int dif = 0;
    for (int i = 0; i < TH_MAP_WIDTH * TH_MAP_HEIGHT; i++) {
        const th_tile *a = &m->tiles[i], *b = &mapa.tiles[i];
        dif += a->floor != b->floor || a->wall_n != b->wall_n || a->wall_w != b->wall_w || a->flags != b->flags ||
               a->parcel != b->parcel || a->object != b->object || a->room != b->room;
    }
    return dif;
}

// La sala del escenario cuya puerta está en (x, y), o 0.
static th_rnode sala_con_puerta(const th_rules *esc, th_rnode salas, int x, int y)
{
    for (uint32_t k = 0; k < th_rules_len(esc, salas); k++) {
        const th_rnode sa = th_rules_at(esc, salas, k), pu = th_rules_get(esc, sa, "puerta");
        if (th_rules_int(esc, th_rules_get(esc, pu, "x"), -1) == x && th_rules_int(esc, th_rules_get(esc, pu, "y"), -1) == y)
            return sa;
    }
    return 0;
}

// Las salas sin puerta del escenario (una obra cerrada sin terminar,
// UIEditRoom:close → cancel → removeRoom, sigue en world.rooms fuera del mapa
// y ocupa su número): se crean en su turno, por número, para que las demás
// reciban el suyo (#world.rooms + 1). Crea las de número menor que «hasta»
// (todas las que quedan, con hasta 0) y devuelve cuántas.
static int crear_sin_puerta(th_sim *s, const th_rules *esc, th_rnode salas, int hasta, int *errores)
{
    int creadas = 0;
    for (;;) {
        int mayor = 0;
        for (int k = 0; k < s->n_rooms; k++)
            if (s->rooms[k].num > mayor)
                mayor = s->rooms[k].num;
        th_rnode sa = 0;
        int num = 0;
        for (uint32_t k = 0; k < th_rules_len(esc, salas); k++) {
            const th_rnode c = th_rules_at(esc, salas, k);
            const int nc = th_rules_int(esc, th_rules_get(esc, c, "num"), 0);
            if (th_rules_get(esc, c, "puerta") || nc <= mayor || (hasta && nc >= hasta) || (sa && nc >= num))
                continue;
            sa = c;
            num = nc;
        }
        if (!sa)
            return creadas;
        const char *sid = th_rules_str(esc, th_rules_get(esc, sa, "id"), NULL);
        const int type = th_sim_room_type(s, sid ? sid : "");
        if (type == TH_NONE || s->n_rooms >= TH_SIM_ROOMS) {
            printf("  la sala %d (%s) sin puerta: el tipo no existe o no caben más salas (TH_SIM_ROOMS = %d)\n", num,
                   sid ? sid : "?", TH_SIM_ROOMS);
            (*errores)++;
            return creadas;
        }
        const int ri = th_sim_new_room(s, type, th_rules_int(esc, th_rules_get(esc, sa, "x"), 0),
                                       th_rules_int(esc, th_rules_get(esc, sa, "y"), 0),
                                       th_rules_int(esc, th_rules_get(esc, sa, "ancho"), 0),
                                       th_rules_int(esc, th_rules_get(esc, sa, "alto"), 0), TH_NONE, TH_NONE, TH_NONE);
        if (ri < 0 || s->rooms[ri].num != num) {
            printf("  la sala %d (%s) sin puerta quedó con el número %d\n", num, sid ? sid : "?",
                   ri < 0 ? -1 : s->rooms[ri].num);
            (*errores)++;
        }
        creadas++;
    }
}

static int orden_salas(int argc, char **argv)
{
    const char *volcar = argc == 8 && strcmp(argv[6], "--volcar") == 0 ? argv[7] : NULL;
    if (argc != 6 && !volcar)
        return uso();
    th_sim *s = th64sim_cargar_escenario(argv[2], argv[3], NULL, 1);
    if (!s)
        return 1;
    size_t len;
    uint8_t *dv = leer_fichero(argv[3], &len);
    th_rules vac, esc;
    if (!dv || th_rules_init(&vac, dv, len) != 0) {
        fprintf(stderr, "%s no es un escenario válido\n", argv[3]);
        return 1;
    }
    uint8_t *de = leer_fichero(argv[4], &len);
    if (!de || th_rules_init(&esc, de, len) != 0) {
        fprintf(stderr, "%s no es un escenario válido\n", argv[4]);
        return 1;
    }
    const th_rules *defs = s->defs;
    const int nivel = th_rules_int(&esc, th_rules_get(&esc, esc.root, "nivel"), 0);
    int errores = 0;
    // 1. El inicio: el nivel con sus parcelas.
    const uint32_t v_celdas = th_rules_len(&vac, th_rules_get(&vac, vac.root, "celdas"));
    const uint32_t v_salas = th_rules_len(&vac, th_rules_get(&vac, vac.root, "salas"));
    const uint32_t v_objetos = th_rules_len(&vac, th_rules_get(&vac, vac.root, "objetos"));
    if (v_celdas || v_salas || th_rules_int(&vac, th_rules_get(&vac, vac.root, "nivel"), 0) != nivel) {
        printf("  el escenario vacío trae %u casillas y %u salas (tiene que traer solo los objetos del nivel) o es de "
               "otro nivel\n",
               v_celdas, v_salas);
        errores++;
    }
    const int dif_inicio = comprobar_inicio(s, argv[2], nivel);
    if (dif_inicio < 0)
        return 1;
    if (dif_inicio) {
        printf("  el inicio no es el nivel %d con sus parcelas: %d casillas distintas\n", nivel, dif_inicio);
        errores++;
    }
    // 2. Las capas finales de CorsixTH, para deducir las ventanas.
    if (leer_capas(argv[5]))
        return 1;
    // 3. Los objetos del escenario, en el orden de world.entities.
    const th_rnode lista = th_rules_get(&esc, esc.root, "objetos");
    const th_rnode salas = th_rules_get(&esc, esc.root, "salas");
    const th_rnode defs_obj = th_rules_get(defs, defs->root, "objetos");
    static const char *const dirs[4] = {"north", "east", "south", "west"};
    const uint32_t n = th_rules_len(&esc, lista), n_salas = th_rules_len(&esc, salas);
    int entrada = 0, colocados = 0, de_borde = 0, rehechas = 0, puertas = 0, ventanas = 0, esclavas = 0, sin_puerta = 0;
    for (uint32_t i = 0; i < n; i++) {
        const th_rnode o = th_rules_at(&esc, lista, i);
        const char *id = th_rules_str(&esc, th_rules_get(&esc, o, "id"), NULL);
        const char *dn = th_rules_str(&esc, th_rules_get(&esc, o, "direccion"), NULL);
        int dir = 0;
        while (dn && dir < 3 && strcmp(dn, dirs[dir]))
            dir++;
        const int x = th_rules_int(&esc, th_rules_get(&esc, o, "x"), -1);
        const int y = th_rules_int(&esc, th_rules_get(&esc, o, "y"), -1);
        const th_rnode def = th_rules_get(defs, defs_obj, id ? id : "");
        th_object ob;
        memset(&ob, 0, sizeof ob);
        if (x < 0 || x >= TH_MAP_WIDTH || y < 0 || y >= TH_MAP_HEIGHT || th_object_init(&ob, defs, def, dir)) {
            fprintf(stderr, "objeto %s de (%d,%d) sin definición o fuera del mapa\n", id ? id : "?", x, y);
            return 1;
        }
        if (ob.oclass == TH_OCLASS_ENTRANCE_DOOR) {
            // Las puso th_map_load, y vacio.trg las trae: tienen que estar ya.
            if (!th_map_has_thob(s->w.map, x, y, ob.thob)) {
                printf("  la puerta de la entrada %s de (%d,%d) no está en el mapa\n", id, x, y);
                errores++;
            }
            entrada++;
            continue;
        }
        if (ob.oclass == TH_OCLASS_SWING_DOOR && !(ob.oflags & TH_OBJF_MASTER)) {
            esclavas++; // la crea finishRoom con la maestra (door2); se comprueba al final
            continue;
        }
        if (ob.oclass == TH_OCLASS_DOOR || ob.oclass == TH_OCLASS_SWING_DOOR) {
            const th_rnode sa = sala_con_puerta(&esc, salas, x, y);
            if (!sa) {
                printf("  la puerta %s de (%d,%d) no es de ninguna sala del escenario\n", id, x, y);
                errores++;
                continue;
            }
            const char *sid = th_rules_str(&esc, th_rules_get(&esc, sa, "id"), NULL);
            const int type = th_sim_room_type(s, sid ? sid : "");
            const int num = th_rules_int(&esc, th_rules_get(&esc, sa, "num"), 0);
            const int rx = th_rules_int(&esc, th_rules_get(&esc, sa, "x"), 0);
            const int ry = th_rules_int(&esc, th_rules_get(&esc, sa, "y"), 0);
            const int w = th_rules_int(&esc, th_rules_get(&esc, sa, "ancho"), 0);
            const int h = th_rules_int(&esc, th_rules_get(&esc, sa, "alto"), 0);
            th_edge door;
            if (type == TH_NONE || !borde_puerta(rx, ry, w, h, x, y, dir == TH_DIR_WEST, &door)) {
                printf("  la sala %d (%s) de (%d,%d) %dx%d: la puerta %s de (%d,%d) %s no está en su borde, o el tipo no "
                       "existe\n",
                       num, sid ? sid : "?", rx, ry, w, h, id, x, y, dirs[dir]);
                errores++;
                continue;
            }
            th_edge win[TH_SIM_WINDOWS];
            int nw = ventanas_de(defs, rx, ry, w, h, win, TH_SIM_WINDOWS);
            if (nw > TH_SIM_WINDOWS) {
                printf("  la sala %d (%s) tiene %d ventanas y el motor admite %d (TH_SIM_WINDOWS): se levantan las "
                       "primeras\n",
                       num, sid, nw, TH_SIM_WINDOWS);
                errores++;
                nw = TH_SIM_WINDOWS;
            }
            sin_puerta += crear_sin_puerta(s, &esc, salas, num, &errores);
            if (s->n_rooms >= TH_SIM_ROOMS) {
                printf("  la sala %d (%s): no caben más salas en el motor (TH_SIM_ROOMS = %d)\n", num, sid, TH_SIM_ROOMS);
                errores++;
                continue;
            }
            const int antes = s->n_rooms;
            int ri = th_build_rebuild_room(s, type, rx, ry, w, h, door, win, nw);
            // Una sala sin objetos obligatorios ni opcionales se confirma sola
            // en enterObjectsPhase y th_build_rebuild_room devuelve -1 aunque
            // la haya levantado (ninguna sala de las reglas es así): la sala
            // nueva es la última de rooms.
            if (ri < 0 && s->n_rooms == antes + 1)
                ri = antes;
            if (ri < 0 || ri >= s->n_rooms || s->n_rooms != antes + 1) {
                printf("  la sala %d (%s) de (%d,%d) %dx%d no se pudo rehacer (%d)\n", num, sid, rx, ry, w, h, ri);
                errores++;
                continue;
            }
            const th_room *r = &s->rooms[ri];
            rehechas++;
            ventanas += nw;
            puertas += (r->door != TH_NONE) + (r->door2 != TH_NONE);
            if (r->door == TH_NONE || !th_map_has_thob(s->w.map, x, y, ob.thob)) {
                printf("  la sala %d (%s): la puerta %s no quedó en (%d,%d)\n", num, sid, id, x, y);
                errores++;
            }
            if (r->num != num) {
                printf("  la sala %d (%s) quedó con el número %d\n", num, sid, r->num);
                errores++;
            }
            continue;
        }
        // Los demás objetos, como los colocó el jugador (World:newObject).
        if (!th_sim_object_fits(s, def)) {
            printf("  el objeto %s de (%d,%d) no cabe en el motor (TH_SIM_OBJECTS = %d, TH_SIM_QUEUES = %d)\n", id, x, y,
                   TH_SIM_OBJECTS, TH_SIM_QUEUES);
            errores++;
            continue;
        }
        if (th_sim_new_object(s, def, x, y, dir) == TH_NONE) {
            printf("  el objeto %s de (%d,%d) no se pudo crear\n", id, x, y);
            errores++;
            continue;
        }
        colocados++;
        de_borde += ob.oclass == TH_OCLASS_SIDE;
    }
    // Las hojas esclavas, al final: en world.entities pueden ir antes que la maestra.
    for (uint32_t i = 0; i < n; i++) {
        const th_rnode o = th_rules_at(&esc, lista, i);
        const char *id = th_rules_str(&esc, th_rules_get(&esc, o, "id"), NULL);
        th_object ob;
        memset(&ob, 0, sizeof ob);
        if (th_object_init(&ob, defs, th_rules_get(defs, defs_obj, id ? id : ""), 0) || ob.oclass != TH_OCLASS_SWING_DOOR ||
            (ob.oflags & TH_OBJF_MASTER))
            continue;
        const int x = th_rules_int(&esc, th_rules_get(&esc, o, "x"), -1);
        const int y = th_rules_int(&esc, th_rules_get(&esc, o, "y"), -1);
        if (!th_map_has_thob(s->w.map, x, y, ob.thob)) {
            printf("  la hoja esclava %s de (%d,%d) no la puso ninguna sala\n", id, x, y);
            errores++;
        }
    }
    sin_puerta += crear_sin_puerta(s, &esc, salas, 0, &errores);
    if (rehechas + sin_puerta != (int)n_salas) {
        printf("  %u salas en el escenario, %d rehechas y %d sin puerta\n", n_salas, rehechas, sin_puerta);
        errores++;
    }
    th_map_update_shadows(s->w.map);
    printf("salas: nivel %d; inicio: el nivel con sus parcelas y %u objetos del nivel (%d casillas distintas); %d salas "
           "rehechas (%d puertas, %d esclavas, %d ventanas) y %d sin puerta; %u objetos: %d de la entrada, %d colocados "
           "(%d de borde); huella del mapa %08lx\n",
           nivel, v_objetos, dif_inicio, rehechas, puertas, esclavas, ventanas, sin_puerta, n, entrada, colocados,
           de_borde, (unsigned long)th_map_hash(s->w.map));
    if (s->w.map->thob_lost) {
        printf("  %u objetos no cupieron en la lista de su casilla\n", (unsigned)s->w.map->thob_lost);
        errores++;
    }
    // Las comparaciones y el volcado leen el mapa estático de este fichero.
    memcpy(&mapa, s->w.map, sizeof mapa);
    free(dv);
    free(de);
    if (volcar && volcar_celdas(volcar)) {
        fprintf(stderr, "no se puede escribir %s\n", volcar);
        return 1;
    }
    const int r = comparar_celdas(argv[5]);
    return r || errores;
}

static int orden_reglas(int argc, char **argv)
{
    size_t len;
    uint8_t *datos = leer_fichero(argv[2], &len);
    th_rules r;
    if (!datos || th_rules_init(&r, datos, len) != 0) {
        fprintf(stderr, "%s no es un fichero de reglas válido\n", argv[2]);
        return 1;
    }
    const th_rnode n = argc > 3 ? th_rules_path(&r, r.root, argv[3]) : r.root;
    if (!n) {
        fprintf(stderr, "no existe la ruta %s\n", argv[3]);
        return 1;
    }
    json_valor(&r, n);
    putchar('\n');
    fprintf(stderr, "huella %08lx\n", (unsigned long)th_rules_hash(&r, n));
    free(datos);
    return 0;
}

// Dibuja el escenario del hito 4 tras unas horas de simulación: el mapa, los
// objetos y, si se pide, las personas.
static int orden_escena4(int argc, char **argv)
{
    // escena4 <assets> <escenario.trg> <bloques.tsh> <sprites.tsh> <anim.tan> <paleta> <unidad> <x> <y> <w> <h>
    //         <horas> <personas 0|1> <salida.ppm> [mezcla.pgm [entidades.pgm]]
    uint8_t *b_datos, *s_datos;
    th_sheet bloques, sprites;
    static uint8_t paleta[256][3];
    if (cargar_hoja(argv[4], &bloques, &b_datos) || cargar_hoja(argv[5], &sprites, &s_datos) ||
        cargar_paleta(argv[7], paleta) != 0)
        return 1;
    th_sim *s = th64sim_cargar_escenario(argv[2], argv[3], argv[6], 1);
    if (!s)
        return 1;
    const int horas = atoi(argv[13]);
    for (int i = 0; i < horas; i++)
        th_sim_tick(s);
    static th_view_entity ents[TH_VISTA_SIM_MAX];
    int n = th_vista_sim_rot(s, giro, ents, (int)(sizeof ents / sizeof ents[0]));
    if (!atoi(argv[14]))
        n -= s->n_order; // sin las personas (van al final)
    th_view v = {.unit = atoi(argv[8]), .x = atoi(argv[9]), .y = atoi(argv[10]), .w = atoi(argv[11]),
                 .h = atoi(argv[12])};
    th_scene sc = {.map = s->w.map, .sheets = {&bloques, &sprites}, .anims = s->w.anims, .entities = ents,
                   .entity_count = n};
    aplicar_giro(&v, &sc);
    int r = dibujar_escena(&v, &sc, paleta, argv[15], argc > 16 ? argv[16] : NULL, argc > 17 ? argv[17] : NULL);
    free(b_datos);
    free(s_datos);
    return r;
}

// La lista de dibujo de un escenario tras unas horas, hecha 'veces' veces:
// lo que tarda cada una y su huella (la misma que escribe la ROM de medida).
static int orden_lista(int argc, char **argv)
{
    // lista <assets> <escenario.trg> <bloques.tsh> <sprites.tsh> <anim.tan> <unidad> <x> <y> <w> <h> <horas> <veces>
    //       [semilla]
    uint8_t *b_datos, *s_datos;
    th_sheet bloques, sprites;
    if (cargar_hoja(argv[4], &bloques, &b_datos) || cargar_hoja(argv[5], &sprites, &s_datos))
        return 1;
    const uint32_t semilla = argc == 15 ? (uint32_t)strtoul(argv[14], NULL, 0) : 1;
    th_sim *s = th64sim_cargar_escenario(argv[2], argv[3], argv[6], semilla);
    if (!s)
        return 1;
    const int horas = atoi(argv[12]), veces = atoi(argv[13]);
    for (int i = 0; i < horas; i++)
        th_sim_tick(s);
    static th_view_entity ents[TH_VISTA_SIM_MAX];
    const int n = th_vista_sim_rot(s, giro, ents, (int)(sizeof ents / sizeof ents[0]));
    th_view v = {.unit = atoi(argv[7]), .x = atoi(argv[8]), .y = atoi(argv[9]), .w = atoi(argv[10]),
                 .h = atoi(argv[11])};
    th_scene sc = {.map = s->w.map, .sheets = {&bloques, &sprites}, .anims = s->w.anims, .entities = ents,
                   .entity_count = n};
    aplicar_giro(&v, &sc);
    static th_draw elementos[4096];
    th_drawlist l = {.items = elementos, .capacity = 4096};
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < veces; i++)
        if (th_view_scene(&v, &sc, &l) < 0)
            return 1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    const double ns = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / (veces ? veces : 1);
    int de_entidades = 0;
    for (int i = 0; i < l.count; i++)
        de_entidades += l.items[i].sheet == TH_SHEET_SPRITES;
    printf("lista: hora %lu, %d entidades, %d sprites (%d de entidades, %d suelos, %d redibujados); %.1f us; "
           "huella %08lx\n",
           (unsigned long)s->hours, n, l.count, de_entidades, l.floor_count, l.redraws, ns / 1000,
           (unsigned long)th_drawlist_hash(&l));
    free(b_datos);
    free(s_datos);
    return 0;
}

// Las tablas de piezas giradas (girar.h), para el oráculo de las vistas
// (bench/scenarios/captura_girada.lua): «giro bloque suelo capa pareja», con
// las banderas TH_ROT_* en los números; la pareja es la de la pared en espejo
// (th_wall_mirror), que es la misma en todos los giros.
static int orden_tabla_giro(char **argv)
{
    FILE *f = fopen(argv[2], "w");
    if (!f)
        return 1;
    for (int r = 1; r <= 3; r++)
        for (int b = 0; b < 256; b++)
            fprintf(f, "%d\t%d\t%u\t%u\t%d\n", r, b, th_rot_floor(r, b), th_rot_layer(r, b), th_wall_mirror(b));
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    // Opciones que valen para cualquier orden: se quitan de argv.
    for (int i = 1; i < argc; i++)
        if (strncmp(argv[i], "--giro=", 7) == 0 || strcmp(argv[i], "--camara-girada") == 0 ||
            strcmp(argv[i], "--sin-sombras") == 0) {
            if (argv[i][2] == 'g')
                giro = atoi(argv[i] + 7) & 3;
            else if (argv[i][2] == 'c')
                camara_girada = 1;
            else
                sin_sombras = 1;
            memmove(&argv[i], &argv[i + 1], (size_t)(argc - i) * sizeof argv[0]);
            argc--;
            i--;
        }
    if (argc == 7 && strcmp(argv[1], "hito4") == 0)
        return th64sim_hito4(argv);
    if ((argc == 5 || argc == 6) && strcmp(argv[1], "construir") == 0)
        return th64sim_construir(argc, argv);
    if (argc == 6 && strcmp(argv[1], "carga") == 0)
        return th64sim_carga(argv);
    if (argc == 8 && strcmp(argv[1], "partida") == 0)
        return th64sim_partida(argv);
    if (argc == 4 && strcmp(argv[1], "aparicion") == 0)
        return orden_aparicion(argv);
    if ((argc == 14 || argc == 15) && strcmp(argv[1], "lista") == 0)
        return orden_lista(argc, argv);
    if (argc >= 16 && argc <= 18 && strcmp(argv[1], "escena4") == 0)
        return orden_escena4(argc, argv);
    if (argc == 4 && strcmp(argv[1], "huella") == 0) {
        uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 0);
        int ticks = atoi(argv[3]);
        printf("%08lx\n", (unsigned long)th_golden_sim(seed, ticks));
        return 0;
    }
    if (argc == 6 && strcmp(argv[1], "mapa") == 0 && strcmp(argv[4], "--volcar") == 0) {
        if (cargar_mapa(argv[2], atoi(argv[3])))
            return 1;
        return volcar_celdas(argv[5]);
    }
    if (argc == 6 && strcmp(argv[1], "mapa") == 0 && strcmp(argv[4], "--comparar") == 0) {
        if (cargar_mapa(argv[2], atoi(argv[3])))
            return 1;
        return comparar_celdas(argv[5]);
    }
    if ((argc == 4 || argc == 5) && strcmp(argv[1], "mapa") == 0)
        return orden_mapa(argc, argv);
    if (argc == 5 && strcmp(argv[1], "escenario") == 0)
        return orden_escenario(argv);
    if (argc == 5 && strcmp(argv[1], "banderas") == 0)
        return orden_banderas(argv);
    if ((argc == 6 || argc == 8) && strcmp(argv[1], "salas") == 0)
        return orden_salas(argc, argv);
    if (argc >= 15 && argc <= 17 && strcmp(argv[1], "escena") == 0)
        return orden_escena(argc, argv);
    if (argc == 5 && strcmp(argv[1], "caminata") == 0)
        return orden_caminata(argv);
    if (argc == 5 && strcmp(argv[1], "caminos") == 0)
        return orden_caminos(argv);
    if ((argc == 3 || argc == 4) && strcmp(argv[1], "reglas") == 0)
        return orden_reglas(argc, argv);
    if ((argc == 12 || argc == 13) && strcmp(argv[1], "vista") == 0)
        return orden_vista(argc, argv);
    if (argc == 9 && strcmp(argv[1], "panel") == 0)
        return orden_panel(argv);
    if (argc == 3 && strcmp(argv[1], "tabla_giro") == 0)
        return orden_tabla_giro(argv);
    return uso();
}
