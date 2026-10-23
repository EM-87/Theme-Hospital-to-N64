// th64sim: el núcleo del motor en el PC, sin pantalla.
// - huella: la de la simulación de referencia, para compararla con la de la
//   ROM (tools/motor_check.sh);
// - mapa: carga un nivel convertido (TMP1), escribe su huella y, si se le da
//   el volcado del oráculo de CorsixTH (bench/scenarios/volcar_mapa.lua), lo
//   compara casilla a casilla;
// - vista: dibuja con el rasterizador de referencia (vista/raster.c) lo que se
//   ve del mapa desde una posición de cámara y lo guarda en PPM, para
//   compararlo con una captura de CorsixTH (tools/oraculo_mapa).
// Más adelante cargará un escenario y escribirá el mismo volcado que el
// oráculo de la simulación (docs/04_construccion.md).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raster.h"
#include "th/map.h"
#include "th/version.h"
#include "th_test.h"
#include "vista.h"

void (*th_test_log)(const char *fmt, ...) = NULL;

static th_map mapa;

static int uso(void)
{
    fprintf(stderr,
            "th64sim %s\n"
            "Uso: th64sim huella <semilla> <ticks>\n"
            "     th64sim mapa <nivel.tmp> <número de nivel> [mapa_nivelN.tsv]\n"
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

static int orden_vista(int argc, char **argv)
{
    if (cargar_mapa(argv[2], atoi(argv[3])))
        return 1;
    size_t len;
    uint8_t *hoja = leer_fichero(argv[4], &len);
    th_sheet bloques;
    static uint8_t paleta[256][3];
    if (!hoja || th_sheet_init(&bloques, hoja, len) != 0 || cargar_paleta(argv[5], paleta) != 0) {
        fprintf(stderr, "no se pueden leer la hoja o la paleta\n");
        return 1;
    }
    th_view v = {.unit = atoi(argv[6]), .x = atoi(argv[7]), .y = atoi(argv[8]), .w = atoi(argv[9]),
                 .h = atoi(argv[10])};
    static th_draw elementos[8192];
    th_drawlist lista = {.items = elementos, .capacity = 8192};
    if (th_view_map(&v, &mapa, &bloques, &lista) < 0) {
        fprintf(stderr, "la lista de dibujo no cabe\n");
        return 1;
    }
    uint8_t *rgb = malloc((size_t)v.w * v.h * 3);
    uint8_t *mezcla = malloc((size_t)v.w * v.h);
    th_raster r = {.rgb = rgb, .mezcla = mezcla, .w = v.w, .h = v.h, .paleta = (const uint8_t(*)[3])paleta};
    th_raster_clear(&r, 0, 0, 0);
    th_raster_draw(&r, &bloques, &lista);
    FILE *f = fopen(argv[11], "wb");
    if (!f)
        return 1;
    fprintf(f, "P6\n%d %d\n255\n", v.w, v.h);
    fwrite(rgb, 3, (size_t)v.w * v.h, f);
    fclose(f);
    if (argc > 12 && (f = fopen(argv[12], "wb")) != NULL) {
        for (int i = 0; i < v.w * v.h; i++)
            mezcla[i] = mezcla[i] ? 255 : 0;
        fprintf(f, "P5\n%d %d\n255\n", v.w, v.h);
        fwrite(mezcla, 1, (size_t)v.w * v.h, f);
        fclose(f);
    }
    int sombras = 0;
    for (int i = 0; i < lista.count; i++)
        sombras += (lista.items[i].flags & TH_DRAW_ALPHA25) != 0;
    printf("vista (%d, %d) %dx%d, unidad %d: %d sprites (%d de suelo y sombras, %d sombras)\n", v.x, v.y, v.w, v.h,
           v.unit, lista.count, lista.floor_count, sombras);
    free(rgb);
    free(mezcla);
    free(hoja);
    return 0;
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

int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[1], "huella") == 0) {
        uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 0);
        int ticks = atoi(argv[3]);
        printf("%08lx\n", (unsigned long)th_golden_sim(seed, ticks));
        return 0;
    }
    if ((argc == 4 || argc == 5) && strcmp(argv[1], "mapa") == 0)
        return orden_mapa(argc, argv);
    if ((argc == 12 || argc == 13) && strcmp(argv[1], "vista") == 0)
        return orden_vista(argc, argv);
    return uso();
}
