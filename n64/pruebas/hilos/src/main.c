// Coste de hilos (kernel.h) y corrutinas (coroutine.h): ciclos por cambio y RAM por instancia.
#include <libdragon.h>

static volatile int contador;
static int hilo(void *arg) { for (;;) { contador++; kthread_yield(); } return 0; }
static void corr(void *arg) { for (;;) { contador++; coro_yield(); } }

static int heap_usado(void) { heap_stats_t h; sys_get_heap_stats(&h); return h.used; }

int main(void)
{
    debug_init_emulog();
    const int N = 10000;
    // Corrutinas
    int h0 = heap_usado();
    coroutine_t *co[100];
    for (int i = 0; i < 100; i++) co[i] = coro_create(corr, NULL, 1024);
    int h1 = heap_usado();
    uint32_t t0 = TICKS_READ();
    for (int i = 0; i < N; i++) coro_resume(co[i % 100]);
    uint32_t dt = TICKS_SINCE(t0);
    debugf("TH64 hilos: corrutina pila 1024: %d B por corrutina; resume+yield %lu ciclos\n",
           (h1 - h0) / 100, dt * 2 / N);
    for (int i = 0; i < 100; i++) coro_destroy(co[i]);

    // Hilos del kernel
    kernel_init();
    int h2 = heap_usado();
    kthread_t *th = kthread_new("hilo", 1024, 0, hilo, NULL);
    int h3 = heap_usado();
    contador = 0;
    t0 = TICKS_READ();
    for (int i = 0; i < N; i++) kthread_yield();
    dt = TICKS_SINCE(t0);
    debugf("TH64 hilos: kthread pila 1024: %d B por hilo; %d cambios (yield ida y vuelta) %lu ciclos por yield, contador %d\n",
           h3 - h2, N, dt * 2 / N, contador);
    debugf("TH64 hilos: fin\n");
    if (emux_detect(1)) emux_ioctl_exit();
    for (;;) {}
}
