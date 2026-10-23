// Generador pseudoaleatorio del núcleo: xorshift32 (Marsaglia, 2003).
// Estado de 4 bytes (cabe en la partida guardada), solo enteros, idéntico en
// todas las plataformas. El rango usa multiplicación y desplazamiento en vez de
// módulo.
#ifndef TH_RNG_H
#define TH_RNG_H

#include <stdint.h>

typedef struct th_rng {
    uint32_t s;
} th_rng;

static inline void th_rng_seed(th_rng *r, uint32_t seed) { r->s = seed ? seed : 0x9e3779b9u; }

static inline uint32_t th_rng_u32(th_rng *r)
{
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}

// Entero uniforme en [lo, hi] (como math.random(lo, hi) de CorsixTH).
static inline int32_t th_rng_range(th_rng *r, int32_t lo, int32_t hi)
{
    uint32_t n = (uint32_t)(hi - lo) + 1u;
    return lo + (int32_t)(((uint64_t)th_rng_u32(r) * n) >> 32);
}

#endif
