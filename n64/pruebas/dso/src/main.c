// Prueba de módulos cargables (dlopen + n64dso): RAM antes/después y tiempo de carga.
#include <libdragon.h>
#include <dlfcn.h>

static void heap(const char *what)
{
    heap_stats_t h; sys_get_heap_stats(&h);
    debugf("TH64 dso: %-34s heap usado %7d B\n", what, h.used);
}

int main(void)
{
    debug_init_emulog();
    dfs_init(DFS_DEFAULT_LOCATION);
    heap("inicio");
    for (int vuelta = 0; vuelta < 2; vuelta++) {
        uint32_t t0 = TICKS_READ();
        void *h = dlopen("rom:/sim.dso", RTLD_LOCAL);
        uint32_t t1 = TICKS_READ();
        assertf(h, "dlopen: %s", dlerror());
        heap("tras dlopen(sim.dso)");
        void (*init_map)(void) = dlsym(h, "sim_init_map");
        void (*init_ents)(void) = dlsym(h, "sim_init_ents");
        int (*astar)(int,int,int,int,uint16_t*,int) = dlsym(h, "sim_astar");
        static uint16_t camino[255];
        init_map(); init_ents();
        int len = astar(1, 1, 120, 120, camino, 255);
        debugf("TH64 dso: vuelta %d dlopen %lu us, camino %d pasos\n", vuelta, TICKS_TO_US(TICKS_DISTANCE(t0, t1)), len);
        dlclose(h);
        heap("tras dlclose(sim.dso)");
    }
    uint32_t t2 = TICKS_READ();
    void *m = dlopen("rom:/menu.dso", RTLD_LOCAL);
    debugf("TH64 dso: dlopen(menu.dso) %lu us\n", TICKS_TO_US(TICKS_SINCE(t2)));
    heap("tras dlopen(menu.dso)");
    int (*dib)(int) = dlsym(m, "menu_dibuja");
    debugf("TH64 dso: menu %d bytes\n", dib(20));
    dlclose(m);
    heap("tras dlclose(menu.dso)");
    debugf("TH64 dso: fin\n");
    if (emux_detect(1)) emux_ioctl_exit();
    for (;;) {}
}
