// Línea base en C con el mismo arranque (debugf + malloc) que la prueba de C++.
#include <libdragon.h>
#include <stdlib.h>
typedef struct { int id, coste; } ent_t;
int main(void)
{
    debug_init_emulog();
    int total = 0; ent_t *arr[10];
    for (int i = 0; i < 10; i++) { arr[i] = malloc(sizeof(ent_t)); arr[i]->coste = (i & 1) ? 3 : 7; total += arr[i]->coste; }
    debugf("TH64 c: total=%d\n", total);
    for (int i = 0; i < 10; i++) free(arr[i]);
    for (;;) {}
}
