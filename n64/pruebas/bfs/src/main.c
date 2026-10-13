// Medida para el informe de fidelidad: cuánto cuesta en la VR4300 comprobar
// que todo el hospital sigue siendo alcanzable desde la entrada (lo que exige
// CorsixTH con blocking_off_areas = 1 o 2 cada vez que se coloca un objeto o
// una habitación). Mapa de 128x128 casillas con muros entre casillas y una
// cuadrícula de habitaciones con puerta, recorrido en anchura (BFS).
#include <libdragon.h>
#include <string.h>
#include <stdint.h>

#define W 128
#define H 128
#define N (W * H)
// Bits de paso por casilla: se puede salir hacia el norte, este, sur, oeste.
enum { PN = 1, PE = 2, PS = 4, PW = 8, INDOOR = 16 };

static uint8_t flags[N];
static uint8_t visited[N];
static uint16_t queue[N];

static void build_map(void)
{
    memset(flags, 0, sizeof flags);
    // Edificio de 100x100 casillas; fuera, terreno no transitable.
    for (int y = 10; y < 110; y++)
        for (int x = 10; x < 110; x++)
            flags[y * W + x] = INDOOR | PN | PE | PS | PW;
    // Bordes del edificio: sin paso hacia fuera.
    for (int i = 10; i < 110; i++) {
        flags[10 * W + i] &= ~PN; flags[109 * W + i] &= ~PS;
        flags[i * W + 10] &= ~PW; flags[i * W + 109] &= ~PE;
    }
    // Habitaciones de 6x6 separadas por pasillos de 2: muros con una puerta al sur.
    for (int ry = 12; ry + 6 <= 108; ry += 8)
        for (int rx = 12; rx + 6 <= 108; rx += 8) {
            for (int i = 0; i < 6; i++) {
                int t;
                t = ry * W + rx + i;           flags[t] &= ~PN; flags[t - W] &= ~PS;
                t = (ry + 5) * W + rx + i;     if (i != 2) { flags[t] &= ~PS; flags[t + W] &= ~PN; }
                t = (ry + i) * W + rx;         flags[t] &= ~PW; flags[t - 1] &= ~PE;
                t = (ry + i) * W + rx + 5;     flags[t] &= ~PE; flags[t + 1] &= ~PW;
            }
        }
}

static int bfs(int start)
{
    memset(visited, 0, sizeof visited);
    int head = 0, tail = 0, count = 0;
    queue[tail++] = start; visited[start] = 1;
    while (head < tail) {
        int t = queue[head++];
        uint8_t f = flags[t];
        count++;
        if ((f & PN) && !visited[t - W]) { visited[t - W] = 1; queue[tail++] = t - W; }
        if ((f & PE) && !visited[t + 1]) { visited[t + 1] = 1; queue[tail++] = t + 1; }
        if ((f & PS) && !visited[t + W]) { visited[t + W] = 1; queue[tail++] = t + W; }
        if ((f & PW) && !visited[t - 1]) { visited[t - 1] = 1; queue[tail++] = t - 1; }
    }
    return count;
}

// Variante: visitadas en un mapa de bits (2 KiB, cabe en la caché de datos de 8 KiB).
static uint32_t vbits[N / 32];
#define VIS(t) (vbits[(t) >> 5] & (1u << ((t) & 31)))
#define SETV(t) (vbits[(t) >> 5] |= (1u << ((t) & 31)))
static int bfs_bits(int start)
{
    memset(vbits, 0, sizeof vbits);
    int head = 0, tail = 0, count = 0;
    queue[tail++] = start; SETV(start);
    while (head < tail) {
        int t = queue[head++];
        uint8_t f = flags[t];
        count++;
        if ((f & PN) && !VIS(t - W)) { SETV(t - W); queue[tail++] = t - W; }
        if ((f & PE) && !VIS(t + 1)) { SETV(t + 1); queue[tail++] = t + 1; }
        if ((f & PS) && !VIS(t + W)) { SETV(t + W); queue[tail++] = t + W; }
        if ((f & PW) && !VIS(t - 1)) { SETV(t - 1); queue[tail++] = t - 1; }
    }
    return count;
}

int main(void)
{
    debug_init_emulog();
    build_map();
    int indoor = 0;
    for (int i = 0; i < N; i++) indoor += (flags[i] & INDOOR) != 0;
    uint32_t best = UINT32_MAX, sum = 0; int reached = 0;
    const int reps = 20;
    for (int r = 0; r < reps; r++) {
        uint32_t t0 = get_ticks();
        reached = bfs(11 * W + 11);
        uint32_t dt = get_ticks() - t0;
        sum += dt; if (dt < best) best = dt;
    }
    // get_ticks() avanza a la mitad del reloj de la CPU (46,875 MHz).
    debugf("TH64 bfs: casillas interiores %d, alcanzadas %d\n", indoor, reached);
    debugf("TH64 bfs: min %lu us (%lu ciclos CPU), media %lu us, %d repeticiones\n",
           (unsigned long)TICKS_TO_US(best), (unsigned long)best * 2,
           (unsigned long)TICKS_TO_US(sum / reps), reps);
    debugf("TH64 bfs: memoria temporal %d B (visitadas) + %d B (cola)\n",
           (int)sizeof visited, (int)sizeof queue);
    best = UINT32_MAX; sum = 0;
    for (int r = 0; r < reps; r++) {
        uint32_t t0 = get_ticks();
        reached = bfs_bits(11 * W + 11);
        uint32_t dt = get_ticks() - t0;
        sum += dt; if (dt < best) best = dt;
    }
    debugf("TH64 bfs bits: alcanzadas %d, min %lu us (%lu ciclos CPU), media %lu us; visitadas %d B\n",
           reached, (unsigned long)TICKS_TO_US(best), (unsigned long)best * 2,
           (unsigned long)TICKS_TO_US(sum / reps), (int)sizeof vbits);
    debugf("TH64 bfs: fin\n");
    for (;;) {}
}
