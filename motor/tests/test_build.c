// Pruebas de las órdenes de amueblar el pasillo (th/build.h) en un nivel
// TMP1 sintético, con definiciones y precios escritos a mano. Lo que se
// espera sale del código de CorsixTH (dialogs/furnish_corridor.lua,
// dialogs/place_objects.lua, entities/object.lua y world.lua); la comparación
// con CorsixTH en marcha es la de th64sim construir con la batería
// (tools/hito6/construir.sh).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "th/anim.h"
#include "th/build.h"
#include "th/humanoid.h"
#include "th/map.h"
#include "th/objects.h"
#include "th/path.h"
#include "th/rules.h"
#include "th/sim.h"
#include "th_test.h"

#define CASILLA 10
#define CABECERA 32

// Lo grande (unos 825 KB) va en el montón mientras duran estas pruebas y se
// devuelve al acabar: en la ROM de pruebas, lo que viene detrás necesita ese
// montón (la comparación de la interfaz pide 384 KB).
typedef struct estado {
    uint8_t tmp[CABECERA + CASILLA * TH_MAP_WIDTH * TH_MAP_HEIGHT];
    th_map mapa;
    th_pathfinder buscador;
    th_sim sim;
} estado;
static estado *E;
#define tmp (E->tmp)
#define mapa (E->mapa)
#define buscador (E->buscador)
#define sim (E->sim)
static uint8_t trg_defs[8192], trg_nivel[4096];
static th_rules defs, nivel;

// Las definiciones que importan para colocar: clase, thob, corridor_object y
// las huellas con su adjacent_to_solid_footprint, como las de CorsixTH.
static const char *const definiciones =
    "{\"objetos\": {"
    " \"bench\": {\"id\": \"bench\", \"thob\": 4, \"class\": \"Bench\", \"corridor_object\": 2, \"orientations\": {"
    "  \"north\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"complete_cell\": true},"
    "                             {\"#1\": 0, \"#2\": -1, \"only_passable\": true, \"shareable\": true, \"invisible\": true}],"
    "            \"adjacent_to_solid_footprint\": [[-1, 0], [0, -1], [0, 1], [1, 0]]},"
    "  \"south\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"complete_cell\": true},"
    "                             {\"#1\": 0, \"#2\": 1, \"only_passable\": true, \"shareable\": true, \"invisible\": true}],"
    "            \"adjacent_to_solid_footprint\": [[-1, 0], [0, -1], [0, 1], [1, 0]]}}},"
    " \"plant\": {\"id\": \"plant\", \"thob\": 45, \"class\": \"Plant\", \"corridor_object\": 7, \"orientations\": {"
    "  \"north\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"complete_cell\": true}],"
    "            \"adjacent_to_solid_footprint\": [[-1, 0], [0, -1], [0, 1], [1, 0]]}}},"
    " \"radiator\": {\"id\": \"radiator\", \"thob\": 44, \"class\": \"SideObject\", \"corridor_object\": 5,"
    "  \"orientations\": {"
    "  \"north\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"only_side\": true}]},"
    "  \"west\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"only_side\": true}]}}},"
    " \"desk\": {\"id\": \"desk\", \"thob\": 8, \"orientations\": {"
    "  \"north\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"complete_cell\": true}]}}},"
    " \"reception_desk\": {\"id\": \"reception_desk\", \"thob\": 2, \"corridor_object\": 1, \"orientations\": {"
    "  \"north\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"complete_cell\": true},"
    "                             {\"#1\": 0, \"#2\": 1, \"only_passable\": true}],"
    "            \"use_position_secondary\": [0, 1],"
    "            \"adjacent_to_solid_footprint\": [[-1, 0], [0, -1], [1, 0]]}}},"
    " \"entrance_left_door\": {\"id\": \"entrance_left_door\", \"thob\": 58, \"class\": \"EntranceDoor\","
    "  \"supports_creation_for_map\": true},"
    " \"door\": {\"id\": \"door\", \"thob\": 3, \"class\": \"Door\"},"
    " \"swing_door_left\": {\"id\": \"swing_door_left\", \"thob\": 52, \"class\": \"SwingDoor\"},"
    " \"swing_door_right\": {\"id\": \"swing_door_right\", \"thob\": 53, \"class\": \"SwingDoor\"}"
    "}, \"salas\": {\"gp\": {\"id\": \"gp\", \"objects_additional\": [\"plant\"], \"objects_needed\": {\"desk\": 1}}}}";

// Los precios del nivel (level_config.objects[thob].StartCost), del thob 1 al 50.
static size_t nivel_trg(void)
{
    static char json[4096];
    int k = snprintf(json, sizeof json, "{\"objects\": [");
    for (int thob = 1; thob <= 50; thob++)
        k += snprintf(json + k, sizeof json - (size_t)k, "%s{\"StartCost\": %d}", thob > 1 ? ", " : "",
                      thob == 4 ? 40 : thob == 44 ? 20 : thob == 45 ? 5 : thob == 8 ? 100 : thob == 2 ? 150 : 0);
    snprintf(json + k, sizeof json - (size_t)k, "]}");
    return th_test_trg(json, trg_nivel, sizeof trg_nivel);
}

// Nivel de un jugador con suelo de hospital de (5, 5) a (60, 60) en la
// parcela 1, todo construible y transitable, y la simulación vacía con 200 $.
static void preparar(void)
{
    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, "TMP1", 4);
    tmp[5] = TH_MAP_WIDTH;
    tmp[7] = TH_MAP_HEIGHT;
    tmp[8] = 1;
    tmp[9] = 2;
    tmp[11] = CASILLA;
    for (int y = 0; y < TH_MAP_HEIGHT; y++)
        for (int x = 0; x < TH_MAP_WIDTH; x++) {
            uint8_t *c = &tmp[CABECERA + CASILLA * (y * TH_MAP_WIDTH + x)];
            c[0] = 1;
            if (x >= 5 && y >= 5 && x <= 60 && y <= 60) {
                c[0] = 17;
                c[3] = TH_TILE_FILE_FLAGS;
                c[8] = 1;
            }
        }
    TH_CHECK_EQ(th_map_load(&mapa, tmp, sizeof(tmp), 0), TH_MAP_OK);
    memset(&sim, 0, sizeof sim);
    sim.defs = &defs;
    sim.level = &nivel;
    sim.w.map = &mapa;
    sim.w.pf = &buscador;
    // th_world.sim se llama como el atajo de arriba: sin él un momento.
#undef sim
    E->sim.w.sim = &E->sim;
#define sim (E->sim)
    sim.radiator_thob = 44;
    sim.hosp.balance = 200;
    for (int i = 0; i < TH_SIM_OBJECTS; i++)
        sim.obj[i].user = sim.obj[i].reserved_for = sim.obj[i].queue = TH_NONE;
}

static th_rnode def(const char *id)
{
    char ruta[64] = "objetos.";
    strcat(ruta, id);
    return th_rules_path(&defs, defs.root, ruta);
}

static int orden(int kind, const char *id, int x, int y, int dir, int qty, int *bad)
{
    th_order o = {0, (uint8_t)kind, (uint8_t)x, (uint8_t)y, (uint8_t)dir, (uint8_t)qty, 0, 0, id ? def(id) : 0};
    th_order_result r;
    const int m = th_build_apply(&sim, &o, &r);
    if (bad)
        *bad = r.bad;
    return m;
}

static uint32_t banderas(int x, int y)
{
    return th_map_tile(&mapa, x, y)->flags;
}

// Comprar, colocar, cerrar con lo que sobra y el dinero (furnish_corridor.lua
// y removeObject: se devuelve el precio de lo no colocado).
static void test_comprar_colocar(void)
{
    preparar();
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 3, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, 80);
    TH_CHECK_EQ(sim.hosp.value, 120);
    TH_CHECK_EQ(sim.hosp.money_out, 120);
    TH_CHECK_EQ(sim.place.open, 1);
    TH_CHECK_EQ(sim.place.item[0].qty, 3);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "bench", 10, 10, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.n_obj, 1);
    TH_CHECK_EQ(sim.obj[0].x, 10);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 10, 10, 0), 4);
    TH_CHECK_EQ(banderas(10, 10) & (TH_TILE_BUILDABLE | TH_TILE_PASSABLE), 0);
    TH_CHECK_EQ(sim.place.item[0].qty, 2);
    // Encima: el origen está ocupado (una casilla mala; la de uso es invisible).
    int bad = 0;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "bench", 10, 10, TH_DIR_SOUTH, 0, &bad), TH_REASON_CASILLAS);
    TH_CHECK_EQ(bad, 1);
    // Fuera del hospital: el origen en el césped y la de uso también.
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "bench", 2, 2, TH_DIR_NORTH, 0, &bad), TH_REASON_CASILLAS);
    TH_CHECK_EQ(bad, 1);
    // Fuera del mapa: cuentan aunque sean invisibles.
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "bench", 0, 0, TH_DIR_NORTH, 0, &bad), TH_REASON_CASILLAS);
    TH_CHECK_EQ(bad, 2);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "bench", 20, 20, TH_DIR_WEST, 0, NULL), TH_REASON_ORIENTACION);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 20, 20, TH_DIR_NORTH, 0, NULL), TH_REASON_SIN_OBJETO);
    // Cerrar devuelve los dos que quedan.
    TH_CHECK_EQ(orden(TH_ORD_CERRAR, NULL, 0, 0, 0, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, 160);
    TH_CHECK_EQ(sim.hosp.value, 40);
    TH_CHECK_EQ(sim.hosp.money_in, 80);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(orden(TH_ORD_CERRAR, NULL, 0, 0, 0, 0, NULL), TH_REASON_SIN_VENTANA);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "bench", 20, 20, TH_DIR_NORTH, 0, NULL), TH_REASON_SIN_VENTANA);
    // El último que se coloca cierra la ventana.
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "radiator", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "radiator", 30, 30, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(banderas(30, 30) & TH_TILE_TRAVEL_N, 0);
    TH_CHECK_EQ(sim.hosp.balance, 140);
    // Comprar con la ventana abierta la cierra: devuelve lo que no se colocó.
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "plant", 0, 0, 0, 2, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, 130);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, 100);
    TH_CHECK_EQ(sim.place.item[0].def, def("bench"));
    // Sin dinero, no compra; y lo que no es de pasillo, tampoco. Las dos
    // cierran antes la ventana de colocar (el banco vuelve: +40).
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 99, NULL), TH_REASON_SIN_DINERO);
    // Más de 99, con dinero para 99: la centésima pulsación falla y no se compra nada.
    sim.hosp.balance += 1000;
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "plant", 0, 0, 0, 100, NULL), TH_REASON_MAXIMO);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(sim.hosp.balance, 1140);
    sim.hosp.balance -= 1000;
    TH_CHECK_EQ(sim.hosp.balance, 140);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "desk", 0, 0, 0, 1, NULL), TH_REASON_NO_ES_DE_PASILLO);
    // Comprar 0 no pulsa «+», así que no mira el saldo, ni en negativo:
    // confirm sin nada que comprar solo cierra.
    sim.hosp.balance = -100;
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 1, NULL), TH_REASON_SIN_DINERO);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(sim.hosp.balance, -100);
    // Lo que no es de pasillo tampoco se compra con la ventana de colocar
    // abierta: la ventana se cierra antes y devuelve lo que quedaba.
    sim.hosp.balance = 140;
    orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 2, NULL);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "bench", 40, 40, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, 60);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "desk", 0, 0, 0, 1, NULL), TH_REASON_NO_ES_DE_PASILLO);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(sim.hosp.balance, 100);
}

// Coger y mover gratis, deshacer y vender (removeObject: move_cancellation y
// sale_previously_placed).
static void test_coger(void)
{
    preparar();
    const uint32_t antes = banderas(10, 9), libre = banderas(12, 10);
    orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 1, NULL);
    orden(TH_ORD_COLOCAR, "bench", 10, 10, TH_DIR_NORTH, 0, NULL);
    TH_CHECK(banderas(10, 9) != antes);
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 11, 11, 0, 0, NULL), TH_REASON_SIN_OBJETO);
    // Con la ventana de colocar abierta, el clic no llega al banco.
    orden(TH_ORD_COMPRAR, "plant", 0, 0, 0, 1, NULL);
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 10, 10, 0, 0, NULL), TH_REASON_OTRA_VENTANA);
    TH_CHECK_EQ(sim.place.open, 1);
    orden(TH_ORD_CERRAR, NULL, 0, 0, 0, 0, NULL);
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 10, 10, 0, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[0].oflags & TH_OBJF_PICKED_UP, TH_OBJF_PICKED_UP);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 10, 10, 0), 4); // sigue en el mapa
    TH_CHECK_EQ(sim.place.item[0].existing, 1);
    // Moverlo: gratis, y la casilla de antes queda libre.
    const int32_t saldo = sim.hosp.balance;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "bench", 12, 10, TH_DIR_SOUTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, saldo);
    TH_CHECK_EQ(sim.obj[0].x, 12);
    TH_CHECK_EQ(sim.obj[0].dir, TH_DIR_SOUTH);
    TH_CHECK_EQ(sim.obj[0].oflags & TH_OBJF_PICKED_UP, 0);
    TH_CHECK_EQ(banderas(10, 10), libre);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 10, 10), 0);
    TH_CHECK_EQ(sim.n_obj, 1);
    TH_CHECK_EQ(sim.place.open, 0);
    // Moverlo a su misma casilla: vale.
    orden(TH_ORD_RECOGER, NULL, 12, 10, 0, 0, NULL);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "bench", 12, 10, TH_DIR_SOUTH, 0, NULL), TH_REASON_OK);
    // Deshacer: vuelve sin comprobar nada.
    orden(TH_ORD_RECOGER, NULL, 12, 10, 0, 0, NULL);
    TH_CHECK_EQ(orden(TH_ORD_DESHACER, NULL, 0, 0, 0, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[0].x, 12);
    TH_CHECK_EQ(sim.obj[0].oflags & (TH_OBJF_PICKED_UP | TH_OBJF_ON_MAP), TH_OBJF_ON_MAP);
    TH_CHECK_EQ(sim.hosp.balance, saldo);
    // Vender: devuelve el precio y lo quita del mapa.
    orden(TH_ORD_RECOGER, NULL, 12, 10, 0, 0, NULL);
    TH_CHECK_EQ(orden(TH_ORD_VENDER, NULL, 0, 0, 0, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, saldo + 40);
    TH_CHECK_EQ(sim.n_obj, 0);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 12, 10), 0);
    TH_CHECK_EQ(banderas(12, 10), libre);
    TH_CHECK_EQ(banderas(10, 9), antes); // la casilla de uso del primer sitio, como al principio
    // Las puertas de la entrada no se cogen.
    th_map_entrance_door(&mapa, 30, 30, 58, 1, 1);
    th_object_init(&sim.obj[0], &defs, def("entrance_left_door"), TH_DIR_WEST);
    sim.n_obj = 1;
    th_objects w = th_sim_objects(&sim);
    th_object_adopt(&w, 0, 30, 30);
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 30, 30, 0, 0, NULL), TH_REASON_NO_SE_PUEDE);
}

// El bloqueo de zonas en el pasillo (modo estricto): una pared de x = 20 con
// un solo hueco en (20, 30).
static void test_bloqueo(void)
{
    preparar();
    for (int y = 5; y <= 60; y++)
        if (y != 30)
            th_map_tile(&mapa, 20, y)->flags &= ~(uint32_t)TH_TILE_PASSABLE;
    th_map_update_pathfinding(&mapa);
    orden(TH_ORD_COMPRAR, "plant", 0, 0, 0, 3, NULL);
    const uint32_t hueco = banderas(20, 30);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 20, 30, TH_DIR_NORTH, 0, NULL), TH_REASON_BLOQUEO);
    TH_CHECK_EQ(banderas(20, 30), hueco); // comprobar no cambia nada
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 25, 30, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    orden(TH_ORD_CERRAR, NULL, 0, 0, 0, 0, NULL);
    // Un radiador que corta el hueco por el oeste: también.
    orden(TH_ORD_COMPRAR, "radiator", 0, 0, 0, 1, NULL);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "radiator", 20, 30, TH_DIR_WEST, 0, NULL), TH_REASON_BLOQUEO);
    TH_CHECK_EQ(banderas(20, 30) & TH_TILE_TRAVEL_W, TH_TILE_TRAVEL_W);
    TH_CHECK_EQ(banderas(19, 30) & TH_TILE_TRAVEL_E, TH_TILE_TRAVEL_E);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "radiator", 20, 30, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    // La excepción de la casilla aislada, con la rareza de isIsolated: un
    // pasillo de una casilla en x = 5 (de y = 26 a 31, sin salida por abajo)
    // y una bolsa pasable fuera del hospital en (4, 30). La planta en (5, 30)
    // vale: (4, 30) queda aislada, y (5, 31) llega a (5, 29) porque la huella
    // se queda pasable tras isIsolated. Sin la excepción, o sin la rareza,
    // daría bloqueo.
    preparar();
    for (int y = 26; y <= 32; y++)
        th_map_tile(&mapa, 6, y)->flags &= ~(uint32_t)TH_TILE_PASSABLE;
    th_map_tile(&mapa, 5, 32)->flags &= ~(uint32_t)TH_TILE_PASSABLE;
    th_map_tile(&mapa, 4, 30)->flags |= TH_TILE_PASSABLE;
    th_map_update_pathfinding(&mapa);
    orden(TH_ORD_COMPRAR, "plant", 0, 0, 0, 1, NULL);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 5, 30, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(banderas(5, 30) & TH_TILE_PASSABLE, 0);
}

// prepareFootprintTilesForBuild: colocar encima quita toda la basura de la casilla.
static void test_basura(void)
{
    preparar();
    th_sim_add_litter(&sim, 40, 40, TH_LITTER_PAPER, 0);
    th_sim_add_litter(&sim, 40, 40, TH_LITTER_BOTTLE, 0);
    th_sim_add_litter(&sim, 41, 40, TH_LITTER_PAPER, 0);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 40, 40, 0), TH_LITTER_THOB);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 40, 40), 2);
    TH_CHECK_EQ(th_map_has_object_list(&mapa, 40, 40), 1);
    orden(TH_ORD_COMPRAR, "plant", 0, 0, 0, 1, NULL);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 40, 40, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.n_litter, 1);
    TH_CHECK_EQ(sim.litter[0].x, 41);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 40, 40), 1);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 40, 40, 0), 45);
}

// anyHumanoidObscuringArea: alguien en el origen, o en una vecina (sin
// diagonales) caminando hacia él, o en una vecina con un caminar sin camino
// (sin path_x: el que no llegó a empezar). La batería de CorsixTH no tiene gente.
static void test_persona(void)
{
    preparar();
    orden(TH_ORD_COMPRAR, "plant", 0, 0, 0, 9, NULL);
    th_person *p = &sim.hum[0];
    p->alive = 1;
    p->h.tile_x = 25;
    p->h.tile_y = 25;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 25, 25, TH_DIR_NORTH, 0, NULL), TH_REASON_PERSONA);
    // En la vecina, quieto: no tapa.
    p->h.tile_x = 26;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 25, 25, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    // En la vecina, caminando hacia el origen: tapa; hacia otro sitio, no.
    p->h.tile_y = 30;
    p->h.queue_len = 1;
    p->h.queue[0].kind = TH_ACTION_WALK;
    p->h.queue[0].state = TH_AS_STARTED;
    p->h.path_len = 2;
    p->h.cur_x = 25;
    p->h.cur_y = 30;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 25, 30, TH_DIR_NORTH, 0, NULL), TH_REASON_PERSONA);
    p->h.cur_x = 27;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 25, 30, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    // En diagonal no cuenta, ni caminando hacia el origen.
    p->h.tile_x = 26;
    p->h.tile_y = 36;
    p->h.cur_x = 25;
    p->h.cur_y = 35;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 25, 35, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    // En la vecina de abajo, caminando hacia el origen (el siguiente paso).
    p->h.tile_x = 27;
    p->h.tile_y = 41;
    p->h.cur_x = 27;
    p->h.cur_y = 40;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 27, 40, TH_DIR_NORTH, 0, NULL), TH_REASON_PERSONA);
    // Un caminar que no llegó a empezar (sin camino, o a su propia casilla):
    // path_len y cur son los del paseo anterior, hacia otro sitio, pero la
    // acción no tiene path_x y tapa. Empezado (reiniciado sin camino), vale.
    p->h.tile_x = 26;
    p->h.tile_y = 45;
    p->h.queue[0].state = 0;
    p->h.cur_x = 28;
    p->h.cur_y = 45;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 25, 45, TH_DIR_NORTH, 0, NULL), TH_REASON_PERSONA);
    p->h.queue[0].state = TH_AS_STARTED;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 25, 45, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    p->alive = 0;
}

// El tope del motor (th_sim_object_fits), que CorsixTH no tiene: con los
// huecos llenos, colocar uno nuevo se rechaza con «lleno» después de las
// comprobaciones de CorsixTH y sin tocar nada; mover sí vale. Lo vendido deja
// un hueco que se reutiliza, y el orden de world.entities lo guarda
// entity_seq, que es lo que mira coger.
static void test_lleno(void)
{
    preparar();
    sim.hosp.balance = 2000;
    // Plantas en las casillas pares, de dos en dos, con la 200 en (51, 51).
    int n = 0;
    for (int y = 6; y < 60 && n < TH_SIM_OBJECTS; y += 2)
        for (int x = 6; x < 60 && n < TH_SIM_OBJECTS; x += 2, n++) {
            const int i = th_sim_new_object(&sim, def("plant"), n == 200 ? 51 : x, n == 200 ? 51 : y, TH_DIR_NORTH);
            TH_CHECK_EQ(i, n);
        }
    TH_CHECK_EQ(sim.n_obj, TH_SIM_OBJECTS);
    TH_CHECK_EQ(sim.obj[1].x, 8);
    TH_CHECK_EQ(th_sim_object_fits(&sim, def("plant")), 0);
    TH_CHECK_EQ(th_sim_new_object(&sim, def("plant"), 7, 7, TH_DIR_NORTH), TH_NONE);
    th_sim_add_litter(&sim, 7, 7, TH_LITTER_PAPER, 0);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "plant", 0, 0, 0, 1, NULL), TH_REASON_OK);
    const int32_t saldo = sim.hosp.balance;
    int bad = 0;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 8, 8, TH_DIR_NORTH, 0, &bad), TH_REASON_CASILLAS); // antes que lleno
    TH_CHECK_EQ(bad, 1);
    th_person *q = &sim.hum[0]; // y alguien delante, también antes
    q->alive = 1;
    q->h.tile_x = 7;
    q->h.tile_y = 7;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 7, 7, TH_DIR_NORTH, 0, NULL), TH_REASON_PERSONA);
    q->alive = 0;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 7, 7, TH_DIR_NORTH, 0, NULL), TH_REASON_LLENO);
    TH_CHECK_EQ(sim.place.open, 1);
    TH_CHECK_EQ(sim.place.item[0].qty, 1);
    TH_CHECK_EQ(sim.n_litter, 1);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 7, 7, 0), TH_LITTER_THOB);
    TH_CHECK_EQ(sim.hosp.balance, saldo);
    TH_CHECK_EQ(sim.n_obj, TH_SIM_OBJECTS);
    // Cerrar devuelve la unidad; mover una planta a (7, 7) sí vale.
    TH_CHECK_EQ(orden(TH_ORD_CERRAR, NULL, 0, 0, 0, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, saldo + 5);
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 6, 6, 0, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 7, 7, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[0].x, 7);
    TH_CHECK_EQ(sim.n_litter, 0);
    // Vender la 1 deja su hueco: n_obj no baja, y la siguiente lo ocupa con
    // el entity_seq más alto.
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 8, 6, 0, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_VENDER, NULL, 0, 0, 0, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[1].def, 0);
    TH_CHECK_EQ(sim.n_obj, TH_SIM_OBJECTS);
    TH_CHECK_EQ(th_sim_object_fits(&sim, def("plant")), 1);
    const uint32_t ultimo = sim.entity_seq;
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "plant", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 9, 9, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[1].x, 9);
    TH_CHECK_EQ(sim.obj[1].entity_seq, ultimo + 1);
    TH_CHECK_EQ(sim.n_obj, TH_SIM_OBJECTS);
    // Coger «el último de world.entities» con ese origen es el de mayor
    // entity_seq, aunque esté en un hueco más bajo: dos plantas en (51, 51),
    // la 200 y una nueva en el hueco 3.
    th_sim_destroy_object(&sim, 3);
    TH_CHECK_EQ(th_sim_new_object(&sim, def("plant"), 51, 51, TH_DIR_NORTH), 3);
    TH_CHECK(sim.obj[3].entity_seq > sim.obj[200].entity_seq);
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 51, 51, 0, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.place.item[0].existing, 3 + 1);
    TH_CHECK_EQ(orden(TH_ORD_DESHACER, NULL, 0, 0, 0, 0, NULL), TH_REASON_OK);
    // Un mostrador necesita además una cola libre (TH_SIM_QUEUES, con las
    // puertas): sin ella no cabe aunque haya hueco.
    th_sim_destroy_object(&sim, 5);
    TH_CHECK_EQ(th_sim_object_fits(&sim, def("reception_desk")), 1);
    sim.n_queues = TH_SIM_QUEUES;
    for (int k = 0; k < TH_SIM_QUEUES; k++)
        sim.queues[k].max_size = 6;
    TH_CHECK_EQ(th_sim_object_fits(&sim, def("reception_desk")), 0);
    TH_CHECK_EQ(th_sim_object_fits(&sim, def("plant")), 1);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "reception_desk", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "reception_desk", 7, 13, TH_DIR_NORTH, 0, NULL), TH_REASON_LLENO);
    sim.queues[4].max_size = 0; // una cola libre (la de un mostrador vendido)
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "reception_desk", 7, 13, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[5].type, TH_OBJ_RECEPTION_DESK);
    TH_CHECK_EQ(sim.obj[5].queue, 4);
    TH_CHECK_EQ(sim.n_desks, 1);
    TH_CHECK_EQ(sim.desks[0], 5);
}

// Entity:getRoom: la sala de un objeto es la de su casilla, también al sacar
// de una sala activa un objeto de pasillo (room.objects de la sala vieja, que
// CorsixTH no limpia al mover, es otra cosa).
static void test_sala(void)
{
    preparar();
    // Una consulta activa de 3 × 3 en (40..42, 40..42), que admite plantas.
    sim.n_room_types = 1;
    sim.room_types[0] = "gp";
    sim.n_rooms = 1;
    th_room *r = &sim.rooms[0];
    r->num = 1;
    r->type = 0;
    r->is_active = 1;
    r->x = r->y = 40;
    r->w = r->h = 3;
    for (int y = 40; y <= 42; y++)
        for (int x = 40; x <= 42; x++)
            th_map_tile(&mapa, x, y)->room = 1;
    // Su puerta, en el mapa (colocar dentro exige room.door.tile_x).
    r->door = (uint8_t)th_sim_new_object(&sim, def("door"), 40, 40, TH_DIR_NORTH);
    TH_CHECK(r->door != TH_NONE);
    sim.obj[r->door].room = 1;
    orden(TH_ORD_COMPRAR, "plant", 0, 0, 0, 1, NULL);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 41, 41, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[1].room, 1);
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 41, 41, 0, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 20, 20, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[1].room, 0);
    orden(TH_ORD_RECOGER, NULL, 20, 20, 0, 0, NULL);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 42, 42, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[1].room, 1);
    // Un banco no es de la consulta: fuera.
    orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 1, NULL);
    int bad = 0;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "bench", 40, 41, TH_DIR_NORTH, 0, &bad), TH_REASON_CASILLAS);
    TH_CHECK_EQ(bad, 1);
}

// ReceptionDesk:setTile → checkForNearbyStaff: un mostrador nuevo, o movido,
// reserva a la recepcionista libre más cercana, que va hacia él.
static void test_mostrador(void)
{
    preparar();
    static th_anims anims;
    th_test_anims(&anims);
    sim.w.anims = &anims;
    sim.hosp.balance = 1000;
    th_person *p = &sim.hum[0];
    p->alive = 1;
    p->kind = TH_HUM_RECEPTIONIST;
    p->u.s.associated_desk = TH_NONE;
    p->entity_seq = ++sim.entity_seq;
    sim.order[0] = 0;
    sim.n_order = 1;
    th_humanoid_init(&sim.w, &p->h, th_walk_anims_find("Receptionist"));
    th_humanoid_set_tile(&sim.w, &p->h, 20, 20);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "reception_desk", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "reception_desk", 30, 30, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.n_desks, 1);
    TH_CHECK_EQ(sim.obj[0].entity_seq, p->entity_seq + 1); // detrás de ella en world.entities
    TH_CHECK_EQ(sim.obj[0].reserved_for, 0);
    TH_CHECK_EQ(p->u.s.associated_desk, 0);
    TH_CHECK_EQ(p->h.queue_len, 2);
    TH_CHECK_EQ(p->h.queue[0].kind, TH_ACTION_WALK);
    TH_CHECK_EQ(p->h.queue[0].x, 30);
    TH_CHECK_EQ(p->h.queue[0].y, 31);
    TH_CHECK_EQ(p->h.queue[1].kind, TH_ACTION_STAFF_RECEPTION);
    // Movido: lo mismo (coger no le quita todavía la reserva, que es el
    // handleRemovedObject pendiente; aquí se quita a mano para verlo).
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 30, 30, 0, 0, NULL), TH_REASON_OK);
    sim.obj[0].reserved_for = TH_NONE;
    p->u.s.associated_desk = TH_NONE;
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "reception_desk", 34, 30, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[0].reserved_for, 0);
    TH_CHECK_EQ(p->u.s.associated_desk, 0);
    // El caminar de antes se queda (must_happen); el nuevo va detrás.
    TH_CHECK_EQ(p->h.queue_len, 4);
    TH_CHECK_EQ(p->h.queue[2].x, 34);
    // Otro mostrador, con ella ya reservada: se queda sin nadie.
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "reception_desk", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "reception_desk", 40, 30, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[1].reserved_for, TH_NONE);
    TH_CHECK_EQ(sim.n_desks, 2);
    // Caben más de 8 mostradores: tantos como colas.
    for (int k = 0; k < 9; k++)
        TH_CHECK_EQ(th_sim_new_object(&sim, def("reception_desk"), 10 + 2 * k, 50, TH_DIR_NORTH), 2 + k);
    TH_CHECK_EQ(sim.n_desks, 11);
    TH_CHECK_EQ(sim.desks[10], 10);
    TH_CHECK_EQ(sim.n_queues, 11);
    p->alive = 0;
    sim.n_order = 0;
    sim.w.anims = NULL;
}

// La cola: cada orden, al empezar el tick de su hora, y avisa al aplicarla.
static int avisos, ultimo_motivo;
static void al_aplicar(void *ctx, const th_sim *s, const th_order *o, const th_order_result *r)
{
    (void)ctx;
    (void)s;
    (void)o;
    avisos++;
    ultimo_motivo = r->reason;
}

static void test_cola(void)
{
    preparar();
    sim.ev.order = al_aplicar;
    avisos = 0;
    th_order a = {5, TH_ORD_COMPRAR, 0, 0, 0, 1, 0, 0, def("plant")}, b = {3, TH_ORD_CERRAR, 0, 0, 0, 0, 0, 0, 0};
    TH_CHECK_EQ(th_sim_push_order(&sim, &a), 0);
    TH_CHECK_EQ(th_sim_push_order(&sim, &b), 0);
    sim.hours = 3;
    th_sim_apply_orders(&sim);
    TH_CHECK_EQ(avisos, 1);
    TH_CHECK_EQ(ultimo_motivo, TH_REASON_SIN_VENTANA);
    TH_CHECK_EQ(sim.n_orders, 1);
    sim.hours = 5;
    th_sim_apply_orders(&sim);
    TH_CHECK_EQ(avisos, 2);
    TH_CHECK_EQ(ultimo_motivo, TH_REASON_OK);
    TH_CHECK_EQ(sim.place.open, 1);
    for (int i = 0; i < TH_SIM_ORDERS; i++)
        th_sim_push_order(&sim, &b);
    TH_CHECK_EQ(th_sim_push_order(&sim, &b), -1);
    TH_CHECK_EQ(strcmp(th_build_reason_name(TH_REASON_CASILLAS), "casillas"), 0);
    TH_CHECK_EQ(strcmp(th_build_reason_name(TH_REASON_LLENO), "lleno"), 0);
}

void th_test_build(void)
{
    const size_t n = th_test_trg(definiciones, trg_defs, sizeof(trg_defs));
    const size_t m = nivel_trg();
    TH_CHECK(n > 0);
    TH_CHECK(m > 0);
    E = malloc(sizeof *E);
    TH_CHECK(E != NULL);
    if (!n || !m || !E) {
        free(E);
        E = NULL;
        return;
    }
    TH_CHECK_EQ(th_rules_init(&defs, trg_defs, n), 0);
    TH_CHECK_EQ(th_rules_init(&nivel, trg_nivel, m), 0);
    test_comprar_colocar();
    test_coger();
    test_bloqueo();
    test_basura();
    test_persona();
    test_lleno();
    test_sala();
    test_mostrador();
    test_cola();
    free(E);
    E = NULL;
}
