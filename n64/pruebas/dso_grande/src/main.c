// Fase 4b (modo demo): coste en RAM y tiempo de los módulos cargables (DSO).
// ventana.dso: módulo pequeño; lua.dso: el núcleo de Lua 5.5 entero, como
// ejemplo de un módulo grande de «modo completo».
// Con -DESTATICO el núcleo de Lua va enlazado en el ejecutable, para comparar.
#include <libdragon.h>
#include <dlfcn.h>
#include <malloc.h>

static int heap_used(void)
{
    heap_stats_t h;
    sys_get_heap_stats(&h);
    return h.used;
}

static void heap(const char *what)
{
    heap_stats_t h;
    sys_get_heap_stats(&h);
    debugf("TH64 dso: %-40s usado %7d B  libre %7d B (%d KiB)\n", what, h.used, h.total - h.used, (h.total - h.used) / 1024);
}

static int largest(void)
{
    int lo = 0, hi = 8 * 1024 * 1024;
    while (hi - lo > 64) {
        int mid = (lo + hi) / 2;
        void * volatile p = malloc(mid);
        if (p) { free(p); lo = mid; } else hi = mid;
    }
    return lo;
}

static const char *prueba = "local t = {} for i = 1, 200 do t[i] = i * i end local s = 0 for i = 1, #t do s = s + t[i] end return s";

#ifdef ESTATICO
extern int modo_completo_ejecuta(const char *codigo);
#endif

int main(void)
{
    debug_init_emulog();
    dfs_init(DFS_DEFAULT_LOCATION);
    heap("inicio");
    debugf("TH64 dso: mayor bloque al inicio %d B\n", largest());
#ifdef ESTATICO
    debugf("TH64 dso: estatico, resultado %d\n", modo_completo_ejecuta(prueba));
    heap("tras ejecutar Lua estatico");
#else
    int u0 = heap_used();
    uint32_t t0 = TICKS_READ();
    void *v = dlopen("rom:/ventana.dso", RTLD_LOCAL);
    uint32_t t1 = TICKS_READ();
    assertf(v, "dlopen: %s", dlerror());
    int u1 = heap_used();
    debugf("TH64 dso: dlopen(ventana.dso) 1.a vez %lu us, +%d B\n", TICKS_TO_US(TICKS_DISTANCE(t0, t1)), u1 - u0);
    int (*dib)(int) = dlsym(v, "ventana_dibuja");
    debugf("TH64 dso: ventana %d\n", dib(100));
    dlclose(v);
    int u2 = heap_used();
    debugf("TH64 dso: tras dlclose(ventana.dso) queda +%d B (tabla de simbolos del ejecutable)\n", u2 - u0);
    t0 = TICKS_READ();
    v = dlopen("rom:/ventana.dso", RTLD_LOCAL);
    t1 = TICKS_READ();
    debugf("TH64 dso: dlopen(ventana.dso) 2.a vez %lu us, +%d B\n", TICKS_TO_US(TICKS_DISTANCE(t0, t1)), heap_used() - u2);
    dlclose(v);

    for (int vuelta = 0; vuelta < 2; vuelta++) {
        int a = heap_used();
        t0 = TICKS_READ();
        void *h = dlopen("rom:/lua.dso", RTLD_LOCAL);
        t1 = TICKS_READ();
        assertf(h, "dlopen: %s", dlerror());
        int b = heap_used();
        debugf("TH64 dso: dlopen(lua.dso) vuelta %d: %lu us, +%d B\n", vuelta, TICKS_TO_US(TICKS_DISTANCE(t0, t1)), b - a);
        int (*run)(const char *) = dlsym(h, "modo_completo_ejecuta");
        debugf("TH64 dso: lua.dso resultado %d\n", run(prueba));
        dlclose(h);
        debugf("TH64 dso: tras dlclose(lua.dso) %+d B respecto a antes de abrirlo; mayor bloque %d B\n", heap_used() - a, largest());
    }
#endif
    heap("fin");
    debugf("TH64 dso: fin\n");
    for (;;) {}
}
