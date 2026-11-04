// Construir (hito 6.3): las órdenes del jugador y lo que hacen, como las
// ventanas de CorsixTH v0.70.1. Paso 2, amueblar el pasillo:
// UIFurnishCorridor (dialogs/furnish_corridor.lua), UIPlaceObjects
// (dialogs/place_objects.lua), Object:onClick (entities/object.lua) y las
// comprobaciones de World (world.lua), con el bloqueo de zonas en el modo 2
// (blocking_off_areas = 2, la excepción 5 de la fase 4).
//
// Una orden es un paso de la interfaz: el mismo que da la batería de CorsixTH
// (bench/lib/ordenes.lua), que es el oráculo. El núcleo la valida al
// aplicarla; la interfaz de la ROM solo enseña lo que pasaría.
#ifndef TH_BUILD_H
#define TH_BUILD_H

#include "th/sim.h"

enum {
    TH_ORD_NONE,
    TH_ORD_COMPRAR,  // def, qty: UIFurnishCorridor, purchaseItem y confirm
    TH_ORD_COLOCAR,  // def, x, y, dir: UIPlaceObjects:onMouseUp
    TH_ORD_CERRAR,   // UIPlaceObjects:close (devuelve lo que no se colocó)
    TH_ORD_RECOGER,  // x, y: Object:onClick con el botón derecho
    TH_ORD_VENDER,   // UIPlaceObjects:sell
    TH_ORD_DESHACER, // UIPlaceObjects:undo
    // Las salas rectangulares (paso 3): UIEditRoom (dialogs/edit_room.lua).
    TH_ORD_SALA,           // def (salas.<id>), x, y, w, h: UIBuildRoom:buildRoom, setBlueprintRect y confirm
    TH_ORD_PUERTA,         // x, y, dir (lado): setDoorBlueprint y confirm
    TH_ORD_VENTANA,        // x, y, dir: setWindowBlueprint y placeWindowBlueprint
    TH_ORD_QUITAR_VENTANA, // x, y, dir: tryRemoveWindowFromWall
    TH_ORD_FIN_VENTANAS,   // confirm en la fase de ventanas: despejar, finishRoom y la fase de objetos
    TH_ORD_COMPRAR_SALA,   // def, qty: purchaseItems (la tienda de la sala) y confirm
    TH_ORD_FIN_SALA,       // confirm en la fase de objetos: cobra la sala y la termina
    TH_ORD_ATRAS,          // cancel
};

// Motivos de rechazo, con los nombres de la batería (th_build_reason_name).
enum {
    TH_REASON_OK,
    TH_REASON_NO_ES_DE_PASILLO,
    TH_REASON_SIN_DINERO,
    TH_REASON_MAXIMO,
    TH_REASON_SIN_VENTANA,
    TH_REASON_SIN_OBJETO,
    TH_REASON_ORIENTACION,
    TH_REASON_CASILLAS, // th_order_result.bad: cuántas casillas de la huella (o del plano)
    TH_REASON_BLOQUEO,
    TH_REASON_PERSONA,
    TH_REASON_NO_SE_PUEDE,
    TH_REASON_ORDEN,        // orden que no existe
    TH_REASON_OTRA_VENTANA, // con la ventana de colocar abierta, el clic no llega al mundo
    TH_REASON_LLENO,        // el tope del motor (th_sim_object_fits): CorsixTH no lo tiene
    // Las salas.
    TH_REASON_NO_DISPONIBLE,      // la sala no está descubierta en el nivel
    TH_REASON_PEQUENA,            // ancho o alto menor que minimum_size
    TH_REASON_ZONAS_INACCESIBLES, // el plano deja casillas de alrededor sin unir (checkReachability)
    TH_REASON_FASE,               // la obra no está en la fase de esa orden (o no hay obra)
    TH_REASON_POSICION,           // la puerta o la ventana no está en un borde que dé screenToWall
    TH_REASON_PUERTA_INVALIDA,
    TH_REASON_VENTANA_INVALIDA,
    TH_REASON_NO_ES_DE_SALA, // el objeto no está en la tienda de la sala (objects_additional)
    TH_REASON_FALTAN_OBJETOS, // faltan objetos obligatorios (checkEnableConfirm)
};

// th_order y th_order_result están en th/sim.h, que los guarda en la cola.

// Aplica la orden ahora mismo. Devuelve el motivo (TH_REASON_OK si se aceptó).
int th_build_apply(th_sim *s, const th_order *o, th_order_result *r);
const char *th_build_reason_name(int reason);

// Hospital:getObjectBuildCost: StartCost del nivel (sin la investigación,
// que no rebaja los objetos de pasillo).
int32_t th_build_object_cost(const th_sim *s, th_rnode def);

// La comprobación del plano en (x, y) para la fila activa de la ventana, en
// la orientación dir (UIPlaceObjects:setBlueprintCell): 1 si vale. En *bad,
// las casillas de la huella malas que se ven en el plano (las invisibles no
// cuentan) más las que caen fuera del mapa.
// Usa el buscador de caminos de la simulación (s->w.pf) y toca de paso las
// banderas del mapa, como CorsixTH: solo vale donde se aplican las órdenes,
// al empezar el tick. La vista previa de la interfaz de la ROM (paso 7) no la
// puede llamar con la simulación a medias: necesitará una búsqueda propia.
int th_build_check_cell(th_sim *s, th_rnode def, int x, int y, int dir, int *bad);

// La cola de órdenes: cada una se aplica al empezar el tick de su hora
// (th_sim_tick), en el orden en que llegó. Devuelve -1 si la cola está llena.
int th_sim_push_order(th_sim *s, const th_order *o);
void th_sim_apply_orders(th_sim *s);

// Al empezar cada tick, antes de las órdenes: una obra que espera a que se
// despeje el área (UIEditRoom:clearArea; CorsixTH lo sondea cada 10 ticks de
// interfaz) la termina en cuanto no queda nadie.
void th_build_tick(th_sim *s);

// Hospital:getRoomBuildCost: el precio de la sala en el nivel más el de sus
// objetos obligatorios (World:getAvailableRooms).
int32_t th_build_room_cost(const th_sim *s, int type);

// Rehacer una sala ya construida sin pasar por las órdenes ni validar nada
// (th64sim salas: el oráculo de las paredes con los hospitales exportados de
// CorsixTH): el plano se cierra, se le ponen la puerta y las ventanas,
// finishRoom levanta las paredes y las puertas y marca la sala, y queda
// construida y activa. No mueve dinero. Devuelve el índice en rooms, o -1.
int th_build_rebuild_room(th_sim *s, int type, int x, int y, int w, int h, th_edge door, const th_edge *windows,
                          int n_windows);

#endif
