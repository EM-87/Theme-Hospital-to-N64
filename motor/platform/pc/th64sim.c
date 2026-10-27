// th64sim: el núcleo del motor en el PC, sin pantalla.
// - huella: la de la simulación de referencia, para compararla con la de la
//   ROM (tools/motor_check.sh);
// - mapa: carga un nivel convertido (TMP1), escribe su huella y, si se le da
//   el volcado del oráculo de CorsixTH (bench/scenarios/volcar_mapa.lua), lo
//   compara casilla a casilla;
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
// - hito4: corre ese escenario con la simulación y escribe la traza de
//   bench/scenarios/traza_hito4.lua (hito4.c).
// Más adelante cargará un escenario y escribirá el mismo volcado que el
// oráculo de la simulación (docs/04_construccion.md).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "raster.h"
#include "th/anim.h"
#include "th/humanoid.h"
#include "th/map.h"
#include "th/path.h"
#include "th/rules.h"
#include "th/scenario.h"
#include "th/sim.h"
#include "th/version.h"
#include "th_test.h"
#include "objetos.h"
#include "vista.h"

void (*th_test_log)(const char *fmt, ...) = NULL;

static th_map mapa;
static th_pathfinder buscador;

static int uso(void)
{
    fprintf(stderr,
            "th64sim %s\n"
            "Uso: th64sim huella <semilla> <ticks>\n"
            "     th64sim mapa <nivel.tmp> <número de nivel> [mapa_nivelN.tsv | --volcar celdas.tsv]\n"
            "     th64sim escenario <nivel.tmp> <escenario.trg> <celdas.tsv>\n"
            "     th64sim hito4 <assets> <escenario.trg> <semilla> <horas> <dir_salida>\n"
            "     th64sim carga <assets> <escenario.trg> <semilla> <horas>\n"
            "     th64sim escena4 <assets> <escenario.trg> <bloques.tsh> <sprites.tsh> <animaciones.tan> <paleta> "
            "<unidad> <x> <y> <ancho> <alto> <horas> <personas 0|1> <salida.ppm> [mezcla.pgm [entidades.pgm]]\n"
            "     th64sim lista <assets> <escenario.trg> <bloques.tsh> <sprites.tsh> <animaciones.tan> <unidad> <x> <y> "
            "<ancho> <alto> <horas> <veces>\n"
            "     th64sim caminos <nivel.tmp> <número de nivel> <caminos_nivelN.tsv>\n"
            "     th64sim caminata <nivel.tmp> <animaciones.tan> <caminata.tsv>\n"
            "     th64sim escena <nivel.tmp> <bloques.tsh> <sprites.tsh> <animaciones.tan> <paleta> <unidad> <x> <y> "
            "<ancho> <alto> <caminata.tsv> <tick> <salida.ppm> [mezcla.pgm [entidades.pgm]]\n"
            "     th64sim reglas <reglas.trg> [ruta]\n"
            "     th64sim vista <nivel.tmp> <número de nivel> <bloques.tsh> <paleta> <unidad> <x> <y> <ancho> <alto> "
            "<salida.ppm> [mezcla.pgm]\n"
            "       paleta: .tpl del conversor o MPALETTE.DAT del juego (6 bits por componente)\n"
            "       mezcla.pgm: 255 donde el último sprite pintado era semitransparente (sombras)\n",
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
    // Los objetos del mapa aún no se colocan: se compara el del fichero, que
    // CorsixTH solo coloca si la parcela tiene dueño.
    v[C_THOB] = th_map_parcel_owner(&mapa, t->parcel) || t->parcel == 0 ? t->object : 0;
}

// ¿Hay un objeto del mapa colocado a 3 casillas o menos? Sus huellas cambian
// banderas que este hito todavía no calcula.
static int cerca_de_objeto(int x, int y)
{
    for (int dy = -3; dy <= 3; dy++)
        for (int dx = -3; dx <= 3; dx++) {
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= TH_MAP_WIDTH || ny >= TH_MAP_HEIGHT)
                continue;
            const th_tile *t = th_map_tile_c(&mapa, nx, ny);
            if (t->object && (t->parcel == 0 || th_map_parcel_owner(&mapa, t->parcel)))
                return 1;
        }
    return 0;
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
    long dif[C_NUM] = {0}, dif_obj[C_NUM] = {0};
    int filas = 0, casillas_dif = 0, casillas_obj = 0, mostradas = 0;
    while (fgets(linea, sizeof(linea), f)) {
        long o[C_NUM], v[C_NUM];
        char *p = linea;
        for (int i = 0; i < C_NUM; i++)
            o[i] = strtol(p, &p, 10);
        if (o[C_X] < 0 || o[C_X] >= TH_MAP_WIDTH || o[C_Y] < 0 || o[C_Y] >= TH_MAP_HEIGHT)
            continue;
        filas++;
        valores(th_map_tile_c(&mapa, (int)o[C_X], (int)o[C_Y]), (int)o[C_X], (int)o[C_Y], v);
        int distinta = 0, obj = cerca_de_objeto((int)o[C_X], (int)o[C_Y]);
        for (int i = C_SUELO; i < C_NUM; i++) {
            if (o[i] == v[i])
                continue;
            distinta = 1;
            if (obj) {
                dif_obj[i]++;
            } else {
                dif[i]++;
                if (mostradas++ < 10)
                    printf("  (%ld,%ld) %s: th64 %ld, CorsixTH %ld\n", o[C_X], o[C_Y], nombres[i], v[i], o[i]);
            }
        }
        casillas_dif += distinta && !obj;
        casillas_obj += distinta && obj;
    }
    fclose(f);
    printf("oráculo: %d casillas; %d distintas lejos de objetos, %d junto a objetos del mapa\n", filas,
           casillas_dif, casillas_obj);
    for (int i = C_SUELO; i < C_NUM; i++)
        if (dif[i] || dif_obj[i])
            printf("  %-15s %5ld lejos de objetos, %5ld junto a objetos\n", nombres[i], dif[i], dif_obj[i]);
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
        return th_map_parcel_owner(&mapa, t->parcel) || t->parcel == 0 ? t->object : 0;
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
    int r = dibujar_escena(&v, &sc, paleta, argv[11], argc > 12 ? argv[12] : NULL, NULL);
    free(hoja);
    return r;
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

int th64sim_hito4(char **argv); // hito4.c
int th64sim_carga(char **argv); // hito4.c
th_sim *th64sim_cargar_escenario(const char *assets, const char *trg, const char *tan_ruta, uint32_t semilla);

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
    static th_view_entity ents[TH_SIM_OBJECTS * 3 + TH_SIM_HUMANOIDS];
    int n = th_vista_sim(s, ents, (int)(sizeof ents / sizeof ents[0]));
    if (!atoi(argv[14]))
        n -= s->n_order; // sin las personas (van al final)
    th_view v = {.unit = atoi(argv[8]), .x = atoi(argv[9]), .y = atoi(argv[10]), .w = atoi(argv[11]),
                 .h = atoi(argv[12])};
    th_scene sc = {.map = s->w.map, .sheets = {&bloques, &sprites}, .anims = s->w.anims, .entities = ents,
                   .entity_count = n};
    int r = dibujar_escena(&v, &sc, paleta, argv[15], argc > 16 ? argv[16] : NULL, argc > 17 ? argv[17] : NULL);
    free(b_datos);
    free(s_datos);
    return r;
}

// La lista de dibujo de un escenario tras unas horas, hecha 'veces' veces:
// lo que tarda cada una y su huella (la misma que escribe la ROM de medida).
static int orden_lista(char **argv)
{
    // lista <assets> <escenario.trg> <bloques.tsh> <sprites.tsh> <anim.tan> <unidad> <x> <y> <w> <h> <horas> <veces>
    uint8_t *b_datos, *s_datos;
    th_sheet bloques, sprites;
    if (cargar_hoja(argv[4], &bloques, &b_datos) || cargar_hoja(argv[5], &sprites, &s_datos))
        return 1;
    th_sim *s = th64sim_cargar_escenario(argv[2], argv[3], argv[6], 1);
    if (!s)
        return 1;
    const int horas = atoi(argv[12]), veces = atoi(argv[13]);
    for (int i = 0; i < horas; i++)
        th_sim_tick(s);
    static th_view_entity ents[TH_SIM_OBJECTS * 3 + TH_SIM_HUMANOIDS];
    const int n = th_vista_sim(s, ents, (int)(sizeof ents / sizeof ents[0]));
    th_view v = {.unit = atoi(argv[7]), .x = atoi(argv[8]), .y = atoi(argv[9]), .w = atoi(argv[10]),
                 .h = atoi(argv[11])};
    th_scene sc = {.map = s->w.map, .sheets = {&bloques, &sprites}, .anims = s->w.anims, .entities = ents,
                   .entity_count = n};
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

int main(int argc, char **argv)
{
    if (argc == 7 && strcmp(argv[1], "hito4") == 0)
        return th64sim_hito4(argv);
    if (argc == 6 && strcmp(argv[1], "carga") == 0)
        return th64sim_carga(argv);
    if (argc == 14 && strcmp(argv[1], "lista") == 0)
        return orden_lista(argv);
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
    if ((argc == 4 || argc == 5) && strcmp(argv[1], "mapa") == 0)
        return orden_mapa(argc, argv);
    if (argc == 5 && strcmp(argv[1], "escenario") == 0)
        return orden_escenario(argv);
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
    return uso();
}
