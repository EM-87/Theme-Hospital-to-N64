// Capa de plataforma de ejemplo: ejecuta en la N64 las mismas pruebas del
// núcleo que en el PC y mide el coste de cada tipo numérico en la VR4300.
#include <libdragon.h>
#include <stdarg.h>

#include "th/fixed.h"
#include "th_test.h"

static void log_debugf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    debugf("TH64 test: %s", buf);
}

void (*th_test_log)(const char *fmt, ...) = log_debugf;

#define N 200000
#define BARRIER(v) __asm__ volatile("" : "+r"(v))
#define BARRIERF(v) __asm__ volatile("" : "+f"(v))

static uint32_t cycles(uint64_t t0, uint64_t t1) // ciclos de CPU por iteración x100
{
    return (uint32_t)(((t1 - t0) * 2u * 100u) / N);
}

static void bench(void)
{
    uint64_t t0, t1;
    uint32_t base;
    int32_t x = 12345;
    t0 = get_ticks();
    for (int i = 0; i < N; i++) BARRIER(x);
    t1 = get_ticks();
    base = cycles(t0, t1);
    debugf("TH64 bench: %-26s %4lu.%02lu ciclos/iter\n", "bucle vacio", base / 100, base % 100);
#define REPORT(name)                                                                   \
    do {                                                                               \
        uint32_t c = cycles(t0, t1) - base;                                            \
        debugf("TH64 bench: %-26s %4lu.%02lu ciclos/op (neto)\n", name, c / 100, c % 100); \
    } while (0)

    t0 = get_ticks();
    for (int i = 0; i < N; i++) { x = x + 7; BARRIER(x); }
    t1 = get_ticks(); REPORT("int32 suma");

    int32_t m = 3;
    BARRIER(m); // multiplicador opaco: sin él, gcc cambia x*3 por desplazamiento y suma
    t0 = get_ticks();
    for (int i = 0; i < N; i++) { x = x * m + 1; BARRIER(x); }
    t1 = get_ticks(); REPORT("int32 mul+suma");
    t0 = get_ticks();
    for (int i = 0; i < N; i++) { x = x * 3 + 1; BARRIER(x); }
    t1 = get_ticks(); REPORT("int32 x*3+1 (constante)");

    int32_t d = 1000000007;
    t0 = get_ticks();
    for (int i = 0; i < N; i++) { x = d / ((x & 0xff) | 1); BARRIER(x); }
    t1 = get_ticks(); REPORT("int32 div");

    fx32 fx = fx_ratio(3, 2), k = fx_ratio(2, 3);
    t0 = get_ticks();
    for (int i = 0; i < N; i++) { fx = fx_mul(fx, k) + FX_ONE; BARRIER(fx); }
    t1 = get_ticks(); REPORT("Q16.16 fx_mul+suma");

    t0 = get_ticks();
    for (int i = 0; i < N; i++) { fx = fx_div(FX_ONE * 3, fx | 1); BARRIER(fx); }
    t1 = get_ticks(); REPORT("Q16.16 fx_div");

    float f = 1.5f, fa = 0.999f, fb = 0.25f;
    t0 = get_ticks();
    for (int i = 0; i < N; i++) { f = f * fa + fb; BARRIERF(f); }
    t1 = get_ticks(); REPORT("float mul+suma");

    t0 = get_ticks();
    for (int i = 0; i < N; i++) { f = fa / f + fb; BARRIERF(f); }
    t1 = get_ticks(); REPORT("float div+suma");

    double g = 1.5, ga = 0.999, gb = 0.25;
    t0 = get_ticks();
    for (int i = 0; i < N; i++) { g = g * ga + gb; BARRIERF(g); }
    t1 = get_ticks(); REPORT("double mul+suma");

    t0 = get_ticks();
    for (int i = 0; i < N; i++) { g = ga / g + gb; BARRIERF(g); }
    t1 = get_ticks(); REPORT("double div+suma");

    t0 = get_ticks();
    for (int i = 0; i < N; i++) { x = (int32_t)(f * 100.0f); f += 0.5f; BARRIER(x); BARRIERF(f); }
    t1 = get_ticks(); REPORT("float->int32 + suma");
    debugf("TH64 bench: control %ld %ld %ld\n", (long)x, (long)fx, (long)(f + (float)g));
}

int main(void)
{
    debug_init_emulog();
    debugf("TH64 test: inicio (N64, %s)\n", __VERSION__);
    int fails = th_run_core_tests();
    debugf("TH64 test: huella golden_sim_float(1, 10000) = %08lx\n",
           (unsigned long)th_golden_sim_float(1, 10000));
    debugf("TH64 test: resultado %s\n", fails ? "FALLO" : "OK");
#ifdef TH_BUG_UB
    {   // Error a propósito: producto Q16.16 sin intermedio de 64 bits.
        volatile fx32 a = fx_from_int(300);
        fx32 b = a;
        debugf("TH64 test: provocando desbordamiento con signo...\n");
        fx32 r = (a * b) >> 16;
        debugf("TH64 test: resultado erroneo %ld (no deberia verse con UBSan)\n", (long)r);
    }
#endif
    bench();
    debugf("TH64 test: fin\n");
    for (;;) {}
}
