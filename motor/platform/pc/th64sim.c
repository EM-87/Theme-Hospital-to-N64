// th64sim: el núcleo del motor en el PC, sin pantalla.
// Por ahora solo calcula la huella de la simulación de referencia, para
// compararla con la de la ROM (tools/motor_check.sh). Más adelante cargará un
// nivel convertido y un escenario, y escribirá el mismo volcado que el oráculo
// de CorsixTH (docs/04_construccion.md).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "th/version.h"
#include "th_test.h"

void (*th_test_log)(const char *fmt, ...) = NULL;

static int uso(void)
{
    fprintf(stderr, "th64sim %s\nUso: th64sim huella <semilla> <ticks>\n", TH64_VERSION);
    return 2;
}

int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[1], "huella") == 0) {
        uint32_t seed = (uint32_t)strtoul(argv[2], NULL, 0);
        int ticks = atoi(argv[3]);
        printf("%08lx\n", (unsigned long)th_golden_sim(seed, ticks));
        return 0;
    }
    return uso();
}
