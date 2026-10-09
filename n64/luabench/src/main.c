// Banco de pruebas de CPU para la fase 1: ejecuta los microbenchmarks de
// filesystem/run.lua con el intérprete Lua compilado para la N64 y una
// búsqueda en anchura en C (el pathfinding de CorsixTH es C++). Los
// resultados salen por ISViewer con el prefijo TH64LUA / TH64C y también en
// pantalla, para poder leerlos en una consola real sin cable USB.
#include <stdarg.h>
#include <stdio.h>
#include <libdragon.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "../../luabench_common/bfs.h"

// Calibrado: bucle de 3 instrucciones (addiu, bnez y el nop del hueco de
// retardo) que cabe en la caché. A 1 ciclo por instrucción, 3 ciclos por vuelta.
static void __attribute__((noinline)) calib_loop(int n)
{
    __asm__ volatile(
        ".set noreorder\n"
        "1: addiu %0, %0, -1\n"
        "bnez %0, 1b\n"
        "nop\n"
        ".set reorder\n" : "+r"(n));
}

// Calibrado de memoria: suma de 1 MiB, mucho mayor que la caché de datos (8 KiB).
static uint32_t calib_mem(void)
{
    static uint32_t buf[256 * 1024];
    for (int i = 0; i < 256 * 1024; i++) buf[i] = i;
    data_cache_hit_writeback_invalidate(buf, sizeof(buf));
    volatile uint32_t *p = buf; // que el compilador no pueda resolver la suma
    uint32_t sum = 0;
    for (int r = 0; r < 4; r++)
        for (int i = 0; i < 256 * 1024; i += 4) // una lectura por línea de 16 bytes
            sum += p[i];
    return sum;
}

// Escribe una línea por ISViewer y en la consola de pantalla.
static void out(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    debugf("%s\n", buf);
    printf("%s\n", buf);
    console_render();
}

static int l_us(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)TIMER_MICROS_LL(get_ticks()));
    return 1;
}

static int l_log(lua_State *L)
{
    out("%s", luaL_checkstring(L, 1));
    return 0;
}

// Solo las bibliotecas que usan los benchmarks: io, os y package necesitan
// servicios del sistema que la N64 no tiene.
static void open_libs(lua_State *L)
{
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
    lua_pop(L, 5);
}

int main(void)
{
    debug_init_emulog();
    dfs_init(DFS_DEFAULT_LOCATION);
    console_init();
    console_set_render_mode(RENDER_MANUAL);

    long long c0 = TIMER_MICROS_LL(get_ticks());
    calib_loop(10000000);
    long long c1 = TIMER_MICROS_LL(get_ticks());
    out("TH64CAL bucle us=%lld mhz_efectivos=%.2f", c1 - c0, 30000000.0 / (c1 - c0));
    c0 = TIMER_MICROS_LL(get_ticks());
    uint32_t ms = calib_mem();
    c1 = TIMER_MICROS_LL(get_ticks());
    out("TH64CAL memoria us=%lld lineas=%d ns_por_linea=%.1f chk=%lu", c1 - c0, 4 * 65536,
           (c1 - c0) * 1000.0 / (4 * 65536), (unsigned long)ms);

    out("TH64C bfs_inicio");
    long long t0 = TIMER_MICROS_LL(get_ticks());
    long chk = th64_bfs_bench();
    long long t1 = TIMER_MICROS_LL(get_ticks());
    out("TH64C bfs us=%lld chk=%ld", t1 - t0, chk);

    lua_State *L = luaL_newstate();
    open_libs(L);
    lua_register(L, "th64_us", l_us);
    lua_register(L, "th64_log", l_log);
    if (luaL_dofile(L, "rom:/run.lua") != LUA_OK)
        out("TH64LUA error %s", lua_tostring(L, -1));
    heap_stats_t heap;
    sys_get_heap_stats(&heap);
    out("TH64LUA heap_usado_kib=%d", heap.used / 1024);
    out("TH64 fin");
    while (1) {}
}
