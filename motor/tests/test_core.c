// Pruebas del núcleo: pool, coma fija, rng y, en test_map.c, el mapa. Mismo fichero en el
// PC (gcc/clang, con o sin sanitizers) y en la N64 (ROM que lo ejecuta en ares).
#include "th_test.h"

#include "th/fixed.h"
#include "th/pool.h"
#include "th/rng.h"

int th_test_failures;
int th_test_checks;

// Huella esperada de th_golden_sim(1, 10000), obtenida en el PC. Si la N64 da
// otra, el núcleo no es portable.
#ifndef TH_GOLDEN_EXPECTED
#define TH_GOLDEN_EXPECTED 0xd38fbaefu
#endif

static void test_fixed(void)
{
    TH_CHECK_EQ(fx_from_int(3), 196608);
    TH_CHECK_EQ(fx_mul(fx_ratio(3, 2), fx_from_int(2)), fx_from_int(3));
    TH_CHECK_EQ(fx_mul(fx_ratio(-3, 2), fx_ratio(1, 2)), -49152); // -0,75
    TH_CHECK_EQ(fx_mul(1, 1), 0);                                // 2^-32 redondea a 0
    TH_CHECK_EQ(fx_div(fx_from_int(1), fx_from_int(3)), 21845);
    TH_CHECK_EQ(fx_div(fx_from_int(-1), fx_from_int(3)), -21845); // trunca hacia cero
    TH_CHECK_EQ(fx_floor(fx_ratio(3, 2)), 1);
    TH_CHECK_EQ(fx_floor(fx_ratio(-1, 2)), -1);
    TH_CHECK_EQ(fx_floor(fx_from_int(-2)), -2);
    TH_CHECK_EQ(fx_ratio(3, 4), 49152);
    TH_CHECK_EQ(fx_clamp(fx_from_int(5), 0, FX_ONE), FX_ONE);
    // Producto intermedio de 3.932.225.536: cabe en 64 bits pero no en 32.
    TH_CHECK_EQ(fx_mul(fx_ratio(60001, 2), fx_ratio(1, 30000)), 60001);
}

typedef struct {
    int32_t a, b;
} small_t;

TH_POOL_STORAGE(tp, small_t, 4);

static void test_pool(void)
{
    th_pool p;
    TH_POOL_INIT(&p, tp, small_t, 4);
    th_handle h[5];
    for (int i = 0; i < 4; i++) {
        h[i] = th_pool_alloc(&p);
        TH_CHECK(h[i] != TH_HANDLE_NULL);
        small_t *s = th_pool_get(&p, h[i]);
        TH_CHECK(s != NULL);
        if (s)
            s->a = 100 + i;
    }
    h[4] = th_pool_alloc(&p);
    TH_CHECK_EQ(h[4], TH_HANDLE_NULL); // lleno
    TH_CHECK_EQ(p.count, 4);
    TH_CHECK_EQ(th_pool_free(&p, h[1]), 1);
    TH_CHECK_EQ(th_pool_free(&p, h[1]), 0);     // doble liberación detectada
    TH_CHECK(th_pool_get(&p, h[1]) == NULL);    // asa caducada
    th_handle n = th_pool_alloc(&p);            // reutiliza el hueco 1
    TH_CHECK_EQ(n & 0xffffu, h[1] & 0xffffu);
    TH_CHECK(n != h[1]);                        // con otra generación
    TH_CHECK(th_pool_get(&p, h[1]) == NULL);
    small_t *s = th_pool_get(&p, n);
    TH_CHECK(s != NULL && s->a == 0);           // llega a cero
    // Recorrido en orden de índice
    uint16_t cur = 0;
    int idx = 0;
    int32_t seen[4] = {0, 0, 0, 0};
    for (small_t *e; (e = th_pool_next(&p, &cur, NULL)) != NULL;)
        seen[idx++] = e->a;
    TH_CHECK_EQ(idx, 4);
    TH_CHECK_EQ(seen[0], 100);
    TH_CHECK_EQ(seen[1], 0);
    TH_CHECK_EQ(seen[2], 102);
    TH_CHECK_EQ(seen[3], 103);
    TH_CHECK(th_pool_get(&p, 0) == NULL);
    TH_CHECK(th_pool_get(&p, 0x00010009u) == NULL); // índice fuera de rango
}

static void test_rng(void)
{
    th_rng r;
    th_rng_seed(&r, 1);
    TH_CHECK_EQ(th_rng_u32(&r), 270369u); // xorshift32(1), calculado a mano
    int hist[6] = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 60000; i++) {
        int32_t v = th_rng_range(&r, 1, 6);
        TH_CHECK(v >= 1 && v <= 6);
        if (v >= 1 && v <= 6)
            hist[v - 1]++;
    }
    for (int i = 0; i < 6; i++)
        TH_CHECK(hist[i] > 9500 && hist[i] < 10500);
}

// --- Simulación de referencia -------------------------------------------------
// Pacientes con sed y felicidad; aparecen, se mueven, beben y se van. Toda la
// aritmética es entera o Q16.16; el recorrido es en orden de índice.
typedef struct {
    fx32 happiness, thirst;
    int16_t x, y;
} pat_t;

#define SIM_CAP 384
TH_POOL_STORAGE(sim, pat_t, SIM_CAP);

static uint32_t fnv(uint32_t h, uint32_t v)
{
    for (int i = 0; i < 4; i++) {
        h ^= (v >> (8 * i)) & 0xffu; // por bytes en orden fijo: no depende del endianness
        h *= 16777619u;
    }
    return h;
}

uint32_t th_golden_sim(uint32_t seed, int ticks)
{
    th_pool p;
    th_rng r;
    TH_POOL_INIT(&p, sim, pat_t, SIM_CAP);
    th_rng_seed(&r, seed);
    uint32_t h = 2166136261u;
    const fx32 thirst_rate = fx_ratio(1, 50), decay = fx_ratio(1, 100), drink = fx_ratio(1, 10);
    for (int t = 0; t < ticks; t++) {
        if (th_rng_range(&r, 0, 3) == 0) {
            th_handle nh = th_pool_alloc(&p);
            pat_t *e = th_pool_get(&p, nh);
            if (e) {
                e->happiness = fx_ratio(th_rng_range(&r, 50, 100), 100);
                e->x = (int16_t)th_rng_range(&r, 0, 127);
                e->y = 127;
            }
        }
        uint16_t cur = 0;
        th_handle eh;
        for (pat_t *e; (e = th_pool_next(&p, &cur, &eh)) != NULL;) {
            e->thirst += thirst_rate;
            e->happiness -= fx_mul(e->thirst, decay);
            e->x = (int16_t)fx_clamp(e->x + th_rng_range(&r, -1, 1), 0, 127);
            e->y = (int16_t)fx_clamp(e->y - th_rng_range(&r, 0, 1), 0, 127);
            if (e->thirst > FX_ONE) {
                e->thirst = 0;
                e->happiness = fx_clamp(e->happiness + drink, 0, FX_ONE);
            }
            if (e->happiness <= 0 || e->y == 0)
                th_pool_free(&p, eh);
            else
                h = fnv(fnv(fnv(h, (uint32_t)e->happiness), (uint32_t)e->thirst),
                        ((uint32_t)(uint16_t)e->x << 16) | (uint16_t)e->y);
        }
        h = fnv(h, p.count);
    }
    return h;
}

// Igual, pero con float: depende de cómo redondee cada compilador y FPU.
typedef struct {
    float happiness, thirst;
    int16_t x, y;
} patf_t;
TH_POOL_STORAGE(simf, patf_t, SIM_CAP);

uint32_t th_golden_sim_float(uint32_t seed, int ticks)
{
    th_pool p;
    th_rng r;
    TH_POOL_INIT(&p, simf, patf_t, SIM_CAP);
    th_rng_seed(&r, seed);
    uint32_t h = 2166136261u;
    for (int t = 0; t < ticks; t++) {
        if (th_rng_range(&r, 0, 3) == 0) {
            th_handle nh = th_pool_alloc(&p);
            patf_t *e = th_pool_get(&p, nh);
            if (e) {
                e->happiness = (float)th_rng_range(&r, 50, 100) / 100.0f;
                e->x = (int16_t)th_rng_range(&r, 0, 127);
                e->y = 127;
            }
        }
        uint16_t cur = 0;
        th_handle eh;
        for (patf_t *e; (e = th_pool_next(&p, &cur, &eh)) != NULL;) {
            e->thirst += 1.0f / 50.0f;
            e->happiness -= e->thirst * (1.0f / 100.0f);
            int x = e->x + th_rng_range(&r, -1, 1), y = e->y - th_rng_range(&r, 0, 1);
            e->x = (int16_t)(x < 0 ? 0 : (x > 127 ? 127 : x));
            e->y = (int16_t)(y < 0 ? 0 : y);
            if (e->thirst > 1.0f) {
                e->thirst = 0;
                e->happiness = e->happiness + 0.1f > 1.0f ? 1.0f : e->happiness + 0.1f;
            }
            if (e->happiness <= 0 || e->y == 0) {
                th_pool_free(&p, eh);
            } else {
                union { float f; uint32_t u; } a = {e->happiness}, b = {e->thirst};
                h = fnv(fnv(fnv(h, a.u), b.u), ((uint32_t)(uint16_t)e->x << 16) | (uint16_t)e->y);
            }
        }
        h = fnv(h, p.count);
    }
    return h;
}

static void test_golden(void)
{
    uint32_t g = th_golden_sim(1, 10000);
    th_test_log("huella golden_sim(1, 10000) = %08lx\n", (unsigned long)g);
    TH_CHECK_EQ(th_golden_sim(1, 10000), g); // repetible en la misma plataforma
    if (TH_GOLDEN_EXPECTED != 0u)
        TH_CHECK_EQ(g, TH_GOLDEN_EXPECTED);
}

int th_run_core_tests(void)
{
    th_test_failures = 0;
    th_test_checks = 0;
    test_fixed();
    test_pool();
    test_rng();
    test_golden();
    th_test_map();
    th_test_log("pruebas: %d comprobaciones, %d fallos\n", th_test_checks, th_test_failures);
    return th_test_failures;
}
