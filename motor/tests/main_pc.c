// Ejecutable de pruebas del núcleo en el PC (ctest lo ejecuta en cada compilación).
#include <stdarg.h>
#include <stdio.h>

#include "th_test.h"

static void log_stdout(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void (*th_test_log)(const char *fmt, ...) = log_stdout;

int main(void)
{
    int f = th_run_core_tests();
    // Informativo: el float no da la misma huella en el PC y en la N64 (docs/04_construccion.md).
    printf("huella golden_sim_float(1, 10000) = %08lx\n", (unsigned long)th_golden_sim_float(1, 10000));
    return f ? 1 : 0;
}
