// th64sim construir <assets> <inicio.trg> <dir_corsixth> [--seguir]
//
// Hito 6.3, pasos 2 y 3: repite con el núcleo (th/build.h) la lista de órdenes
// que aplicó CorsixTH (bench/scenarios/construir.lua) y la compara orden a
// orden: aceptada o rechazada y el motivo, la fase en la que queda la obra, el
// saldo, el valor del hospital y el dinero que entra y sale, los movimientos
// de dinero (importe, motivo y cambio del valor), las 16.384 casillas (con
// todas las columnas de celdas.tsv), la lista de objetos y las salas (tras
// cada orden, contra la última foto que escribió CorsixTH: la escribe solo
// cuando cambian, así que vale hasta la siguiente).
//
// <inicio.trg> es inicio/escenario.json de CorsixTH pasado a TRG1 con
// tools/oraculo_hito4/escenario.py. Del directorio de CorsixTH se leen
// ordenes.tsv, resultados.tsv, dinero.tsv, cambios.tsv, objetos.tsv,
// salas.tsv, inicio/celdas.tsv y celdas.tsv. Las columnas se buscan por su
// nombre.
// Para en la primera orden distinta, salvo con --seguir.
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "th/build.h"
#include "th/map.h"
#include "th/objects.h"
#include "th/sim.h"
#include "th64sim.h"

#define MAX_COLS 40
#define LINEA 4096

static const char *const dir_nombres[4] = {"north", "east", "south", "west"};

// Lo que se cuenta de una orden distinta, para escribirlo tras su línea.
static char nota[8192];
static void anotar(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void anotar(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const size_t n = strlen(nota);
    vsnprintf(nota + n, sizeof nota - n, fmt, ap);
    va_end(ap);
}


// Una tabla TSV con cabecera: las columnas por nombre.
typedef struct tabla {
    FILE *f;
    char col[MAX_COLS][32];
    int n;
    char linea[LINEA];
    char *campo[MAX_COLS];
    int nc;      // campos de la fila leída
    int hay;     // hay una fila leída esperando
    long n_fila; // su n (la primera columna «n»), o -1
} tabla;

static int abrir_tabla(tabla *t, const char *dir, const char *nombre)
{
    char ruta[1024];
    snprintf(ruta, sizeof ruta, "%s/%s", dir, nombre);
    memset(t, 0, sizeof *t);
    t->f = fopen(ruta, "r");
    if (!t->f)
        return -1;
    if (!fgets(t->linea, sizeof t->linea, t->f))
        return -1;
    for (char *tok = strtok(t->linea, "\t\r\n"); tok && t->n < MAX_COLS; tok = strtok(NULL, "\t\r\n"))
        snprintf(t->col[t->n++], sizeof t->col[0], "%s", tok);
    return 0;
}

static int columna(const tabla *t, const char *nombre)
{
    for (int i = 0; i < t->n; i++)
        if (!strcmp(t->col[i], nombre))
            return i;
    return -1;
}

// Lee la fila siguiente (los campos vacíos cuentan).
static int leer_fila(tabla *t)
{
    t->hay = 0;
    if (!t->f || !fgets(t->linea, sizeof t->linea, t->f))
        return 0;
    t->linea[strcspn(t->linea, "\r\n")] = 0;
    t->nc = 0;
    char *p = t->linea;
    while (t->nc < MAX_COLS) {
        t->campo[t->nc++] = p;
        char *tab = strchr(p, '\t');
        if (!tab)
            break;
        *tab = 0;
        p = tab + 1;
    }
    const int cn = columna(t, "n");
    t->n_fila = cn >= 0 && cn < t->nc ? strtol(t->campo[cn], NULL, 10) : -1;
    t->hay = 1;
    return 1;
}

static const char *campo(const tabla *t, const char *nombre)
{
    const int c = columna(t, nombre);
    return c >= 0 && c < t->nc ? t->campo[c] : "";
}

// ---------------------------------------------------------------------------
// Las casillas

static const struct {
    const char *nombre;
    uint32_t bit;
} banderas[] = {
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

static long valor(const th_map *m, int x, int y, const char *col)
{
    const th_tile *t = th_map_tile_c(m, x, y);
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
    for (unsigned i = 0; i < sizeof banderas / sizeof banderas[0]; i++)
        if (!strcmp(col, banderas[i].nombre))
            return (t->flags & banderas[i].bit) != 0;
    return -1;
}

// El mapa de CorsixTH tras cada orden: el de partida más los cambios.
static int32_t esperado[TH_MAP_WIDTH * TH_MAP_HEIGHT][MAX_COLS];
static char col_celdas[MAX_COLS][32];
static int n_col_celdas;

static int cargar_celdas(const char *dir, const char *nombre)
{
    tabla t;
    if (abrir_tabla(&t, dir, nombre))
        return -1;
    n_col_celdas = t.n;
    memcpy(col_celdas, t.col, sizeof col_celdas);
    int filas = 0;
    while (leer_fila(&t)) {
        const int x = atoi(campo(&t, "x")), y = atoi(campo(&t, "y"));
        if (x < 0 || y < 0 || x >= TH_MAP_WIDTH || y >= TH_MAP_HEIGHT)
            continue;
        for (int c = 0; c < t.n && c < t.nc; c++)
            esperado[y * TH_MAP_WIDTH + x][c] = (int32_t)strtol(t.campo[c], NULL, 10);
        filas++;
    }
    fclose(t.f);
    return filas == TH_MAP_WIDTH * TH_MAP_HEIGHT ? 0 : -1;
}

// Compara el mapa del núcleo con el esperado; escribe las primeras diferencias.
static int comparar_mapa(const th_map *m, int mostrar)
{
    int distintas = 0, mostradas = 0;
    for (int i = 0; i < TH_MAP_WIDTH * TH_MAP_HEIGHT; i++) {
        const int x = i % TH_MAP_WIDTH, y = i / TH_MAP_WIDTH;
        int distinta = 0;
        for (int c = 0; c < n_col_celdas; c++) {
            const char *col = col_celdas[c];
            if (!strcmp(col, "x") || !strcmp(col, "y"))
                continue;
            const long v = valor(m, x, y, col);
            if (v == -1 || v == esperado[i][c])
                continue;
            distinta = 1;
            if (mostrar && mostradas++ < 10)
                anotar("    (%d,%d) %s: th64 %ld, CorsixTH %ld\n", x, y, col, v, (long)esperado[i][c]);
        }
        distintas += distinta;
    }
    return distintas;
}

// Aplica a esperado las filas de cambios.tsv de la orden n.
static void aplicar_cambios(tabla *t, long n)
{
    if (!t->f)
        return;
    if (!t->hay && !leer_fila(t))
        return;
    while (t->hay && t->n_fila <= n) {
        if (t->n_fila == n) {
            const int x = atoi(campo(t, "x")), y = atoi(campo(t, "y"));
            if (x >= 0 && y >= 0 && x < TH_MAP_WIDTH && y < TH_MAP_HEIGHT)
                for (int c = 0; c < n_col_celdas; c++)
                    esperado[y * TH_MAP_WIDTH + x][c] = (int32_t)strtol(campo(t, col_celdas[c]), NULL, 10);
        }
        if (!leer_fila(t))
            break;
    }
}

// ---------------------------------------------------------------------------
// El dinero

typedef struct movimiento {
    long importe, valor;
    char motivo[64];
} movimiento;

// Una orden mueve como mucho 99 devoluciones, lo cogido vendido y la compra
// (comprar con la ventana de colocar abierta). Si hay más, se cuentan y la
// comparación falla: no se compara a medias.
#define MOVS_MAX 256
static movimiento movs[MOVS_MAX];
static int n_movs, movs_perdidos;
static const th_sim *sim_actual;
static int32_t valor_visto; // el valor del hospital tras el último movimiento (o al empezar la orden)

static void al_mover_dinero(void *ctx, const th_sim *s, int32_t amount, int reason, int who)
{
    (void)ctx;
    if (n_movs >= MOVS_MAX) {
        movs_perdidos++;
        return;
    }
    movimiento *m = &movs[n_movs++];
    m->importe = amount;
    // Lo que cambió el valor del hospital con este movimiento (en CorsixTH,
    // el valueChange de spendMoney o receiveMoney): money() ya lo aplicó.
    m->valor = s->hosp.value - valor_visto;
    valor_visto = s->hosp.value;
    // Con hire_staff (Hospital:addStaff), who es la persona contratada, no un
    // th_rnode (th/sim.h, TH_MONEY_HIRE_STAFF): el motivo lleva su clase.
    if (reason == TH_MONEY_HIRE_STAFF) {
        snprintf(m->motivo, sizeof m->motivo, "hire_staff:%s", th64sim_clase_nombre(s, who));
        return;
    }
    const char *id = th_rules_str(s->defs, th_rules_get(s->defs, (th_rnode)who, "id"), NULL);
    snprintf(m->motivo, sizeof m->motivo, "%s:%s",
             reason == TH_MONEY_BUY_OBJECT    ? "buy_object"
             : reason == TH_MONEY_SELL_OBJECT ? "sell_object"
             : reason == TH_MONEY_BUILD_ROOM  ? "build_room"
                                              : "?",
             id ? id : "?");
    (void)sim_actual;
}

static int comparar_dinero(tabla *t, long n, int mostrar)
{
    static movimiento suyos[MOVS_MAX];
    int k = 0, suyos_total = 0;
    if (t->f && (t->hay || leer_fila(t)))
        while (t->hay && t->n_fila <= n) {
            if (t->n_fila == n) {
                suyos_total++;
                if (k < MOVS_MAX) {
                    movimiento *m = &suyos[k++];
                    m->importe = strtol(campo(t, "importe"), NULL, 10);
                    m->valor = strtol(campo(t, "valor"), NULL, 10);
                    snprintf(m->motivo, sizeof m->motivo, "%s", campo(t, "motivo"));
                }
            }
            if (!leer_fila(t))
                break;
        }
    int distintos = 0;
    // Todos los movimientos, y los mismos de cada lado.
    if (movs_perdidos || suyos_total != k || n_movs != suyos_total) {
        distintos++;
        if (mostrar)
            anotar("    movimientos: th64 %d, CorsixTH %d\n", n_movs + movs_perdidos, suyos_total);
    }
    // Como conjunto (el orden de los movimientos de una orden sale de pairs):
    // importe, motivo y cambio del valor.
    static int usados[MOVS_MAX];
    memset(usados, 0, sizeof usados);
    for (int i = 0; i < n_movs; i++) {
        int hallado = -1;
        for (int j = 0; j < k && hallado < 0; j++)
            if (!usados[j] && suyos[j].importe == movs[i].importe && suyos[j].valor == movs[i].valor &&
                !strcmp(suyos[j].motivo, movs[i].motivo))
                hallado = j;
        if (hallado >= 0)
            usados[hallado] = 1;
        else {
            distintos++;
            if (mostrar)
                anotar("    dinero: th64 %+ld %s (valor %+ld), CorsixTH no\n", movs[i].importe, movs[i].motivo,
                       movs[i].valor);
        }
    }
    for (int j = 0; j < k; j++)
        if (!usados[j]) {
            distintos++;
            if (mostrar)
                anotar("    dinero: CorsixTH %+ld %s (valor %+ld), th64 no\n", suyos[j].importe, suyos[j].motivo,
                       suyos[j].valor);
        }
    return distintos;
}

// ---------------------------------------------------------------------------
// Los objetos

// Su sitio en world.objects de su casilla: cuántos objetos y basuras de esa
// casilla tienen un seq menor.
static int sitio(const th_sim *s, int i)
{
    const th_object *o = &s->obj[i];
    int k = 0;
    for (int j = 0; j < s->n_obj; j++) {
        const th_object *p = &s->obj[j];
        if (p->def && (p->oflags & TH_OBJF_ON_MAP) && p->x == o->x && p->y == o->y && p->seq < o->seq)
            k++;
    }
    for (int j = 0; j < s->n_litter; j++)
        if (s->litter[j].x == o->x && s->litter[j].y == o->y && s->litter[j].seq < o->seq)
            k++;
    return k;
}

// Un objeto del núcleo como lo escribe la batería: id x,y dirección sala recogido.
static void describir_objeto(const th_sim *s, int i, char *out, size_t tam)
{
    const th_object *o = &s->obj[i];
    const char *id = th_rules_str(s->defs, th_rules_get(s->defs, o->def, "id"), NULL);
    snprintf(out, tam, "%s %d,%d %s sala %d recogido %d", id ? id : "?", o->x, o->y, dir_nombres[o->dir & 3], o->room,
             (o->oflags & TH_OBJF_PICKED_UP) != 0);
}

#define FILAS_OBJETOS 512

// La última foto de objetos.tsv: CorsixTH la escribe solo cuando su lista
// cambia, así que vale hasta la siguiente y se compara en todas las órdenes.
static char foto_obj[FILAS_OBJETOS][160];
static int foto_sitios[FILAS_OBJETOS];
static int foto_obj_n = -1; // filas (-1: CorsixTH no ha escrito ninguna foto)

static int comparar_objetos(const th_sim *s, tabla *t, long n, int mostrar)
{
    // Si CorsixTH escribió una foto en esta orden, es la nueva.
    if (t->f && (t->hay || leer_fila(t))) {
        while (t->hay && t->n_fila < n)
            if (!leer_fila(t))
                break;
        if (t->hay && t->n_fila == n) {
            foto_obj_n = 0;
            while (t->hay && t->n_fila == n) {
                if (foto_obj_n < FILAS_OBJETOS) {
                    snprintf(foto_obj[foto_obj_n], sizeof foto_obj[0], "%s %s,%s %s sala %s recogido %s", campo(t, "id"),
                             campo(t, "x"), campo(t, "y"), campo(t, "direccion"), campo(t, "sala"), campo(t, "recogido"));
                    const char *orden = campo(t, "orden");
                    foto_sitios[foto_obj_n] = *orden ? atoi(orden) : -1;
                }
                foto_obj_n++;
                if (!leer_fila(t))
                    break;
            }
        }
    }
    if (foto_obj_n < 0)
        return 0; // antes de la primera foto, los dos lados siguen en el inicio
    const int ns = foto_obj_n;
    // Los objetos vivos del núcleo, en el orden de world.entities (el de alta,
    // entity_seq: el índice no vale, los huecos se reutilizan).
    int nuestros[TH_SIM_OBJECTS], nn = 0;
    for (int i = 0; i < s->n_obj; i++)
        if (s->obj[i].def && (s->obj[i].oflags & TH_OBJF_ON_MAP)) {
            int j = nn++;
            while (j > 0 && s->obj[nuestros[j - 1]].entity_seq > s->obj[i].entity_seq) {
                nuestros[j] = nuestros[j - 1];
                j--;
            }
            nuestros[j] = i;
        }
    // Contra las filas de la foto (world.entities), con su sitio en la lista
    // de su casilla («orden»; -1 si la columna no está).
    int distintos = 0;
    for (int k = 0; k < nn || k < ns; k++) {
        char mio[160] = "(nada)";
        const char *suyo = k < ns && k < FILAS_OBJETOS ? foto_obj[k] : "(nada)";
        if (k < nn)
            describir_objeto(s, nuestros[k], mio, sizeof mio);
        int igual = !strcmp(suyo, mio);
        // Las dos hojas de una puerta doble van seguidas en world.entities,
        // en el orden en que pairs(blueprint_wall_anims) las recorre en
        // er:finishRoom: el de la tabla de Lua (ltable.c), que depende de las
        // coordenadas (la clave módulo el tamaño de la tabla). El núcleo no
        // copia ese accidente: la pareja se compara sin orden.
        if (!igual && k + 1 < nn && k + 1 < ns && k + 1 < FILAS_OBJETOS &&
            s->obj[nuestros[k]].oclass == TH_OCLASS_SWING_DOOR && s->obj[nuestros[k + 1]].oclass == TH_OCLASS_SWING_DOOR) {
            char otro[160];
            describir_objeto(s, nuestros[k + 1], otro, sizeof otro);
            if (!strcmp(foto_obj[k], otro) && !strcmp(foto_obj[k + 1], mio)) {
                const int tmp = nuestros[k];
                nuestros[k] = nuestros[k + 1];
                nuestros[k + 1] = tmp;
                describir_objeto(s, nuestros[k], mio, sizeof mio);
                igual = 1;
            }
        }
        if (igual && k < nn && k < FILAS_OBJETOS && foto_sitios[k] >= 0 && foto_sitios[k] != sitio(s, nuestros[k])) {
            igual = 0;
            snprintf(mio + strlen(mio), sizeof mio - strlen(mio), " (sitio %d, CorsixTH %d)", sitio(s, nuestros[k]),
                     foto_sitios[k]);
        }
        if (!igual) {
            distintos++;
            if (mostrar && distintos <= 5)
                anotar("    objeto %d: th64 %s; CorsixTH %s\n", k, mio, suyo);
        }
    }
    if (ns != nn) {
        distintos++;
        if (mostrar)
            anotar("    objetos: th64 %d, CorsixTH %d\n", nn, ns);
    }
    return distintos;
}

// ---------------------------------------------------------------------------
// Las órdenes

// Los nombres de las órdenes, por TH_ORD_*: los de bench/lib/ordenes.lua.
static const char *const orden_nombres[] = {
    [TH_ORD_NONE] = "?",
    [TH_ORD_COMPRAR] = "comprar",
    [TH_ORD_COLOCAR] = "colocar",
    [TH_ORD_CERRAR] = "cerrar",
    [TH_ORD_RECOGER] = "recoger",
    [TH_ORD_VENDER] = "vender",
    [TH_ORD_DESHACER] = "deshacer",
    [TH_ORD_SALA] = "sala",
    [TH_ORD_PUERTA] = "puerta",
    [TH_ORD_VENTANA] = "ventana",
    [TH_ORD_QUITAR_VENTANA] = "quitar_ventana",
    [TH_ORD_FIN_VENTANAS] = "fin_ventanas",
    [TH_ORD_COMPRAR_SALA] = "comprar_sala",
    [TH_ORD_FIN_SALA] = "fin_sala",
    [TH_ORD_ATRAS] = "atras",
    [TH_ORD_CONTRATAR] = "contratar",
};

const char *th64sim_nombre_orden(int kind)
{
    const int n = (int)(sizeof orden_nombres / sizeof orden_nombres[0]);
    return kind > 0 && kind < n && orden_nombres[kind] ? orden_nombres[kind] : "?";
}

// Las clases de «contratar», por TH_HUM_* (la clase va en th_order.dir).
static const char *const clase_nombres[] = {[TH_HUM_DOCTOR] = "doctor",
                                            [TH_HUM_NURSE] = "nurse",
                                            [TH_HUM_HANDYMAN] = "handyman",
                                            [TH_HUM_RECEPTIONIST] = "receptionist"};

const char *th64sim_clase_nombre(const th_sim *s, int who)
{
    if (who < 0 || who >= TH_SIM_HUMANOIDS)
        return "?";
    const int k = s->hum[who].kind;
    return k >= TH_HUM_DOCTOR && k <= TH_HUM_RECEPTIONIST && clase_nombres[k] ? clase_nombres[k] : "?";
}

// El resumen del perfil de una orden de contratar (th_order.def,
// th_staff_profile_digest), en la columna def_id del registro: 8 cifras
// hexadecimales. Lo lee th64sim_leer_orden si la lista lo trae.
const char *th64sim_resumen_de(uint32_t def)
{
    static char texto[12];
    snprintf(texto, sizeof texto, "%08lx", (unsigned long)def);
    return texto;
}

const char *th64sim_id_de(const th_sim *s, th_rnode def)
{
    static const char *const grupos[] = {"objetos", "salas"};
    if (!def)
        return "-";
    for (int g = 0; g < 2; g++) {
        const th_rnode m = th_rules_get(s->defs, s->defs->root, grupos[g]);
        for (uint32_t i = 0; i < th_rules_len(s->defs, m); i++)
            if (th_rules_entry_value(s->defs, m, i) == def)
                return th_rules_str(s->defs, th_rules_entry_key(s->defs, m, i), NULL);
    }
    return "-";
}

static uint8_t direccion(const char *v)
{
    for (int d = 0; d < 4; d++)
        if (!strcmp(v, dir_nombres[d]))
            return (uint8_t)d;
    fprintf(stderr, "dirección desconocida «%s»: se toma el norte\n", v);
    return 0;
}

int th64sim_leer_orden(const th_sim *s, char *linea, long *n, th_order *o, char *texto, size_t tam)
{
    char *campos[8];
    int nc = 0;
    linea[strcspn(linea, "\r\n")] = 0;
    if (!*linea || *linea == '#')
        return 0;
    for (char *p = strtok(linea, "\t"); p && nc < 8; p = strtok(NULL, "\t"))
        campos[nc++] = p;
    // La cabecera («n» en la batería, «tick» en th64sim partida): la primera
    // columna no es un número.
    char *fin;
    if (nc < 2)
        return 0;
    *n = strtol(campos[0], &fin, 10);
    if (fin == campos[0] || *fin)
        return 0;
    memset(o, 0, sizeof *o);
    texto[0] = 0;
    for (int i = 1; i < nc; i++)
        snprintf(texto + strlen(texto), tam - strlen(texto), "%s%s", i > 1 ? " " : "", campos[i]);
    const char *nombre = campos[1];
    const th_rnode objs = th_rules_get(s->defs, s->defs->root, "objetos");
    if (!strcmp(nombre, "comprar") && nc >= 4) {
        o->kind = TH_ORD_COMPRAR;
        o->def = th_rules_get(s->defs, objs, campos[2]);
        const long q = strtol(campos[3], NULL, 10);
        o->qty = (uint8_t)(q > 255 ? 255 : q < 0 ? 0 : q);
    } else if (!strcmp(nombre, "colocar") && nc >= 6) {
        o->kind = TH_ORD_COLOCAR;
        o->def = th_rules_get(s->defs, objs, campos[2]);
        o->x = (uint8_t)atoi(campos[3]);
        o->y = (uint8_t)atoi(campos[4]);
        o->dir = direccion(campos[5]);
    } else if (!strcmp(nombre, "recoger") && nc >= 4) {
        o->kind = TH_ORD_RECOGER;
        o->x = (uint8_t)atoi(campos[2]);
        o->y = (uint8_t)atoi(campos[3]);
    } else if (!strcmp(nombre, "cerrar"))
        o->kind = TH_ORD_CERRAR;
    else if (!strcmp(nombre, "vender"))
        o->kind = TH_ORD_VENDER;
    else if (!strcmp(nombre, "deshacer"))
        o->kind = TH_ORD_DESHACER;
    else if (!strcmp(nombre, "sala") && nc >= 7) {
        o->kind = TH_ORD_SALA;
        o->def = th_rules_get(s->defs, th_rules_get(s->defs, s->defs->root, "salas"), campos[2]);
        o->x = (uint8_t)atoi(campos[3]);
        o->y = (uint8_t)atoi(campos[4]);
        o->w = (uint8_t)atoi(campos[5]);
        o->h = (uint8_t)atoi(campos[6]);
    } else if ((!strcmp(nombre, "puerta") || !strcmp(nombre, "ventana") || !strcmp(nombre, "quitar_ventana")) && nc >= 5) {
        o->kind = nombre[0] == 'p' ? TH_ORD_PUERTA : nombre[0] == 'v' ? TH_ORD_VENTANA : TH_ORD_QUITAR_VENTANA;
        o->x = (uint8_t)atoi(campos[2]);
        o->y = (uint8_t)atoi(campos[3]);
        o->dir = direccion(campos[4]);
    } else if (!strcmp(nombre, "fin_ventanas"))
        o->kind = TH_ORD_FIN_VENTANAS;
    else if (!strcmp(nombre, "comprar_sala") && nc >= 4) {
        o->kind = TH_ORD_COMPRAR_SALA;
        o->def = th_rules_get(s->defs, objs, campos[2]);
        const long q = strtol(campos[3], NULL, 10);
        o->qty = (uint8_t)(q > 255 ? 255 : q < 0 ? 0 : q);
    } else if (!strcmp(nombre, "fin_sala"))
        o->kind = TH_ORD_FIN_SALA;
    else if (!strcmp(nombre, "atras"))
        o->kind = TH_ORD_ATRAS;
    else if (!strcmp(nombre, "contratar") && nc >= 6) {
        // UIHireStaff:hire y UIPlaceStaff:onMouseUp (dialogs/hire_staff.lua:126-147,
        // dialogs/place_staff.lua:144-176): la clase, el candidato de la lista
        // de esa clase y la casilla donde se suelta (th/build.h).
        o->kind = TH_ORD_NONE;
        for (int c = TH_HUM_DOCTOR; c <= TH_HUM_RECEPTIONIST; c++)
            if (!strcmp(campos[2], clase_nombres[c])) {
                o->kind = TH_ORD_CONTRATAR;
                o->dir = (uint8_t)c;
            }
        const long q = strtol(campos[3], NULL, 10);
        o->qty = (uint8_t)(q > 255 ? 255 : q < 0 ? 0 : q);
        o->x = (uint8_t)atoi(campos[4]);
        o->y = (uint8_t)atoi(campos[5]);
        // El resumen del perfil (th_order.def), si la lista lo trae: lo
        // escribe la ROM (y th64sim partida) en la columna def_id, en
        // hexadecimal. Sin él, def queda a 0 y th64sim partida lo toma del
        // candidato de ese índice en el mercado del tick en que se aplica.
        if (nc >= 7 && strcmp(campos[6], "-") != 0) {
            char *fin_r;
            const unsigned long r = strtoul(campos[6], &fin_r, 16);
            if (fin_r == campos[6] || *fin_r || r > 0xffffffffUL)
                fprintf(stderr, "resumen del perfil no válido «%s»: se toma del mercado\n", campos[6]);
            else
                o->def = (th_rnode)r;
        }
    } else
        o->kind = TH_ORD_NONE;
    return 1;
}

// ---------------------------------------------------------------------------
// Las salas (salas.tsv: n, id, tipo, x, y, ancho, alto, puerta_x, puerta_y,
// puerta_lado, construida, activa, fase), en el orden de world.rooms (el id).

static const char *const fase_nombres[] = {"-", "walls", "door", "windows", "clear_area", "objects"};

static int booleano(const char *v)
{
    return !strcmp(v, "1") || !strcmp(v, "true");
}

static void describir_sala(const th_sim *s, int i, char *out, size_t tam)
{
    const th_room *r = &s->rooms[i];
    char puerta[32] = "- - -";
    if (r->door != TH_NONE && (s->obj[r->door].oflags & TH_OBJF_ON_MAP)) {
        const th_object *d = &s->obj[r->door];
        // El lado desde dentro: norte u oeste si la casilla de la puerta es
        // de la sala; si no, sur o este.
        const int inside = th_map_tile_c(s->w.map, d->x, d->y)->room == r->num;
        const char *lado = d->dir == TH_DIR_WEST ? (inside ? "west" : "east") : (inside ? "north" : "south");
        snprintf(puerta, sizeof puerta, "%d %d %s", d->x, d->y, lado);
    }
    const int fase = s->obra.phase != TH_OBRA_NONE && s->obra.room == i ? s->obra.phase : 0;
    snprintf(out, tam, "%d %s %d %d %d %d %s %d %d %s", r->num, s->room_types[r->type], r->x, r->y, r->w, r->h, puerta,
             r->built, r->is_active, fase_nombres[fase]);
}

// La última foto de salas.tsv, como la de los objetos.
#define FILAS_SALAS 64
static char foto_salas[FILAS_SALAS][160];
static int foto_salas_n = -1;

static int comparar_salas(const th_sim *s, tabla *t, long n, int mostrar)
{
    if (t->f && (t->hay || leer_fila(t))) {
        while (t->hay && t->n_fila < n)
            if (!leer_fila(t))
                break;
        if (t->hay && t->n_fila == n) {
            foto_salas_n = 0;
            while (t->hay && t->n_fila == n) {
                if (foto_salas_n < FILAS_SALAS) {
                    const char *px = campo(t, "puerta_x"), *py = campo(t, "puerta_y"), *pl = campo(t, "puerta_lado");
                    snprintf(foto_salas[foto_salas_n], sizeof foto_salas[0], "%s %s %s %s %s %s %s %s %s %d %d %s",
                             campo(t, "id"), campo(t, "tipo"), campo(t, "x"), campo(t, "y"), campo(t, "ancho"),
                             campo(t, "alto"), *px ? px : "-", *py ? py : "-", *pl ? pl : "-",
                             booleano(campo(t, "construida")), booleano(campo(t, "activa")), campo(t, "fase"));
                }
                foto_salas_n++;
                if (!leer_fila(t))
                    break;
            }
        }
    }
    if (foto_salas_n < 0)
        return 0;
    // Las salas del núcleo por id, que es el orden de world.rooms.
    int orden[TH_SIM_ROOMS], nn = 0;
    for (int i = 0; i < s->n_rooms; i++) {
        int j = nn++;
        while (j > 0 && s->rooms[orden[j - 1]].num > s->rooms[i].num) {
            orden[j] = orden[j - 1];
            j--;
        }
        orden[j] = i;
    }
    const int k = foto_salas_n;
    int distintos = 0;
    for (int i = 0; i < k && i < FILAS_SALAS; i++) {
        char mio[160] = "(nada)";
        if (i < nn)
            describir_sala(s, orden[i], mio, sizeof mio);
        if (strcmp(foto_salas[i], mio)) {
            distintos++;
            if (mostrar && distintos <= 5)
                anotar("    sala %d: th64 %s; CorsixTH %s\n", i, mio, foto_salas[i]);
        }
    }
    if (k != nn) {
        distintos++;
        if (mostrar)
            anotar("    salas: th64 %d, CorsixTH %d\n", nn, k);
    }
    return distintos;
}

int th64sim_construir(int argc, char **argv)
{
    const char *assets = argv[2], *trg = argv[3], *dir = argv[4];
    const int seguir = argc > 5 && !strcmp(argv[5], "--seguir");
    th_sim *s = th64sim_cargar_escenario(assets, trg, NULL, 1);
    if (!s)
        return 1;
    sim_actual = s;
    s->ev.money = al_mover_dinero;
    char ruta[1024];
    snprintf(ruta, sizeof ruta, "%s/inicio", dir);
    if (cargar_celdas(ruta, "celdas.tsv")) {
        fprintf(stderr, "no se puede leer %s/celdas.tsv\n", ruta);
        return 1;
    }
    nota[0] = 0;
    int dif = comparar_mapa(s->w.map, 1);
    printf("inicio: %d casillas distintas\n%s", dif, nota);
    if (dif && !seguir)
        return 1;
    tabla t_res, t_din, t_cam, t_obj, t_sal;
    if (abrir_tabla(&t_res, dir, "resultados.tsv")) {
        fprintf(stderr, "no se puede leer %s/resultados.tsv\n", dir);
        return 1;
    }
    abrir_tabla(&t_din, dir, "dinero.tsv");
    abrir_tabla(&t_cam, dir, "cambios.tsv");
    abrir_tabla(&t_obj, dir, "objetos.tsv");
    abrir_tabla(&t_sal, dir, "salas.tsv");
    snprintf(ruta, sizeof ruta, "%s/ordenes.tsv", dir);
    FILE *f = fopen(ruta, "r");
    if (!f) {
        fprintf(stderr, "no se puede leer %s\n", ruta);
        return 1;
    }
    static char linea[LINEA];
    int ordenes = 0, iguales = 0, aceptadas = 0, primera = 0;
    while (fgets(linea, sizeof linea, f)) {
        long n;
        th_order o;
        char texto[256];
        if (!th64sim_leer_orden(s, linea, &n, &o, texto, sizeof texto))
            continue;
        ordenes++;
        n_movs = movs_perdidos = 0;
        valor_visto = s->hosp.value;
        th_order_result r;
        th_build_apply(s, &o, &r);
        aceptadas += r.reason == TH_REASON_OK;
        char motivo[48];
        if (r.reason == TH_REASON_CASILLAS)
            snprintf(motivo, sizeof motivo, "casillas:%d", r.bad);
        else
            snprintf(motivo, sizeof motivo, "%s", th_build_reason_name(r.reason));
        // Lo de CorsixTH para esta orden.
        while (t_res.f && leer_fila(&t_res) && t_res.n_fila < n)
            ;
        const int hay_res = t_res.hay && t_res.n_fila == n;
        const char *su_res = hay_res ? campo(&t_res, "resultado") : "?";
        const char *su_mot = hay_res ? campo(&t_res, "motivo") : "?";
        const int acept = r.reason == TH_REASON_OK;
        int distinto = 0;
        char detalle[1024] = "";
        if (!hay_res || strcmp(su_res, acept ? "aceptada" : "rechazada") ||
            (!acept && strcmp(su_mot, motivo))) {
            distinto = 1;
            snprintf(detalle + strlen(detalle), sizeof detalle - strlen(detalle),
                     "    resultado: th64 %s %s, CorsixTH %s %s\n", acept ? "aceptada" : "rechazada",
                     acept ? "" : motivo, su_res, su_mot);
        }
        // La fase en la que queda la obra (detalle: walls, door, windows,
        // clear_area, objects o «-»).
        const char *su_fase = hay_res ? campo(&t_res, "detalle") : "";
        if (*su_fase && strcmp(su_fase, fase_nombres[s->obra.phase])) {
            distinto = 1;
            snprintf(detalle + strlen(detalle), sizeof detalle - strlen(detalle), "    fase: th64 %s, CorsixTH %s\n",
                     fase_nombres[s->obra.phase], su_fase);
        }
        static const char *const cifras[] = {"saldo", "valor", "money_in", "money_out"};
        const long nuestras[] = {s->hosp.balance, s->hosp.value, s->hosp.money_in, s->hosp.money_out};
        for (int i = 0; i < 4 && hay_res; i++) {
            const char *v = campo(&t_res, cifras[i]);
            if (*v && strtol(v, NULL, 10) != nuestras[i]) {
                distinto = 1;
                snprintf(detalle + strlen(detalle), sizeof detalle - strlen(detalle), "    %s: th64 %ld, CorsixTH %s\n",
                         cifras[i], nuestras[i], v);
            }
        }
        const int mostrar = !primera;
        nota[0] = 0;
        aplicar_cambios(&t_cam, n);
        const int din = comparar_dinero(&t_din, n, mostrar);
        const int cas = comparar_mapa(s->w.map, mostrar);
        const int obj = comparar_objetos(s, &t_obj, n, mostrar);
        const int sal = comparar_salas(s, &t_sal, n, mostrar);
        distinto |= din || cas || obj || sal;
        printf("%3ld %-40s %s%s%s", n, texto, acept ? "aceptada" : "rechazada ", acept ? "" : motivo,
               distinto ? "  DISTINTA" : "");
        if (cas)
            printf(" (%d casillas)", cas);
        putchar('\n');
        if (distinto && mostrar) {
            fputs(detalle, stdout);
            fputs(nota, stdout);
        }
        if (distinto) {
            if (!primera)
                primera = (int)n;
            if (!seguir)
                break;
        } else
            iguales++;
    }
    fclose(f);
    printf("construir: %d órdenes, %d aceptadas; %d iguales que en CorsixTH", ordenes, aceptadas, iguales);
    if (primera)
        printf("; la primera distinta, la %d", primera);
    printf("; huella %08lx\n", (unsigned long)th_sim_hash(s));
    if (s->obra.phase != TH_OBRA_NONE) {
        // El lado de CorsixTH asegura que no queda ninguna obra abierta.
        printf("al final queda una obra abierta (fase %s)\n", fase_nombres[s->obra.phase]);
        return 1;
    }
    if (!primera) {
        // Al final, el celdas.tsv entero de CorsixTH.
        if (cargar_celdas(dir, "celdas.tsv") == 0) {
            nota[0] = 0;
            dif = comparar_mapa(s->w.map, 1);
            printf("final: %d casillas distintas de celdas.tsv\n%s", dif, nota);
            if (dif)
                return 1;
        }
    }
    return primera ? 1 : 0;
}
