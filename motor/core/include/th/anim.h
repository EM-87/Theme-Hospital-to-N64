// Tablas de animación en el formato TAN1 del conversor (tools/conv/anim.py):
// primer fotograma de cada animación, encadenamiento de fotogramas, listas de
// elementos y elementos. Solo lectura, sobre el fichero cargado, y con las
// mismas reglas que animation_manager de CorsixTH (th_gfx.cpp).
#ifndef TH_ANIM_H
#define TH_ANIM_H

#include <stddef.h>
#include <stdint.h>

#define TH_ANIM_LAYERS 13      // max_number_of_layers de CorsixTH
#define TH_ANIM_LIST_END 0xFFFFu

// Banderas de dibujo de un elemento o de una animación (thdf_* de CorsixTH).
enum {
    TH_ANIM_FLIP_H = 1u << 0,
    TH_ANIM_FLIP_V = 1u << 1,
    TH_ANIM_ALPHA50 = 1u << 2,
    TH_ANIM_ALPHA75 = 1u << 3,
};

// DrawingLayers de CorsixTH (utility.lua): orden de dibujo de las entidades
// dentro de su casilla; la vista además redibuja algunas junto a una animación.
enum { TH_LAYER_NORTH_SIDE = 1, TH_LAYER_ENTITY = 4, TH_LAYER_EAST_SIDE = 8, TH_LAYER_SOUTH_SIDE = 9 };

typedef struct th_anims {
    const uint8_t *data;
    uint16_t count;
    uint32_t frames, list_len, elements;
    uint32_t off_anim, off_frame, off_list, off_elem;
} th_anims;

typedef struct th_anim_element {
    uint16_t sprite;
    uint8_t x, y;   // desplazamiento crudo; en pantalla, x − 141 e y − 186 (a escala)
    uint8_t layer;  // capa (0–12) e id dentro de la capa (0 = siempre se dibuja)
    uint8_t layer_id;
    uint8_t flags;  // TH_ANIM_*
} th_anim_element;

int th_anims_init(th_anims *a, const uint8_t *data, size_t len); // 0 = bien

// animation_manager::get_first_frame y get_next_frame.
uint32_t th_anims_first_frame(const th_anims *a, unsigned anim);
uint32_t th_anims_next_frame(const th_anims *a, uint32_t frame);
// Duración en fotogramas (getAnimLength de CorsixTH, sin los ajustes de su Lua).
unsigned th_anims_length(const th_anims *a, unsigned anim);
// ¿Tiene más de un fotograma? (is_multiple_frame_animation_fn)
int th_anims_is_multi_frame(const th_anims *a, unsigned anim);

// Elementos de un fotograma: th_anims_list_entry(a, i) para i desde
// th_anims_frame_list(a, frame) hasta que devuelva TH_ANIM_LIST_END.
uint32_t th_anims_frame_list(const th_anims *a, uint32_t frame);
uint16_t th_anims_list_entry(const th_anims *a, uint32_t i);
void th_anims_element(const th_anims *a, uint16_t e, th_anim_element *out);

#endif
