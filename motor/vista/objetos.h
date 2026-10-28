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

#endif
