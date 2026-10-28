#include "girar.h"

#include <string.h>

_Static_assert(TH_MAP_WIDTH == TH_MAP_HEIGHT, "las vistas giradas piden un mapa cuadrado");

enum { NORTE = 0, ESTE = 1, SUR = 2, OESTE = 3 }; // como TH_DIR_*
#define LADO TH_MAP_WIDTH

void th_rot_tile(int rot, int x, int y, int *vx, int *vy)
{
    switch (rot & 3) {
    case 0: *vx = x; *vy = y; break;
    case 1: *vx = LADO - 1 - y; *vy = x; break;
    case 2: *vx = LADO - 1 - x; *vy = LADO - 1 - y; break;
    default: *vx = y; *vy = LADO - 1 - x; break;
    }
}

void th_rot_tile_inv(int rot, int vx, int vy, int *x, int *y)
{
    th_rot_tile(4 - (rot & 3), vx, vy, x, y);
}

void th_rot_offset(int rot, int px, int py, int *qx, int *qy)
{
    // Un vector (dx, dy) gira a (−dy, dx) con r = 1.
    switch (rot & 3) {
    case 0: *qx = px; *qy = py; break;
    case 1: *qx = -2 * py; *qy = px / 2; break;
    case 2: *qx = -px; *qy = -py; break;
    default: *qx = 2 * py; *qy = -px / 2; break;
    }
}

static void girar_punto(int rot, int32_t *a, int32_t *b, int32_t lado)
{
    const int32_t x = *a, y = *b;
    switch (rot & 3) {
    case 0: break;
    case 1: *a = lado - y; *b = x; break;
    case 2: *a = lado - x; *b = lado - y; break;
    default: *a = y; *b = lado - x; break;
    }
}

void th_rot_point(int from, int to, int32_t *a, int32_t *b, int32_t lado)
{
    girar_punto(4 - (from & 3), a, b, lado);
    girar_punto(to, a, b, lado);
}

// --- Las piezas ---------------------------------------------------------
//
// Suelos y decorado con dirección, por familias. En cada familia, cada pieza
// se describe por lo que hay en 4 sitios de la casilla; al girar la cámara r
// veces, lo del sitio i pasa al i + r. La pieza girada es la de la familia
// con esa descripción; si no la hay, la que la tiene en espejo, dibujada en
// espejo; y si tampoco, la más parecida (la que difiere en menos sitios).

typedef struct {
    uint8_t block;
    char forma[5];
} pieza;

typedef struct {
    const pieza *piezas;
    int n;
    uint8_t espejo[4]; // el sitio i va al espejo[i] en el espejo
} familia;

// Por esquinas (NE, SE, SO, NO), como «marching squares». El espejo de la
// pantalla cambia el norte por el oeste y el este por el sur: NE por SO.
#define ESPEJO_ESQUINAS {2, 1, 0, 3}
// Por bordes (N, E, S, O): el norte por el oeste y el este por el sur.
#define ESPEJO_BORDES {3, 2, 1, 0}

// Césped (c) y camino (p) con bordillo: los bloques 6 a 14 y los 61 a 63,
// más el césped y el camino lisos para cuando no haya otra.
static const pieza cesped[] = {
    {1, "cccc"},  {5, "pppp"},  {6, "cpcc"},  {7, "cppc"},  {8, "ccpc"},  {9, "ccpp"},  {10, "cccp"}, {11, "pccp"},
    {12, "pccc"}, {13, "ppcc"}, {14, "pppc"}, {61, "pcpp"}, {62, "ppcp"}, {63, "cppp"},
};

// El estanque: agua (a) y césped (c), con piedras en la orilla.
static const pieza estanque[] = {
    {69, "aaaa"}, {60, "caac"}, {65, "ccaa"}, {68, "acca"}, {78, "aacc"},
    {59, "ccac"}, {80, "ccca"}, {77, "accc"}, {79, "cacc"},
};

// Los setos: por qué bordes sigue el seto (1).
static const pieza setos[] = {
    {176, "1100"}, {177, "0110"}, {178, "0011"}, {179, "1001"}, {180, "0101"}, {181, "1010"},
    {182, "0111"}, {183, "1101"}, {184, "1110"}, {185, "1011"}, {186, "1111"},
};
// Los setos con algo encima van solos: rectos y sin otra pieza que su espejo.
static const pieza seto_arbol[] = {{187, "1010"}};
static const pieza seto_bayas[] = {{188, "1010"}};
static const pieza seto_190[] = {{190, "0101"}};
static const pieza seto_trepadora[] = {{191, "0101"}};

// Las farolas: hacia dónde apunta el brazo.
static const pieza farolas[] = {{209, "0001"}, {210, "1000"}};

static const familia familias_suelo[] = {
    {cesped, sizeof cesped / sizeof cesped[0], ESPEJO_ESQUINAS},
    {estanque, sizeof estanque / sizeof estanque[0], ESPEJO_ESQUINAS},
};

static const familia familias_capa[] = {
    {setos, sizeof setos / sizeof setos[0], ESPEJO_BORDES},
    {seto_arbol, 1, ESPEJO_BORDES},
    {seto_bayas, 1, ESPEJO_BORDES},
    {seto_190, 1, ESPEJO_BORDES},
    {seto_trepadora, 1, ESPEJO_BORDES},
    {farolas, 2, ESPEJO_BORDES},
};

// Las carreteras de los niveles van todas de norte a sur, con sus marcas a
// un lado u otro. Girada 90°, la carretera va de este a oeste: es la misma
// pieza en espejo. Girada 180°, es la pieza con las marcas al otro lado (la
// pareja), y girada 270°, la pareja en espejo. Sin pareja, la misma.
static const uint8_t carreteras[][2] = {
    {41, 41}, {42, 52}, {43, 51}, {44, 44}, {45, 45}, {46, 46}, {47, 47}, {48, 48}, {49, 49}, {50, 58},
    {51, 43}, {52, 42}, {53, 53}, {54, 42}, {55, 55}, {56, 56}, {57, 57}, {58, 50},
};

static int distancia(const char *a, const char *b)
{
    int d = 0;
    for (int i = 0; i < 4; i++)
        d += a[i] != b[i];
    return d;
}

static uint16_t girar_en_familia(const familia *f, const pieza *p, int r)
{
    char buscada[5] = {0}, espejo[5] = {0};
    for (int i = 0; i < 4; i++)
        buscada[(i + r) & 3] = p->forma[i];
    const pieza *mejor = p;
    int mejor_d = 5, en_espejo = 0;
    for (int k = 0; k < f->n; k++) {
        const int d = distancia(f->piezas[k].forma, buscada);
        if (d < mejor_d) {
            mejor = &f->piezas[k];
            mejor_d = d;
        }
    }
    for (int k = 0; k < f->n && mejor_d > 0; k++) {
        for (int i = 0; i < 4; i++)
            espejo[f->espejo[i]] = f->piezas[k].forma[i];
        const int d = distancia(espejo, buscada);
        if (d < mejor_d) {
            mejor = &f->piezas[k];
            mejor_d = d;
            en_espejo = 1;
        }
    }
    return (uint16_t)(mejor->block | (en_espejo ? TH_ROT_FLIP : 0));
}

// Las paredes (walls/*.lua de CorsixTH): cada bloque es de un juego (las
// salas blancas, amarillas, azules y verdes, y las paredes de fuera del
// hospital), con su cara (la de dentro de la sala o la de fuera; las
// ventanas de las salas no tienen), su clase, su borde (norte u oeste) y,
// en las piezas de un extremo, su lado en pantalla.
enum { CARA_NINGUNA, CARA_DENTRO, CARA_FUERA };
enum { CLASE_LISA, CLASE_VENTANA, CLASE_VENTANA_LARGA, CLASE_AMPLIACION, CLASE_BATIENTE, CLASE_PILAR };
enum { LADO_NINGUNO, LADO_IZQUIERDO, LADO_DERECHO };

typedef struct {
    uint8_t block, juego, cara, clase, borde, lado;
} pared;

static pared paredes[96];
static int n_paredes;

static void anadir(int block, int juego, int cara, int clase, int borde, int lado)
{
    paredes[n_paredes++] = (pared){(uint8_t)block, (uint8_t)juego, (uint8_t)cara, (uint8_t)clase, (uint8_t)borde,
                                   (uint8_t)lado};
}

static void preparar_paredes(void)
{
    // inside_tiles, outside_tiles, window_tiles, swing_inside_tiles y
    // swing_outside_tiles de cada color. En las puertas batientes, «left» es
    // la pieza del extremo izquierdo en pantalla y «right» la del derecho.
    static const uint8_t salas[4][14] = {
        // dentro N, O; fuera N, O; ventana N, O; batiente dentro NL, OL, NR, OR; fuera NL, OL, NR, OR
        {82, 83, 90, 91, 128, 129, 98, 99, 130, 131, 106, 107, 148, 149},  // blanca
        {84, 85, 92, 93, 132, 133, 100, 101, 134, 135, 108, 109, 150, 151}, // amarilla
        {86, 87, 94, 95, 136, 137, 102, 103, 138, 139, 110, 111, 152, 153}, // azul
        {88, 89, 96, 97, 140, 141, 104, 105, 146, 147, 112, 113, 154, 155}, // verde
    };
    for (int j = 0; j < 4; j++) {
        const uint8_t *s = salas[j];
        anadir(s[0], j, CARA_DENTRO, CLASE_LISA, NORTE, LADO_NINGUNO);
        anadir(s[1], j, CARA_DENTRO, CLASE_LISA, OESTE, LADO_NINGUNO);
        anadir(s[2], j, CARA_FUERA, CLASE_LISA, NORTE, LADO_NINGUNO);
        anadir(s[3], j, CARA_FUERA, CLASE_LISA, OESTE, LADO_NINGUNO);
        anadir(s[4], j, CARA_NINGUNA, CLASE_VENTANA, NORTE, LADO_NINGUNO);
        anadir(s[5], j, CARA_NINGUNA, CLASE_VENTANA, OESTE, LADO_NINGUNO);
        for (int k = 0; k < 2; k++) {
            const int cara = k ? CARA_FUERA : CARA_DENTRO;
            anadir(s[6 + 4 * k], j, cara, CLASE_BATIENTE, NORTE, LADO_IZQUIERDO);
            anadir(s[7 + 4 * k], j, cara, CLASE_BATIENTE, OESTE, LADO_IZQUIERDO);
            anadir(s[8 + 4 * k], j, cara, CLASE_BATIENTE, NORTE, LADO_DERECHO);
            anadir(s[9 + 4 * k], j, cara, CLASE_BATIENTE, OESTE, LADO_DERECHO);
        }
    }
    // Las de fuera del hospital (external.lua). Las ventanas son de dos
    // casillas: _1 es la mitad izquierda en pantalla y _2 la derecha. Las
    // ampliaciones son las paredes divisorias entre parcelas.
    const int ext = 4;
    anadir(122, ext, CARA_DENTRO, CLASE_LISA, NORTE, LADO_NINGUNO);
    anadir(124, ext, CARA_DENTRO, CLASE_VENTANA, NORTE, LADO_IZQUIERDO);
    anadir(126, ext, CARA_DENTRO, CLASE_VENTANA, NORTE, LADO_DERECHO);
    anadir(144, ext, CARA_DENTRO, CLASE_AMPLIACION, NORTE, LADO_NINGUNO);
    anadir(123, ext, CARA_DENTRO, CLASE_LISA, OESTE, LADO_NINGUNO);
    anadir(127, ext, CARA_DENTRO, CLASE_VENTANA, OESTE, LADO_IZQUIERDO);
    anadir(125, ext, CARA_DENTRO, CLASE_VENTANA, OESTE, LADO_DERECHO);
    anadir(145, ext, CARA_DENTRO, CLASE_AMPLIACION, OESTE, LADO_NINGUNO);
    anadir(114, ext, CARA_FUERA, CLASE_LISA, NORTE, LADO_NINGUNO);
    anadir(116, ext, CARA_FUERA, CLASE_VENTANA, NORTE, LADO_IZQUIERDO);
    anadir(118, ext, CARA_FUERA, CLASE_VENTANA, NORTE, LADO_DERECHO);
    anadir(120, ext, CARA_FUERA, CLASE_VENTANA_LARGA, NORTE, LADO_NINGUNO);
    anadir(142, ext, CARA_FUERA, CLASE_AMPLIACION, NORTE, LADO_NINGUNO);
    anadir(115, ext, CARA_FUERA, CLASE_LISA, OESTE, LADO_NINGUNO);
    anadir(119, ext, CARA_FUERA, CLASE_VENTANA, OESTE, LADO_IZQUIERDO);
    anadir(117, ext, CARA_FUERA, CLASE_VENTANA, OESTE, LADO_DERECHO);
    anadir(121, ext, CARA_FUERA, CLASE_VENTANA_LARGA, OESTE, LADO_NINGUNO);
    anadir(143, ext, CARA_FUERA, CLASE_AMPLIACION, OESTE, LADO_NINGUNO);
    // Los pilares del final de una pared de fuera: de ladrillo por fuera y
    // blancos por dentro, en un extremo del borde.
    anadir(159, ext, CARA_FUERA, CLASE_PILAR, NORTE, LADO_IZQUIERDO);
    anadir(157, ext, CARA_FUERA, CLASE_PILAR, NORTE, LADO_DERECHO);
    anadir(158, ext, CARA_FUERA, CLASE_PILAR, OESTE, LADO_IZQUIERDO);
    anadir(160, ext, CARA_FUERA, CLASE_PILAR, OESTE, LADO_DERECHO);
    anadir(163, ext, CARA_DENTRO, CLASE_PILAR, NORTE, LADO_IZQUIERDO);
    anadir(161, ext, CARA_DENTRO, CLASE_PILAR, NORTE, LADO_DERECHO);
    anadir(162, ext, CARA_DENTRO, CLASE_PILAR, OESTE, LADO_IZQUIERDO);
    anadir(164, ext, CARA_DENTRO, CLASE_PILAR, OESTE, LADO_DERECHO);
}

static int buscar_pared(int juego, int cara, int clase, int borde, int lado)
{
    for (int i = 0; i < n_paredes; i++) {
        const pared *p = &paredes[i];
        if (p->juego == juego && p->cara == cara && p->clase == clase && p->borde == borde && p->lado == lado)
            return p->block;
    }
    return 0;
}

// La pared p con la cámara girada r: en el mundo está en el borde p->borde
// de su casilla; en la vista, ese borde mira a p->borde + r. Si es el norte o
// el oeste, la pared sigue en esa casilla y la cámara la ve por la misma
// cara. Si no, es el borde norte u oeste de la casilla vecina, y la cámara la
// ve desde el otro lado: cambia la cara. Las piezas de un extremo cambian de
// lado si el borde se recorre al revés en pantalla.
static int girar_pared(const pared *p, int r)
{
    const int e = (p->borde + r) & 3;
    const int otro_lado = !(e == NORTE || e == OESTE);
    const int borde = otro_lado ? (e + 2) & 3 : e;
    int cara = p->cara;
    if (otro_lado && cara != CARA_NINGUNA)
        cara = cara == CARA_DENTRO ? CARA_FUERA : CARA_DENTRO;
    int lado = LADO_NINGUNO;
    if (p->lado != LADO_NINGUNO) {
        // Hacia dónde queda el extremo, en casillas: en un borde norte, la
        // izquierda es −x; en uno oeste, +y.
        const int izq = p->lado == LADO_IZQUIERDO;
        int dx = p->borde == NORTE ? (izq ? -1 : 1) : 0, dy = p->borde == NORTE ? 0 : (izq ? 1 : -1);
        for (int k = 0; k < r; k++) {
            const int t = dx;
            dx = -dy;
            dy = t;
        }
        lado = (borde == NORTE ? dx < 0 : dy > 0) ? LADO_IZQUIERDO : LADO_DERECHO;
    }
    int b = buscar_pared(p->juego, cara, p->clase, borde, lado);
    if (!b) // la ventana larga solo existe por fuera
        b = buscar_pared(p->juego, cara, CLASE_LISA, borde, LADO_NINGUNO);
    return b ? b : p->block;
}

// La pareja en espejo de p (th_wall_mirror): la misma pieza en el otro
// borde, con la mitad de pantalla cambiada.
static int pareja(const pared *p)
{
    const int lado = p->lado == LADO_IZQUIERDO ? LADO_DERECHO : p->lado == LADO_DERECHO ? LADO_IZQUIERDO : LADO_NINGUNO;
    const int b = buscar_pared(p->juego, p->cara, p->clase, p->borde == NORTE ? OESTE : NORTE, lado);
    return b ? b : p->block;
}

static uint16_t tabla_suelo[3][256], tabla_capa[3][256];
static uint8_t tabla_espejo[256];
static int tablas_hechas;

static void preparar_tablas(void)
{
    if (tablas_hechas)
        return;
    preparar_paredes();
    for (int b = 0; b < 256; b++)
        tabla_espejo[b] = (uint8_t)b;
    for (int i = 0; i < n_paredes; i++)
        tabla_espejo[paredes[i].block] = (uint8_t)pareja(&paredes[i]);
    for (int r = 1; r <= 3; r++) {
        uint16_t *ts = tabla_suelo[r - 1], *tc = tabla_capa[r - 1];
        for (int b = 0; b < 256; b++) {
            ts[b] = (uint16_t)b;
            tc[b] = (uint16_t)(b | TH_ROT_TILE); // decorado sin dirección
        }
        for (unsigned f = 0; f < sizeof familias_suelo / sizeof familias_suelo[0]; f++)
            for (int k = 0; k < familias_suelo[f].n; k++) {
                const pieza *p = &familias_suelo[f].piezas[k];
                ts[p->block] = girar_en_familia(&familias_suelo[f], p, r);
            }
        for (unsigned i = 0; i < sizeof carreteras / sizeof carreteras[0]; i++) {
            const int b = carreteras[i][0];
            ts[b] = (uint16_t)(r == 1 ? b | TH_ROT_FLIP : r == 2 ? carreteras[i][1] : carreteras[i][1] | TH_ROT_FLIP);
        }
        for (unsigned f = 0; f < sizeof familias_capa / sizeof familias_capa[0]; f++)
            for (int k = 0; k < familias_capa[f].n; k++) {
                const pieza *p = &familias_capa[f].piezas[k];
                tc[p->block] = (uint16_t)(girar_en_familia(&familias_capa[f], p, r) | TH_ROT_TILE);
            }
        for (int i = 0; i < n_paredes; i++)
            tc[paredes[i].block] = (uint16_t)girar_pared(&paredes[i], r);
        tc[0] = 0;
    }
    tablas_hechas = 1;
}

uint16_t th_rot_floor(int rot, int block)
{
    if (!(rot & 3) || block < 0 || block > 255)
        return (uint16_t)block;
    preparar_tablas();
    return tabla_suelo[(rot & 3) - 1][block];
}

int th_wall_mirror(int block)
{
    if (block < 0 || block > 255)
        return block;
    preparar_tablas();
    return tabla_espejo[block];
}

uint16_t th_rot_layer(int rot, int block)
{
    if (block < 0 || block > 255)
        return 0;
    if (!(rot & 3))
        return (uint16_t)(block >= 82 && block <= 164 ? block : block | TH_ROT_TILE);
    preparar_tablas();
    return tabla_capa[(rot & 3) - 1][block];
}

// --- El mapa girado -----------------------------------------------------

int th_rot_map_update(th_rot_map *g, const th_map *m, int rot)
{
    rot &= 3;
    if (!rot || (g->map == m && g->version == m->version && g->rot == rot))
        return 0;
    preparar_tablas();
    const uint16_t *ts = tabla_suelo[rot - 1], *tc = tabla_capa[rot - 1];
    // Los bordes norte y oeste de la vista son, en el mundo, el borde
    // (D − rot) de la casilla: el norte o el oeste de la propia casilla, o el
    // norte o el oeste de la vecina del sur o del este. Es lo mismo para todo
    // el mapa: se mira una vez. La pieza de cada borde ya va con la luz de la
    // vista (th_rot_wall_flip): si toca, la de su pareja en espejo.
    struct {
        int norte;  // en la capa norte (si no, en la oeste)
        int vecina; // 0, la del sur (+W) o la del este (+1)
        int espejo; // la pareja en espejo
    } borde[2];
    for (int k = 0; k < 2; k++) {
        const int d = ((k ? OESTE : NORTE) - rot) & 3;
        borde[k].norte = d == NORTE || d == SUR;
        borde[k].vecina = d == SUR ? LADO : d == ESTE ? 1 : 0;
        borde[k].espejo = th_rot_wall_flip(rot, k);
    }
    // Las sombras son las del mundo (el sol no gira). La media sombra se
    // dibuja con la forma girada (vista.c); la de la pared cae en la cara sur
    // de la pared norte, que solo se ve con la cámara girada 3 veces, como
    // pared oeste.
    const uint8_t sombras = TH_VT_SHADOW_HALF | TH_VT_SHADOW_FULL;
    const int sombra_pared = rot == 3;
    // Al avanzar una casilla en la vista (vx + 1), la del mundo avanza paso_x.
    int x0, y0, x1, y1;
    th_rot_tile_inv(rot, 0, 0, &x0, &y0);
    th_rot_tile_inv(rot, 1, 0, &x1, &y1);
    const int dx = x1 - x0, dy = y1 - y0, paso_x = dy * LADO + dx;
    th_vtile *v = g->tiles;
    for (int vy = 0; vy < LADO; vy++) {
        int x, y;
        th_rot_tile_inv(rot, 0, vy, &x, &y);
        const th_tile *t = th_map_tile_c(m, x, y);
        for (int vx = 0; vx < LADO; vx++, v++, t += paso_x, x += dx, y += dy) {
            const uint16_t f = ts[t->floor];
            uint8_t flags = (uint8_t)((t->flags >> 11) & sombras); // TH_TILE_SHADOW_* → TH_VT_*
            if (sombra_pared && (t->flags & TH_TILE_SHADOW_WALL))
                flags |= TH_VT_SHADOW_WALL_W;
            if (f & TH_ROT_FLIP)
                flags |= TH_VT_FLIP_FLOOR;
            uint8_t capas[2] = {0, 0}, deco[2] = {0, 0};
            // El decorado de la casilla sigue en la capa en la que estaba.
            uint16_t c = tc[t->wall_n];
            if (c & TH_ROT_TILE) {
                deco[0] = (uint8_t)c;
                flags |= c & TH_ROT_FLIP ? TH_VT_FLIP_SC_N : 0;
            }
            c = tc[t->wall_w];
            if (c & TH_ROT_TILE) {
                deco[1] = (uint8_t)c;
                flags |= c & TH_ROT_FLIP ? TH_VT_FLIP_SC_W : 0;
            }
            for (int k = 0; k < 2; k++) {
                if ((borde[k].vecina == LADO && y + 1 >= LADO) || (borde[k].vecina == 1 && x + 1 >= LADO))
                    continue;
                const th_tile *o = t + borde[k].vecina;
                const uint8_t b = borde[k].norte ? o->wall_n : o->wall_w;
                c = tc[b];
                if (b && !(c & TH_ROT_TILE))
                    capas[k] = borde[k].espejo ? tabla_espejo[c & 0xFF] : (uint8_t)c;
            }
            *v = (th_vtile){.floor = (uint8_t)f, .wall_n = capas[0], .wall_w = capas[1], .sc_n = deco[0],
                            .sc_w = deco[1], .flags = flags};
        }
    }
    g->map = m;
    g->version = m->version;
    g->rot = (int8_t)rot;
    return 1;
}
