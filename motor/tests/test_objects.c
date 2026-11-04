// Pruebas de ocupar y desocupar casillas (th/objects.h) en un nivel TMP1
// sintético, con definiciones escritas a mano. Lo que se espera sale del
// código de CorsixTH (entities/object.lua, objects/door.lua, doors/*.lua y
// world.lua) y de los experimentos 1 y 2 del hito 6.3. La comparación con
// hospitales de verdad está en th64sim banderas (52 escenarios de CorsixTH).
#include <stdio.h>
#include <string.h>

#include "th/map.h"
#include "th/objects.h"
#include "th/path.h"
#include "th/rules.h"
#include "th_test.h"

#define CASILLA 10
#define CABECERA 32

static uint8_t tmp[CABECERA + CASILLA * TH_MAP_WIDTH * TH_MAP_HEIGHT];
static th_map mapa;
static th_pathfinder buscador;
static uint8_t trg[8192];
static th_rules defs;
static th_object objs[24];
static uint32_t seq;
static th_objects w;

// Definiciones: las de CorsixTH para el banco, los objetos de borde y las
// puertas (solo lo que se usa para ocupar), y tres inventadas para probar
// need_<dir>_side, optional y una casilla de uso a 12 pasos.
static const char *const definiciones =
    "{\"objetos\": {"
    " \"bench\": {\"id\": \"bench\", \"thob\": 4, \"class\": \"Bench\", \"orientations\": {"
    "  \"north\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"complete_cell\": true},"
    "                             {\"#1\": 0, \"#2\": -1, \"only_passable\": true, \"shareable\": true}]},"
    "  \"south\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"complete_cell\": true},"
    "                             {\"#1\": 0, \"#2\": 1, \"only_passable\": true, \"shareable\": true}]}}},"
    " \"radiator\": {\"id\": \"radiator\", \"thob\": 44, \"class\": \"SideObject\", \"orientations\": {"
    "  \"north\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"only_side\": true}]},"
    "  \"south\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"only_side\": true}]}}},"
    " \"extinguisher\": {\"id\": \"extinguisher\", \"thob\": 43, \"class\": \"SideObject\", \"orientations\": {"
    "  \"west\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"only_side\": true}]}}},"
    " \"bin\": {\"id\": \"bin\", \"thob\": 50, \"class\": \"SideObject\", \"orientations\": {"
    "  \"east\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"only_side\": true}]}}},"
    " \"door\": {\"id\": \"door\", \"thob\": 3, \"class\": \"Door\"},"
    " \"swing_door_right\": {\"id\": \"swing_door_right\", \"thob\": 53, \"class\": \"SwingDoor\"},"
    " \"swing_door_left\": {\"id\": \"swing_door_left\", \"thob\": 52, \"class\": \"SwingDoor\"},"
    " \"entrance_right_door\": {\"id\": \"entrance_right_door\", \"thob\": 59, \"class\": \"EntranceDoor\","
    "  \"supports_creation_for_map\": true},"
    " \"entrance_left_door\": {\"id\": \"entrance_left_door\", \"thob\": 58, \"class\": \"EntranceDoor\","
    "  \"supports_creation_for_map\": true},"
    " \"litter\": {\"id\": \"litter\", \"thob\": 62, \"class\": \"Litter\"},"
    " \"maquina\": {\"id\": \"maquina\", \"thob\": 30, \"orientations\": {"
    "  \"north\": {\"footprint\": [[0, 0], {\"#1\": 1, \"#2\": 0, \"need_north_side\": true},"
    "                             {\"#1\": 0, \"#2\": 1, \"only_passable\": true}]}}},"
    " \"opcional\": {\"id\": \"opcional\", \"thob\": 31, \"orientations\": {"
    "  \"north\": {\"footprint\": [{\"#1\": 0, \"#2\": 0, \"complete_cell\": true},"
    "                             {\"#1\": -1, \"#2\": 0, \"only_passable\": true, \"optional\": true},"
    "                             {\"#1\": 1, \"#2\": 0, \"only_passable\": true, \"optional\": true}]}}},"
    " \"lejano\": {\"id\": \"lejano\", \"thob\": 32, \"orientations\": {"
    "  \"north\": {\"footprint\": [[0, 0], {\"#1\": 0, \"#2\": -12, \"only_passable\": true}]}}}"
    "}}";

#define SOMBRAS (TH_TILE_SHADOW_HALF | TH_TILE_SHADOW_FULL | TH_TILE_SHADOW_WALL)
#define TRANSITO (TH_TILE_TRAVEL_N | TH_TILE_TRAVEL_E | TH_TILE_TRAVEL_S | TH_TILE_TRAVEL_W)
// Una casilla de dentro, lejos de los bordes, recién cargada.
#define BASE (TH_TILE_FILE_FLAGS | TRANSITO)
#define LADOS TH_TILE_BUILDABLE_SIDES

// Nivel de un jugador: césped, y suelo de hospital de (5, 5) a (60, 60) en la
// parcela 1, con todas las banderas del fichero. Con puertas, también la
// parcela 2, de (61, 5) a (80, 60) y sin dueño, con dos puertas de la entrada
// al oeste: la izquierda en (70, 30) y la derecha en (70, 31).
static void preparar_nivel(int puertas)
{
    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, "TMP1", 4);
    tmp[5] = TH_MAP_WIDTH;
    tmp[7] = TH_MAP_HEIGHT;
    tmp[8] = 1;
    tmp[9] = puertas ? 3 : 2;
    tmp[11] = CASILLA;
    for (int y = 0; y < TH_MAP_HEIGHT; y++)
        for (int x = 0; x < TH_MAP_WIDTH; x++) {
            uint8_t *c = &tmp[CABECERA + CASILLA * (y * TH_MAP_WIDTH + x)];
            c[0] = 1;
            if (y >= 5 && y <= 60 && (x >= 5 && x <= 60 ? 1 : puertas && x >= 61 && x <= 80 ? 2 : 0)) {
                c[0] = 17;
                c[3] = TH_TILE_FILE_FLAGS;
                c[8] = x <= 60 ? 1 : 2;
            }
        }
    if (puertas) {
        uint8_t *izq = &tmp[CABECERA + CASILLA * (30 * TH_MAP_WIDTH + 70)];
        uint8_t *der = &tmp[CABECERA + CASILLA * (31 * TH_MAP_WIDTH + 70)];
        izq[6] = 58;
        izq[7] = 1;
        der[6] = 59;
        der[7] = 1;
    }
    TH_CHECK_EQ(th_map_load(&mapa, tmp, sizeof(tmp), 0), TH_MAP_OK);
    memset(objs, 0, sizeof(objs));
    seq = 0;
    w = (th_objects){&mapa, &buscador, &defs, objs, 0, &seq, NULL, 0};
}

static void preparar(void)
{
    preparar_nivel(0);
}

static int nuevo(const char *id, int dir)
{
    char ruta[64] = "objetos.";
    strcat(ruta, id);
    th_object *o = &objs[w.n_obj];
    TH_CHECK_EQ(th_object_init(o, &defs, th_rules_path(&defs, defs.root, ruta), dir), 0);
    return w.n_obj++;
}

static uint32_t banderas(int x, int y)
{
    return th_map_tile(&mapa, x, y)->flags & ~(uint32_t)SOMBRAS;
}

static void test_clases(void)
{
    preparar();
    th_object o;
    TH_CHECK_EQ(th_object_init(&o, &defs, th_rules_path(&defs, defs.root, "objetos.litter"), 0), -1);
    TH_CHECK_EQ(th_object_init(&o, &defs, 0, 0), -1);
    TH_CHECK_EQ(objs[nuevo("bench", TH_DIR_NORTH)].oclass, TH_OCLASS_OBJECT);
    TH_CHECK_EQ(objs[nuevo("radiator", TH_DIR_NORTH)].oclass, TH_OCLASS_SIDE);
    TH_CHECK_EQ(objs[nuevo("door", TH_DIR_NORTH)].oclass, TH_OCLASS_DOOR);
    const int sd = nuevo("swing_door_right", TH_DIR_WEST), si = nuevo("swing_door_left", TH_DIR_WEST);
    TH_CHECK_EQ(objs[sd].oclass, TH_OCLASS_SWING_DOOR);
    TH_CHECK_EQ(objs[sd].oflags, TH_OBJF_MASTER);
    TH_CHECK_EQ(objs[si].oflags, 0);
    const int ed = nuevo("entrance_right_door", TH_DIR_NORTH), ei = nuevo("entrance_left_door", TH_DIR_NORTH);
    TH_CHECK_EQ(objs[ed].oclass, TH_OCLASS_ENTRANCE_DOOR);
    TH_CHECK_EQ(objs[ed].oflags, TH_OBJF_MASTER);
    TH_CHECK_EQ(objs[ei].oflags, 0);
    TH_CHECK_EQ(objs[ed].thob, 59);
}

// Experimento 1 del hito 6.3: el banco norte deja el origen sin construir ni
// pasar y con los cuatro bordes ocupados, y la casilla de uso pasable pero
// sin construir, con el borde sur (el que da al origen) ocupado.
static void test_banco(void)
{
    preparar();
    const int b = nuevo("bench", TH_DIR_NORTH);
    th_object_set_tile(&w, b, 10, 10);
    TH_CHECK_EQ(banderas(10, 10), BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_PASSABLE | LADOS));
    TH_CHECK_EQ(banderas(10, 9), BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_S));
    TH_CHECK_EQ(th_map_thob_count(&mapa, 10, 10), 1);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 10, 10, 0), 4);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 10, 9), 0); // solo la casilla de origen
    TH_CHECK_EQ(objs[b].oflags & TH_OBJF_ON_MAP, TH_OBJF_ON_MAP);
    TH_CHECK_EQ(th_objects_first_at(&w, 10, 10), b);
    // Moverlo deja las casillas de antes como estaban.
    th_object_set_tile(&w, b, 12, 10);
    TH_CHECK_EQ(banderas(10, 10), BASE);
    TH_CHECK_EQ(banderas(10, 9), BASE);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 10, 10), 0);
    TH_CHECK_EQ(banderas(12, 9), BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_S));
    // Venderlo, también.
    th_object_set_tile(&w, b, -1, -1);
    TH_CHECK_EQ(banderas(12, 10), BASE);
    TH_CHECK_EQ(banderas(12, 9), BASE);
    TH_CHECK_EQ(objs[b].oflags & TH_OBJF_ON_MAP, 0);
    TH_CHECK_EQ(th_objects_first_at(&w, 12, 10), -1);
    // need_north_side y una casilla sin etiquetas, dada como lista [x, y].
    const int m = nuevo("maquina", TH_DIR_NORTH);
    th_object_set_tile(&w, m, 20, 20);
    TH_CHECK_EQ(banderas(20, 20),
                BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_PASSABLE | TH_TILE_BUILDABLE_E | TH_TILE_BUILDABLE_S));
    TH_CHECK_EQ(banderas(21, 20),
                BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_PASSABLE | TH_TILE_BUILDABLE_N | TH_TILE_BUILDABLE_W));
    TH_CHECK_EQ(banderas(20, 21), BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_N));
    th_object_set_tile(&w, m, -1, -1);
    TH_CHECK_EQ(banderas(20, 20), BASE);
    TH_CHECK_EQ(banderas(21, 20), BASE);
    TH_CHECK_EQ(banderas(20, 21), BASE);
}

// Dos bancos con la misma casilla de uso: al quitar uno, la casilla sigue sin
// construir porque es del otro (isTilePartOfNearbyObject).
static void test_compartida(void)
{
    preparar();
    const int a = nuevo("bench", TH_DIR_NORTH), b = nuevo("bench", TH_DIR_SOUTH);
    th_object_set_tile(&w, a, 20, 20);
    th_object_set_tile(&w, b, 20, 18);
    TH_CHECK_EQ(banderas(20, 19), BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_S | TH_TILE_BUILDABLE_N));
    TH_CHECK(th_objects_part_of_nearby(&w, 20, 19, 10));
    TH_CHECK(!th_objects_part_of_nearby(&w, 22, 19, 10));
    th_object_set_tile(&w, a, -1, -1);
    TH_CHECK_EQ(banderas(20, 19), BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_N));
    th_object_set_tile(&w, b, -1, -1);
    TH_CHECK_EQ(banderas(20, 19), BASE);
}

// Experimento 2: los objetos de borde quitan el paso a los dos lados del
// borde y dejan la casilla construible y pasable.
static void test_borde(void)
{
    preparar();
    const int r = nuevo("radiator", TH_DIR_NORTH);
    th_object_set_tile(&w, r, 30, 30);
    TH_CHECK_EQ(banderas(30, 30), BASE & ~(uint32_t)(TH_TILE_BUILDABLE_N | TH_TILE_TRAVEL_N));
    TH_CHECK_EQ(banderas(30, 29), BASE & ~(uint32_t)TH_TILE_TRAVEL_S);
    TH_CHECK_EQ(objs[r].oflags & TH_OBJF_SIDE_PASSABLE, TH_OBJF_SIDE_PASSABLE);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 30, 30, 0), 44);
    th_object_set_tile(&w, r, -1, -1);
    TH_CHECK_EQ(banderas(30, 30), BASE);
    TH_CHECK_EQ(banderas(30, 29), BASE);
    TH_CHECK_EQ(objs[r].oflags & TH_OBJF_SIDE_PASSABLE, 0);
    // Contra una pared el paso ya estaba cortado: no lo apunta y al quitarlo
    // no lo devuelve.
    th_map_tile(&mapa, 32, 30)->wall_n = 82;
    th_map_update_pathfinding(&mapa);
    const int r2 = nuevo("radiator", TH_DIR_NORTH);
    th_object_set_tile(&w, r2, 32, 30);
    TH_CHECK_EQ(objs[r2].oflags & TH_OBJF_SIDE_PASSABLE, 0);
    TH_CHECK_EQ(banderas(32, 30), BASE & ~(uint32_t)(TH_TILE_BUILDABLE_N | TH_TILE_TRAVEL_N));
    th_object_set_tile(&w, r2, -1, -1);
    TH_CHECK_EQ(banderas(32, 30), BASE & ~(uint32_t)TH_TILE_TRAVEL_N);
    TH_CHECK_EQ(banderas(32, 29), BASE & ~(uint32_t)TH_TILE_TRAVEL_S);
    // La papelera mirando al este ocupa el borde oeste.
    const int p = nuevo("bin", TH_DIR_EAST);
    th_object_set_tile(&w, p, 34, 30);
    TH_CHECK_EQ(banderas(34, 30), BASE & ~(uint32_t)(TH_TILE_BUILDABLE_W | TH_TILE_TRAVEL_W));
    TH_CHECK_EQ(banderas(33, 30), BASE & ~(uint32_t)TH_TILE_TRAVEL_E);
    TH_CHECK_EQ(banderas(35, 30), BASE);
    // Dos radiadores en el mismo borde, uno a cada lado: el segundo no
    // apunta el corte, y quitar el primero abre el borde aunque el segundo
    // siga ahí (lo arregla el siguiente resetSideObjects).
    const int a = nuevo("radiator", TH_DIR_SOUTH), b = nuevo("radiator", TH_DIR_NORTH);
    th_object_set_tile(&w, a, 40, 14);
    th_object_set_tile(&w, b, 40, 15);
    TH_CHECK_EQ(objs[b].oflags & TH_OBJF_SIDE_PASSABLE, 0);
    th_object_set_tile(&w, a, -1, -1);
    TH_CHECK_EQ(banderas(40, 15) & TH_TILE_TRAVEL_N, TH_TILE_TRAVEL_N);
    th_objects_reset_side(&w);
    TH_CHECK_EQ(banderas(40, 15) & TH_TILE_TRAVEL_N, 0);
    TH_CHECK_EQ(banderas(40, 14) & TH_TILE_TRAVEL_S, 0);
}

// World:resetSideObjects con dos objetos de borde en la misma casilla: el
// primero pasa dos veces y el segundo ninguna (ipairs sobre una lista que
// cambia). Como el que pasa queda el último, en la vez siguiente pasa el
// otro, y el que no pasa pierde su corte si antes se rehízo el tránsito.
static void test_reinicio(void)
{
    preparar();
    const int r = nuevo("radiator", TH_DIR_NORTH), e = nuevo("extinguisher", TH_DIR_WEST);
    th_object_set_tile(&w, r, 45, 45);
    th_object_set_tile(&w, e, 45, 45);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 45, 45, 0), 44);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 45, 45, 1), 43);
    TH_CHECK_EQ(th_objects_first_at(&w, 45, 45), r);
    const uint32_t h = th_map_hash(&mapa);
    th_objects_reset_side(&w);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 45, 45, 0), 43); // getCellFlags.thob pasa a ser el extintor
    TH_CHECK_EQ(th_map_thob_at(&mapa, 45, 45, 1), 44);
    TH_CHECK_EQ(th_objects_first_at(&w, 45, 45), e);
    TH_CHECK(th_map_hash(&mapa) != h);
    TH_CHECK_EQ(banderas(45, 45),
                BASE & ~(uint32_t)(TH_TILE_BUILDABLE_N | TH_TILE_TRAVEL_N | TH_TILE_BUILDABLE_W | TH_TILE_TRAVEL_W));
    // markRoom rehace el tránsito con las paredes; resetSideObjects lo vuelve
    // a cortar, pero ahora el extintor va primero y el radiador se queda sin
    // su corte.
    th_map_update_pathfinding(&mapa);
    th_objects_reset_side(&w);
    TH_CHECK_EQ(banderas(45, 45) & TRANSITO, TRANSITO & ~(uint32_t)TH_TILE_TRAVEL_W);
    TH_CHECK_EQ(banderas(44, 45) & TH_TILE_TRAVEL_E, 0);
    TH_CHECK_EQ(banderas(45, 44) & TH_TILE_TRAVEL_S, TH_TILE_TRAVEL_S);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 45, 45, 0), 44);
    TH_CHECK_EQ(th_objects_first_at(&w, 45, 45), r);
}

// Lo mismo cuando la lista de tipos de la casilla no tiene todos sus objetos,
// como al cargar un escenario que solo traía el primero: resetSideObjects
// recorre world.objects (los objetos), no esa lista, así que al quitar el
// radiador el extintor vuelve a cortar su borde (hallazgo 1 de la revisión).
static void test_reinicio_sin_lista(void)
{
    preparar();
    const int r = nuevo("radiator", TH_DIR_NORTH), e = nuevo("extinguisher", TH_DIR_WEST);
    th_object_set_tile(&w, r, 45, 45);
    th_object_set_tile(&w, e, 45, 45);
    th_map_clear_thobs(&mapa, 45, 45);
    th_map_add_thob(&mapa, 45, 45, 44);
    // destroyEntity del radiador: setTile(nil), updatePathfinding y resetSideObjects.
    th_object_set_tile(&w, r, -1, -1);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 45, 45), 0);
    th_map_update_pathfinding(&mapa);
    TH_CHECK_EQ(banderas(45, 45) & TH_TILE_TRAVEL_W, TH_TILE_TRAVEL_W);
    th_objects_reset_side(&w);
    TH_CHECK_EQ(banderas(45, 45) & TRANSITO, TRANSITO & ~(uint32_t)TH_TILE_TRAVEL_W);
    TH_CHECK_EQ(banderas(44, 45) & TH_TILE_TRAVEL_E, 0);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 45, 45), 1);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 45, 45, 0), 43);
    TH_CHECK_EQ(th_objects_first_at(&w, 45, 45), e);
}

// world.objects: la lista de una casilla se abre con el primer objeto que
// llega y no se cierra aunque se vaya (getObjects da {} y no nil, y
// Patient:setTile no tira basura ahí). Solo la del origen.
static void test_listas(void)
{
    preparar();
    const int b = nuevo("bench", TH_DIR_NORTH);
    TH_CHECK_EQ(th_map_has_object_list(&mapa, 10, 10), 0);
    th_object_set_tile(&w, b, 10, 10);
    TH_CHECK_EQ(th_map_has_object_list(&mapa, 10, 10), 1);
    TH_CHECK_EQ(th_map_has_object_list(&mapa, 10, 9), 0);
    th_object_set_tile(&w, b, 12, 10);
    th_object_set_tile(&w, b, -1, -1);
    TH_CHECK_EQ(th_map_has_object_list(&mapa, 10, 10), 1);
    TH_CHECK_EQ(th_map_has_object_list(&mapa, 12, 10), 1);
    TH_CHECK_EQ(th_map_has_object_list(&mapa, 11, 10), 0);
    const int a = nuevo("entrance_left_door", TH_DIR_WEST);
    th_object_adopt(&w, a, 30, 30);
    TH_CHECK_EQ(th_map_has_object_list(&mapa, 30, 30), 1);
}

// World:setPlotOwner crea con newObject las puertas de la entrada de la
// parcela comprada. Aquí se crean con th_object_init_map_object y se adoptan;
// sin ellas, isTilePartOfNearbyObject no ve sus casillas de paso (hallazgo 3
// de la revisión, nivel 2 de la campaña).
static void test_parcela(void)
{
    preparar_nivel(1);
    TH_CHECK_EQ(mapa.n_map_objects, 2);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 70, 31), 0);
    uint32_t puestos = 0;
    TH_CHECK(th_map_set_parcel_owner(&mapa, 2, 1, &puestos) >= 0);
    TH_CHECK_EQ(puestos, 3u);
    for (int i = 0; i < mapa.n_map_objects; i++) {
        const th_map_object *mo = &mapa.map_objects[i];
        if (!(puestos & (1u << i)))
            continue;
        const int k = w.n_obj++;
        TH_CHECK_EQ(th_object_init_map_object(&objs[k], &defs, mo), 0);
        th_object_adopt(&w, k, mo->x, mo->y);
    }
    TH_CHECK_EQ(objs[0].thob, 58);
    TH_CHECK_EQ(objs[0].oflags & TH_OBJF_MASTER, 0);
    TH_CHECK_EQ(objs[1].thob, 59);
    TH_CHECK_EQ(objs[1].oclass, TH_OCLASS_ENTRANCE_DOOR);
    TH_CHECK_EQ(objs[1].oflags & TH_OBJF_MASTER, TH_OBJF_MASTER);
    TH_CHECK_EQ(objs[1].dir, TH_DIR_WEST);
    TH_CHECK_EQ(th_objects_first_at(&w, 70, 31), 1);
    // Un banco al norte con la casilla de uso en (70, 32), que es de paso de
    // la puerta derecha: al quitarlo, sigue sin construir.
    TH_CHECK_EQ(banderas(70, 32) & TH_TILE_BUILDABLE, 0);
    const int b = nuevo("bench", TH_DIR_NORTH);
    th_object_set_tile(&w, b, 70, 33);
    th_object_set_tile(&w, b, -1, -1);
    TH_CHECK_EQ(banderas(70, 32) & TH_TILE_BUILDABLE, 0);
    TH_CHECK_EQ(banderas(70, 33) & TH_TILE_BUILDABLE, TH_TILE_BUILDABLE);
    // Los demás objetos del fichero no se crean.
    const th_map_object banco = {10, 10, 4, 0, 1}, raro = {10, 10, 99, 1, 1};
    th_object o;
    TH_CHECK_EQ(th_object_init_map_object(&o, &defs, &banco), -1);
    TH_CHECK_EQ(th_object_init_map_object(&o, &defs, &raro), -1);
}

static void test_puertas(void)
{
    preparar();
    const int d = nuevo("door", TH_DIR_NORTH);
    th_object_set_tile(&w, d, 45, 45);
    TH_CHECK_EQ(banderas(45, 45),
                (BASE | TH_TILE_DOOR_N | TH_TILE_TALL_N | TH_TILE_DO_NOT_IDLE) & ~(uint32_t)TH_TILE_BUILDABLE);
    TH_CHECK_EQ(banderas(45, 44), (BASE | TH_TILE_DO_NOT_IDLE) & ~(uint32_t)TH_TILE_BUILDABLE);
    TH_CHECK_EQ(th_map_thob_at(&mapa, 45, 45, 0), 3);
    // Un banco con la casilla de uso en la de fuera de la puerta: al quitar
    // la puerta, esa casilla sigue sin construir.
    const int b = nuevo("bench", TH_DIR_SOUTH);
    th_object_set_tile(&w, b, 45, 43);
    th_object_set_tile(&w, d, -1, -1);
    TH_CHECK_EQ(banderas(45, 45), BASE);
    TH_CHECK_EQ(banderas(45, 44), BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_N));
    TH_CHECK_EQ(th_map_thob_count(&mapa, 45, 45), 0);
    // Puerta doble al oeste: la maestra marca sus 6 casillas de paso; la
    // esclava solo sus banderas de puerta.
    const int m = nuevo("swing_door_right", TH_DIR_WEST), s = nuevo("swing_door_left", TH_DIR_WEST);
    th_object_set_tile(&w, m, 50, 50);
    th_object_set_tile(&w, s, 50, 49);
    for (int y = 49; y <= 51; y++)
        for (int x = 49; x <= 50; x++) {
            uint32_t esperado = (BASE | TH_TILE_DO_NOT_IDLE) & ~(uint32_t)TH_TILE_BUILDABLE;
            if (x == 50 && (y == 50 || y == 49))
                esperado |= TH_TILE_DOOR_W | TH_TILE_TALL_W;
            TH_CHECK_EQ(banderas(x, y), esperado);
        }
    TH_CHECK_EQ(banderas(51, 50), BASE);
    TH_CHECK_EQ(banderas(50, 52), BASE);
    th_object_set_tile(&w, s, -1, -1);
    th_object_set_tile(&w, m, -1, -1);
    for (int y = 48; y <= 52; y++)
        for (int x = 48; x <= 51; x++)
            TH_CHECK_EQ(banderas(x, y), BASE);
    // Puerta de la entrada: lo mismo que th_map_entrance_door, más su sitio
    // en la lista de objetos.
    const int e = nuevo("entrance_right_door", TH_DIR_NORTH);
    th_object_set_tile(&w, e, 55, 55);
    TH_CHECK_EQ(banderas(55, 55), (BASE | TH_TILE_TALL_N | TH_TILE_DO_NOT_IDLE) & ~(uint32_t)TH_TILE_BUILDABLE);
    TH_CHECK_EQ(banderas(55, 54), (BASE | TH_TILE_DO_NOT_IDLE) & ~(uint32_t)TH_TILE_BUILDABLE);
    TH_CHECK_EQ(banderas(56, 55), (BASE | TH_TILE_DO_NOT_IDLE) & ~(uint32_t)TH_TILE_BUILDABLE);
    TH_CHECK_EQ(banderas(56, 54), (BASE | TH_TILE_DO_NOT_IDLE) & ~(uint32_t)TH_TILE_BUILDABLE);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 55, 55), 1);
    TH_CHECK_EQ(th_objects_part_of_nearby(&w, 56, 54, 10), 1);
    th_object_set_tile(&w, e, -1, -1);
    TH_CHECK_EQ(banderas(55, 55), BASE);
    TH_CHECK_EQ(banderas(56, 54), BASE);
    TH_CHECK_EQ(th_map_thob_count(&mapa, 55, 55), 0);
    // Una que ya puso el mapa se adopta sin tocarlo.
    th_map_entrance_door(&mapa, 30, 30, 58, 1, 1);
    const uint32_t h = th_map_hash(&mapa);
    const int a = nuevo("entrance_left_door", TH_DIR_WEST);
    th_object_adopt(&w, a, 30, 30);
    TH_CHECK_EQ(th_map_hash(&mapa), h);
    TH_CHECK_EQ(th_objects_first_at(&w, 30, 30), a);
    th_object_set_tile(&w, a, -1, -1);
    TH_CHECK_EQ(banderas(30, 30), BASE);
    TH_CHECK_EQ(banderas(29, 30), BASE);
}

// Casillas opcionales: solo cambia la primera que vale (en la sala del
// origen y, si es de paso, alcanzable desde el hospital).
static void test_opcional(void)
{
    preparar();
    for (int y = 30; y <= 35; y++)
        for (int x = 20; x <= 25; x++)
            th_map_tile(&mapa, x, y)->room = 7;
    const int o = nuevo("opcional", TH_DIR_NORTH);
    th_object_set_tile(&w, o, 20, 32);
    TH_CHECK_EQ(banderas(19, 32), BASE); // fuera de la sala 7
    TH_CHECK_EQ(banderas(21, 32), BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_W));
    th_object_set_tile(&w, o, -1, -1);
    TH_CHECK_EQ(banderas(20, 32), BASE);
    TH_CHECK_EQ(banderas(21, 32), BASE);
    // Fuera de las salas: la de la izquierda es césped, que no se pisa.
    const uint32_t cesped = banderas(4, 20);
    th_object_set_tile(&w, o, 5, 20);
    TH_CHECK_EQ(banderas(4, 20), cesped);
    TH_CHECK_EQ(banderas(6, 20), BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_W));
    th_object_set_tile(&w, o, -1, -1);
    // Las dos valen: solo cambia la primera de la huella, la de la izquierda.
    th_object_set_tile(&w, o, 22, 32);
    TH_CHECK_EQ(banderas(21, 32), BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_E));
    TH_CHECK_EQ(banderas(23, 32), BASE);
    th_object_set_tile(&w, o, -1, -1);
    for (int x = 21; x <= 23; x++)
        TH_CHECK_EQ(banderas(x, 32), BASE);
    // La de la izquierda es el origen de un banco, que no es de paso: cambia
    // la de la derecha. Al quitar el objeto, deoccupyTilesByObjectFootprintAt
    // restaura también la que no ocupó (rareza 7): el origen del banco recupera
    // su borde este y vuelve a ser pasable y construible, como en CorsixTH.
    const int b = nuevo("bench", TH_DIR_NORTH);
    th_object_set_tile(&w, b, 21, 32);
    const uint32_t banco = BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_PASSABLE | LADOS);
    TH_CHECK_EQ(banderas(21, 32), banco);
    th_object_set_tile(&w, o, 22, 32);
    TH_CHECK_EQ(banderas(21, 32), banco);
    TH_CHECK_EQ(banderas(23, 32), BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_W));
    th_object_set_tile(&w, o, -1, -1);
    TH_CHECK_EQ(banderas(21, 32),
                BASE & ~(uint32_t)(TH_TILE_BUILDABLE_N | TH_TILE_BUILDABLE_S | TH_TILE_BUILDABLE_W));
    TH_CHECK_EQ(banderas(23, 32), BASE);
}

static void test_alcance(void)
{
    preparar();
    TH_CHECK_EQ(th_path_reachable_from_hospital(&buscador, &mapa, 30, 30), 1);
    TH_CHECK_EQ(th_path_reachable_from_hospital(&buscador, &mapa, 2, 2), 0); // césped: no se pisa
    th_map_tile(&mapa, 4, 30)->flags |= TH_TILE_PASSABLE; // pasable, fuera del hospital, junto a él
    TH_CHECK_EQ(th_path_reachable_from_hospital(&buscador, &mapa, 4, 30), 1);
    th_map_tile(&mapa, 1, 1)->flags |= TH_TILE_PASSABLE; // pasable y aislada
    TH_CHECK_EQ(th_path_reachable_from_hospital(&buscador, &mapa, 1, 1), 0);
    TH_CHECK_EQ(th_path_reachable_from_hospital(&buscador, &mapa, -1, 0), 0);
}

// Dos suposiciones de CorsixTH que se copian: isTilePartOfNearbyObject solo
// mira hasta 10 pasos, y solo el primer objeto de cada casilla (getObject).
static void test_rarezas(void)
{
    preparar();
    const int l = nuevo("lejano", TH_DIR_NORTH), b = nuevo("bench", TH_DIR_NORTH);
    th_object_set_tile(&w, l, 40, 20); // su casilla de uso, (40, 8), a 12 pasos
    th_object_set_tile(&w, b, 40, 9);  // la misma casilla de uso
    TH_CHECK_EQ(banderas(40, 8), BASE & ~(uint32_t)(TH_TILE_BUILDABLE | TH_TILE_BUILDABLE_S));
    th_object_set_tile(&w, b, -1, -1);
    TH_CHECK_EQ(banderas(40, 8), BASE); // construible otra vez, aunque sea del lejano
    // Un radiador y un banco con el mismo origen: el radiador va primero y
    // tapa al banco, así que su casilla de uso no cuenta como suya.
    const int r = nuevo("radiator", TH_DIR_NORTH), b1 = nuevo("bench", TH_DIR_NORTH), b2 = nuevo("bench", TH_DIR_SOUTH);
    th_object_set_tile(&w, r, 50, 20);
    th_object_set_tile(&w, b1, 50, 20);
    th_object_set_tile(&w, b2, 50, 18);
    TH_CHECK_EQ(th_objects_first_at(&w, 50, 20), r);
    TH_CHECK(th_objects_part_of_nearby(&w, 50, 19, 10)); // por el banco b2
    th_object_set_tile(&w, b2, -1, -1);
    TH_CHECK(!th_objects_part_of_nearby(&w, 50, 19, 10));
    TH_CHECK_EQ(banderas(50, 19) & TH_TILE_BUILDABLE, TH_TILE_BUILDABLE);
}

// th_test_trg: una lista de 64 cabe; una de 65 no (devuelve 0 en vez de
// truncarla sin avisar).
static size_t lista_trg(int n, uint8_t *buf, size_t cap)
{
    char json[512];
    int k = snprintf(json, sizeof json, "{\"l\": [");
    for (int i = 0; i < n; i++)
        k += snprintf(json + k, sizeof json - (size_t)k, i ? ", %d" : "%d", i);
    snprintf(json + k, sizeof json - (size_t)k, "]}");
    return th_test_trg(json, buf, cap);
}

static void test_trg_json(void)
{
    static uint8_t buf[2048];
    th_rules r;
    const size_t n = lista_trg(64, buf, sizeof buf);
    TH_CHECK(n > 0);
    TH_CHECK_EQ(th_rules_init(&r, buf, n), 0);
    TH_CHECK_EQ(th_rules_len(&r, th_rules_get(&r, r.root, "l")), 64);
    TH_CHECK_EQ(th_rules_int(&r, th_rules_at(&r, th_rules_get(&r, r.root, "l"), 63), -1), 63);
    TH_CHECK_EQ(lista_trg(65, buf, sizeof buf), 0);
}

void th_test_objects(void)
{
    test_trg_json();
    const size_t n = th_test_trg(definiciones, trg, sizeof(trg));
    TH_CHECK(n > 0);
    TH_CHECK_EQ(th_rules_init(&defs, trg, n), 0);
    if (n == 0)
        return;
    test_clases();
    test_banco();
    test_compartida();
    test_borde();
    test_reinicio();
    test_reinicio_sin_lista();
    test_listas();
    test_parcela();
    test_puertas();
    test_opcional();
    test_alcance();
    test_rarezas();
}
