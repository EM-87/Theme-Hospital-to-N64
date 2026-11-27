// Puntos de pausa de la simulación. El núcleo llama a th_pausar() entre
// humanoide y humanoide del tick, al empezar cada búsqueda de camino y cada
// 128 nodos de una búsqueda (un humanoide puede hacer cien búsquedas cortas en
// un tick: cuando sale alguien de una cola larga, los demás buscan banco).
// Con las órdenes del jugador (el riesgo 1 de la demo), además: entre orden y
// orden (th_sim_apply_orders) y tras la obra que termina sola
// (th_build_tick), y cada 16 filas al leer el mapa para la temperatura al
// acabar una orden que lo cambió (th_sim_temperature_flush; la orden solo
// lo apunta con th_sim_map_changed). En esas, la orden está entera: el mapa,
// la lista de objetos (s->place) y la fase de la obra (s->obra), que las
// ventanas leen, como los dejó. La ROM dibuja el mapa mientras la
// simulación está suspendida: tests/test_build.c (test_ordenes_pausadas)
// mira en todas las pausas de unas órdenes por th_sim_tick, también las de
// los caminos, que las sombras van con las paredes y que no se pasa a
// través de ninguna pared. Lo que esa prueba no cubre (riesgos de un
// fotograma, solo de dibujo; ni la demo ni las cuatro listas de la batería
// de construir los dan):
//   - Las pausas de los caminos (path.c) caen a media orden: al poner o
//     quitar un objeto, la lista de objetos o la fase de la obra pueden ser
//     las de antes durante un fotograma.
//   - finish_room deja las sombras en espera (th_map_hold_shadows) mientras
//     pone las paredes y las puertas; si una puerta busca camino y la ROM se
//     suspende ahí, las paredes que cambien de tipo de sombra se dibujan con
//     la de antes. Las del plano ya estaban puestas, así que en la práctica
//     no cambian (0 pausas mal con las sombras en espera en la prueba y en
//     la batería).
//   - Deshacer una obra con paredes (TH_ORD_ATRAS, o una orden «sala» que
//     abandona la que hay: remove_wall_line y th_map_unmark_room) tiene
//     pausas de caminos con las paredes ya quitadas y las sombras sin
//     rehacer: en la lista «salas» de la batería, 26 pausas con 2 casillas
//     con la sombra que no toca, en las órdenes 174, 192 y 248 (atrás) y 241
//     (sala); las mismas antes del riesgo 1. La demo no deshace obras.
// Las entidades no salen del mapa: la ROM las dibuja de la instantánea del
// tick anterior hasta que este acaba, también entre humanoide y humanoide.
// En la N64 la simulación corre en una corrutina (platform/n64/simulacion.c)
// y el gancho la suspende si se le ha acabado el tiempo del fotograma; el
// resto lo hace en el fotograma siguiente. En el PC no hay gancho. No cambia
// nada de lo que se calcula: solo cuándo.
#ifndef TH_PAUSA_H
#define TH_PAUSA_H

extern void (*th_pausa)(void);

static inline void th_pausar(void)
{
    if (th_pausa)
        th_pausa();
}

#endif
