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
    TH_REASON_CASILLAS, // th_order_result.bad: cuántas casillas de la huella
    TH_REASON_BLOQUEO,
    TH_REASON_PERSONA,
    TH_REASON_NO_SE_PUEDE,
    TH_REASON_ORDEN,        // orden que no existe
    TH_REASON_OTRA_VENTANA, // con la ventana de colocar abierta, el clic no llega al mundo
    TH_REASON_LLENO,        // el tope del motor (th_sim_object_fits): CorsixTH no lo tiene
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

#endif
