// Error a propósito 3: se lee un campo que nadie ha escrito (memoria dinámica).
#include <stdio.h>
#include <stdlib.h>
#include "th/fixed.h"
typedef struct { fx32 happiness, thirst; } pat_t;
int main(void)
{
    pat_t *p = malloc(4 * sizeof *p);
    p[0].happiness = FX_ONE;                    // thirst sin inicializar
    if (p[0].thirst > FX_ONE / 2) puts("sed");  // decisión con un valor indeterminado
    else puts("sin sed");
    free(p);
    return 0;
}
