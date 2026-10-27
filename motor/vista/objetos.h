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

#endif
