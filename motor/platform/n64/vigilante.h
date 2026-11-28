// El vigilante de la ROM de diagnóstico (make VIGILANTE=1): un temporizador
// que mira cada 20 ms cuánto lleva la CPU en la fase del bucle que le dijo
// vigilante_fase y, si pasa del límite de esa fase, para la ROM con un assert
// de libdragon (la pantalla del inspector, con el rastreo de la pila) que dice
// la fase, los registros del RSP y del RDP, el contexto de la partida y las
// últimas fases con lo que duró cada una. Va 40 ms por delante de los
// RSP_WAIT_LOOP(200) de libdragon (display_get, rspq_wait, el cambio de búfer
// de la cola, el mezclador), que en la consola acaban en «wait loop timed out»
// con el RSP dormido y sin decir dónde (la primera línea de esa pantalla la
// come el borde de la tele). Sin TH_VIGILANTE no hay nada.
#pragma once
#include <stdint.h>

#ifdef TH_VIGILANTE
void vigilante_iniciar(void);
// La fase en que entra la CPU y cuántos ms puede durar; NULL desarma.
void vigilante_fase(const char *nombre, int limite_ms);
// Lo que se enseña además de la fase (una vez por fotograma).
void vigilante_contexto(uint32_t fotograma, uint32_t hora, int velocidad, int pausa, int personas, int ventana);
#else
static inline void vigilante_iniciar(void) {}
static inline void vigilante_fase(const char *nombre, int limite_ms)
{
    (void)nombre;
    (void)limite_ms;
}
static inline void vigilante_contexto(uint32_t fotograma, uint32_t hora, int velocidad, int pausa, int personas,
                                      int ventana)
{
    (void)fotograma;
    (void)hora;
    (void)velocidad;
    (void)pausa;
    (void)personas;
    (void)ventana;
}
#endif
