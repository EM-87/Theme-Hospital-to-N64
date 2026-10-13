// ROM de prueba de herramientas: mide ciclos del módulo sim.c con TICKS_READ,
// con profile.h y con los contadores EMUX de ares; deja una función y una
// variable para probar GDB; y, con -DSALIR_AL_FINAL, cierra ares por EMUX.
#include <libdragon.h>
#include "sim.h"

// Los PROFILE_* pegan el nombre del slot: tiene que ser un identificador (enum).
enum { SLOT_ASTAR = PROFILE_SLOT_USER, SLOT_TICK };

volatile uint32_t gdb_contador;          // variable para leer desde GDB
volatile uint32_t gdb_ultimo_checksum;

__attribute__((noinline)) void gdb_objetivo(uint32_t v)
{
    gdb_contador++;
    gdb_ultimo_checksum = v;
}

static uint16_t camino[PATH_SLOT];

int main(void)
{
    debug_init_emulog();
    uint32_t emux = emux_detect(1);   // subcódigo 1: EMUX_FEAT1_* (así lo usa libdragon)
    debugf("TH64 tools: emux=%08lx (0 = sin modo homebrew)\n", emux);
    profile_init(&(profile_parms_t){ .num_slots = 128, .dump_stderr_interval = -1 });
    profile_register(SLOT_ASTAR, "sim_astar", 0);
    profile_register(SLOT_TICK, "sim_tick", 0);

    sim_init_map();
    sim_init_ents();

    // 1) A*: 20 búsquedas entre esquinas opuestas y puntos pseudoaleatorios.
    uint32_t seed = 12345, total_len = 0;
    int nodes0 = sim_nodes_expanded;
    uint32_t t0 = TICKS_READ();
    if (emux) emux_prof_start(10);
    for (int i = 0; i < 20; i++) {
        seed = seed * 1103515245u + 12345u;
        int sx = 1 + (seed >> 8) % 6, sy = 1 + (seed >> 12) % 6;
        int tx = 120 + (seed >> 16) % 6, ty = 120 + (seed >> 20) % 6;
        PROFILE_START(SLOT_ASTAR, 0);
        int len = sim_astar(sx, sy, tx, ty, camino, PATH_SLOT);
        PROFILE_STOP(SLOT_ASTAR, 0);
        if (len > 0) total_len += len;
    }
    if (emux) emux_prof_stop(10);
    uint32_t dt = TICKS_DISTANCE(t0, TICKS_READ());
    int nodes = sim_nodes_expanded - nodes0;
    debugf("TH64 tools: astar 20 busquedas, %ld pasos, %d nodos, %lu ticks = %lu ciclos CPU, %lu ciclos/nodo, %lu us\n",
           total_len, nodes, dt, dt * 2, (dt * 2) / (nodes ? nodes : 1), TICKS_TO_US(dt));
    if (emux)
        debugf("TH64 tools: astar EMUX ciclos=%llu dcache aciertos=%llu fallos=%llu icache fallos=%llu\n",
               emux_prof_read(10, EMUX_PROF_CYCLES), emux_prof_read(10, EMUX_PROF_DCACHE_HITS),
               emux_prof_read(10, EMUX_PROF_DCACHE_MISSES), emux_prof_read(10, EMUX_PROF_ICACHE_MISSES));

    // 2) Entidades: 361 humanoides (hospital "lleno") durante 200 ticks.
    for (int i = 0; i < 361; i++) sim_spawn(i < 304 ? 0 : 1 + (i % 3));
    uint32_t tmax = 0, tsum = 0;
    if (emux) emux_prof_start(11);
    for (int t = 0; t < 200; t++) {
        uint32_t a = TICKS_READ();
        PROFILE_START(SLOT_TICK, 0);
        sim_tick();
        PROFILE_STOP(SLOT_TICK, 0);
        uint32_t d = TICKS_DISTANCE(a, TICKS_READ());
        tsum += d; if (d > tmax) tmax = d;
        if (sim_live() < 300) sim_spawn(0);
    }
    if (emux) emux_prof_stop(11);
    profile_next_frame();
    debugf("TH64 tools: tick medio %lu ciclos (%lu us), max %lu ciclos, vivos %d, caminos %lu, fallidos %lu\n",
           (tsum * 2) / 200, TICKS_TO_US(tsum / 200), tmax * 2, sim_live(), sim_stats[0], sim_stats[1]);
    if (emux)
        debugf("TH64 tools: ticks EMUX ciclos=%llu dcache fallos=%llu icache fallos=%llu\n",
               emux_prof_read(11, EMUX_PROF_CYCLES), emux_prof_read(11, EMUX_PROF_DCACHE_MISSES),
               emux_prof_read(11, EMUX_PROF_ICACHE_MISSES));
    uint32_t cs = sim_checksum();
    debugf("TH64 tools: checksum %08lx\n", cs);
    profile_dump();
    debugf("TH64 tools: fin\n");

#ifdef SALIR_AL_FINAL
    if (emux) emux_ioctl_exit();
#endif
    for (uint32_t i = 0;; i++) {
        gdb_objetivo(cs + i);
        wait_ms(16);
    }
}
