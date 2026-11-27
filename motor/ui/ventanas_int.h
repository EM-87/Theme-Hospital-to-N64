// Lo que comparten los ficheros de las ventanas de la partida (ventanas.h):
// ventanas.c (el fotograma, las órdenes, los mensajes y lo común),
// ventana_construir.c, ventana_amueblar.c, ventana_colocar.c (colocar y la
// ventana de la obra, que es la misma, como UIEditRoom hereda de
// UIPlaceObjects), obra.c (el plano, la puerta y las ventanas sobre el mapa)
// y ventana_contratar.c (contratar y colocar personal).
//
// Las disposiciones son de 320 × 240 sobre las piezas de DATAM/MUTTON01
// (mutton01.h): CorsixTH nunca usó esa hoja, así que no hay referencia
// (decisión 6 de docs/06_plan.md). Cada medida va en un comentario con la
// pieza y la fila o columna de donde sale (medidas con un volcado en ASCII de
// cada pieza: dónde empieza y acaba cada recuadro hundido).
#ifndef TH_VENTANAS_INT_H
#define TH_VENTANAS_INT_H

#include "mutton01.h"
#include "texto.h"
#include "ventanas.h"

#define GW_FILAS 10     // filas de las listas de construir y amueblar (las 10 de CorsixTH)
#define GW_COL_FILAS 6  // filas visibles de la lista de colocar (s->place tiene hasta 16)
#define GW_EN_VUELO_MAX 900 // fotogramas que se espera la respuesta de una orden (30 s) antes de olvidarla

enum { GW_OBRA_LOCAL = 0 }; // la obra antes de que el núcleo la tenga: el plano que se arrastra

typedef struct gw_compra {
    th_rnode def;
    uint8_t qty;
} gw_compra;

struct th_game_ui {
    th_game_ui_env env;
    int active; // TH_GW_*
    th_ui_window w; // la ventana abierta (una sola; se rehace al cambiar de ventana)

    // El fotograma en curso (solo dentro de th_game_ui_frame).
    const th_game_ui_input *in;
    th_sim *s;
    const th_view *v;
    int rot;
    th_game_ui_output *out;
    int tx, ty;      // la casilla del MUNDO bajo el puntero
    int sobre_mundo; // el puntero no está sobre ninguna ventana de la pila
    int en_vuelo;    // órdenes mandadas sin respuesta (th_game_ui_order_result)
    int en_vuelo_fotogramas;

    // Lo que se dibuja sobre el mapa.
    th_view_overlays ov;
    th_view_entity ents[TH_GAME_UI_ENTITIES];
    int n_ents;
    th_view_entity top[8];
    int n_top;
    uint8_t capas[TH_ANIM_LAYERS];

    // El aviso de la barra del panel, y si suena (out->reject_sound: solo
    // cuando la propia ventana rechaza, como wrong2 en CorsixTH).
    char msg[160];
    int msg_fotogramas;
    uint8_t sonar;

    // Construir: la categoría (−1: ninguna), las salas de la lista (índices en
    // room_types) y la fila bajo el puntero (−1).
    int cat;
    int n_salas;
    uint8_t salas[GW_FILAS];
    int hover;
    char coste[32];

    // Amueblar: los objetos de la lista con su cantidad, el total y el precio
    // del objeto bajo el puntero; tienda = 1 dentro de una obra (la tienda de
    // la sala, TH_ORD_COMPRAR_SALA). Lo comprado que queda por pedir al núcleo
    // (pend): TH_ORD_COMPRAR lleva un solo objeto y abre la lista de colocar
    // con él, cerrando la que hubiera, así que el siguiente se pide cuando la
    // lista se vacía.
    int tienda;
    int n_obj;
    th_rnode obj_def[GW_FILAS];
    uint8_t obj_qty[GW_FILAS];
    uint8_t obj_ini[GW_FILAS]; // start_qty: lo que ya había en la lista de la sala al abrir la tienda
    int32_t total, precio;
    int n_pend;
    int pend_tienda; // 1: lo pendiente es de la tienda de la obra (TH_ORD_COMPRAR_SALA), no del pasillo
    gw_compra pend[GW_FILAS];
    char precio_txt[32], total_txt[32];

    // Colocar: la fila elegida, el objeto en la mano y su orientación (la que
    // va en TH_ORD_COLOCAR: el giro es de la ventana), el modo coger.
    int fila;
    th_rnode en_mano;
    int orient;
    int coger;
    int col_n;  // filas que enseña la ventana (la ventana se rehace si cambia)
    int col_b0; // el botón de la primera fila
    // La lista en el orden de la ventana (col_orden: el puesto → la fila de
    // s->place, por la huella como addObjects), la primera a la vista (se
    // desplaza con más de GW_COL_FILAS) y lo que había (filas y unidades) en
    // el fotograma anterior, para ver si creció.
    int col_total, col_primera, col_unidades;
    uint8_t col_orden[TH_SIM_PLACE_ITEMS];
    const char *col_nombre[GW_COL_FILAS];
    uint8_t col_qty[GW_COL_FILAS];
    int col_activa;
    int col_region; // la ayuda de la descripción (su ruta cambia con la fase)
    const char *col_titulo, *col_texto;
    // La vista previa (th_build_check_cell, solo con sim_idle): el último
    // resultado, y la simulación, lo construido (construido: las órdenes que
    // han vuelto) y la fase de la obra con que se calculó (previa, en
    // ventana_colocar.c, no lo repite mientras no cambien); calculos cuenta
    // las llamadas, para test_ventanas.
    struct {
        th_rnode def;
        int x, y, dir, ok, hay, fase;
        const th_sim *s;
        uint32_t construido;
        uint32_t calculos;
    } previa;
    uint32_t construido;

    // La obra: el tipo de sala, el plano que se arrastra (en el mundo), si ya
    // la tiene el núcleo.
    int obra_tipo;
    int arrastre;
    int arrastre_sin_marca; // el arrastre empezó sin el botón en th_ui_stack.down (pegado al borde)
    int ax, ay;         // la casilla donde bajó el botón
    int px, py, pw, ph; // el plano
    int obra_iniciada;  // el núcleo pasó de las paredes
    int plano_ok;       // el plano arrastrado vale aquí (sin casillas malas ni pequeño): el confirmar
    struct {
        int x, y, side, ok, hay;
    } borde; // la última comprobación de puerta o ventana

    // Contratar: la clase (0–3, TH_HUM_DOCTOR + clase; −1, ninguna), el candidato, y el
    // elegido para colocarlo (su perfil y su resumen).
    int clase, idx;
    th_staff_profile perfil;
    uint32_t resumen;
    int esperando_contrato;
    struct {
        int x, y, ok, hay;
    } sitio;
    char nombre[48], sueldo[24], titulos[64], descripcion[200], cuentas[4][4];
    int n_cand[4];
    int hay_candidato;
    int32_t habilidad; // millonésimas
    uint8_t titulos_bits, es_medico;
};

// --- ventanas.c: lo común ---------------------------------------------------
// Manda una orden (a la salida del fotograma). En pausa no manda nada y pone
// el aviso de la pausa (decisión 4). Devuelve 1 si la mandó.
int gw_mandar(th_game_ui *g, const th_order *o);
// Pone un aviso en la barra durante 3 s: el texto de esa ruta de los
// textos (english.trg).
void gw_aviso(th_game_ui *g, const char *ruta);
// El mismo aviso con el sonido de rechazo (wrong2.wav en CorsixTH): solo donde
// la ventana rechaza por sí misma, hire_staff.lua:135 y 140, furnish_corridor.
// lua:106, build_room.lua:194 y edit_room.lua:935; un rechazo del núcleo no
// suena (salvo el de edit_room.lua:935, zonas_inaccesibles).
void gw_aviso_sonido(th_game_ui *g, const char *ruta);
// El aviso sin dinero de contratar: cannotAfford (hire_staff.lua:148-156)
// elige al azar entre cannot_afford y cash_low_consider_loan; aquí, por el
// bit bajo del resumen del perfil.
static inline const char *gw_sin_dinero(uint32_t resumen)
{
    return resumen & 1 ? "adviser.warnings.cash_low_consider_loan" : "adviser.warnings.cannot_afford";
}
// Abre la ventana which, ya montada en g->w por quien llama (cierra la
// anterior en la pila).
void gw_abrir_ventana(th_game_ui *g, int which);
// Cierra la ventana (de la pila) y vuelve a TH_GW_NONE.
void gw_cerrar(th_game_ui *g);
// El nombre de una definición con «name»: {"$ref": "_S.<ruta>"} en los textos.
const char *gw_nombre(const th_game_ui *g, const th_rules *defs, th_rnode def);
// ¿El objeto tiene esa orientación en las reglas?
int gw_tiene_orientacion(const th_rules *defs, th_rnode def, int dir);
// Un panel con botón que ocupa la pieza entera o un rectángulo de ella.
int gw_boton(th_ui_window *w, int sprite, int x, int y, int bx, int by, int bw, int bh, int activo, int id);
// La casilla del mundo bajo un píxel de la pantalla.
void gw_casilla(const th_game_ui *g, int sx, int sy, int *x, int *y);
static inline int gw_en_mapa(int x, int y)
{
    return x >= 0 && y >= 0 && x < TH_MAP_WIDTH && y < TH_MAP_HEIGHT;
}

// --- ventana_construir.c ------------------------------------------------------
void gw_construir_abrir(th_game_ui *g);
void gw_construir_boton(th_game_ui *g, int id, int mouse);
void gw_construir_frame(th_game_ui *g);

// --- ventana_amueblar.c -------------------------------------------------------
// tienda = 0: los objetos de pasillo (UIFurnishCorridor sin edit_dialog); 1:
// la tienda de la sala en obra (objects_additional).
void gw_amueblar_abrir(th_game_ui *g, int tienda);
void gw_amueblar_boton(th_game_ui *g, int id, int mouse);
void gw_amueblar_frame(th_game_ui *g);
// Pide al núcleo lo comprado que queda pendiente, si toca.
void gw_amueblar_pendientes(th_game_ui *g);

// --- ventana_colocar.c (colocar y la obra) -------------------------------------
void gw_colocar_abrir(th_game_ui *g, int which); // TH_GW_COLOCAR o TH_GW_OBRA (obra.c pone el tipo antes)
void gw_colocar_boton(th_game_ui *g, int id, int mouse);
void gw_colocar_frame(th_game_ui *g);
// La fila de la lista bajo el puntero (g->hover, que se pinta en azul).
void gw_colocar_hover(th_game_ui *g);
// Un TH_ORD_RECOGER aceptado dentro de la obra: el objeto cogido en la mano
// y el modo coger apagado.
void gw_colocar_recogido(th_game_ui *g, const th_sim *s);
void gw_colocar_escape(th_game_ui *g);
// Las filas, los textos y los botones de la ventana según la lista y la fase
// (la rehace si cambia el número de filas).
void gw_colocar_refrescar(th_game_ui *g);
// Lo que hace el objeto en la mano sobre el mapa (también en la fase de
// objetos de la obra).
void gw_colocar_mundo(th_game_ui *g);

// --- obra.c ---------------------------------------------------------------------
void gw_obra_empezar(th_game_ui *g, int tipo);
void gw_obra_frame(th_game_ui *g); // el plano, la puerta y las ventanas, según la fase
// UIEditRoom:screenToWall sobre la vista: el borde del plano bajo el puntero
// (casilla de dentro y lado, en el mundo). 0 si no hay.
int gw_obra_borde(const th_game_ui *g, int *x, int *y, int *side);
// Los botones de la ventana de la obra: cancelar (atrás) y confirmar.
void gw_obra_cancelar(th_game_ui *g);
void gw_obra_confirmar(th_game_ui *g);

// --- ventana_contratar.c --------------------------------------------------------
void gw_contratar_abrir(th_game_ui *g);
void gw_contratar_boton(th_game_ui *g, int id, int mouse);
void gw_contratar_frame(th_game_ui *g);
void gw_colocar_personal_frame(th_game_ui *g);
// ¿Cerrar colocar personal pide TH_ORD_DEVOLVER? (hay alguien en la mano y su
// TH_ORD_CONTRATAR no está en vuelo).
static inline int gw_persona_por_devolver(const th_game_ui *g)
{
    return g->active == TH_GW_COLOCAR_PERSONAL && g->clase >= 0 && !g->esperando_contrato;
}
// Cancelar la colocación (el derecho o Escape): TH_ORD_DEVOLVER y cerrar.
void gw_colocar_personal_cancelar(th_game_ui *g);

#endif
