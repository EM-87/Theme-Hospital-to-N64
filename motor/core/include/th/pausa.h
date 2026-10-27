// Puntos de pausa de la simulación. El núcleo llama a th_pausar() entre
// humanoide y humanoide del tick, al empezar cada búsqueda de camino y cada
// 128 nodos de una búsqueda (un humanoide puede hacer cien búsquedas cortas en
// un tick: cuando sale alguien de una cola larga, los demás buscan banco).
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
