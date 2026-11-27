// Los objetos y las personas de la simulación (th/sim.h) como entidades de la
// vista, como los dibuja CorsixTH: Object:initOrientation y setTile para los
// objetos (animación de reposo, espejo, casilla de anclaje, desplazamiento,
// capa de dibujo, lista temprana y las partes recortadas de los objetos de
// varias casillas), la basura del suelo (Litter:setLitterType) y la animación
// en curso de cada humanoide.
#ifndef TH_VISTA_OBJETOS_H
#define TH_VISTA_OBJETOS_H

#include "th/sim.h"
#include "vista.h"

// Entidades que puede dar th_vista_sim como mucho.
#define TH_VISTA_SIM_MAX (TH_SIM_OBJECTS * 3 + TH_SIM_LITTER + TH_SIM_HUMANOIDS)

// Rellena out con las entidades de la simulación; devuelve cuántas son.
int th_vista_sim(const th_sim *s, th_view_entity *out, int max);

// Lo mismo con la cámara girada rot (girar.h): las entidades van en casillas
// de la vista. Los objetos se colocan con la orientación girada o, si no se
// pueden poner así, con la opuesta (de frente en lugar de espaldas); las
// personas, con la animación que mira a la dirección girada, y quien usa un
// objeto, como lo pondría use_object con el objeto girado.
int th_vista_sim_rot(const th_sim *s, int rot, th_view_entity *out, int max);

// Un humanoide (ya puesto en e como con la cámara sin girar) con la cámara
// girada rot: en su posición girada y, si su animación es de las que van por
// parejas (caminar, quieto, llamar y cruzar puertas, como en walk_anims de
// CorsixTH), con la que mira a la dirección girada; si esa dirección no está
// dibujada (las de las necesidades solo miran al este o al sur), con la
// opuesta. Para quien usa un objeto hace falta la simulación: th_vista_sim_rot.
void th_vista_humanoide_rot(const th_humanoid *h, const th_anims *anims, int rot, th_view_entity *e);

// --- Fantasmas: lo que se dibuja sin estar en el mapa ----------------------
//
// pal: 0 (la paleta normal), TH_ANIM_PAL_GREY o TH_ANIM_PAL_RED (vista.h).
// CorsixTH pinta en gris el objeto y el empleado donde no se pueden soltar, y
// con la paleta normal donde sí. Las entidades van en casillas de la vista:
// se añaden a th_scene.entities (y se tapan con las paredes y los objetos como
// si estuvieran en el mapa) o, como CorsixTH, encima de todo con
// th_view_entity_on_top.

// El objeto de definición def (objects.<id> de las reglas) con origen en la
// casilla (x, y) del mundo y orientación dir (TH_DIR_*), con la cámara
// girada rot: lo mismo que th_vista_sim_rot haría con él en el mapa (la
// animación de reposo de la orientación o de su espejo, render_attach_position,
// animation_offset, capa de dibujo, lista temprana y, en los de varias
// columnas, una entidad recortada por columna), como UIPlaceObjects:
// setOrientation (place_objects.lua:417-472). Devuelve cuántas entidades
// puso (0 si el objeto no tiene animación para esa orientación).
int th_vista_objeto_fantasma(const th_rules *r, const th_anims *anims, th_rnode def, int x, int y, int dir, int rot,
                             int pal, th_view_entity *out, int max);

// Una persona de la clase w (th_walk_anims_find("Doctor"), "Nurse",
// "Handyman", "Receptionist"…) quieta en la casilla (x, y) del mundo,
// mirando a dir (TH_DIR_*, en la vista: el mismo dibujo con la cámara
// girada), con las capas layers (TH_ANIM_LAYERS bytes que deben durar
// mientras se dibuje, o NULL). UIPlaceStaff (place_staff.lua:43-50) usa la
// animación idle_east sin espejo (TH_DIR_EAST) y solo la capa 5 del perfil
// (profile.layer5, la cabeza).
void th_vista_persona_fantasma(const th_walk_anims *w, const th_anims *anims, int x, int y, int dir, int rot, int pal,
                               const uint8_t *layers, th_view_entity *out);

#endif
