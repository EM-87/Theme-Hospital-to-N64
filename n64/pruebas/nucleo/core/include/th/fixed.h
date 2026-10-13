// Coma fija Q16.16 para el estado de la simulación.
// C portable (C17): sin libdragon, sin float, mismo resultado en x86-64 y en la
// VR4300. Los productos usan un intermedio de 64 bits (dmult en la VR4300).
#ifndef TH_FIXED_H
#define TH_FIXED_H

#include <stdint.h>

typedef int32_t fx32; // Q16.16: 1.0 == 65536

#define FX_ONE ((fx32)65536)
#define FX_FRAC_BITS 16

static inline fx32 fx_from_int(int32_t i) { return (fx32)(i * FX_ONE); }

// Truncado hacia -infinito (división entera con signo, no desplazamiento:
// el desplazamiento a la derecha de negativos depende de la implementación).
static inline int32_t fx_floor(fx32 a)
{
    return (a >= 0) ? (a / FX_ONE) : -((-(int64_t)a + FX_ONE - 1) / FX_ONE);
}

// Producto con redondeo al más cercano; intermedio de 64 bits.
static inline fx32 fx_mul(fx32 a, fx32 b)
{
    int64_t p = (int64_t)a * (int64_t)b;
    p += (p >= 0) ? (FX_ONE / 2) : -(FX_ONE / 2);
    return (fx32)(p / FX_ONE);
}

// Cociente truncado hacia cero; b != 0.
static inline fx32 fx_div(fx32 a, fx32 b)
{
    return (fx32)(((int64_t)a * FX_ONE) / b);
}

// Fracción p/q en Q16.16 (p.ej. una tabla de datos: 3/4 -> 49152).
static inline fx32 fx_ratio(int32_t p, int32_t q)
{
    return (fx32)(((int64_t)p * FX_ONE) / q);
}

static inline fx32 fx_clamp(fx32 a, fx32 lo, fx32 hi)
{
    return a < lo ? lo : (a > hi ? hi : a);
}

#endif
