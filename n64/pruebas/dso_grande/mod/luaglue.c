// Módulo «modo completo» de prueba: el núcleo de Lua 5.5 entero dentro de un
// DSO (código grande y realista, con muchas reubicaciones e importaciones de libc).
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <stdlib.h>

static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    (void)ud; (void)osize;
    if (nsize == 0) { free(ptr); return NULL; }
    return realloc(ptr, nsize);
}

// Ejecuta un trozo de Lua y devuelve el entero que deja en la pila (o -1).
int modo_completo_ejecuta(const char *codigo)
{
    lua_State *L = lua_newstate(l_alloc, NULL, 0);
    if (!L) return -2;
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L, 4);
    int r = -1;
    if (luaL_dostring(L, codigo) == 0 && lua_isinteger(L, -1)) r = (int)lua_tointeger(L, -1);
    lua_close(L);
    return r;
}
