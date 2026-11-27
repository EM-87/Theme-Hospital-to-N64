// Pruebas de las órdenes de amueblar el pasillo y de construir salas
// rectangulares (th/build.h) en un nivel TMP1 sintético, con definiciones y
// precios escritos a mano. Lo que se espera sale del código de CorsixTH
// (dialogs/furnish_corridor.lua, dialogs/place_objects.lua,
// dialogs/edit_room.lua, entities/object.lua, objects/door.lua y world.lua);
// la comparación con CorsixTH en marcha es la de th64sim construir con la
// batería (tools/hito6/construir.sh).
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
static uint8_t trg_defs[16384], trg_nivel[8192];
static th_rules defs, nivel;

// Las definiciones que importan para colocar: clase, thob, corridor_object y
// las huellas con su adjacent_to_solid_footprint, como las de CorsixTH. Para
// las salas: gp (minimum_size 4, paredes blancas, suelo 18, un escritorio
// obligatorio; opcionales la planta, el radiador, el sofá y un analizador, los
// dos últimos con StartAvail 0 en el nivel: el sofá se vende porque no es
// investigable y el analizador no; level_config_id 7), ward (6,
// suelo 21, puerta doble, id 9) y staff_room (necesita un sofá que el nivel
// no da: no está descubierta). Los juegos de paredes white y external son
// los de walls/white.lua y walls/external.lua. La consulta pide un médico
// (required_staff, como rooms/gp.lua).
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
    "  \"north\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"complete_cell\": true}], \"use_position\": [0, 1]}}},"
    " \"cabinet\": {\"id\": \"cabinet\", \"thob\": 14, \"orientations\": {"
    "  \"north\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"complete_cell\": true}], \"use_position\": [0, 1]}}},"
    " \"sofa\": {\"id\": \"sofa\", \"thob\": 19, \"orientations\": {"
    "  \"north\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"complete_cell\": true}]}}},"
    " \"analyser\": {\"id\": \"analyser\", \"thob\": 20, \"research_category\": \"diagnosis\", \"orientations\": {"
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
    "}, \"salas\": {"
    " \"gp\": {\"id\": \"gp\", \"level_config_id\": 7, \"minimum_size\": 4, \"wall_type\": \"white\","
    "  \"floor_tile\": 18, \"objects_needed\": {\"desk\": 1}, \"required_staff\": {\"Doctor\": 1},"
    "  \"objects_additional\": [\"plant\", \"radiator\", \"sofa\", \"analyser\"]},"
    " \"ward\": {\"id\": \"ward\", \"level_config_id\": 9, \"minimum_size\": 6, \"wall_type\": \"white\","
    "  \"floor_tile\": 21, \"swing_doors\": true, \"objects_needed\": {\"desk\": 1},"
    "  \"objects_additional\": [\"plant\"]},"
    " \"staff_room\": {\"id\": \"staff_room\", \"level_config_id\": 25, \"minimum_size\": 4, \"wall_type\": \"white\","
    "  \"floor_tile\": 17, \"objects_needed\": {\"sofa\": 1}, \"objects_additional\": [\"plant\"]}"
    "}, \"paredes\": {"
    " \"white\": {\"id\": \"white\", \"inside_tiles\": {\"north\": 82, \"west\": 83},"
    "  \"outside_tiles\": {\"north\": 90, \"west\": 91}, \"window_tiles\": {\"north\": 128, \"west\": 129},"
    "  \"swing_inside_tiles\": {\"north_left\": 98, \"west_left\": 99, \"north_right\": 130, \"west_right\": 131},"
    "  \"swing_outside_tiles\": {\"north_left\": 106, \"west_left\": 107, \"north_right\": 148,"
    "   \"west_right\": 149}},"
    " \"external\": {\"id\": \"external\","
    "  \"inside_tiles\": {\"north\": 122, \"north_window_1\": 124, \"north_window_2\": 126,"
    "   \"north_expansion\": 144, \"west\": 123, \"west_window_1\": 127, \"west_window_2\": 125,"
    "   \"west_expansion\": 145},"
    "  \"outside_tiles\": {\"north\": 114, \"north_window_1\": 116, \"north_window_2\": 118, \"north_window_long\": 120,"
    "   \"north_expansion\": 142, \"west\": 115, \"west_window_1\": 119, \"west_window_2\": 117,"
    "   \"west_window_long\": 121, \"west_expansion\": 143},"
    "  \"window_tiles\": {}}"
    "}}";

// El nivel: los precios de los objetos (level_config.objects[thob].StartCost,
// del thob 1 al 50), todos disponibles desde el principio (AvailableForLevel y
// StartAvail) salvo el sofá (19) y el analizador (20), y el precio de las salas (rooms[id].Cost,
// un mapa con claves enteras como el del conversor): el GP 2.280, la
// enfermería 1.700 y la sala de personal 1.350, los del nivel 1. Para
// contratar, staff_levels: en el mes 0, 3 médicos, 2 enfermeras, 1 bedel y 2
// recepcionistas con las tasas del nivel 1 (junior 1 de 10, psiquiatra 1 de
// 3); en el mes 3, solo cambian los médicos (2): el resto se hereda. Y lo
// mínimo que load_level exige (expertise y las listas de visuales).
static size_t nivel_trg(void)
{
    static char json[8192];
    int k = snprintf(json, sizeof json,
                     "{\"rooms\": {\"#7\": {\"Cost\": 2280}, \"#9\": {\"Cost\": 1700}, \"#25\": {\"Cost\": 1350}},"
                     " \"staff_levels\": {\"#0\": {\"Month\": 0, \"Doctors\": 3, \"Nurses\": 2, \"Handymen\": 1,"
                     " \"Receptionists\": 2, \"JrRate\": 10, \"ShrkRate\": 3, \"SurgRate\": 0, \"RschRate\": 0,"
                     " \"ConsRate\": 0, \"Seed\": 4953}, \"#1\": {\"Month\": 3, \"Doctors\": 2, \"Seed\": 8654}},"
                     " \"expertise\": [{\"MaxDiagDiff\": 0}], \"visuals_available\": [{\"Value\": 0}],"
                     " \"non_visuals_available\": [{\"Value\": 0}],"
                     " \"objects\": [");
    for (int thob = 1; thob <= 50; thob++)
        k += snprintf(json + k, sizeof json - (size_t)k,
                      "%s{\"StartCost\": %d, \"AvailableForLevel\": 1, \"StartAvail\": %d}", thob > 1 ? ", " : "",
                      thob == 4 ? 40 : thob == 44 ? 20 : thob == 45 ? 5 : thob == 8 ? 100 : thob == 2 ? 150
                      : thob == 19 ? 150 : 0,
                      thob == 19 || thob == 20 ? 0 : 1);
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

// ---------------------------------------------------------------------------
// Las salas rectangulares (UIEditRoom, dialogs/edit_room.lua). Lo que se
// espera sale de su código (las líneas son de CorsixTH v0.70.1) y la
// comparación con CorsixTH en marcha es la batería tools/hito6/ordenes/salas.tsv.
// Las cifras: el GP sintético vale rooms[7].Cost + escritorio = 2.280 + 100 =
// 2.380 (World:getAvailableRooms), la enfermería 1.700 + 100 = 1.800; al
// entrar en la fase de objetos se cobra el escritorio (buy_object, 100) y al
// confirmar, build_cost − 100 = 2.280 (build_room). Coordenadas desde 0.

#define SALDO 10000

// Los movimientos de dinero (th_sim_events.money): importe, motivo y quién.
static struct {
    int32_t amount;
    int reason, who;
} dinero[8];
static int n_dinero;

static void al_mover_dinero(void *ctx, const th_sim *s, int32_t amount, int reason, int who)
{
    (void)ctx;
    (void)s;
    if (n_dinero < (int)(sizeof dinero / sizeof dinero[0])) {
        dinero[n_dinero].amount = amount;
        dinero[n_dinero].reason = reason;
        dinero[n_dinero].who = who;
    }
    n_dinero++;
}

// Lo de preparar() más los tipos de sala (gp = 0, ward = 1, staff_room = 2),
// 10.000 $ y la traza del dinero.
static void preparar_salas(void)
{
    preparar();
    sim.n_room_types = 3;
    sim.room_types[0] = "gp";
    sim.room_types[1] = "ward";
    sim.room_types[2] = "staff_room";
    sim.hosp.balance = SALDO;
    sim.ev.money = al_mover_dinero;
    n_dinero = 0;
}

static th_rnode def_sala(const char *id)
{
    char ruta[64] = "salas.";
    strcat(ruta, id);
    return th_rules_path(&defs, defs.root, ruta);
}

// La orden sala: el tipo y el plano (x, y, ancho, alto).
static int orden_sala(const char *tipo, int x, int y, int w, int h, int *bad)
{
    th_order o = {0, TH_ORD_SALA, (uint8_t)x, (uint8_t)y, 0, 0, (uint8_t)w, (uint8_t)h, def_sala(tipo)};
    th_order_result r;
    const int m = th_build_apply(&sim, &o, &r);
    if (bad)
        *bad = r.bad;
    return m;
}

// Una orden sin argumentos.
static int orden0(int kind)
{
    return orden(kind, NULL, 0, 0, 0, 0, NULL);
}

// Una puerta o una ventana: la casilla de dentro y su lado.
static int borde(int kind, int x, int y, int lado)
{
    return orden(kind, NULL, x, y, lado, 0, NULL);
}

static int bloque_n(int x, int y)
{
    return th_map_tile(&mapa, x, y)->wall_n;
}

static int bloque_w(int x, int y)
{
    return th_map_tile(&mapa, x, y)->wall_w;
}

// Cuántas casillas del rectángulo tienen exactamente esas banderas de entre
// TH_TILE_PASSABLE y TH_TILE_BLUEPRINT.
static int plano(int x, int y, int w, int h, uint32_t esperado)
{
    int n = 0;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            n += (banderas(xx, yy) & (TH_TILE_PASSABLE | TH_TILE_BLUEPRINT)) == esperado;
    return n;
}

// Cuántas casillas del rectángulo tienen ese suelo, ese número de sala y
// TH_TILE_ROOM si el número no es 0.
static int marcadas(int x, int y, int w, int h, int suelo, int sala)
{
    int n = 0;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) {
            const th_tile *t = th_map_tile(&mapa, xx, yy);
            n += t->floor == suelo && t->room == sala && ((t->flags & TH_TILE_ROOM) != 0) == (sala != 0);
        }
    return n;
}

// Cuántas casillas del rectángulo son casillas de paso de una puerta
// (buildable apagado y doNotIdle), y cuántas no lo son (buildable sin doNotIdle).
static int paso_puerta(int x, int y, int w, int h, int de_paso)
{
    int n = 0;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            n += (banderas(xx, yy) & (TH_TILE_BUILDABLE | TH_TILE_DO_NOT_IDLE)) ==
                 (de_paso ? TH_TILE_DO_NOT_IDLE : TH_TILE_BUILDABLE);
    return n;
}

// El objeto con origen en (x, y) que va el último en world.entities, o -1.
static int objeto_en(int x, int y)
{
    int obj = -1;
    for (int i = 0; i < sim.n_obj; i++) {
        const th_object *o = &sim.obj[i];
        if (o->def && (o->oflags & TH_OBJF_ON_MAP) && o->x == x && o->y == y &&
            (obj < 0 || o->entity_seq > sim.obj[obj].entity_seq))
            obj = i;
    }
    return obj;
}

// La fila de la ventana de colocar con esa definición, o -1.
static int fila(const char *id)
{
    for (int k = 0; k < sim.place.n; k++)
        if (sim.place.item[k].def == def(id))
            return k;
    return -1;
}

// Un GP de 4 × 4 en (10, 10) (casillas 10..13) hasta la fase que se pida:
// door; windows, con la puerta al sur en (11, 13), que deja el objeto en la
// casilla de fuera (11, 14) mirando al norte; u objects, con fin_ventanas, que
// cobra el escritorio (−100). Cierra antes la obra que hubiera.
static void gp_hasta(int fase)
{
    TH_CHECK_EQ(orden_sala("gp", 10, 10, 4, 4, NULL), TH_REASON_OK);
    if (fase == TH_OBRA_DOOR)
        return;
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 11, 13, TH_DIR_SOUTH), TH_REASON_OK);
    if (fase == TH_OBRA_WINDOWS)
        return;
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_OBJECTS);
}

// El plano (fase walls): P0, descubierta y balance >= build_cost
// (build_room.lua:181-183); P1, minimum_size por lado (edit_room.lua:1180);
// P2-P4 por casilla, ni de otra sala, buildable y del jugador
// (th_lua_map.cpp:232-236); P5, el recorte al mapa (edit_room.lua:1170-1173);
// P6, el anillo con el plano cerrado (checkReachability, 873-962).
static void test_sala_plano(void)
{
    preparar_salas();
    int bad = 0;
    // P0b: la sala de personal no está descubierta (su sofá tiene StartAvail
    // 0 en el nivel), y una definición que no es una sala tampoco vale.
    TH_CHECK_EQ(orden_sala("staff_room", 10, 10, 4, 4, &bad), TH_REASON_NO_DISPONIBLE);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    th_order o = {0, TH_ORD_SALA, 10, 10, 0, 0, 4, 4, def("bench")};
    TH_CHECK_EQ(th_build_apply(&sim, &o, NULL), TH_REASON_NO_DISPONIBLE);
    // P0: build_cost = Cost + objetos obligatorios; con un dólar menos no se
    // abre la ventana. El plano no cuesta nada.
    TH_CHECK_EQ(th_build_room_cost(&sim, 0), 2380);
    TH_CHECK_EQ(th_build_room_cost(&sim, 1), 1800);
    sim.hosp.balance = 2379;
    TH_CHECK_EQ(orden_sala("gp", 10, 10, 4, 4, &bad), TH_REASON_SIN_DINERO);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    sim.hosp.balance = 2380;
    TH_CHECK_EQ(orden_sala("gp", 10, 10, 4, 4, &bad), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, 2380);
    sim.hosp.balance = SALDO;
    // P1: el ancho y el alto por separado (2 × 8 son 16 casillas y no vale);
    // la enfermería pide 6. La ventana se queda abierta con el plano en rojo
    // y el botón apagado: la obra sigue en walls, sin cerrar el plano.
    TH_CHECK_EQ(orden_sala("gp", 10, 10, 3, 4, &bad), TH_REASON_PEQUENA);
    TH_CHECK_EQ(orden_sala("gp", 10, 10, 4, 3, &bad), TH_REASON_PEQUENA);
    TH_CHECK_EQ(orden_sala("gp", 10, 10, 2, 8, &bad), TH_REASON_PEQUENA);
    TH_CHECK_EQ(orden_sala("ward", 10, 10, 5, 6, &bad), TH_REASON_PEQUENA);
    TH_CHECK_EQ(orden_sala("ward", 10, 10, 6, 5, &bad), TH_REASON_PEQUENA);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WALLS);
    TH_CHECK_EQ(sim.obra.type, 1);
    TH_CHECK_EQ(plano(10, 10, 6, 5, TH_TILE_PASSABLE), 30);
    // P2-P4: dos columnas de césped (parcela 0: ni buildable ni del jugador).
    TH_CHECK_EQ(orden_sala("gp", 3, 10, 4, 4, &bad), TH_REASON_CASILLAS);
    TH_CHECK_EQ(bad, 8);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WALLS);
    // Una casilla de otra sala.
    th_map_tile(&mapa, 20, 20)->flags |= TH_TILE_ROOM;
    TH_CHECK_EQ(orden_sala("gp", 17, 17, 4, 4, &bad), TH_REASON_CASILLAS);
    TH_CHECK_EQ(bad, 1);
    th_map_tile(&mapa, 20, 20)->flags &= ~(uint32_t)TH_TILE_ROOM;
    // Un banco del pasillo: su origen y su casilla de uso dejan de ser
    // buildable (occupyTilesByObjectFootprintAt): dos casillas malas.
    const int banco = th_sim_new_object(&sim, def("bench"), 30, 30, TH_DIR_NORTH); // uso en (30, 29)
    TH_CHECK_EQ(orden_sala("gp", 28, 28, 4, 4, &bad), TH_REASON_CASILLAS);
    TH_CHECK_EQ(bad, 2);
    th_sim_destroy_object(&sim, banco);
    // Un radiador es only_side y no quita buildable: la sala lo cubre.
    th_sim_new_object(&sim, def("radiator"), 40, 40, TH_DIR_NORTH);
    TH_CHECK_EQ(orden_sala("gp", 38, 38, 4, 4, &bad), TH_REASON_OK);
    // P6: el pasillo de x = 20 cortado salvo en (20, 30), como en
    // test_bloqueo; el plano que tapa el hueco deja las dos mitades sin unir
    // y vuelve a walls con el plano transitable (returnToWallPhase(true)).
    preparar_salas();
    for (int y = 5; y <= 60; y++)
        if (y != 30)
            th_map_tile(&mapa, 20, y)->flags &= ~(uint32_t)TH_TILE_PASSABLE;
    th_map_update_pathfinding(&mapa);
    TH_CHECK_EQ(orden_sala("gp", 18, 28, 4, 4, &bad), TH_REASON_ZONAS_INACCESIBLES);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WALLS);
    TH_CHECK_EQ(plano(18, 28, 4, 4, TH_TILE_PASSABLE), 13); // (20, 28), (20, 29) y (20, 31) no lo eran
    TH_CHECK_EQ(plano(18, 28, 4, 4, 0), 3);
    // Junto al hueco, el anillo sigue unido por (20, 30): vale.
    TH_CHECK_EQ(orden_sala("gp", 18, 31, 4, 4, &bad), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    // Aceptada: fase door, y el plano cerrado (enterDoorPhase): passable
    // apagado y guardado en TH_TILE_BLUEPRINT; alrededor, nada cambia.
    preparar_salas();
    TH_CHECK_EQ(orden_sala("gp", 10, 10, 4, 4, &bad), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    TH_CHECK_EQ(sim.obra.type, 0);
    TH_CHECK_EQ(sim.obra.x, 10);
    TH_CHECK_EQ(sim.obra.y, 10);
    TH_CHECK_EQ(sim.obra.w, 4);
    TH_CHECK_EQ(sim.obra.h, 4);
    TH_CHECK_EQ(sim.obra.room, TH_NONE);
    TH_CHECK_EQ(sim.obra.paid, 0);
    TH_CHECK_EQ(sim.obra.has_door, 0);
    TH_CHECK_EQ(plano(10, 10, 4, 4, TH_TILE_BLUEPRINT), 16);
    TH_CHECK_EQ(plano(9, 9, 6, 6, TH_TILE_BLUEPRINT), 16);
    TH_CHECK_EQ(plano(9, 9, 6, 6, TH_TILE_PASSABLE), 20);
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
    TH_CHECK_EQ(sim.n_rooms, 0);
    // Atrás: a walls, con el plano transitable; otra vez, se abandona.
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WALLS);
    TH_CHECK_EQ(plano(10, 10, 4, 4, TH_TILE_PASSABLE), 16);
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_SIN_VENTANA);
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
    // Una sala con otra obra abierta la cierra antes (UI:addWindow): su
    // plano se abre.
    TH_CHECK_EQ(orden_sala("gp", 10, 10, 4, 4, &bad), TH_REASON_OK);
    TH_CHECK_EQ(orden_sala("gp", 30, 30, 4, 4, &bad), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.x, 30);
    TH_CHECK_EQ(plano(10, 10, 4, 4, TH_TILE_PASSABLE), 16);
    TH_CHECK_EQ(plano(30, 30, 4, 4, TH_TILE_BLUEPRINT), 16);
    // Comprar para el pasillo abandona la obra (GameUI:setEditRoom(false):
    // verifyOrAbortRoom) y abre la ventana de colocar; otra sala la cierra y
    // devuelve lo que no se colocó.
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "plant", 0, 0, 0, 2, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(plano(30, 30, 4, 4, TH_TILE_PASSABLE), 16);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 10);
    TH_CHECK_EQ(orden_sala("gp", 10, 10, 4, 4, &bad), TH_REASON_OK);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
    // P5: el recorte al mapa (setBlueprintRect): la sala nunca llega a la
    // última columna ni a la última fila, donde irían sus paredes.
    TH_CHECK_EQ(orden_sala("gp", 125, 10, 6, 4, &bad), TH_REASON_PEQUENA); // w = 127 − 125 = 2
    TH_CHECK_EQ(sim.obra.w, 2);
    TH_CHECK_EQ(orden_sala("gp", 10, 124, 4, 10, &bad), TH_REASON_PEQUENA); // h = 127 − 124 = 3
    TH_CHECK_EQ(sim.obra.h, 3);
    TH_CHECK_EQ(orden_sala("gp", 120, 10, 8, 4, &bad), TH_REASON_CASILLAS); // w = 7, todo césped
    TH_CHECK_EQ(sim.obra.w, 7);
    TH_CHECK_EQ(bad, 28);
}

// La puerta (fase door): las posiciones que da screenToWall
// (edit_room.lua:753-869: sin esquinas) y la validez de setDoorBlueprint
// (1296-1423): D1, sin pared en el tramo (checkDoorWalls); D2, las casillas
// de dentro y de fuera del jugador, sin objeto delante salvo basura y, si es
// transitable, sin uso exclusivo (validDoorTile); D3, lo mismo en las tres
// casillas de la doble.
static void test_puerta(void)
{
    preparar_salas();
    // fase: sin obra y con el plano rechazado (walls).
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 11, 10, TH_DIR_NORTH), TH_REASON_FASE);
    TH_CHECK_EQ(orden_sala("gp", 10, 10, 3, 4, NULL), TH_REASON_PEQUENA);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 11, 10, TH_DIR_NORTH), TH_REASON_FASE);
    gp_hasta(TH_OBRA_DOOR);
    // posicion: ni esquinas ni casillas que no sean del borde.
    static const struct {
        uint8_t x, y, lado;
    } malas[] = {{10, 10, TH_DIR_NORTH}, {13, 10, TH_DIR_NORTH}, {11, 11, TH_DIR_NORTH}, {11, 9, TH_DIR_NORTH},
                 {10, 13, TH_DIR_SOUTH}, {13, 13, TH_DIR_SOUTH}, {11, 12, TH_DIR_SOUTH}, {11, 14, TH_DIR_SOUTH},
                 {10, 10, TH_DIR_WEST},  {10, 13, TH_DIR_WEST},  {11, 11, TH_DIR_WEST},
                 {13, 10, TH_DIR_EAST},  {13, 13, TH_DIR_EAST},  {12, 11, TH_DIR_EAST}};
    for (unsigned i = 0; i < sizeof malas / sizeof malas[0]; i++)
        TH_CHECK_EQ(borde(TH_ORD_PUERTA, malas[i].x, malas[i].y, malas[i].lado), TH_REASON_POSICION);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    TH_CHECK_EQ(sim.obra.has_door, 0);
    // D1: ya hay pared en el tramo: la capa norte de (11, 10); al sur, la
    // capa norte de la fila de fuera (11, 14) y al este la capa oeste de la
    // columna de fuera (doorWallOffsetCalculations).
    th_map_tile(&mapa, 11, 10)->wall_n = 82;
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 11, 10, TH_DIR_NORTH), TH_REASON_PUERTA_INVALIDA);
    th_map_tile(&mapa, 11, 10)->wall_n = 0;
    th_map_tile(&mapa, 11, 14)->wall_n = 90;
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 11, 13, TH_DIR_SOUTH), TH_REASON_PUERTA_INVALIDA);
    th_map_tile(&mapa, 11, 14)->wall_n = 0;
    th_map_tile(&mapa, 14, 11)->wall_w = 91;
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 13, 11, TH_DIR_EAST), TH_REASON_PUERTA_INVALIDA);
    th_map_tile(&mapa, 14, 11)->wall_w = 0;
    // D2b: un objeto en la casilla de fuera (thob distinto de 0).
    int obj = th_sim_new_object(&sim, def("plant"), 11, 9, TH_DIR_NORTH);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 11, 10, TH_DIR_NORTH), TH_REASON_PUERTA_INVALIDA);
    th_sim_destroy_object(&sim, obj);
    obj = th_sim_new_object(&sim, def("radiator"), 12, 9, TH_DIR_NORTH);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 12, 10, TH_DIR_NORTH), TH_REASON_PUERTA_INVALIDA);
    th_sim_destroy_object(&sim, obj);
    // La basura (62) no estorba, y thob es solo el primero de la lista de la
    // casilla (th_lua_map.cpp:600-603): con la basura antes que el radiador,
    // la puerta vale.
    th_sim_add_litter(&sim, 12, 9, TH_LITTER_PAPER, 0);
    obj = th_sim_new_object(&sim, def("radiator"), 12, 9, TH_DIR_NORTH);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 12, 9, 0), TH_LITTER_THOB);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 12, 9, 1), 44);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 12, 10, TH_DIR_NORTH), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WINDOWS);
    th_sim_destroy_object(&sim, obj);
    th_sim_remove_litter_at(&sim, 12, 9);
    // D2c: la casilla de fuera es la de uso exclusivo de un mostrador
    // (only_passable sin shareable: isTileExclusivelyPassable); la de un
    // banco es compartible y vale.
    gp_hasta(TH_OBRA_DOOR);
    obj = th_sim_new_object(&sim, def("reception_desk"), 11, 8, TH_DIR_NORTH); // uso en (11, 9)
    TH_CHECK_EQ(th_map_thob_at(&mapa, 11, 9, 0), 0);
    TH_CHECK_EQ(banderas(11, 9) & TH_TILE_PASSABLE, TH_TILE_PASSABLE);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 11, 10, TH_DIR_NORTH), TH_REASON_PUERTA_INVALIDA);
    th_sim_destroy_object(&sim, obj);
    obj = th_sim_new_object(&sim, def("bench"), 11, 8, TH_DIR_SOUTH); // uso en (11, 9), shareable
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 11, 10, TH_DIR_NORTH), TH_REASON_OK);
    th_sim_destroy_object(&sim, obj);
    // Aceptada en los cuatro lados: pasa a windows con la puerta guardada
    // (confirm(true): enterWindowsPhase), y ya no admite otra.
    static const struct {
        uint8_t x, y, lado;
    } buenas[] = {{11, 10, TH_DIR_NORTH}, {12, 10, TH_DIR_NORTH}, {11, 13, TH_DIR_SOUTH}, {12, 13, TH_DIR_SOUTH},
                  {10, 11, TH_DIR_WEST},  {10, 12, TH_DIR_WEST},  {13, 11, TH_DIR_EAST},  {13, 12, TH_DIR_EAST}};
    for (unsigned i = 0; i < sizeof buenas / sizeof buenas[0]; i++) {
        gp_hasta(TH_OBRA_DOOR);
        TH_CHECK_EQ(borde(TH_ORD_PUERTA, buenas[i].x, buenas[i].y, buenas[i].lado), TH_REASON_OK);
        TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WINDOWS);
        TH_CHECK_EQ(sim.obra.has_door, 1);
        TH_CHECK_EQ(sim.obra.door.x, buenas[i].x);
        TH_CHECK_EQ(sim.obra.door.y, buenas[i].y);
        TH_CHECK_EQ(sim.obra.door.side, buenas[i].lado);
        TH_CHECK_EQ(borde(TH_ORD_PUERTA, buenas[i].x, buenas[i].y, buenas[i].lado), TH_REASON_FASE);
    }
    // Nada de esto cuesta dinero ni crea la sala ni la puerta.
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
    TH_CHECK_EQ(sim.n_rooms, 0);
    TH_CHECK_EQ(sim.n_obj, 0);
    // La doble de la enfermería (6 × 6 en (30, 30), casillas 30..35): la
    // hoja maestra va en el centro de tres; al norte desde rx+3 (se salta
    // otra casilla junto a la esquina), al sur desde rx+2 y al oeste y al
    // este desde ry+2: 2 posiciones al norte y 3 en cada otro lado.
    static const struct {
        uint8_t x, y, lado, ok;
    } doble[] = {{31, 30, TH_DIR_NORTH, 0}, {32, 30, TH_DIR_NORTH, 0}, {33, 30, TH_DIR_NORTH, 1},
                 {34, 30, TH_DIR_NORTH, 1}, {35, 30, TH_DIR_NORTH, 0}, {31, 35, TH_DIR_SOUTH, 0},
                 {32, 35, TH_DIR_SOUTH, 1}, {34, 35, TH_DIR_SOUTH, 1}, {35, 35, TH_DIR_SOUTH, 0},
                 {30, 31, TH_DIR_WEST, 0},  {30, 32, TH_DIR_WEST, 1},  {30, 34, TH_DIR_WEST, 1},
                 {30, 35, TH_DIR_WEST, 0},  {35, 31, TH_DIR_EAST, 0},  {35, 32, TH_DIR_EAST, 1},
                 {35, 34, TH_DIR_EAST, 1},  {35, 35, TH_DIR_EAST, 0}};
    for (unsigned i = 0; i < sizeof doble / sizeof doble[0]; i++) {
        TH_CHECK_EQ(orden_sala("ward", 30, 30, 6, 6, NULL), TH_REASON_OK);
        TH_CHECK_EQ(borde(TH_ORD_PUERTA, doble[i].x, doble[i].y, doble[i].lado),
                    doble[i].ok ? TH_REASON_OK : TH_REASON_POSICION);
    }
    // D3: también las casillas de fuera y de dentro de las otras dos hojas, y
    // sus tramos de pared: una planta delante de la hoja «nothing» (34, 29)
    // la invalida; en (35, 29), no.
    TH_CHECK_EQ(orden_sala("ward", 30, 30, 6, 6, NULL), TH_REASON_OK);
    obj = th_sim_new_object(&sim, def("plant"), 34, 29, TH_DIR_NORTH);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 33, 30, TH_DIR_NORTH), TH_REASON_PUERTA_INVALIDA);
    th_sim_destroy_object(&sim, obj);
    obj = th_sim_new_object(&sim, def("plant"), 35, 29, TH_DIR_NORTH);
    th_map_tile(&mapa, 32, 30)->wall_n = 82; // el tramo de la esclava
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 33, 30, TH_DIR_NORTH), TH_REASON_PUERTA_INVALIDA);
    th_map_tile(&mapa, 32, 30)->wall_n = 0;
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 33, 30, TH_DIR_NORTH), TH_REASON_OK);
    th_sim_destroy_object(&sim, obj);
}

// Las ventanas (fase windows, setWindowBlueprint, edit_room.lua:1434-1500):
// las posiciones de la puerta normal; W1, sin etiqueta en el tramo (la
// puerta, las hojas de la doble, otra ventana); W2, sin un bloque de pared
// conocido en la capa (getWallIdFromBlockId). Quitar es
// tryRemoveWindowFromWall (1106-1132). Son gratis y sin límite en CorsixTH;
// el motor guarda TH_SIM_WINDOWS.
static void test_ventana(void)
{
    preparar_salas();
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 12, 10, TH_DIR_NORTH), TH_REASON_FASE);
    gp_hasta(TH_OBRA_DOOR);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 12, 10, TH_DIR_NORTH), TH_REASON_FASE);
    TH_CHECK_EQ(borde(TH_ORD_QUITAR_VENTANA, 12, 10, TH_DIR_NORTH), TH_REASON_FASE);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 11, 10, TH_DIR_NORTH), TH_REASON_OK);
    // posicion.
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 10, 10, TH_DIR_NORTH), TH_REASON_POSICION);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 13, 10, TH_DIR_NORTH), TH_REASON_POSICION);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 10, 10, TH_DIR_WEST), TH_REASON_POSICION);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 12, 11, TH_DIR_NORTH), TH_REASON_POSICION);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 14, 11, TH_DIR_EAST), TH_REASON_POSICION);
    TH_CHECK_EQ(borde(TH_ORD_QUITAR_VENTANA, 10, 10, TH_DIR_NORTH), TH_REASON_POSICION);
    // W1: sobre la puerta.
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 11, 10, TH_DIR_NORTH), TH_REASON_VENTANA_INVALIDA);
    // Poner, repetir, quitar donde no hay, quitar y volver a poner.
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 12, 10, TH_DIR_NORTH), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.n_windows, 1);
    TH_CHECK_EQ(sim.obra.windows[0].x, 12);
    TH_CHECK_EQ(sim.obra.windows[0].y, 10);
    TH_CHECK_EQ(sim.obra.windows[0].side, TH_DIR_NORTH);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 12, 10, TH_DIR_NORTH), TH_REASON_VENTANA_INVALIDA);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 10, 11, TH_DIR_WEST), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 13, 12, TH_DIR_EAST), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 12, 13, TH_DIR_SOUTH), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.n_windows, 4);
    TH_CHECK_EQ(borde(TH_ORD_QUITAR_VENTANA, 10, 12, TH_DIR_WEST), TH_REASON_SIN_VENTANA);
    TH_CHECK_EQ(borde(TH_ORD_QUITAR_VENTANA, 10, 11, TH_DIR_WEST), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.n_windows, 3);
    TH_CHECK_EQ(sim.obra.windows[1].x, 13);
    TH_CHECK_EQ(sim.obra.windows[1].side, TH_DIR_EAST);
    TH_CHECK_EQ(borde(TH_ORD_QUITAR_VENTANA, 10, 11, TH_DIR_WEST), TH_REASON_SIN_VENTANA);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 10, 11, TH_DIR_WEST), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.n_windows, 4);
    // W2: un bloque de pared conocido en la capa (la interior de otra sala,
    // 83, o una divisoria de la fachada, 145); uno que no está en las tablas
    // (un adorno, 200) no es pared para getWallIdFromBlockId.
    th_map_tile(&mapa, 10, 12)->wall_w = 83;
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 10, 12, TH_DIR_WEST), TH_REASON_VENTANA_INVALIDA);
    th_map_tile(&mapa, 10, 12)->wall_w = 145;
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 10, 12, TH_DIR_WEST), TH_REASON_VENTANA_INVALIDA);
    th_map_tile(&mapa, 10, 12)->wall_w = 200;
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 10, 12, TH_DIR_WEST), TH_REASON_OK);
    th_map_tile(&mapa, 10, 12)->wall_w = 0;
    TH_CHECK_EQ(sim.obra.n_windows, 5);
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
    // La doble: ni sobre las hojas ni sobre «nothing» (W1); las ventanas no
    // tienen el caso doble de las posiciones.
    TH_CHECK_EQ(orden_sala("ward", 30, 30, 6, 6, NULL), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 33, 30, TH_DIR_NORTH), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 32, 30, TH_DIR_NORTH), TH_REASON_VENTANA_INVALIDA);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 33, 30, TH_DIR_NORTH), TH_REASON_VENTANA_INVALIDA);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 34, 30, TH_DIR_NORTH), TH_REASON_VENTANA_INVALIDA);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 31, 30, TH_DIR_NORTH), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 31, 35, TH_DIR_SOUTH), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 30, 31, TH_DIR_WEST), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.n_windows, 3);
    // El tope del motor: TH_SIM_WINDOWS ventanas, la siguiente da lleno.
    TH_CHECK_EQ(orden_sala("gp", 20, 20, 20, 20, NULL), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 21, 20, TH_DIR_NORTH), TH_REASON_OK);
    int puestas = 0, motivo = TH_REASON_OK;
    for (int x = 22; x <= 38 && motivo == TH_REASON_OK; x++)
        puestas += (motivo = borde(TH_ORD_VENTANA, x, 20, TH_DIR_NORTH)) == TH_REASON_OK;
    for (int y = 21; y <= 38 && motivo == TH_REASON_OK; y++)
        puestas += (motivo = borde(TH_ORD_VENTANA, 20, y, TH_DIR_WEST)) == TH_REASON_OK;
    TH_CHECK_EQ(puestas, TH_SIM_WINDOWS);
    TH_CHECK_EQ(motivo, TH_REASON_LLENO);
    TH_CHECK_EQ(sim.obra.n_windows, TH_SIM_WINDOWS);
}

// fin_ventanas: confirm en windows → clear_area → finishRoom, resetSideObjects
// y enterObjectsPhase (edit_room.lua:224-232, 259-357, 359-552, 970-1002): las
// paredes por tramos con walls/white.lua, la puerta como objeto (door.lua:99-146),
// markRoom (th_lua_map.cpp:754-784), la basura de dentro
// (prepareRectangleTilesForBuild) y la lista con el escritorio cobrado
// (addObjects(lista, true): buy_object).
static void test_fin_ventanas(void)
{
    preparar_salas();
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_FASE);
    gp_hasta(TH_OBRA_DOOR);
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_FASE);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 11, 13, TH_DIR_SOUTH), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 12, 10, TH_DIR_NORTH), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 10, 12, TH_DIR_WEST), TH_REASON_OK);
    // Basura dentro y fuera.
    th_sim_add_litter(&sim, 12, 12, TH_LITTER_PAPER, 0);
    th_sim_add_litter(&sim, 11, 11, TH_LITTER_BOTTLE, 0);
    th_sim_add_litter(&sim, 12, 15, TH_LITTER_PAPER, 0);
    TH_CHECK_EQ(sim.n_litter, 3);
    n_dinero = 0;
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_OBJECTS);
    TH_CHECK_EQ(sim.obra.room, 0);
    TH_CHECK_EQ(sim.obra.paid, 0);
    TH_CHECK_EQ(sim.obra.has_door, 1);
    TH_CHECK_EQ(sim.obra.n_windows, 2);
    // La sala en world.rooms (World:newRoom, Room:initRoom): id 1, ni
    // terminada ni activa, con su puerta y sin esclava.
    TH_CHECK_EQ(sim.n_rooms, 1);
    const th_room *r = &sim.rooms[0];
    TH_CHECK_EQ(r->num, 1);
    TH_CHECK_EQ(r->type, 0);
    TH_CHECK_EQ(r->x, 10);
    TH_CHECK_EQ(r->y, 10);
    TH_CHECK_EQ(r->w, 4);
    TH_CHECK_EQ(r->h, 4);
    TH_CHECK_EQ(r->built, 0);
    TH_CHECK_EQ(r->is_active, 0);
    TH_CHECK_EQ(r->door2, TH_NONE);
    TH_CHECK(r->door != TH_NONE);
    const th_object *puerta = &sim.obj[r->door];
    TH_CHECK_EQ(puerta->thob, 3);
    TH_CHECK_EQ(puerta->oclass, TH_OCLASS_DOOR);
    TH_CHECK_EQ(puerta->x, 11);
    TH_CHECK_EQ(puerta->y, 14);
    TH_CHECK_EQ(puerta->dir, TH_DIR_NORTH);
    TH_CHECK_EQ(puerta->room, 1);
    TH_CHECK_EQ(puerta->oflags & TH_OBJF_ON_MAP, TH_OBJF_ON_MAP);
    TH_CHECK(puerta->queue != TH_NONE);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 11, 14, 0), 3);
    TH_CHECK_EQ(objeto_en(11, 14), r->door);
    TH_CHECK_EQ(sim.n_obj, 1);
    // Las paredes, bloque a bloque (finishRoom): la esquina (10, 10) con las
    // dos interiores; el norte en la capa norte de (11..13, 10) y el oeste en
    // la capa oeste de (10, 11..13), interiores salvo las ventanas; el sur en
    // la capa norte de la fila de fuera (10..13, 14), exteriores salvo la
    // puerta, que no lleva pared; el este en la capa oeste de la columna de
    // fuera (14, 10..13).
    TH_CHECK_EQ(bloque_n(10, 10), 82);
    TH_CHECK_EQ(bloque_w(10, 10), 83);
    TH_CHECK_EQ(bloque_n(11, 10), 82);
    TH_CHECK_EQ(bloque_n(12, 10), 128);
    TH_CHECK_EQ(bloque_n(13, 10), 82);
    TH_CHECK_EQ(bloque_w(10, 11), 83);
    TH_CHECK_EQ(bloque_w(10, 12), 129);
    TH_CHECK_EQ(bloque_w(10, 13), 83);
    TH_CHECK_EQ(bloque_n(10, 14), 90);
    TH_CHECK_EQ(bloque_n(11, 14), 0);
    TH_CHECK_EQ(bloque_n(12, 14), 90);
    TH_CHECK_EQ(bloque_n(13, 14), 90);
    for (int y = 10; y <= 13; y++)
        TH_CHECK_EQ(bloque_w(14, y), 91);
    // Las otras capas de esas casillas siguen a 0, y (14, 14) no se toca.
    TH_CHECK_EQ(bloque_w(11, 14), 0);
    TH_CHECK_EQ(bloque_n(14, 11), 0);
    TH_CHECK_EQ(bloque_n(14, 14), 0);
    TH_CHECK_EQ(bloque_w(14, 14), 0);
    TH_CHECK_EQ(bloque_w(13, 11), 0);
    TH_CHECK_EQ(bloque_n(11, 11), 0);
    // markRoom: el suelo 18, room y el id 1 en las 16 casillas, con passable
    // de vuelta y sin la marca del plano; fuera, el suelo 17 y sin sala.
    TH_CHECK_EQ(marcadas(10, 10, 4, 4, 18, 1), 16);
    TH_CHECK_EQ(plano(10, 10, 4, 4, TH_TILE_PASSABLE), 16);
    TH_CHECK_EQ(marcadas(9, 9, 6, 6, 17, 0), 20);
    // La puerta (Door:setTile): doorNorth y tallNorth en su casilla; en sus
    // dos casillas de paso, buildable apagado y doNotIdle; en las de al lado, no.
    TH_CHECK_EQ(banderas(11, 14) & (TH_TILE_DOOR_N | TH_TILE_DOOR_W | TH_TILE_TALL_N | TH_TILE_TALL_W),
                TH_TILE_DOOR_N | TH_TILE_TALL_N);
    TH_CHECK_EQ(banderas(11, 13) & (TH_TILE_DOOR_N | TH_TILE_DOOR_W | TH_TILE_TALL_N | TH_TILE_TALL_W), 0);
    TH_CHECK_EQ(paso_puerta(11, 13, 1, 2, 1), 2);
    TH_CHECK_EQ(paso_puerta(10, 13, 1, 3, 0), 3);
    TH_CHECK_EQ(paso_puerta(12, 13, 1, 3, 0), 3);
    TH_CHECK_EQ(paso_puerta(11, 15, 1, 1, 0), 1);
    // El tránsito (update_pathfinding): cortado por las paredes y las
    // ventanas por los dos lados, abierto en la puerta; dentro, libre.
    const uint32_t todo = TH_TILE_TRAVEL_N | TH_TILE_TRAVEL_E | TH_TILE_TRAVEL_S | TH_TILE_TRAVEL_W;
    TH_CHECK_EQ(banderas(12, 10) & TH_TILE_TRAVEL_N, 0);
    TH_CHECK_EQ(banderas(12, 9) & TH_TILE_TRAVEL_S, 0);
    TH_CHECK_EQ(banderas(11, 10) & TH_TILE_TRAVEL_N, 0);
    TH_CHECK_EQ(banderas(10, 12) & TH_TILE_TRAVEL_W, 0);
    TH_CHECK_EQ(banderas(9, 12) & TH_TILE_TRAVEL_E, 0);
    TH_CHECK_EQ(banderas(13, 12) & TH_TILE_TRAVEL_E, 0);
    TH_CHECK_EQ(banderas(14, 12) & TH_TILE_TRAVEL_W, 0);
    TH_CHECK_EQ(banderas(12, 13) & TH_TILE_TRAVEL_S, 0);
    TH_CHECK_EQ(banderas(12, 14) & TH_TILE_TRAVEL_N, 0);
    TH_CHECK_EQ(banderas(11, 13) & TH_TILE_TRAVEL_S, TH_TILE_TRAVEL_S);
    TH_CHECK_EQ(banderas(11, 14) & TH_TILE_TRAVEL_N, TH_TILE_TRAVEL_N);
    TH_CHECK_EQ(banderas(11, 11) & todo, todo);
    TH_CHECK_EQ(banderas(12, 12) & todo, todo);
    // Las sombras se recalculan: la pared este da media sombra en su casilla.
    TH_CHECK_EQ(banderas(14, 11) & TH_TILE_SHADOW_HALF, TH_TILE_SHADOW_HALF);
    // La basura de dentro desaparece; la de fuera sigue.
    TH_CHECK_EQ(sim.n_litter, 1);
    TH_CHECK_EQ(sim.litter[0].x, 12);
    TH_CHECK_EQ(sim.litter[0].y, 15);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 12, 12), 0);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 11, 11), 0);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 12, 15, 0), TH_LITTER_THOB);
    // El dinero: el escritorio obligatorio, cobrado al entrar en la fase de
    // objetos (buy_object), y el valor del hospital.
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 100);
    TH_CHECK_EQ(sim.hosp.value, 100);
    TH_CHECK_EQ(sim.hosp.money_out, 100);
    TH_CHECK_EQ(sim.hosp.money_in, 0);
    TH_CHECK_EQ(n_dinero, 1);
    TH_CHECK_EQ(dinero[0].amount, -100);
    TH_CHECK_EQ(dinero[0].reason, TH_MONEY_BUY_OBJECT);
    TH_CHECK_EQ(dinero[0].who, (int)def("desk"));
    // La ventana de colocar: el escritorio, una unidad, mirando al norte.
    TH_CHECK_EQ(sim.place.open, 1);
    TH_CHECK_EQ(sim.place.n, 1);
    TH_CHECK_EQ(sim.place.item[0].def, def("desk"));
    TH_CHECK_EQ(sim.place.item[0].qty, 1);
    TH_CHECK_EQ(sim.place.item[0].picked, 0);
    TH_CHECK_EQ(sim.place.active, 0);
    TH_CHECK_EQ(sim.place.orient, TH_DIR_NORTH);
    // Despejar el área (clearArea, Humanoid:isObscuringArea): con alguien
    // dentro del rectángulo ampliado en una casilla, la obra espera en
    // clear_area sin cobrar y la termina th_build_tick cuando ya no hay
    // nadie; en el borde del rectángulo ampliado solo cuenta quien camina
    // hacia dentro. Atrás desde clear_area vuelve a walls.
    preparar_salas();
    gp_hasta(TH_OBRA_WINDOWS);
    th_person *p = &sim.hum[0];
    p->alive = 1;
    p->h.tile_x = 14; // en el anillo, quieto: no tapa
    p->h.tile_y = 10;
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_OBJECTS);
    preparar_salas();
    gp_hasta(TH_OBRA_WINDOWS);
    p = &sim.hum[0];
    p->alive = 1;
    p->h.tile_x = 12;
    p->h.tile_y = 12;
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_CLEAR);
    TH_CHECK_EQ(sim.n_rooms, 0);
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
    TH_CHECK_EQ(plano(10, 10, 4, 4, TH_TILE_BLUEPRINT), 16);
    th_build_tick(&sim);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_CLEAR);
    p->h.tile_x = 20;
    th_build_tick(&sim);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_OBJECTS);
    TH_CHECK_EQ(sim.n_rooms, 1);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 100);
    TH_CHECK_EQ(marcadas(10, 10, 4, 4, 18, 1), 16);
    preparar_salas();
    gp_hasta(TH_OBRA_WINDOWS);
    p = &sim.hum[0];
    p->alive = 1;
    p->h.tile_x = 12;
    p->h.tile_y = 12;
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WALLS);
    TH_CHECK_EQ(sim.obra.has_door, 0);
    TH_CHECK_EQ(plano(10, 10, 4, 4, TH_TILE_PASSABLE), 16);
    p->alive = 0;
}

// La puerta doble de la enfermería (swing_doors): la esclava en la coordenada
// menor, la maestra en el centro y «nothing» en la mayor (edit_room.lua:1339-1350);
// finishRoom (493-540) crea swing_door_right (53) en la maestra y
// swing_door_left (52) en la esclava, y pone pared en la esclava y en
// «nothing» (swing_inside_tiles dentro, swing_outside_tiles fuera; al norte
// north_left la esclava y north_right «nothing», al oeste al revés), así que
// solo se pasa por la maestra. Door:setTile marca door*/tall* en las dos
// hojas; las 6 casillas de paso son de la maestra
// (swing_door_right.lua:171-188) y la esclava no tiene.
static void test_puerta_doble(void)
{
    static const struct {
        uint8_t dx, dy, lado;    // la orden puerta
        uint8_t sx, sy;          // la casilla de la esclava (su capa)
        uint8_t nx, ny;          // la de «nothing»
        uint8_t west;            // la capa oeste
        uint8_t esclava, nada;   // sus bloques
        uint8_t lx[3], ly[3];    // tres tramos lisos del mismo lado
        uint8_t liso;            // su bloque
        uint8_t px, py, pw, ph;  // las casillas de paso de la maestra
        uint8_t fx[2], fy[2];    // dos casillas de al lado que no lo son
        uint8_t dir;             // la orientación de las hojas
    } casos[] = {
        {33, 30, TH_DIR_NORTH, 32, 30, 34, 30, 0, 98, 130, {31, 35, 30}, {30, 30, 30}, 82, 32, 29, 3, 2,
         {31, 35}, {29, 29}, TH_DIR_NORTH},
        {33, 35, TH_DIR_SOUTH, 32, 36, 34, 36, 0, 106, 148, {30, 31, 35}, {36, 36, 36}, 90, 32, 35, 3, 2,
         {31, 35}, {36, 36}, TH_DIR_NORTH},
        {30, 33, TH_DIR_WEST, 30, 32, 30, 34, 1, 131, 99, {30, 30, 30}, {31, 35, 30}, 83, 29, 32, 2, 3,
         {29, 29}, {31, 35}, TH_DIR_WEST},
        {35, 33, TH_DIR_EAST, 36, 32, 36, 34, 1, 149, 107, {36, 36, 36}, {30, 31, 35}, 91, 35, 32, 2, 3,
         {36, 36}, {31, 35}, TH_DIR_WEST},
    };
    for (unsigned i = 0; i < sizeof casos / sizeof casos[0]; i++) {
        // La casilla de la maestra: la del tramo de la puerta (doorWallOffsetCalculations).
        const int tx = casos[i].lado == TH_DIR_EAST ? casos[i].dx + 1 : casos[i].dx;
        const int ty = casos[i].lado == TH_DIR_SOUTH ? casos[i].dy + 1 : casos[i].dy;
        preparar_salas();
        TH_CHECK_EQ(orden_sala("ward", 30, 30, 6, 6, NULL), TH_REASON_OK);
        TH_CHECK_EQ(borde(TH_ORD_PUERTA, casos[i].dx, casos[i].dy, casos[i].lado), TH_REASON_OK);
        TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
        TH_CHECK_EQ(sim.obra.phase, TH_OBRA_OBJECTS);
        TH_CHECK_EQ(sim.n_rooms, 1);
        const th_room *r = &sim.rooms[0];
        TH_CHECK_EQ(r->num, 1);
        TH_CHECK_EQ(r->type, 1);
        TH_CHECK(r->door != TH_NONE);
        TH_CHECK(r->door2 != TH_NONE);
        TH_CHECK(r->door != r->door2);
        const th_object *maestra = &sim.obj[r->door], *esclava = &sim.obj[r->door2];
        TH_CHECK_EQ(maestra->thob, 53);
        TH_CHECK_EQ(maestra->oclass, TH_OCLASS_SWING_DOOR);
        TH_CHECK_EQ(maestra->oflags & TH_OBJF_MASTER, TH_OBJF_MASTER);
        TH_CHECK_EQ(maestra->x, tx);
        TH_CHECK_EQ(maestra->y, ty);
        TH_CHECK_EQ(maestra->dir, casos[i].dir);
        TH_CHECK_EQ(maestra->room, 1);
        TH_CHECK(maestra->queue != TH_NONE);
        TH_CHECK_EQ(esclava->thob, 52);
        TH_CHECK_EQ(esclava->oclass, TH_OCLASS_SWING_DOOR);
        TH_CHECK_EQ(esclava->oflags & TH_OBJF_MASTER, 0);
        TH_CHECK_EQ(esclava->x, casos[i].sx);
        TH_CHECK_EQ(esclava->y, casos[i].sy);
        TH_CHECK_EQ(esclava->dir, casos[i].dir);
        TH_CHECK_EQ(esclava->room, 0); // Room:initRoom solo hace setupDoor con la maestra: la esclava no tiene sala
        TH_CHECK_EQ(esclava->queue, TH_NONE);
        TH_CHECK_EQ(sim.n_obj, 2);
        TH_CHECK_EQ(th_map_thob_at(&mapa, tx, ty, 0), 53);
        TH_CHECK_EQ(th_map_thob_at(&mapa, casos[i].sx, casos[i].sy, 0), 52);
        TH_CHECK_EQ(th_map_thob_count(&mapa, casos[i].nx, casos[i].ny), 0);
        // Las paredes: la esclava y «nothing» con sus bloques batientes, la
        // maestra sin pared, y los tramos lisos.
        const int w = casos[i].west;
        TH_CHECK_EQ(w ? bloque_w(casos[i].sx, casos[i].sy) : bloque_n(casos[i].sx, casos[i].sy), casos[i].esclava);
        TH_CHECK_EQ(w ? bloque_w(tx, ty) : bloque_n(tx, ty), 0);
        TH_CHECK_EQ(w ? bloque_w(casos[i].nx, casos[i].ny) : bloque_n(casos[i].nx, casos[i].ny), casos[i].nada);
        for (int k = 0; k < 3; k++)
            TH_CHECK_EQ(w ? bloque_w(casos[i].lx[k], casos[i].ly[k]) : bloque_n(casos[i].lx[k], casos[i].ly[k]),
                        casos[i].liso);
        // Las banderas: door y tall en las dos hojas, no en «nothing»; las 6
        // casillas de paso de la maestra, y ninguna más.
        const uint32_t door = w ? TH_TILE_DOOR_W | TH_TILE_TALL_W : TH_TILE_DOOR_N | TH_TILE_TALL_N;
        const uint32_t todas = TH_TILE_DOOR_N | TH_TILE_DOOR_W | TH_TILE_TALL_N | TH_TILE_TALL_W;
        TH_CHECK_EQ(banderas(tx, ty) & todas, door);
        TH_CHECK_EQ(banderas(casos[i].sx, casos[i].sy) & todas, door);
        TH_CHECK_EQ(banderas(casos[i].nx, casos[i].ny) & todas, 0);
        TH_CHECK_EQ(paso_puerta(casos[i].px, casos[i].py, casos[i].pw, casos[i].ph, 1), 6);
        for (int k = 0; k < 2; k++)
            TH_CHECK_EQ(paso_puerta(casos[i].fx[k], casos[i].fy[k], 1, 1, 0), 1);
        // El tránsito: solo por la maestra.
        const uint32_t hacia = w ? TH_TILE_TRAVEL_W : TH_TILE_TRAVEL_N, desde = w ? TH_TILE_TRAVEL_E : TH_TILE_TRAVEL_S;
        TH_CHECK_EQ(banderas(tx, ty) & hacia, hacia);
        TH_CHECK_EQ(banderas(tx - w, ty - !w) & desde, desde);
        TH_CHECK_EQ(banderas(casos[i].sx, casos[i].sy) & hacia, 0);
        TH_CHECK_EQ(banderas(casos[i].nx, casos[i].ny) & hacia, 0);
        TH_CHECK_EQ(banderas(casos[i].sx - w, casos[i].sy - !w) & desde, 0);
        // La sala: suelo 21, id 1; el escritorio cobrado.
        TH_CHECK_EQ(marcadas(30, 30, 6, 6, 21, 1), 36);
        TH_CHECK_EQ(plano(30, 30, 6, 6, TH_TILE_PASSABLE), 36);
        TH_CHECK_EQ(sim.hosp.balance, SALDO - 100);
    }
    // Atrás desde objects: las dos hojas se destruyen (Door:setTile(nil)) y
    // las paredes y banderas de las tres casillas vuelven a 0.
    preparar_salas();
    TH_CHECK_EQ(orden_sala("ward", 30, 30, 6, 6, NULL), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 33, 30, TH_DIR_NORTH), TH_REASON_OK);
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    TH_CHECK_EQ(sim.n_obj, 0);
    TH_CHECK_EQ(sim.rooms[0].door, TH_NONE);
    TH_CHECK_EQ(sim.rooms[0].door2, TH_NONE);
    TH_CHECK_EQ(bloque_n(32, 30), 0);
    TH_CHECK_EQ(bloque_n(33, 30), 0);
    TH_CHECK_EQ(bloque_n(34, 30), 0);
    TH_CHECK_EQ(bloque_n(31, 30), 0);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 32, 30), 0);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 33, 30), 0);
    TH_CHECK_EQ(banderas(33, 30) & (TH_TILE_DOOR_N | TH_TILE_TALL_N | TH_TILE_DO_NOT_IDLE), 0);
    TH_CHECK_EQ(banderas(32, 30) & (TH_TILE_DOOR_N | TH_TILE_TALL_N | TH_TILE_DO_NOT_IDLE), 0);
    TH_CHECK_EQ(paso_puerta(32, 29, 3, 2, 0), 6);
    TH_CHECK_EQ(marcadas(30, 30, 6, 6, 17, 0), 36);
    TH_CHECK_EQ(plano(30, 30, 6, 6, TH_TILE_BLUEPRINT), 36);
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
}

// La fase de objetos: colocar dentro de la sala (UIEditRoom:placeObject con
// el bloqueo blando del modo 2, po:_isNonSideObjectPlacementValid con
// room.is_active falso), la tienda de la sala (purchaseItems: los
// objects_additional, UIFurnishCorridor), coger (Object:onClick en edición:
// el objeto se destruye y vuelve a la lista sin cobrar), fin_sala
// (checkEnableConfirm y confirm: build_room = build_cost − objetos,
// markRoomAsBuilt y close) y las paredes compartidas con otra sala.
static void test_objetos_obra(void)
{
    preparar_salas();
    gp_hasta(TH_OBRA_WINDOWS);
    // En otra fase, nada de esto.
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "desk", 12, 11, TH_DIR_NORTH, 0, NULL), TH_REASON_FASE);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "plant", 0, 0, 0, 1, NULL), TH_REASON_FASE);
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 12, 11, 0, 0, NULL), TH_REASON_FASE);
    TH_CHECK_EQ(orden0(TH_ORD_FIN_SALA), TH_REASON_FASE);
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    int bad = 0;
    // Fuera de la sala: casillas (willObjectsFootprintTileBeWithinItsAllowedRoomIfLocatedAt).
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "desk", 20, 20, TH_DIR_NORTH, 0, &bad), TH_REASON_CASILLAS);
    TH_CHECK_EQ(bad, 1);
    // En la casilla de dentro de la puerta: no es buildable (Door:setTile).
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "desk", 11, 13, TH_DIR_NORTH, 0, &bad), TH_REASON_CASILLAS);
    TH_CHECK_EQ(bad, 1);
    // Dentro: vale, con la sala 1 y sin cobrar otra vez; la ventana de la
    // obra sigue abierta, vacía (UIEditRoom:placeObject no la cierra).
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "desk", 12, 11, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    const int mesa = objeto_en(12, 11);
    TH_CHECK(mesa >= 0);
    TH_CHECK_EQ(sim.obj[mesa].thob, 8);
    TH_CHECK_EQ(sim.obj[mesa].room, 1);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 100);
    TH_CHECK_EQ(sim.place.open, 1);
    TH_CHECK_EQ(sim.place.n, 0);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "desk", 12, 12, TH_DIR_NORTH, 0, NULL), TH_REASON_SIN_OBJETO);
    // La tienda de la sala: solo sus objects_additional (planta y radiador).
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "desk", 0, 0, 0, 1, NULL), TH_REASON_NO_ES_DE_SALA);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "bench", 0, 0, 0, 1, NULL), TH_REASON_NO_ES_DE_SALA);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "plant", 0, 0, 0, 2, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 110);
    TH_CHECK_EQ(sim.place.n, 1);
    TH_CHECK_EQ(sim.place.item[0].def, def("plant"));
    TH_CHECK_EQ(sim.place.item[0].qty, 2);
    TH_CHECK_EQ(sim.place.item[0].picked, 0);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "radiator", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 130);
    TH_CHECK_EQ(sim.place.n, 2);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "plant", 0, 0, 0, 0, NULL), TH_REASON_OK); // confirm sin cambios
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 130);
    TH_CHECK_EQ(sim.place.item[0].qty, 2);
    // sin_dinero: el saldo tiene que llegar al total de lo nuevo
    // (UIFurnishCorridor:purchaseItem); si no, no se compra nada.
    sim.hosp.balance = 9;
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "plant", 0, 0, 0, 2, NULL), TH_REASON_SIN_DINERO);
    TH_CHECK_EQ(sim.hosp.balance, 9);
    TH_CHECK_EQ(sim.place.item[0].qty, 2);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "plant", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, 4);
    TH_CHECK_EQ(sim.place.item[0].qty, 3);
    sim.hosp.balance = SALDO - 135;
    // maximo: 99 en la fila; la pulsación número 100 falla.
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "plant", 0, 0, 0, 96, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.place.item[0].qty, 99);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 615);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "plant", 0, 0, 0, 1, NULL), TH_REASON_MAXIMO);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 615);
    TH_CHECK_EQ(sim.place.item[0].qty, 99);
    // Una planta sobre la casilla de uso del escritorio le quita el acceso a
    // la puerta (wouldObjectBreakRoomObjectsAccessToTheRoomDoor): bloqueo.
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 12, 12, TH_DIR_NORTH, 0, &bad), TH_REASON_BLOQUEO);
    TH_CHECK_EQ(bad, 0);
    // Coger el escritorio: se destruye y vuelve a la lista sin cobrar, como
    // fila activa con su orientación; faltan_objetos hasta volverlo a poner.
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 12, 11, 0, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[mesa].def, 0);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 12, 11), 0);
    TH_CHECK_EQ(banderas(12, 11) & TH_TILE_BUILDABLE, TH_TILE_BUILDABLE);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 615);
    TH_CHECK_EQ(sim.place.n, 3);
    const int fd = fila("desk");
    TH_CHECK(fd >= 0);
    TH_CHECK_EQ(sim.place.item[fd].qty, 1);
    TH_CHECK_EQ(sim.place.item[fd].picked, 1);
    TH_CHECK_EQ(sim.place.item[fd].existing, 0);
    TH_CHECK_EQ(sim.place.active, fd);
    TH_CHECK_EQ(sim.place.orient, TH_DIR_NORTH);
    TH_CHECK_EQ(orden0(TH_ORD_FIN_SALA), TH_REASON_FALTAN_OBJETOS);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_OBJECTS);
    // Ni la puerta ni un objeto del pasillo se cogen desde la obra
    // (Object:onClick: «not room and window»).
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 11, 14, 0, 0, NULL), TH_REASON_NO_SE_PUEDE);
    const int fuera = th_sim_new_object(&sim, def("plant"), 20, 20, TH_DIR_NORTH);
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 20, 20, 0, 0, NULL), TH_REASON_NO_SE_PUEDE);
    th_sim_destroy_object(&sim, fuera);
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 12, 12, 0, 0, NULL), TH_REASON_SIN_OBJETO);
    // Colocar lo cogido y una planta de la lista no cobra (ya pagados).
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "desk", 12, 11, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 13, 12, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.obj[objeto_en(13, 12)].room, 1);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 615);
    TH_CHECK_EQ(sim.place.n, 2);
    // fin_sala: cobra build_cost − 100 = 2.280 (build_room), termina la sala
    // (built, is_active) y cierra la ventana: devuelve lo no colocado
    // (98 plantas y el radiador, sell_object a precio entero).
    n_dinero = 0;
    TH_CHECK_EQ(orden0(TH_ORD_FIN_SALA), TH_REASON_OK);
    TH_CHECK_EQ(n_dinero, 1 + 98 + 1);
    TH_CHECK_EQ(dinero[0].amount, -2280);
    TH_CHECK_EQ(dinero[0].reason, TH_MONEY_BUILD_ROOM);
    TH_CHECK_EQ(dinero[0].who, (int)def_sala("gp"));
    TH_CHECK_EQ(dinero[1].amount, 5);
    TH_CHECK_EQ(dinero[1].reason, TH_MONEY_SELL_OBJECT);
    TH_CHECK_EQ(dinero[1].who, (int)def("plant"));
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 615 - 2280 + 490 + 20);
    TH_CHECK_EQ(sim.hosp.money_out, 100 + 10 + 20 + 5 + 480 + 2280);
    TH_CHECK_EQ(sim.hosp.money_in, 490 + 20);
    TH_CHECK_EQ(sim.hosp.value, 2280 + 100 + 5);
    TH_CHECK_EQ(sim.rooms[0].built, 1);
    TH_CHECK_EQ(sim.rooms[0].is_active, 1);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(sim.n_rooms, 1);
    TH_CHECK_EQ(sim.obj[objeto_en(12, 11)].room, 1);
    TH_CHECK_EQ(sim.obj[objeto_en(13, 12)].thob, 45);
    TH_CHECK_EQ(orden0(TH_ORD_FIN_SALA), TH_REASON_FASE);
    // El plano de otra sala sobre esta: casillas (P2); tampoco sobre la
    // casilla de fuera de su puerta, que no es buildable.
    TH_CHECK_EQ(orden_sala("gp", 12, 12, 4, 4, &bad), TH_REASON_CASILLAS);
    TH_CHECK_EQ(bad, 4);
    TH_CHECK_EQ(orden_sala("gp", 10, 14, 4, 4, &bad), TH_REASON_CASILLAS);
    TH_CHECK_EQ(bad, 1);
    // Las casillas de las paredes este y sur del plano no se miran: una sala
    // pegada por el oeste comparte la pared de la sala 1, que se deja como
    // está (fuera, «la del vecino»: finishRoom 520-522), y al abandonarla
    // sigue ahí (_remove_wall_line).
    TH_CHECK_EQ(orden_sala("gp", 6, 10, 4, 4, &bad), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 7, 13, TH_DIR_SOUTH), TH_REASON_OK);
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.n_rooms, 2);
    TH_CHECK_EQ(sim.rooms[1].num, 2);
    // Desde esta obra no se puede coger la planta de la sala 1 aunque sea un
    // objeto de pasillo: el hit test del mundo solo da lo que está en la sala
    // en obra (setWorldHitTest(self.room), game_ui.lua:437-444).
    TH_CHECK_EQ(orden(TH_ORD_RECOGER, NULL, 13, 12, 0, 0, NULL), TH_REASON_NO_SE_PUEDE);
    TH_CHECK_EQ(sim.obj[objeto_en(13, 12)].thob, 45);
    for (int y = 10; y <= 13; y++)
        TH_CHECK_EQ(bloque_w(10, y), 83);
    TH_CHECK_EQ(bloque_n(6, 10), 82);
    TH_CHECK_EQ(bloque_w(6, 10), 83);
    TH_CHECK_EQ(bloque_n(9, 10), 82);
    TH_CHECK_EQ(bloque_w(6, 12), 83);
    TH_CHECK_EQ(bloque_n(6, 14), 90);
    TH_CHECK_EQ(bloque_n(7, 14), 0);
    TH_CHECK_EQ(bloque_n(9, 14), 90);
    TH_CHECK_EQ(marcadas(6, 10, 4, 4, 18, 2), 16);
    TH_CHECK_EQ(marcadas(10, 10, 4, 4, 18, 1), 16);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 615 - 2280 + 510 - 100);
    TH_CHECK_EQ(orden0(TH_ORD_CERRAR), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 615 - 2280 + 510);
    for (int y = 10; y <= 13; y++)
        TH_CHECK_EQ(bloque_w(10, y), 83);
    TH_CHECK_EQ(bloque_n(6, 10), 0);
    TH_CHECK_EQ(bloque_w(6, 10), 0);
    TH_CHECK_EQ(bloque_n(9, 10), 0);
    TH_CHECK_EQ(bloque_n(6, 14), 0);
    TH_CHECK_EQ(marcadas(6, 10, 4, 4, 17, 0), 16);
    TH_CHECK_EQ(marcadas(10, 10, 4, 4, 18, 1), 16);
    TH_CHECK_EQ(plano(6, 10, 4, 4, TH_TILE_PASSABLE), 16);
    // cerrar es Escape (setEditRoom(false) → verifyOrAbortRoom): la sala 2,
    // sin terminar, sale de world.rooms (abortRoom); la 1 sigue como estaba.
    TH_CHECK_EQ(sim.n_rooms, 1);
    TH_CHECK_EQ(sim.rooms[0].num, 1);
    TH_CHECK_EQ(sim.rooms[0].is_active, 1);
}

// cerrar (UIEditRoom:close, edit_room.lua:98-124): en objects con todo lo
// obligatorio colocado confirma la sala; si no, cancela hasta walls (la sala
// sale del mapa y todo se devuelve, sin dinero por la sala, que no estaba
// pagada) y la abandona. Es lo que hace la interfaz al cerrar la obra con
// Escape o al abrir otra ventana desde el panel: GameUI:setEditRoom(false) →
// verifyOrAbortRoom (game_ui.lua:1259-1284; edit_room.lua:131-164), que con
// abortRoom quita la sala de world.rooms; UIEditRoom:close() a secas, que la
// dejaría en world.rooms sin levantar, no lo llama la interfaz nunca.
// La tienda de la obra (er:purchaseItems): un opcional con StartAvail 0 se
// vende si no es investigable (sin research_category no tiene entrada en
// research_progress: la videoconsola del nivel 5) y no, si lo es (discovered
// empieza en StartAvail). Y el tope de la lista (TH_SIM_PLACE_ITEMS) antes de
// cobrar.
static void test_tienda_descubierta(void)
{
    preparar_salas();
    gp_hasta(TH_OBRA_OBJECTS);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "analyser", 0, 0, 0, 1, NULL), TH_REASON_NO_ES_DE_SALA);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "sofa", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 100 - 150);
    TH_CHECK_EQ(sim.place.n, 2); // el escritorio obligatorio y el sofá
    TH_CHECK_EQ(sim.place.item[1].def, def("sofa"));
    // Con la lista llena, un tipo nuevo es lleno y no cobra; uno que ya está, sí.
    const int n = sim.place.n;
    sim.place.n = TH_SIM_PLACE_ITEMS;
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "plant", 0, 0, 0, 1, NULL), TH_REASON_LLENO);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 250);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "sofa", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 400);
    sim.place.n = (uint8_t)n;
    TH_CHECK_EQ(sim.place.item[1].qty, 2);
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_OK); // se devuelven el escritorio y los dos sofás
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
}

static void test_cerrar_obra(void)
{
    preparar_salas();
    gp_hasta(TH_OBRA_OBJECTS);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "desk", 12, 11, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "plant", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden0(TH_ORD_CERRAR), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 105 - 2280 + 5);
    TH_CHECK_EQ(sim.hosp.value, 2380);
    TH_CHECK_EQ(sim.rooms[0].built, 1);
    TH_CHECK_EQ(sim.rooms[0].is_active, 1);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(sim.n_rooms, 1);
    TH_CHECK(objeto_en(12, 11) >= 0);
    TH_CHECK_EQ(orden0(TH_ORD_CERRAR), TH_REASON_SIN_VENTANA);
    // Sin el escritorio colocado.
    preparar_salas();
    gp_hasta(TH_OBRA_WINDOWS);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 12, 10, TH_DIR_NORTH), TH_REASON_OK);
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "plant", 0, 0, 0, 2, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "plant", 13, 12, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 110);
    TH_CHECK_EQ(sim.n_obj, 2);
    n_dinero = 0;
    TH_CHECK_EQ(orden0(TH_ORD_CERRAR), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(sim.place.open, 0);
    // Todo devuelto a precio entero (sell_object): el escritorio y las dos
    // plantas, también la colocada.
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
    TH_CHECK_EQ(sim.hosp.value, 0);
    TH_CHECK_EQ(sim.hosp.money_in, 110);
    TH_CHECK_EQ(n_dinero, 3);
    TH_CHECK_EQ(dinero[0].reason, TH_MONEY_SELL_OBJECT);
    // Sin sala en el mapa: paredes a 0, suelo original, sin room,
    // transitable; la puerta y la planta destruidas, con sus banderas.
    TH_CHECK_EQ(bloque_n(10, 10), 0);
    TH_CHECK_EQ(bloque_w(10, 10), 0);
    TH_CHECK_EQ(bloque_n(12, 10), 0);
    TH_CHECK_EQ(bloque_n(10, 14), 0);
    TH_CHECK_EQ(bloque_w(14, 11), 0);
    TH_CHECK_EQ(marcadas(10, 10, 4, 4, 17, 0), 16);
    TH_CHECK_EQ(plano(10, 10, 4, 4, TH_TILE_PASSABLE), 16);
    TH_CHECK_EQ(sim.n_obj, 0);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 11, 14), 0);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 13, 12), 0);
    TH_CHECK_EQ(banderas(11, 14) & (TH_TILE_DOOR_N | TH_TILE_TALL_N | TH_TILE_DO_NOT_IDLE), 0);
    TH_CHECK_EQ(paso_puerta(11, 13, 1, 2, 0), 2);
    TH_CHECK_EQ(banderas(13, 12) & TH_TILE_BUILDABLE, TH_TILE_BUILDABLE);
    TH_CHECK_EQ(banderas(12, 10) & TH_TILE_TRAVEL_N, TH_TILE_TRAVEL_N);
    // La sala sale de world.rooms (abortRoom: rooms[id] = nil); la siguiente
    // vuelve a ser la 1 (#rooms + 1).
    TH_CHECK_EQ(sim.n_rooms, 0);
    TH_CHECK_EQ(orden_sala("gp", 20, 20, 4, 4, NULL), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 21, 23, TH_DIR_SOUTH), TH_REASON_OK);
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.n_rooms, 1);
    TH_CHECK_EQ(sim.rooms[0].num, 1);
    TH_CHECK_EQ(th_map_tile(&mapa, 21, 21)->room, 1);
}

// atrás (UIEditRoom:cancel, edit_room.lua:171-204): objects → door
// (returnToDoorPhase, 728-751: la sala sale del mapa, los objetos vuelven a
// la lista y se devuelven, y al volver a la fase de objetos se cobran otra
// vez: objects_backup); door y windows → walls (returnToWallPhase: se pierden
// la puerta y las ventanas y el plano vuelve a ser transitable); walls →
// abortRoom (sin dinero: la obra no está pagada; la sala, si la hubo, sale
// de world.rooms).
static void test_atras(void)
{
    preparar_salas();
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_SIN_VENTANA);
    // windows → walls → abandonar.
    gp_hasta(TH_OBRA_WINDOWS);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 12, 10, TH_DIR_NORTH), TH_REASON_OK);
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WALLS);
    TH_CHECK_EQ(sim.obra.has_door, 0);
    TH_CHECK_EQ(sim.obra.n_windows, 0);
    TH_CHECK_EQ(plano(10, 10, 4, 4, TH_TILE_PASSABLE), 16);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 11, 13, TH_DIR_SOUTH), TH_REASON_FASE);
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(sim.n_rooms, 0);
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
    // objects → door.
    gp_hasta(TH_OBRA_WINDOWS);
    TH_CHECK_EQ(borde(TH_ORD_VENTANA, 12, 10, TH_DIR_NORTH), TH_REASON_OK);
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "desk", 12, 11, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "plant", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 105);
    n_dinero = 0;
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    TH_CHECK_EQ(sim.obra.has_door, 0);
    TH_CHECK_EQ(sim.obra.n_windows, 0);
    TH_CHECK_EQ(sim.obra.room, 0); // la sala se reutiliza (Room:initRoom con la misma)
    TH_CHECK_EQ(sim.n_rooms, 1);
    TH_CHECK_EQ(sim.rooms[0].built, 0);
    TH_CHECK_EQ(sim.rooms[0].door, TH_NONE);
    // El dinero de los dos vuelve (sell_object a precio entero) y el valor
    // del hospital, a 0.
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
    TH_CHECK_EQ(sim.hosp.value, 0);
    TH_CHECK_EQ(n_dinero, 2);
    TH_CHECK_EQ(dinero[0].reason, TH_MONEY_SELL_OBJECT);
    TH_CHECK_EQ(dinero[1].reason, TH_MONEY_SELL_OBJECT);
    TH_CHECK_EQ(dinero[0].amount + dinero[1].amount, 105);
    // La sala sale del mapa: paredes, suelo, room, puerta y escritorio; el
    // plano, cerrado otra vez (enterDoorPhase).
    TH_CHECK_EQ(bloque_n(10, 10), 0);
    TH_CHECK_EQ(bloque_w(10, 10), 0);
    TH_CHECK_EQ(bloque_n(12, 10), 0);
    TH_CHECK_EQ(bloque_w(10, 12), 0);
    TH_CHECK_EQ(bloque_n(10, 14), 0);
    TH_CHECK_EQ(bloque_n(11, 14), 0);
    TH_CHECK_EQ(bloque_w(14, 12), 0);
    TH_CHECK_EQ(marcadas(10, 10, 4, 4, 17, 0), 16);
    TH_CHECK_EQ(plano(10, 10, 4, 4, TH_TILE_BLUEPRINT), 16);
    TH_CHECK_EQ(sim.n_obj, 0);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 11, 14), 0);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 12, 11), 0);
    TH_CHECK_EQ(banderas(11, 14) & (TH_TILE_DOOR_N | TH_TILE_TALL_N | TH_TILE_DO_NOT_IDLE), 0);
    TH_CHECK_EQ(paso_puerta(11, 13, 1, 2, 0), 2);
    TH_CHECK_EQ(banderas(12, 11) & TH_TILE_BUILDABLE, TH_TILE_BUILDABLE);
    TH_CHECK_EQ(banderas(12, 9) & TH_TILE_TRAVEL_S, TH_TILE_TRAVEL_S); // la ventana ya no corta
    // La lista de la obra queda vacía, guardada en objects_backup.
    TH_CHECK_EQ(sim.place.open, 1);
    TH_CHECK_EQ(sim.place.n, 0);
    TH_CHECK_EQ(sim.obra.n_backup, 2);
    // Otra puerta y fin_ventanas: la misma sala (id 1) con la puerta nueva,
    // y los dos objetos cobrados otra vez.
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 12, 10, TH_DIR_NORTH), TH_REASON_OK);
    n_dinero = 0;
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 105);
    TH_CHECK_EQ(sim.hosp.value, 105);
    TH_CHECK_EQ(n_dinero, 2);
    TH_CHECK_EQ(dinero[0].reason, TH_MONEY_BUY_OBJECT);
    TH_CHECK_EQ(dinero[1].reason, TH_MONEY_BUY_OBJECT);
    TH_CHECK_EQ(sim.n_rooms, 1);
    TH_CHECK_EQ(sim.obra.room, 0);
    TH_CHECK_EQ(sim.rooms[0].num, 1);
    TH_CHECK_EQ(sim.rooms[0].door, objeto_en(12, 10));
    TH_CHECK_EQ(th_map_thob_at(&mapa, 12, 10, 0), 3);
    TH_CHECK_EQ(bloque_n(12, 10), 0);
    TH_CHECK_EQ(bloque_n(11, 14), 90); // ya no hay puerta al sur
    TH_CHECK_EQ(sim.place.n, 2);
    TH_CHECK(fila("desk") >= 0);
    TH_CHECK(fila("plant") >= 0);
    TH_CHECK_EQ(sim.obra.n_backup, 0);
    TH_CHECK_EQ(marcadas(10, 10, 4, 4, 18, 1), 16);
    // atrás hasta abandonar: la sala sale de world.rooms (abortRoom) y el
    // dinero vuelve, sin más apuntes por la sala.
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
    n_dinero = 0;
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WALLS);
    TH_CHECK_EQ(orden0(TH_ORD_ATRAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(n_dinero, 0);
    TH_CHECK_EQ(sim.n_rooms, 0);
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
    TH_CHECK_EQ(sim.hosp.value, 0);
    TH_CHECK_EQ(sim.n_obj, 0);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(marcadas(10, 10, 4, 4, 17, 0), 16);
    TH_CHECK_EQ(plano(10, 10, 4, 4, TH_TILE_PASSABLE), 16);
    TH_CHECK_EQ(bloque_n(12, 10), 0);
}

// vender y deshacer con una obra abierta se rechazan (restricción del
// vocabulario). comprar para el pasillo cierra la obra antes como
// GameUI:setEditRoom(false) → verifyOrAbortRoom (edit_room.lua:131-139): con
// todo colocado confirma la sala; si no, abortRoom (sin dinero: no estaba
// pagada; la sala sale de world.rooms); después compra.
static void test_vender_comprar_obra(void)
{
    preparar_salas();
    gp_hasta(TH_OBRA_DOOR);
    TH_CHECK_EQ(orden0(TH_ORD_VENDER), TH_REASON_NO_SE_PUEDE);
    TH_CHECK_EQ(orden0(TH_ORD_DESHACER), TH_REASON_NO_SE_PUEDE);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 11, 13, TH_DIR_SOUTH), TH_REASON_OK);
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(orden0(TH_ORD_VENDER), TH_REASON_NO_SE_PUEDE);
    TH_CHECK_EQ(orden0(TH_ORD_DESHACER), TH_REASON_NO_SE_PUEDE);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_OBJECTS);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 100);
    // Con el escritorio colocado: la sala se confirma (−2.280) y se compra
    // el banco (−40) para el pasillo.
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "desk", 12, 11, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 100 - 2280 - 40);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(sim.n_rooms, 1);
    TH_CHECK_EQ(sim.rooms[0].built, 1);
    TH_CHECK_EQ(sim.rooms[0].is_active, 1);
    TH_CHECK_EQ(sim.place.open, 1);
    TH_CHECK_EQ(sim.place.n, 1);
    TH_CHECK_EQ(sim.place.item[0].def, def("bench"));
    TH_CHECK_EQ(orden0(TH_ORD_CERRAR), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 2380);
    // Sin el escritorio: se abandona (todo devuelto) y se compra.
    preparar_salas();
    gp_hasta(TH_OBRA_OBJECTS);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR_SALA, "plant", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.hosp.balance, SALDO - 40);
    TH_CHECK_EQ(sim.hosp.value, 40);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(sim.n_rooms, 0);
    TH_CHECK_EQ(sim.n_obj, 0);
    TH_CHECK_EQ(marcadas(10, 10, 4, 4, 17, 0), 16);
    TH_CHECK_EQ(plano(10, 10, 4, 4, TH_TILE_PASSABLE), 16);
    TH_CHECK_EQ(bloque_n(10, 10), 0);
    TH_CHECK_EQ(bloque_n(10, 14), 0);
    TH_CHECK_EQ(sim.place.open, 1);
    TH_CHECK_EQ(sim.place.n, 1);
    TH_CHECK_EQ(sim.place.item[0].def, def("bench"));
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "bench", 12, 11, TH_DIR_NORTH, 0, NULL), TH_REASON_OK); // ya es pasillo
}

// El tope del motor, que CorsixTH no tiene: con TH_SIM_ROOMS salas, sin cola
// libre para la puerta o sin hueco para sus objetos (dos en la doble),
// fin_ventanas da lleno y la obra se queda en windows sin tocar nada.
static void test_tope_salas(void)
{
    preparar_salas();
    gp_hasta(TH_OBRA_WINDOWS);
    sim.n_rooms = TH_SIM_ROOMS;
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_LLENO);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_WINDOWS);
    TH_CHECK_EQ(sim.n_obj, 0);
    TH_CHECK_EQ(sim.hosp.balance, SALDO);
    TH_CHECK_EQ(bloque_n(10, 10), 0);
    TH_CHECK_EQ(plano(10, 10, 4, 4, TH_TILE_BLUEPRINT), 16);
    sim.n_rooms = 0;
    // Sin cola libre para la puerta (como un mostrador: th_sim_object_fits).
    sim.n_queues = TH_SIM_QUEUES;
    for (int k = 0; k < TH_SIM_QUEUES; k++)
        sim.queues[k].max_size = 6;
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_LLENO);
    sim.queues[TH_SIM_QUEUES - 1].max_size = 0;
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.n_rooms, 1);
    TH_CHECK_EQ(sim.obj[sim.rooms[0].door].queue, TH_SIM_QUEUES - 1);
    // Un solo hueco de objeto: la doble necesita dos.
    preparar_salas();
    TH_CHECK_EQ(orden_sala("ward", 30, 30, 6, 6, NULL), TH_REASON_OK);
    TH_CHECK_EQ(borde(TH_ORD_PUERTA, 33, 30, TH_DIR_NORTH), TH_REASON_OK);
    for (int i = 1; i < TH_SIM_OBJECTS; i++)
        sim.obj[i].def = def("plant");
    sim.n_obj = TH_SIM_OBJECTS;
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_LLENO);
    sim.obj[1].def = 0;
    TH_CHECK_EQ(orden0(TH_ORD_FIN_VENTANAS), TH_REASON_OK);
    TH_CHECK_EQ(sim.rooms[0].door + sim.rooms[0].door2, 1); // los huecos 0 y 1
}

// ---------------------------------------------------------------------------
// Contratar (la demo jugable): el mercado (World:makeAvailableStaff y
// StaffProfile:randomise), TH_ORD_CONTRATAR (UIHireStaff:hire y
// UIPlaceStaff:onMouseUp), el personal que entra en una sala (room.lua:317-412),
// el aviso a quien espera sala (seek_room.lua: build_callback), las llegadas
// de una partida nueva (World:new) y discoverDisease (gp.lua:150-153).

static th_anims anims_personal;

// preparar_salas() más lo que load_level lee del nivel para el personal (lo
// del nivel 1: MinSalary 60/75/25/20, SalaryAdd, umbrales 250 y 750), un
// armario para la consulta y el mercado del mes 0 del nivel sintético
// (staff_levels: 3 médicos, 2 enfermeras, 1 bedel y 2 recepcionistas).
static void preparar_personal(void)
{
    preparar_salas();
    th_test_anims(&anims_personal);
    sim.w.anims = &anims_personal;
    static const int32_t minimo[4] = {60, 75, 25, 20};
    static const int32_t suma[8] = {0, 0, -30, 30, 40, 30, 100, 20};
    memcpy(sim.min_salary, minimo, sizeof minimo);
    memcpy(sim.salary_add, suma, sizeof suma);
    sim.salary_ability_div = 10;
    sim.doctor_threshold = 250000;
    sim.consultant_threshold = 750000;
    sim.max_salary = 2000;
    sim.seed = 1;
    th_rng_seed(&sim.rng, 1);
    sim.hosp.stop_procedure = TH_MICRO;
    sim.expertise = th_rules_get(&nivel, nivel.root, "expertise");
    th_sim_make_available_staff(&sim, 0);
}

// La orden con el resumen del perfil que hay ahora en ese índice (el que ve
// la interfaz), o 0 si no hay candidato.
static int contratar_def(int kind, int idx, int x, int y, uint32_t resumen)
{
    th_order o = {0, TH_ORD_CONTRATAR, (uint8_t)x, (uint8_t)y, (uint8_t)kind, (uint8_t)idx, 0, 0, resumen};
    th_order_result r;
    return th_build_apply(&sim, &o, &r);
}

static uint32_t resumen_de(int kind, int idx)
{
    const th_staff_profile *l;
    const int n = th_sim_candidates(&sim, kind, &l);
    return idx >= 0 && idx < n ? th_staff_profile_digest(&l[idx]) : 0;
}

static int contratar(int kind, int idx, int x, int y)
{
    return contratar_def(kind, idx, x, y, resumen_de(kind, idx));
}

// El último en world.entities (el último contratado).
static int ultima_persona(void)
{
    return sim.n_order ? sim.order[sim.n_order - 1] : -1;
}

static int tiene_accion(const th_humanoid *h, int kind)
{
    for (int i = 0; i < h->queue_len; i++)
        if (h->queue[i].kind == kind)
            return 1;
    return 0;
}

// Un GP de 4 × 4 en (10, 10) terminado: la puerta al sur (el objeto en
// (11, 14)), el escritorio en (12, 11) y un armario en (10, 10).
static void gp_terminada(void)
{
    gp_hasta(TH_OBRA_OBJECTS);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "desk", 12, 11, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden0(TH_ORD_FIN_SALA), TH_REASON_OK);
    TH_CHECK_EQ(sim.rooms[0].built, 1);
    TH_CHECK(th_sim_new_object(&sim, def("cabinet"), 10, 10, TH_DIR_NORTH) != TH_NONE);
}

// El mercado: las cuentas de staff_levels, los títulos y el sueldo de cada
// candidato, determinista y con su propio generador (s->rng no se toca).
static void test_mercado(void)
{
    preparar_personal();
    static const int cuantos[4] = {3, 2, 1, 2};
    for (int k = 0; k < TH_SIM_STAFF_CLASSES; k++)
        TH_CHECK_EQ(sim.n_available[k], cuantos[k]);
    const th_staff_profile *l = NULL;
    TH_CHECK_EQ(th_sim_candidates(&sim, TH_HUM_DOCTOR, &l), 3);
    TH_CHECK(l == sim.available[0]);
    TH_CHECK_EQ(th_sim_candidates(&sim, TH_HUM_RECEPTIONIST, &l), 2);
    TH_CHECK_EQ(th_sim_candidates(&sim, TH_HUM_PATIENT, &l), 0);
    TH_CHECK(l == NULL);
    TH_CHECK_EQ(th_sim_candidates(&sim, 9, NULL), 0);
    TH_CHECK_EQ(sim.market_month, 0);
    for (int k = 0; k < TH_SIM_STAFF_CLASSES; k++)
        for (int i = 0; i < sim.n_available[k]; i++) {
            const th_staff_profile *pf = &sim.available[k][i];
            const int kind = TH_HUM_DOCTOR + k;
            TH_CHECK_EQ(pf->kind, kind);
            TH_CHECK(pf->skill >= 0 && pf->skill < TH_MICRO);
            TH_CHECK(pf->attention >= 0 && pf->attention < TH_MICRO);
            TH_CHECK(pf->name_seed >= 1 && pf->name_seed <= 1009);
            // Sin cirujanos ni investigadores (tasas 0); junior y consultor
            // salen de parseSkillLevel, en todas las clases (en los médicos,
            // sin consultores: ConsRate 0). El sueldo es getFairWage.
            TH_CHECK_EQ(pf->titles & (TH_TITLE_SURGEON | TH_TITLE_RESEARCHER), 0);
            TH_CHECK_EQ((pf->titles & TH_TITLE_JUNIOR) != 0, pf->skill <= 250000);
            TH_CHECK_EQ((pf->titles & TH_TITLE_CONSULTANT) != 0, pf->skill >= 750000);
            if (kind == TH_HUM_DOCTOR)
                TH_CHECK(pf->skill < 750000);
            TH_CHECK_EQ(pf->wage, th_sim_fair_wage(&sim, kind, pf->skill, pf->titles));
            const int lo = kind == TH_HUM_DOCTOR ? 0 : kind == TH_HUM_NURSE ? 10 : kind == TH_HUM_HANDYMAN ? 15 : 13;
            const int hi = kind == TH_HUM_DOCTOR ? 9 : kind == TH_HUM_NURSE ? 12 : kind == TH_HUM_HANDYMAN ? 17 : 14;
            TH_CHECK(pf->hair >= lo && pf->hair <= hi && pf->face >= lo && pf->face <= hi && pf->chin >= lo &&
                     pf->chin <= hi);
            if (kind == TH_HUM_DOCTOR) {
                // Negro (5-9) con la capa 5 a 4; si no (0-4), 2.
                TH_CHECK_EQ(pf->layer5, pf->hair >= 5 ? 4 : 2);
                TH_CHECK_EQ(pf->hair >= 5, pf->face >= 5);
            } else {
                TH_CHECK_EQ(pf->layer5, 2);
                TH_CHECK_EQ(pf->titles & TH_TITLE_PSYCHIATRIST, 0);
            }
        }
    // Determinista y sin gastar el azar de la simulación.
    static th_staff_profile copia[TH_SIM_STAFF_CLASSES][TH_SIM_CANDIDATES];
    memcpy(copia, sim.available, sizeof copia);
    const uint32_t azar = sim.rng.s;
    th_sim_make_available_staff(&sim, 0);
    TH_CHECK_EQ(sim.rng.s, azar);
    TH_CHECK_EQ(memcmp(copia, sim.available, sizeof copia), 0);
    // El mes 2 sigue en la entrada 0; el 3 usa la 1, que solo trae Doctors:
    // el resto se busca hacia atrás (makeAvailableStaff y randomise).
    th_sim_make_available_staff(&sim, 2);
    TH_CHECK_EQ(sim.n_available[0], 3);
    TH_CHECK(memcmp(copia, sim.available, sizeof copia) != 0); // otro mes, otro mercado
    th_sim_make_available_staff(&sim, 3);
    TH_CHECK_EQ(sim.market_month, 3);
    TH_CHECK_EQ(sim.n_available[0], 2);
    TH_CHECK_EQ(sim.n_available[1], 2);
    TH_CHECK_EQ(sim.n_available[2], 1);
    TH_CHECK_EQ(sim.n_available[3], 2);
    // Otra semilla de escenario, otro mercado.
    sim.seed = 2;
    th_sim_make_available_staff(&sim, 0);
    TH_CHECK(memcmp(copia, sim.available, sizeof copia) != 0);
    TH_CHECK_EQ(sim.rng.s, azar);
    // getFairWage a mano (staff_profile.lua:240-255) con el nivel 1: los dos
    // del hito 4 (el médico psiquiatra de 0,697 cobra 204 y la recepcionista
    // de 0,369, 56), junior (−30: con el máximo de 0,25 da 70, y nunca menos
    // que MinSalary, así que un junior del nivel 1 cobra 75), un junior
    // psiquiatra (75 + 20 − 30 + 30), consultor (+100, sin los 30 de doctor)
    // y cirujano investigador.
    TH_CHECK_EQ(th_sim_fair_wage(&sim, TH_HUM_DOCTOR, 697476, TH_TITLE_PSYCHIATRIST), 204);
    TH_CHECK_EQ(th_sim_fair_wage(&sim, TH_HUM_RECEPTIONIST, 368725, 0), 56);
    TH_CHECK_EQ(th_sim_fair_wage(&sim, TH_HUM_DOCTOR, 200000, TH_TITLE_JUNIOR), 75);
    TH_CHECK_EQ(th_sim_fair_wage(&sim, TH_HUM_DOCTOR, 200000, TH_TITLE_JUNIOR | TH_TITLE_PSYCHIATRIST), 95);
    TH_CHECK_EQ(th_sim_fair_wage(&sim, TH_HUM_DOCTOR, 800000, TH_TITLE_CONSULTANT), 255);
    TH_CHECK_EQ(th_sim_fair_wage(&sim, TH_HUM_DOCTOR, 500000, TH_TITLE_SURGEON | TH_TITLE_RESEARCHER), 215);
    TH_CHECK_EQ(th_sim_fair_wage(&sim, TH_HUM_NURSE, 0, 0), 60);
    TH_CHECK_EQ(th_sim_fair_wage(&sim, TH_HUM_HANDYMAN, 999999, TH_TITLE_JUNIOR), 124); // los títulos, solo médicos
}

// TH_ORD_CONTRATAR en el pasillo: los rechazos por orden (candidato, dinero,
// sitio, hueco) y el alta (setProfile, addStaff, onPlaceInCorridor).
static void test_contratar(void)
{
    preparar_personal();
    const th_staff_profile d0 = sim.available[0][0], d1 = sim.available[0][1];
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 3, 20, 20), TH_REASON_SIN_CANDIDATO);
    TH_CHECK_EQ(contratar(TH_HUM_PATIENT, 0, 20, 20), TH_REASON_SIN_CANDIDATO);
    TH_CHECK_EQ(contratar(9, 0, 20, 20), TH_REASON_SIN_CANDIDATO);
    // hire: «balance < profile.wage» (hire_staff.lua:138).
    sim.hosp.balance = d0.wage - 1;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 20, 20), TH_REASON_SIN_DINERO);
    TH_CHECK_EQ(sim.hosp.balance, d0.wage - 1);
    // Fuera del hospital o del mapa, y en una casilla no transitable. Cada
    // no_se_puede manda al candidato 0 al final de su lista (hire más
    // cancelar); con tres médicos, tres seguidos dejan la lista como estaba.
    sim.hosp.balance = 100000;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 2, 2), TH_REASON_NO_SE_PUEDE);
    TH_CHECK_EQ(th_staff_profile_digest(&sim.available[0][0]), th_staff_profile_digest(&d1));
    TH_CHECK_EQ(th_staff_profile_digest(&sim.available[0][2]), th_staff_profile_digest(&d0));
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 200, 20), TH_REASON_NO_SE_PUEDE);
    th_map_tile(&mapa, 21, 20)->flags &= ~(uint32_t)TH_TILE_PASSABLE;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 21, 20), TH_REASON_NO_SE_PUEDE);
    th_map_tile(&mapa, 21, 20)->flags |= TH_TILE_PASSABLE;
    TH_CHECK_EQ(th_staff_profile_digest(&sim.available[0][0]), th_staff_profile_digest(&d0));
    sim.hosp.balance = d0.wage;
    // Sin hueco en hum: el tope del motor.
    for (int i = 0; i < TH_SIM_HUMANOIDS; i++)
        sim.hum[i].alive = 1;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 20, 20), TH_REASON_LLENO);
    for (int i = 0; i < TH_SIM_HUMANOIDS; i++)
        sim.hum[i].alive = 0;
    // Nada de lo rechazado tocó el dinero ni sacó a nadie del mercado.
    TH_CHECK_EQ(sim.n_available[0], 3);
    TH_CHECK_EQ(sim.n_order, 0);
    TH_CHECK_EQ(sim.hosp.money_out, 0);
    // Con el saldo justo se contrata: cobra el sueldo (hire_staff) sin
    // cambiar el valor, y el candidato sale de la lista (table.remove).
    n_dinero = 0;
    const uint32_t azar = sim.rng.s;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 20, 20), TH_REASON_OK);
    TH_CHECK(sim.rng.s != azar); // Humanoid:Humanoid: la salud
    TH_CHECK_EQ(sim.hosp.balance, 0);
    TH_CHECK_EQ(sim.hosp.money_out, d0.wage);
    TH_CHECK_EQ(sim.hosp.value, 0);
    const int w = ultima_persona();
    TH_CHECK_EQ(n_dinero, 1);
    TH_CHECK_EQ(dinero[0].amount, -d0.wage);
    TH_CHECK_EQ(dinero[0].reason, TH_MONEY_HIRE_STAFF);
    TH_CHECK_EQ(dinero[0].who, w);
    TH_CHECK_EQ(sim.n_available[0], 2);
    TH_CHECK_EQ(memcmp(&sim.available[0][0], &d1, sizeof d1), 0);
    TH_CHECK(w >= 0);
    if (w < 0)
        return;
    const th_person *p = &sim.hum[w];
    TH_CHECK_EQ(p->alive, 1);
    TH_CHECK_EQ(p->kind, TH_HUM_DOCTOR);
    TH_CHECK_EQ(p->u.s.skill, d0.skill);
    TH_CHECK_EQ(p->u.s.attention, d0.attention);
    TH_CHECK_EQ(p->u.s.wage, d0.wage);
    TH_CHECK_EQ(p->u.s.psychiatrist, (d0.titles & TH_TITLE_PSYCHIATRIST) != 0);
    TH_CHECK_EQ(p->u.s.junior, (d0.titles & TH_TITLE_JUNIOR) != 0);
    TH_CHECK_EQ(p->u.s.fatigue, 0);
    TH_CHECK_EQ(p->u.s.associated_desk, TH_NONE);
    TH_CHECK_EQ(p->h.layers[5], d0.layer5);
    TH_CHECK_EQ(p->h.tile_x, 20);
    TH_CHECK_EQ(p->h.tile_y, 20);
    TH_CHECK_EQ(p->h.in_room, 0);
    TH_CHECK(tiene_accion(&p->h, TH_ACTION_MEANDER)); // onPlaceInCorridor
    // Una enfermera en otra casilla del pasillo: sin dinero para ella.
    TH_CHECK_EQ(contratar(TH_HUM_NURSE, 0, 22, 20), TH_REASON_SIN_DINERO);
    sim.hosp.balance = 1000;
    TH_CHECK_EQ(contratar(TH_HUM_NURSE, 1, 22, 20), TH_REASON_OK);
    TH_CHECK_EQ(sim.n_available[1], 1);
    TH_CHECK_EQ(sim.hum[ultima_persona()].kind, TH_HUM_NURSE);
    TH_CHECK_EQ(sim.n_order, 2);
    sim.w.anims = NULL;
}

// La orden lleva el resumen del perfil (decisión 5 de docs/06_plan.md): con
// otro resumen, o con el mercado ya regenerado al cambiar de mes, el índice
// es otra persona y la orden se rechaza sin tocar nada.
static void test_contratar_resumen(void)
{
    preparar_personal();
    sim.hosp.balance = 100000;
    const th_staff_profile d0 = sim.available[0][0];
    const uint32_t visto = th_staff_profile_digest(&d0);
    TH_CHECK(visto != th_staff_profile_digest(&sim.available[0][1]));
    th_staff_profile otro = d0;
    otro.wage++;
    TH_CHECK(visto != th_staff_profile_digest(&otro));
    otro = d0;
    otro.hair ^= 1;
    TH_CHECK(visto != th_staff_profile_digest(&otro));
    TH_CHECK_EQ(contratar_def(TH_HUM_DOCTOR, 0, 20, 20, visto ^ 1), TH_REASON_SIN_CANDIDATO);
    TH_CHECK_EQ(contratar_def(TH_HUM_DOCTOR, 1, 20, 20, visto), TH_REASON_SIN_CANDIDATO);
    // end_month regenera el mercado (World:makeAvailableStaff): la orden
    // vista el mes anterior ya no vale.
    th_sim_make_available_staff(&sim, 1);
    TH_CHECK(memcmp(&sim.available[0][0], &d0, sizeof d0) != 0);
    TH_CHECK_EQ(contratar_def(TH_HUM_DOCTOR, 0, 20, 20, visto), TH_REASON_SIN_CANDIDATO);
    TH_CHECK_EQ(sim.n_order, 0);
    TH_CHECK_EQ(sim.hosp.money_out, 0);
    // Y vuelto al mes 0 (la misma semilla), la misma persona otra vez.
    th_sim_make_available_staff(&sim, 0);
    TH_CHECK_EQ(contratar_def(TH_HUM_DOCTOR, 0, 20, 20, visto), TH_REASON_OK);
    TH_CHECK_EQ(sim.hum[ultima_persona()].u.s.wage, d0.wage);
    // onPlaceInCorridor llama a updateSpeed (staff.lua:476): un junior anda
    // despacio y un consultor deprisa desde el primer momento (descansados:
    // los umbrales por defecto de th_sim_init, VeryTired 700 y CrackUpTired 800).
    sim.very_tired = 700;
    sim.crackup_tired = 800;
    sim.available[0][0].titles = TH_TITLE_JUNIOR;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 21, 20), TH_REASON_OK);
    TH_CHECK_EQ(sim.hum[ultima_persona()].h.speed, 0);
    TH_CHECK_EQ(sim.hum[ultima_persona()].h.slow_animation, 1);
    sim.available[0][0].titles = TH_TITLE_CONSULTANT;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 22, 20), TH_REASON_OK);
    TH_CHECK_EQ(sim.hum[ultima_persona()].h.speed, 2);
    TH_CHECK_EQ(sim.hum[ultima_persona()].h.slow_animation, 0);
    sim.w.anims = NULL;
}

// La recepcionista: soltada en el pasillo va al mostrador libre más cercano
// (Receptionist:onPlaceInCorridor) y lo reserva; sobre un mostrador sin
// recepcionista también vale, sobre uno atendido no, y en una sala tampoco.
// dialogHireStaff (bottom_panel.lua:784-787): el botón de contratar cierra la
// ventana de colocar del pasillo, devolviendo lo no colocado, o termina o
// abandona la obra, antes de mirar el candidato; un clic que no vale (más
// cancelar) devuelve el candidato al final de su lista; y TH_ORD_DEVOLVER es
// cancelar sin colocar.
static void test_contratar_cierra(void)
{
    preparar_personal();
    sim.hosp.balance = 10000;
    const int32_t saldo0 = sim.hosp.balance;
    // Un banco comprado sin colocar y un candidato que no existe: la ventana
    // se cierra y el banco se devuelve, aunque la orden se rechace.
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.place.open, 1);
    TH_CHECK(sim.hosp.balance < saldo0);
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 9, 20, 20), TH_REASON_SIN_CANDIDATO);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(sim.hosp.balance, saldo0);
    // Lo mismo con una contratación aceptada.
    const th_staff_profile d0 = sim.available[0][0];
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 20, 20), TH_REASON_OK);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(sim.hosp.balance, saldo0 - d0.wage);
    // Con una obra abierta (el plano aceptado, en la fase de la puerta), la
    // abandona: setEditRoom(false) → verifyOrAbortRoom → abortRoom.
    const int salas0 = sim.n_rooms;
    gp_hasta(TH_OBRA_DOOR);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_DOOR);
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 9, 20, 20), TH_REASON_SIN_CANDIDATO);
    TH_CHECK_EQ(sim.obra.phase, TH_OBRA_NONE);
    TH_CHECK_EQ(sim.n_rooms, salas0);
    TH_CHECK_EQ(th_map_tile(&mapa, 11, 11)->room, 0);
    // Un clic que no vale (en la calle) manda al candidato 0 al final.
    const int n = sim.n_available[0];
    const th_staff_profile c0 = sim.available[0][0], c1 = sim.available[0][1];
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 2, 2), TH_REASON_NO_SE_PUEDE);
    TH_CHECK_EQ(sim.n_available[0], n);
    TH_CHECK_EQ(th_staff_profile_digest(&sim.available[0][0]), th_staff_profile_digest(&c1));
    TH_CHECK_EQ(th_staff_profile_digest(&sim.available[0][n - 1]), th_staff_profile_digest(&c0));
    // Los demás rechazos no lo mueven: el saldo.
    sim.hosp.balance = 0;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 20, 20), TH_REASON_SIN_DINERO);
    TH_CHECK_EQ(th_staff_profile_digest(&sim.available[0][0]), th_staff_profile_digest(&c1));
    sim.hosp.balance = 10000;
}

static int devolver(int kind, int idx, uint32_t resumen)
{
    th_order o = {0, TH_ORD_DEVOLVER, 0, 0, (uint8_t)kind, (uint8_t)idx, 0, 0, resumen};
    th_order_result r;
    return th_build_apply(&sim, &o, &r);
}

static void test_devolver(void)
{
    preparar_personal();
    sim.hosp.balance = 10000;
    const int n = sim.n_available[0];
    const th_staff_profile c0 = sim.available[0][0], c1 = sim.available[0][1];
    // Sin candidato, con otro resumen, con otra clase o sin saldo: nada cambia.
    TH_CHECK_EQ(devolver(TH_HUM_DOCTOR, n, resumen_de(TH_HUM_DOCTOR, 0)), TH_REASON_SIN_CANDIDATO);
    TH_CHECK_EQ(devolver(TH_HUM_DOCTOR, 0, resumen_de(TH_HUM_DOCTOR, 1)), TH_REASON_SIN_CANDIDATO);
    TH_CHECK_EQ(devolver(TH_HUM_PATIENT, 0, resumen_de(TH_HUM_DOCTOR, 0)), TH_REASON_SIN_CANDIDATO);
    sim.hosp.balance = c0.wage - 1;
    TH_CHECK_EQ(devolver(TH_HUM_DOCTOR, 0, resumen_de(TH_HUM_DOCTOR, 0)), TH_REASON_SIN_DINERO);
    sim.hosp.balance = 10000;
    TH_CHECK_EQ(th_staff_profile_digest(&sim.available[0][0]), th_staff_profile_digest(&c0));
    // Devuelto: el 0 pasa al final y el 1 es ahora el 0; la plantilla y el
    // dinero no cambian.
    const int32_t saldo = sim.hosp.balance;
    TH_CHECK_EQ(devolver(TH_HUM_DOCTOR, 0, resumen_de(TH_HUM_DOCTOR, 0)), TH_REASON_OK);
    TH_CHECK_EQ(sim.n_available[0], n);
    TH_CHECK_EQ(th_staff_profile_digest(&sim.available[0][0]), th_staff_profile_digest(&c1));
    TH_CHECK_EQ(th_staff_profile_digest(&sim.available[0][n - 1]), th_staff_profile_digest(&c0));
    TH_CHECK_EQ(sim.n_order, 0);
    TH_CHECK_EQ(sim.hosp.balance, saldo);
    // El último, devuelto, se queda donde está.
    TH_CHECK_EQ(devolver(TH_HUM_DOCTOR, n - 1, resumen_de(TH_HUM_DOCTOR, n - 1)), TH_REASON_OK);
    TH_CHECK_EQ(th_staff_profile_digest(&sim.available[0][n - 1]), th_staff_profile_digest(&c0));
    // Y pasa antes por dialogHireStaff: cierra la ventana de colocar.
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "bench", 0, 0, 0, 1, NULL), TH_REASON_OK);
    TH_CHECK_EQ(sim.place.open, 1);
    TH_CHECK_EQ(devolver(TH_HUM_DOCTOR, 0, resumen_de(TH_HUM_DOCTOR, 0)), TH_REASON_OK);
    TH_CHECK_EQ(sim.place.open, 0);
    TH_CHECK_EQ(sim.hosp.balance, saldo);
}

static void test_contratar_recepcionista(void)
{
    preparar_personal();
    gp_terminada();
    TH_CHECK_EQ(orden(TH_ORD_COMPRAR, "reception_desk", 0, 0, 0, 2, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "reception_desk", 30, 30, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    TH_CHECK_EQ(orden(TH_ORD_COLOCAR, "reception_desk", 40, 30, TH_DIR_NORTH, 0, NULL), TH_REASON_OK);
    const int m1 = objeto_en(30, 30), m2 = objeto_en(40, 30);
    TH_CHECK(m1 >= 0 && m2 >= 0);
    if (m1 < 0 || m2 < 0)
        return;
    // En la consulta, no (allow_in_rooms es falso para ella).
    TH_CHECK_EQ(contratar(TH_HUM_RECEPTIONIST, 0, 12, 12), TH_REASON_NO_SE_PUEDE);
    TH_CHECK_EQ(th_build_staff_placement(&sim, TH_HUM_RECEPTIONIST, 12, 12), TH_REASON_NO_SE_PUEDE);
    TH_CHECK_EQ(th_build_staff_placement(&sim, TH_HUM_DOCTOR, 12, 12), TH_REASON_OK);
    // En el pasillo, junto al primero: lo reserva y va a su casilla de uso.
    TH_CHECK_EQ(contratar(TH_HUM_RECEPTIONIST, 0, 31, 33), TH_REASON_OK);
    const int r1 = ultima_persona();
    TH_CHECK_EQ(sim.hum[r1].kind, TH_HUM_RECEPTIONIST);
    TH_CHECK_EQ(sim.hum[r1].u.s.fatigue, -1);
    TH_CHECK_EQ(sim.obj[m1].reserved_for, r1);
    TH_CHECK_EQ(sim.hum[r1].u.s.associated_desk, m1);
    // El paso del deambular que ya empezó se queda (must_happen); detrás, el
    // camino a la casilla de uso secundaria (30, 31) y atender el mostrador.
    int hacia = 0;
    for (int i = 0; i < sim.hum[r1].h.queue_len; i++)
        hacia += sim.hum[r1].h.queue[i].kind == TH_ACTION_WALK && sim.hum[r1].h.queue[i].x == 30 &&
                 sim.hum[r1].h.queue[i].y == 31;
    TH_CHECK_EQ(hacia, 1);
    TH_CHECK(tiene_accion(&sim.hum[r1].h, TH_ACTION_STAFF_RECEPTION));
    TH_CHECK_EQ(sim.obj[m2].reserved_for, TH_NONE);
    // Sobre el primer mostrador (que no es transitable): vale mientras no
    // tenga receptionist (reserved_for no cuenta, place_staff.lua:118-121).
    TH_CHECK_EQ(th_build_staff_placement(&sim, TH_HUM_RECEPTIONIST, 30, 30), TH_REASON_OK);
    TH_CHECK_EQ(th_build_staff_placement(&sim, TH_HUM_DOCTOR, 30, 30), TH_REASON_NO_SE_PUEDE);
    sim.obj[m1].receptionist = (uint8_t)r1;
    TH_CHECK_EQ(th_build_staff_placement(&sim, TH_HUM_RECEPTIONIST, 30, 30), TH_REASON_NO_SE_PUEDE);
    // La segunda, sobre el otro mostrador: el más cercano libre es ese.
    TH_CHECK_EQ(contratar(TH_HUM_RECEPTIONIST, 0, 40, 30), TH_REASON_OK);
    const int r2 = ultima_persona();
    TH_CHECK_EQ(sim.obj[m2].reserved_for, r2);
    TH_CHECK_EQ(sim.hum[r2].u.s.associated_desk, m2);
    TH_CHECK_EQ(sim.n_available[3], 0);
    TH_CHECK_EQ(contratar(TH_HUM_RECEPTIONIST, 0, 41, 33), TH_REASON_SIN_CANDIDATO);
    sim.w.anims = NULL;
}

// Médicos en la consulta (room.lua:370-412): el primero, a la consulta vacía,
// es su staff_member; el segundo sustituye al de dentro si este no atiende a
// nadie (el viejo sale a deambular); si lo atiende, sale el nuevo. La
// enfermera no sirve: sale. Sin puerta, no se puede soltar dentro.
static void test_contratar_en_sala(void)
{
    preparar_personal();
    gp_terminada();
    th_room *r = &sim.rooms[0];
    TH_CHECK_EQ(r->staff_member, TH_NONE);
    // Sin puerta, nadie dentro (place_staff.lua:107-109).
    const uint8_t puerta = r->door;
    r->door = TH_NONE;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 10, 12), TH_REASON_NO_SE_PUEDE);
    r->door = puerta;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 10, 12), TH_REASON_OK);
    const int a = ultima_persona();
    TH_CHECK_EQ(r->staff_member, a);
    TH_CHECK_EQ(sim.hum[a].h.in_room, r->num);
    TH_CHECK_EQ(sim.hum[a].u.s.last_room, r->num);
    TH_CHECK(th_hset_has(&r->humanoids, a));
    TH_CHECK(tiene_accion(&sim.hum[a].h, TH_ACTION_USE_OBJECT)); // el ciclo del GP
    TH_CHECK(!tiene_accion(&sim.hum[a].h, TH_ACTION_MEANDER));
    // El segundo: el de dentro no atiende a nadie, así que lo sustituye.
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 13, 11), TH_REASON_OK);
    const int b = ultima_persona();
    TH_CHECK_EQ(r->staff_member, b);
    TH_CHECK(th_hset_has(&r->humanoids, b));
    TH_CHECK(tiene_accion(&sim.hum[b].h, TH_ACTION_USE_OBJECT));
    TH_CHECK(sim.hum[a].h.queue[0].flags & TH_AF_IS_LEAVING);
    TH_CHECK_EQ(sim.hum[a].h.queue[1].kind, TH_ACTION_MEANDER);
    // El tercero, con el de dentro atendiendo a un paciente: sale él.
    sim.hum[b].u.s.dealing_with_patient = 1;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 13, 12), TH_REASON_OK);
    const int c = ultima_persona();
    TH_CHECK_EQ(r->staff_member, b);
    TH_CHECK(th_hset_has(&r->humanoids, c));
    TH_CHECK(sim.hum[c].h.queue[0].flags & TH_AF_IS_LEAVING);
    TH_CHECK_EQ(sim.hum[c].h.queue[1].kind, TH_ACTION_MEANDER);
    TH_CHECK(!(sim.hum[b].h.queue[0].flags & TH_AF_IS_LEAVING));
    // Una enfermera no es para la consulta: sale.
    TH_CHECK_EQ(contratar(TH_HUM_NURSE, 0, 11, 11), TH_REASON_OK);
    const int n = ultima_persona();
    TH_CHECK_EQ(r->staff_member, b);
    TH_CHECK(sim.hum[n].h.queue[0].flags & TH_AF_IS_LEAVING);
    TH_CHECK_EQ(sim.hum[n].h.queue[1].kind, TH_ACTION_MEANDER);
    sim.w.anims = NULL;
}

// Un paciente con su SeekRoom en curso para el tipo dado: el estado que deja
// seek_room_start cuando no hay sala.
static int paciente_buscando(int x, int y, int tipo, int diagnosticado)
{
    int who = -1;
    for (int i = 0; i < TH_SIM_HUMANOIDS && who < 0; i++)
        if (!sim.hum[i].alive)
            who = i;
    th_person *p = &sim.hum[who];
    memset(p, 0, sizeof *p);
    p->alive = 1;
    p->kind = TH_HUM_PATIENT;
    p->happiness = TH_MICRO;
    p->u.p.waiting = -1;
    p->u.p.litter_countdown = -1;
    p->u.p.diagnosed = (uint8_t)diagnosticado;
    p->entity_seq = ++sim.entity_seq;
    sim.order[sim.n_order++] = (uint8_t)who;
    th_humanoid_init(&sim.w, &p->h, th_walk_anims_find("Standard Male Patient"));
    th_humanoid_set_tile(&sim.w, &p->h, x, y);
    th_action sr = th_action_new(TH_ACTION_SEEK_ROOM);
    sr.room_type = (uint8_t)tipo;
    if (diagnosticado)
        sr.state |= TH_AS_TREATMENT;
    th_humanoid_set_next_action(&sim.w, &p->h, sr);
    return who;
}

static const th_action *seek_room_de(int who)
{
    const th_humanoid *h = &sim.hum[who].h;
    for (int i = 0; i < h->queue_len; i++)
        if (h->queue[i].kind == TH_ACTION_SEEK_ROOM)
            return &h->queue[i];
    return NULL;
}

// build_callback (seek_room.lua:269-310): quien espera la consulta va a ella
// en cuanto se termina, sin esperar a acabar su paseo; quien espera otra
// sala, no.
static void test_aviso_sala(void)
{
    preparar_personal();
    const int a = paciente_buscando(45, 45, 0, 0); // la consulta (gp = 0)
    const int b = paciente_buscando(50, 45, 1, 1); // un tratamiento en la enfermería: fax y espera
    const th_action *sa = seek_room_de(a), *sb = seek_room_de(b);
    TH_CHECK(sa && (sa->state & TH_AS_DONE_INIT) && (sa->state & TH_AS_DONE_WALK));
    TH_CHECK(sb && (sb->state & TH_AS_DONE_INIT));
    TH_CHECK_EQ(sim.hum[a].u.p.next_room, 0);
    gp_terminada();
    TH_CHECK_EQ(sim.hum[a].u.p.next_room, sim.rooms[0].num);
    const th_humanoid *h = &sim.hum[a].h;
    TH_CHECK(h->queue_len >= 1 && h->queue[h->queue_len - 1].kind == TH_ACTION_WALK &&
             (h->queue[h->queue_len - 1].flags & TH_AF_IS_ENTERING));
    sa = seek_room_de(a);
    TH_CHECK(!sa || (sa->flags & TH_AF_TODO_INTERRUPT)); // y el aviso ya no está registrado
    TH_CHECK_EQ(sim.hum[b].u.p.next_room, 0);
    TH_CHECK(seek_room_de(b) != NULL);
    // La consulta pide médico (callForStaff): con nadie libre, la llamada espera.
    TH_CHECK_EQ(sim.rooms[0].call_open, 1);
    sim.w.anims = NULL;
}

// Una enfermedad sintética que se trata en la consulta (treat[0] = gp): la
// única forma de que checkDiseaseRequirements se cumpla con las salas que el
// núcleo modela hoy.
static void enfermedad_de_consulta(void)
{
    sim.n_dis = 1;
    sim.dis[0].id = "consulta";
    sim.dis[0].n_treat = 1;
    sim.dis[0].treat[0] = 0; // gp
}

// build_callback con un fax abierto: el paciente diagnosticado que buscaba
// tratamiento y mandó NO_TREATMENT va a la sala nueva y el fax desaparece
// (removeMessage, seek_room.lua:306).
static void test_aviso_sala_fax(void)
{
    preparar_personal();
    enfermedad_de_consulta();
    const int a = paciente_buscando(45, 45, 0, 1);
    TH_CHECK_EQ(sim.n_fax, 1);
    TH_CHECK_EQ(sim.fax[0].who, a);
    TH_CHECK_EQ(sim.hum[a].u.p.message, TH_FAX_NO_TREATMENT);
    gp_terminada();
    TH_CHECK_EQ(sim.hum[a].u.p.next_room, sim.rooms[0].num);
    TH_CHECK_EQ(sim.n_fax, 0);
    TH_CHECK_EQ(sim.n_shown, 0);
    TH_CHECK_EQ(sim.hum[a].u.p.message, 0);
    sim.w.anims = NULL;
}

// staff_change_callback (seek_room.lua:244-266): con la consulta construida
// pero sin que el paciente la encuentre al buscar (inactiva), al contratar a
// un médico vuelve a buscar, la encuentra y va, y su fax desaparece; una
// recepcionista o un bedel no cuentan (seek_room.lua:289-291).
static void test_aviso_personal(void)
{
    preparar_personal();
    enfermedad_de_consulta();
    gp_terminada();
    th_room *r = &sim.rooms[0];
    r->is_active = 0;
    const int a = paciente_buscando(45, 45, 0, 1);
    const th_action *sa = seek_room_de(a);
    TH_CHECK(sa && (sa->state & TH_AS_DONE_INIT));
    TH_CHECK_EQ(sim.n_fax, 1);
    TH_CHECK_EQ(sim.hum[a].u.p.message, TH_FAX_NO_TREATMENT);
    r->is_active = 1; // sin build_callback: solo el aviso del personal
    sim.hosp.balance = 100000;
    TH_CHECK_EQ(contratar(TH_HUM_RECEPTIONIST, 0, 20, 20), TH_REASON_OK);
    TH_CHECK_EQ(sim.hum[a].u.p.next_room, 0);
    TH_CHECK_EQ(contratar(TH_HUM_HANDYMAN, 0, 21, 20), TH_REASON_OK);
    TH_CHECK_EQ(sim.hum[a].u.p.next_room, 0);
    TH_CHECK_EQ(sim.n_fax, 1);
    TH_CHECK(seek_room_de(a) == sa);
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 22, 20), TH_REASON_OK);
    TH_CHECK_EQ(sim.hum[a].u.p.next_room, r->num);
    TH_CHECK_EQ(sim.n_fax, 0);
    TH_CHECK_EQ(sim.hum[a].u.p.message, 0);
    sim.w.anims = NULL;
}

// Room:getMissingStaff (room.lua:254-259): el médico de la sala que atiende
// la llamada de otra sala no cuenta, así que el contratado dentro cabe
// (staffFitsInRoom) y no se aplica la sustitución; un bedel que viene por una
// llamada (on_call) no recibe AnswerCallAction (room.lua:343-356).
static void test_falta_personal_llamada(void)
{
    preparar_personal();
    gp_terminada();
    th_room *r = &sim.rooms[0];
    sim.hosp.balance = 100000;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 10, 12), TH_REASON_OK);
    const int a = ultima_persona();
    TH_CHECK_EQ(r->staff_member, a);
    sim.hum[a].u.s.on_call = (uint8_t)(r->num + 1); // la llamada de otra sala
    sim.hum[a].u.s.dealing_with_patient = 1;        // sin on_call: el nuevo saldría
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 13, 11), TH_REASON_OK);
    const int b = ultima_persona();
    TH_CHECK_EQ(r->staff_member, b);
    TH_CHECK(!(sim.hum[b].h.queue[0].flags & TH_AF_IS_LEAVING));
    TH_CHECK(tiene_accion(&sim.hum[b].h, TH_ACTION_USE_OBJECT));
    // El bedel que entra por una llamada (on_call) no cambia de acción; sin
    // ella, AnswerCallAction sustituye a la que tenía.
    TH_CHECK_EQ(contratar(TH_HUM_HANDYMAN, 0, 20, 20), TH_REASON_OK); // en el pasillo
    const int h = ultima_persona();
    const uint16_t antes = sim.hum[h].h.queue[0].serial;
    sim.hum[h].u.s.on_call = r->num;
    th_sim_room_on_enter(&sim, r, h);
    TH_CHECK(th_hset_has(&r->humanoids, h));
    TH_CHECK_EQ(sim.hum[h].h.queue[0].serial, antes);
    TH_CHECK(!tiene_accion(&sim.hum[h].h, TH_ACTION_ANSWER_CALL));
    th_hset_del(&r->humanoids, h);
    sim.hum[h].u.s.on_call = 0;
    th_sim_room_on_enter(&sim, r, h);
    TH_CHECK(tiene_accion(&sim.hum[h].h, TH_ACTION_ANSWER_CALL));
    sim.w.anims = NULL;
}

// GPRoom:dealtWithPatient con el diagnóstico completo: setDiagnosed, la
// búsqueda del tratamiento y, si la enfermedad no estaba descubierta,
// discoverDisease (el libro de casos la marca).
static void test_descubrir(void)
{
    preparar_personal();
    gp_terminada();
    sim.n_dis = 1;
    sim.dis[0].id = "bloaty_head";
    sim.dis[0].expertise_id = 1;
    sim.dis[0].n_treat = 1;
    sim.dis[0].treat[0] = 1;
    sim.n_case = 1;
    sim.casebook[0].id = "bloaty_head";
    sim.casebook[0].price = TH_MICRO;
    sim.casebook[0].reputation = 500;
    sim.hosp.balance = 1000;
    TH_CHECK_EQ(contratar(TH_HUM_DOCTOR, 0, 10, 12), TH_REASON_OK);
    const int doc = ultima_persona();
    th_room *r = &sim.rooms[0];
    TH_CHECK_EQ(r->staff_member, doc);
    // El paciente en la silla de la consulta, con el diagnóstico ya hecho.
    const int pac = paciente_buscando(11, 11, 0, 0);
    th_person *p = &sim.hum[pac];
    p->h.in_room = r->num;
    th_hset_add(&r->humanoids, pac);
    p->h.user_of = (uint8_t)objeto_en(12, 11);
    p->u.p.diagnosis_progress = TH_MICRO;
    r->max_times = 0;
    th_action vuelta = th_action_new(TH_ACTION_USE_OBJECT);
    vuelta.val = 1;
    TH_CHECK_EQ(sim.casebook[0].discovered, 0);
    th_sim_gp_loop_callback(&sim, doc, &vuelta);
    TH_CHECK_EQ(p->u.p.diagnosed, 1);
    TH_CHECK_EQ(sim.casebook[0].discovered, 1);
    TH_CHECK(tiene_accion(&p->h, TH_ACTION_SEEK_ROOM));
    sim.w.anims = NULL;
}

// Las definiciones con las tablas de caminar y de puertas de humanoid.c, que
// th_sim_init comprueba (load_definitions), y un TRG para ellas.
static uint8_t trg_defs_init[32768], trg_escena[2048];
static char json_defs_init[24576];

static size_t defs_init_trg(void)
{
    size_t n = strlen(definiciones) - 1; // sin la llave del final
    memcpy(json_defs_init, definiciones, n);
    int k = (int)n;
    const int cap = (int)sizeof json_defs_init;
    k += snprintf(json_defs_init + k, (size_t)(cap - k), ", \"caminar\": {");
    for (unsigned i = 0; th_walk_anims_at(i); i++) {
        const th_walk_anims *a = th_walk_anims_at(i);
        k += snprintf(json_defs_init + k, (size_t)(cap - k),
                      "%s\"%s\": {\"walk_north\": %u, \"walk_east\": %u, \"idle_north\": %u, \"idle_east\": %u}",
                      i ? ", " : "", a->name, a->walk_north, a->walk_east, a->idle_north, a->idle_east);
    }
    k += snprintf(json_defs_init + k, (size_t)(cap - k), "}, \"puertas\": {");
    for (unsigned i = 0; th_walk_anims_at(i); i++) {
        const th_walk_anims *a = th_walk_anims_at(i);
        k += snprintf(json_defs_init + k, (size_t)(cap - k),
                      "%s\"%s\": {\"leaving\": %u, \"entering\": %u, \"knock_north\": %u, \"knock_east\": %u,"
                      " \"leaving_swing\": %u, \"entering_swing\": %u}",
                      i ? ", " : "", a->name, a->door_leaving, a->door_entering, a->knock_north, a->knock_east,
                      a->swing_leaving, a->swing_entering);
    }
    k += snprintf(json_defs_init + k, (size_t)(cap - k), "}}");
    if (k >= cap)
        return 0;
    return th_test_trg(json_defs_init, trg_defs_init, sizeof trg_defs_init);
}

// Una partida nueva por th_sim_init (World:new, world.lua:151-157): sin
// spawn_dates ni spawn_rate, el ritmo es popn[0].Change y las llegadas del
// primer mes se calculan ya (con force_arrival, al menos una); con
// spawn_dates, las del escenario. El mercado es el del mes 0 y se regenera
// al acabar el mes (con el monthOfGame del mes que acaba).
static void test_partida_nueva(void)
{
    const size_t nd = defs_init_trg();
    TH_CHECK(nd > 0);
    if (!nd)
        return;
    static th_rules defs_init, escena;
    TH_CHECK_EQ(th_rules_init(&defs_init, trg_defs_init, nd), 0);
    th_test_anims(&anims_personal);
    static const char *const escenas[2] = {
        "{\"fecha\": {\"anio\": 1, \"mes\": 1, \"dia\": 1, \"hora\": 0},"
        " \"mundo\": {\"hours_per_tick\": 1, \"popn\": {\"#0\": {\"Month\": 0, \"Change\": 3},"
        "  \"#1\": {\"Month\": 1, \"Change\": 1}}},"
        " \"hospital\": {\"saldo\": 40000, \"valor\": 43525, \"reputacion\": 500, \"abierto\": true},"
        " \"parcelas\": [1]}",
        "{\"fecha\": {\"anio\": 1, \"mes\": 1, \"dia\": 1, \"hora\": 0},"
        " \"mundo\": {\"hours_per_tick\": 1, \"spawn_rate\": 24, \"monthly_spawn_increase\": 0,"
        "  \"spawn_dates\": {\"#5\": 2, \"#9\": 1}, \"popn\": {\"#0\": {\"Month\": 0, \"Change\": 3}}},"
        " \"hospital\": {\"saldo\": 40000, \"abierto\": true}, \"parcelas\": [1]}"};
    uint8_t fechas[2][32];
    for (int e = 0; e < 2; e++) {
        preparar();
        const size_t ne = th_test_trg(escenas[e], trg_escena, sizeof trg_escena);
        TH_CHECK(ne > 0);
        if (!ne)
            return;
        TH_CHECK_EQ(th_rules_init(&escena, trg_escena, ne), 0);
        TH_CHECK_EQ(th_sim_init(&sim, &mapa, &buscador, &anims_personal, &defs_init, &nivel, &escena, 7), 0);
        memcpy(fechas[e], sim.spawn_dates, sizeof fechas[e]);
        int total = 0;
        for (int d = 1; d <= 31; d++)
            total += sim.spawn_dates[d];
        if (e == 0) {
            TH_CHECK_EQ(sim.spawn_rate, 3 * TH_MICRO);
            TH_CHECK_EQ(sim.monthly_spawn_increase, 3 * TH_MICRO);
            TH_CHECK(total >= 1);
            TH_CHECK_EQ(sim.market_month, 0);
            TH_CHECK_EQ(sim.n_available[0], 3);
            TH_CHECK_EQ(sim.n_available[3], 2);
            TH_CHECK_EQ(sim.seed, 7);
            // Lo mismo otra vez: determinista.
            preparar();
            TH_CHECK_EQ(th_sim_init(&sim, &mapa, &buscador, &anims_personal, &defs_init, &nivel, &escena, 7), 0);
            TH_CHECK_EQ(memcmp(fechas[0], sim.spawn_dates, sizeof fechas[0]), 0);
            // Al acabar enero: el mercado de monthOfGame 1 y el ritmo + 1.
            const th_staff_profile primero = sim.available[0][0];
            while (sim.date.month == 1)
                th_sim_tick(&sim);
            TH_CHECK_EQ(sim.market_month, 1);
            TH_CHECK_EQ(sim.spawn_rate, 4 * TH_MICRO);
            TH_CHECK(memcmp(&primero, &sim.available[0][0], sizeof primero) != 0);
            TH_CHECK_EQ(sim.n_available[0], 3);
        } else {
            TH_CHECK_EQ(sim.spawn_rate, 24 * TH_MICRO);
            TH_CHECK_EQ(total, 3);
            TH_CHECK_EQ(sim.spawn_dates[5], 2);
            TH_CHECK_EQ(sim.spawn_dates[9], 1);
        }
    }
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
    test_sala_plano();
    test_puerta();
    test_ventana();
    test_fin_ventanas();
    test_puerta_doble();
    test_objetos_obra();
    test_tienda_descubierta();
    test_cerrar_obra();
    test_atras();
    test_vender_comprar_obra();
    test_tope_salas();
    test_mercado();
    test_contratar();
    test_contratar_resumen();
    test_contratar_cierra();
    test_devolver();
    test_contratar_recepcionista();
    test_contratar_en_sala();
    test_falta_personal_llamada();
    test_aviso_sala();
    test_aviso_sala_fax();
    test_aviso_personal();
    test_descubrir();
    test_partida_nueva();
    free(E);
    E = NULL;
}
