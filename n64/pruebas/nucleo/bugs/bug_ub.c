// Error a propósito 2: producto en coma fija sin intermedio de 64 bits y
// desplazamiento a la izquierda de un negativo.
#include <stdio.h>
#include <stdlib.h>
#include "th/fixed.h"
static fx32 fx_mul_mal(fx32 a, fx32 b) { return (a * b) >> 16; }   // desborda int32
static fx32 fx_div_mal(fx32 a, fx32 b) { return (a << 16) / b; }   // a < 0: UB
int main(int argc, char **argv)
{
    int k = argc > 1 ? atoi(argv[1]) : 300;
    fx32 x = fx_from_int(k);
    printf("%ld %ld\n", (long)fx_mul_mal(x, x), (long)fx_div_mal(-x, fx_from_int(2)));
    return 0;
}
