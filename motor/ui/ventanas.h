// Las ventanas de la partida (la demo jugable, piezas 5 y 6 de docs/06_plan.md):
// construir (UIBuildRoom), amueblar (UIFurnishCorridor), colocar
// (UIPlaceObjects), la obra sobre el mapa (UIEditRoom), contratar (UIHireStaff)
// y colocar personal (UIPlaceStaff), sobre la pila de ventanas (ui.h) y las
// piezas de MUTTON01 (mutton01.h). Es C portable: el anfitrión (la ROM,
// platform/n64/main.c; th64sim y las pruebas en el PC) le da en cada fotograma
// lo que llegó del ratón, la cámara y la simulación entre ticks, y recibe las
// órdenes para la cola del núcleo (th/build.h) y lo que hay que dibujar: las
// superposiciones por casilla (el plano, la huella del objeto, las marcas), las
// entidades del plano (paredes, puerta y ventanas) y los fantasmas que siguen
// al puntero (el objeto o la persona en la mano).
//
// El reparto de trabajo:
// - el anfitrión lleva la pila (th_ui_stack_mouse_down/up/move y
//   th_ui_stack_update en cada fotograma, como interfaz_mandos), dibuja la pila
//   con th_ui_stack_draw (las ventanas de aquí traen su draw), aplica las órdenes
//   (th_sim_push_order con tick = s->hours) y escribe el registro de la partida
//   («TH64 orden», tools/hito6/README.md) desde el gancho th_sim_events.order;
// - este módulo abre y cierra sus ventanas en la pila (th_ui_stack_open con la
//   clase TH_UI_MAIN: una a la vez, como UI:addWindow), reparte los botones y los
//   clics que llegaron al mundo, recoge las pulsaciones de sus botones repetidos
//   (th_ui_stack_take_click), mira el estado del núcleo entre ticks (s->place,
//   s->obra, s->hosp.balance, th_sim_candidates) y decide qué órdenes mandar.
#ifndef TH_VENTANAS_H
#define TH_VENTANAS_H

#include "th/build.h"
#include "th/sim.h"
#include "objetos.h"
#include "panel_inferior.h"
#include "ui.h"
#include "vista.h"

// Las ventanas, por la clase «main» de CorsixTH (una abierta a la vez).
enum {
    TH_GW_NONE = 0,
    TH_GW_CONSTRUIR,        // UIBuildRoom (dialogs/build_room.lua): TH_PI_SALAS
    TH_GW_AMUEBLAR,         // UIFurnishCorridor (dialogs/furnish_corridor.lua): TH_PI_OBJETOS
    TH_GW_COLOCAR,          // UIPlaceObjects (dialogs/place_objects.lua): la lista es s->place
    TH_GW_OBRA,             // UIEditRoom (dialogs/edit_room.lua): la obra es s->obra, con sus fases TH_OBRA_*
    TH_GW_CONTRATAR,        // UIHireStaff (dialogs/hire_staff.lua): TH_PI_CONTRATAR; el mercado, th_sim_candidates
    TH_GW_COLOCAR_PERSONAL, // UIPlaceStaff (dialogs/place_staff.lua): el candidato elegido, en la mano
};

// Lo que el anfitrión da una vez (th_game_ui_init). Se guarda por puntero: tiene
// que durar lo que dure la partida.
typedef struct th_game_ui_env {
    th_ui_stack *stack;        // la pila, con el panel inferior ya dentro (interfaz_pila)
    const th_ui_fonts *fonts;  // las fuentes de la interfaz (interfaz_fuentes)
    const th_rules *texts;     // los textos (english.trg; interfaz_textos): nombres de salas, objetos y personal
    const th_anims *anims;     // las animaciones, para los fantasmas y el plano (th_view_blueprint)
    const th_walk_anims *walk; // las de las personas (th_vista_persona_fantasma)
    int screen_w, screen_h;    // 320 × 240
} th_game_ui_env;

// Lo que pasó en un fotograma (interfaz_orden_t de la ROM, o una prueba).
typedef struct th_game_ui_input {
    int px, py;               // el puntero, en la pantalla
    int world_down, world_up; // botones del ratón que bajaron o subieron sobre el MUNDO (ninguna
                              // ventana los tomó): bits (1 << TH_UI_LEFT) y (1 << TH_UI_RIGHT)
    th_ui_window *win;        // un botón de una ventana de la pila pulsado y soltado (NULL: ninguno)…
    int id, mouse;            // …su id (id o id_right) y con qué botón del ratón (TH_UI_LEFT o TH_UI_RIGHT)
    int panel_button;         // TH_PI_* pulsado en el panel inferior, o 0: TH_PI_SALAS abre construir,
                              // TH_PI_OBJETOS amueblar y TH_PI_CONTRATAR contratar (los demás, nada)
    int escape;               // 1 si se pulsó Escape (R en el mando): cierra la ventana de arriba; en la
                              // obra la abandona o la termina si ya se puede (TH_ORD_CERRAR, el
                              // verifyOrAbortRoom de GameUI:setEditRoom(false); GameUI:onKeyDown escape)
    int paused;               // decisión 4 del plan: en pausa no se mandan órdenes (las ventanas se abren,
                              // se cierran y se recorren; colocar, comprar y contratar esperan)
    int sim_idle;             // 1 si la simulación no está a medio tick: entonces, y solo entonces, se
                              // puede llamar a th_build_check_cell y th_build_staff_placement (la vista
                              // previa); si es 0 se repite el último resultado
} th_game_ui_input;

#define TH_GAME_UI_ORDERS 4   // órdenes en un fotograma (como mucho una por clic; el margen, por si acaso)
#define TH_GAME_UI_ENTITIES 48 // entidades del plano (2(w+h)−1 paredes de un plano de hasta 12 × 12, la
                               // puerta y las ventanas) y fantasmas (un objeto de hasta 3 columnas o una persona)

// Lo que el anfitrión tiene que hacer después del fotograma.
typedef struct th_game_ui_output {
    int n_orders;
    th_order orders[TH_GAME_UI_ORDERS]; // para th_sim_push_order, en este orden; tick lo pone el anfitrión
                                        // (s->hours: se aplican al empezar el tick siguiente). En
                                        // TH_ORD_CONTRATAR, def ya lleva th_staff_profile_digest.
    const th_view_overlays *overlays;   // para th_scene.overlays (NULL: ninguna); casillas del mundo
    const th_view_entity *entities;     // entidades que van en la escena con las demás (las paredes, la puerta
    int n_entities;                     // y las ventanas del plano, th_view_blueprint y th_view_edge): el
                                        // anfitrión las añade a la lista de th_scene antes de th_view_scene
    const th_view_entity *on_top;       // las que van encima de todo (el objeto o la persona que sigue al
    int n_on_top;                       // puntero, th_vista_*_fantasma): th_view_entity_on_top, en orden
    const char *message;                // texto breve para la barra del panel (th_bottom_info.message), o NULL
    // Añadido por la pieza 6 (al final, sin cambiar lo de arriba): 1 si en este
    // fotograma la ventana rechaza algo y tiene que sonar el aviso (wrong2.wav),
    // como hacen las propias ventanas de CorsixTH: hire_staff.lua:134-140 (sin
    // perfil o sin dinero), edit_room.lua:935 (la obra deja zonas sin salida),
    // furnish_corridor.lua:106 (sin dinero). El anfitrión NO suena por su cuenta
    // cuando el núcleo rechaza una orden: en CorsixTH un clic con el plano malo
    // o con alguien encima no hace nada ni suena (place_objects.lua:513-522); si
    // la ventana quiere avisar de un rechazo del núcleo, lo pone aquí en el
    // fotograma siguiente a th_game_ui_order_result. Con message a la vez, el
    // texto sale en la barra con el sonido.
    int reject_sound;
} th_game_ui_output;

// El estado de las ventanas. Es opaco y hay una sola instancia, dentro de
// ventanas.c (sin malloc: el anfitrión no la reserva ni conoce su tamaño).
typedef struct th_game_ui th_game_ui;
th_game_ui *th_game_ui_instance(void);

void th_game_ui_init(th_game_ui *g, const th_game_ui_env *env);

// Un fotograma. v es la cámara con la que se dibuja y rot su giro (0–3,
// girar.h): la casilla del mundo bajo el puntero es th_view_pick seguido de
// th_rot_tile_inv. s es la simulación entre ticks (o a medio tick si
// in->sim_idle es 0: entonces solo se lee lo que no cambia dentro del tick:
// s->place, s->obra, s->hosp.balance, el mercado). out queda relleno entero.
void th_game_ui_frame(th_game_ui *g, const th_game_ui_input *in, th_sim *s, const th_view *v, int rot,
                      th_game_ui_output *out);

// El resultado de una orden aplicada (el gancho th_sim_events.order del
// anfitrión la reenvía): para que la ventana reaccione (un rechazo deja el
// objeto en la mano; una contratación aceptada cierra la de colocar personal).
void th_game_ui_order_result(th_game_ui *g, const th_sim *s, const th_order *o, const th_order_result *r);

// Abre una ventana (TH_GW_CONSTRUIR, TH_GW_AMUEBLAR o TH_GW_CONTRATAR; las demás
// se abren solas desde esas) como lo haría el botón del panel: 0, o −1 si no se
// puede. Para las pruebas y th64sim ventanas.
int th_game_ui_open(th_game_ui *g, int which);
// Cierra lo que haya abierto (al salir de la partida), sin mandar órdenes.
void th_game_ui_close_all(th_game_ui *g);
// La ventana abierta (TH_GW_*), o TH_GW_NONE.
int th_game_ui_active(const th_game_ui *g);

// Añadido por la pieza 5 (no cambia lo de arriba): los ids de los botones de
// sus ventanas (th_ui_button.id e id_right), los que llegan en
// th_game_ui_input.id. El anfitrión no los necesita (reenvía win e id tal
// cual); sirven para las pruebas y th64sim ventanas, que buscan el botón por
// su id para pulsarlo.
enum {
    // construir (UIBuildRoom)
    TH_GWB_CATEGORIA = 1,  // + 0..3: diagnóstico, tratamiento, clínicas, instalaciones
    TH_GWB_SALA = 11,      // + 0..9: las filas de la lista
    TH_GWB_CON_CERRAR = 30,
    // amueblar (UIFurnishCorridor)
    TH_GWB_OBJETO = 100,      // + 0..9: la fila (izquierdo +1)
    TH_GWB_OBJETO_MENOS1 = 120, // + 0..9: la fila con el derecho (−1, on_rightclick)
    TH_GWB_MENOS = 140,       // + 0..9: «−», repetido
    TH_GWB_MAS = 160,         // + 0..9: «+», repetido
    TH_GWB_AMU_CERRAR = 180,
    TH_GWB_AMU_CONFIRMAR = 181,
    // colocar y la obra (UIPlaceObjects y UIEditRoom)
    TH_GWB_COL_CANCELAR = 200,
    TH_GWB_COL_COMPRAR = 201,
    TH_GWB_COL_COGER = 202,
    TH_GWB_COL_CONFIRMAR = 203,
    TH_GWB_COL_FILA = 210, // + 0..5: la fila de la lista
    // contratar (UIHireStaff)
    TH_GWB_PESTANA = 300, // + 0..3: médicos, enfermeras, bedeles, recepcionistas
    TH_GWB_ANTERIOR = 310,
    TH_GWB_SIGUIENTE = 311,
    TH_GWB_CTR_CANCELAR = 312,
    TH_GWB_CONTRATAR = 313,
};

// Notas de la pieza 5 sobre lo de arriba: th_game_ui_open da −1 antes del
// primer th_game_ui_frame (las listas salen de la simulación de un fotograma
// anterior) y, si hay que cerrar algo en el núcleo para abrir la otra (la obra
// o la lista de colocar: TH_ORD_CERRAR), en pausa o fuera de un fotograma
// (entre dos th_game_ui_frame no hay salida para esa orden); dentro del
// fotograma la abre in->panel_button.

// La ventana de la pila que llevan las ventanas de este módulo (una sola
// th_ui_window que se rehace al cambiar de ventana), para las pruebas.
th_ui_window *th_game_ui_window(th_game_ui *g);

// Añadido por la pieza 5: una orden de out->orders que no llegó a la cola
// (th_sim_push_order dio −1, la cola de 16 llena) y por tanto nunca volverá
// por th_game_ui_order_result. El anfitrión la llama por cada una, para que la
// ventana no se quede esperando su respuesta (sin ella, colocar no se abre ni
// salen las compras pendientes hasta pasados 30 s): se trata como un rechazo
// (el objeto o la persona siguen en la mano) y la barra lo dice.
void th_game_ui_order_dropped(th_game_ui *g, const th_order *o);

#endif
