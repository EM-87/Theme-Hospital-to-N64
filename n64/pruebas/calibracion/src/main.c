// Calibración del modelo de ciclos de ares: fallos de caché, aciertos, escrituras sucias.
#include <libdragon.h>

static uint32_t buf[256 * 1024] __attribute__((aligned(16)));   // 1 MiB
static uint32_t small[256] __attribute__((aligned(16)));        // 1 KiB, cabe en la caché

typedef struct { uint64_t cyc, hit, miss, wb, ihit, imiss; uint32_t ticks; } m_t;

static void rd(int s, m_t *m) {
    m->cyc = emux_prof_read(s, EMUX_PROF_CYCLES);
    m->hit = emux_prof_read(s, EMUX_PROF_DCACHE_HITS);
    m->miss = emux_prof_read(s, EMUX_PROF_DCACHE_MISSES);
    m->wb = emux_prof_read(s, EMUX_PROF_DCACHE_WBS);
    m->ihit = emux_prof_read(s, EMUX_PROF_ICACHE_HITS);
    m->imiss = emux_prof_read(s, EMUX_PROF_ICACHE_MISSES);
}
static void show(const char *n, m_t *m, uint32_t lines) {
    debugf("CAL %-14s ticks=%lu ciclos(TICKS*2)=%lu emux_ciclos=%llu dhit=%llu dmiss=%llu dwb=%llu ihit=%llu imiss=%llu ciclos/linea=%.2f\n",
           n, m->ticks, m->ticks * 2, m->cyc, m->hit, m->miss, m->wb, m->ihit, m->imiss,
           lines ? (double)(m->ticks * 2) / lines : 0.0);
}

__attribute__((noinline)) static uint32_t read_lines(volatile uint32_t *p, int n) {
    uint32_t sum = 0;
    for (int i = 0; i < n; i += 4) sum += p[i];
    return sum;
}
__attribute__((noinline)) static void write_lines(volatile uint32_t *p, int n) {
    for (int i = 0; i < n; i += 4) p[i] = i;
}
__attribute__((noinline)) static uint32_t read_small(volatile uint32_t *p, int reps) {
    uint32_t sum = 0;
    for (int r = 0; r < reps; r++)
        for (int i = 0; i < 256; i += 4) sum += p[i];
    return sum;
}
__attribute__((noinline)) static void alu_loop(int n) {
    __asm__ volatile(".set noreorder\n1: addiu %0, %0, -1\n bnez %0, 1b\n nop\n .set reorder\n" : "+r"(n));
}

int main(void)
{
    debug_init_emulog();
    debug_init_isviewer();
    uint32_t emux = emux_detect(1);
    debugf("CAL emux=%08lx\n", emux);
    for (int i = 0; i < 256 * 1024; i++) buf[i] = i;
    data_cache_hit_writeback_invalidate(buf, sizeof(buf));
    m_t m; uint32_t t; volatile uint32_t sink = 0;

    // 0) ALU: 3 instrucciones por vuelta
    emux_prof_clear(900); emux_prof_start(900); t = TICKS_READ();
    alu_loop(1000000);
    m.ticks = TICKS_READ() - t; emux_prof_stop(900); rd(900, &m); show("alu3x1e6", &m, 0);

    // 1) lectura de 1 MiB, una por línea, líneas limpias
    emux_prof_clear(901); emux_prof_start(901); t = TICKS_READ();
    sink += read_lines(buf, 256 * 1024);
    m.ticks = TICKS_READ() - t; emux_prof_stop(901); rd(901, &m); show("leer_limpio", &m, 65536);

    // 2) escritura de 1 MiB, una por línea (fallos + escrituras sucias de víctimas)
    emux_prof_clear(902); emux_prof_start(902); t = TICKS_READ();
    write_lines(buf, 256 * 1024);
    m.ticks = TICKS_READ() - t; emux_prof_stop(902); rd(902, &m); show("escribir", &m, 65536);

    // 3) lectura de 1 MiB con líneas sucias en la caché (cada fallo expulsa una sucia)
    emux_prof_clear(903); emux_prof_start(903); t = TICKS_READ();
    sink += read_lines(buf, 256 * 1024);
    m.ticks = TICKS_READ() - t; emux_prof_stop(903); rd(903, &m); show("leer_tras_esc", &m, 65536);

    // 4) aciertos: 1 KiB 10000 veces
    sink += read_small(small, 1);
    emux_prof_clear(904); emux_prof_start(904); t = TICKS_READ();
    sink += read_small(small, 10000);
    m.ticks = TICKS_READ() - t; emux_prof_stop(904); rd(904, &m); show("aciertos", &m, 640000);

    debugf("CAL sink=%lu fin\n", sink);
    emux_ioctl_exit();
    while (1) {}
}
