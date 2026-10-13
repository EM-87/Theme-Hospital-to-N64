// Error a propósito 1: el almacenamiento es de 4 y se inicializa con 5.
#include <stdio.h>
#include "th/pool.h"
typedef struct { int32_t a, b; } small_t;
TH_POOL_STORAGE(bp, small_t, 4);
int main(void)
{
    th_pool p;
    th_pool_init(&p, bp_items, bp_gen, bp_next, 5, sizeof(small_t)); // ERROR: 5 > 4
    int n = 0;
    while (th_pool_alloc(&p)) n++;
    printf("alocados %d\n", n);
    return 0;
}
