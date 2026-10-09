# FindLua mínimo: exige LUA_INCLUDE_DIR y LUA_LIBRARY y lee la versión de lua.h.
# Lua 5.5 define la versión con macros numéricas LUA_VERSION_*_N.
file(STRINGS "${LUA_INCLUDE_DIR}/lua.h" _lua_ver REGEX "^#define[ \t]+LUA_VERSION_(MAJOR|MINOR|RELEASE)_N[ \t]")
string(REGEX REPLACE ".*LUA_VERSION_MAJOR_N[ \t]+([0-9]+).*" "\\1" _maj "${_lua_ver}")
string(REGEX REPLACE ".*LUA_VERSION_MINOR_N[ \t]+([0-9]+).*" "\\1" _min "${_lua_ver}")
string(REGEX REPLACE ".*LUA_VERSION_RELEASE_N[ \t]+([0-9]+).*" "\\1" _rel "${_lua_ver}")
set(LUA_VERSION_STRING "${_maj}.${_min}.${_rel}")
set(LUA_VERSION_MAJOR ${_maj})
set(LUA_VERSION_MINOR ${_min})
set(LUA_LIBRARIES "${LUA_LIBRARY}" m)
set(LUA_INCLUDE_DIRS "${LUA_INCLUDE_DIR}")
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Lua REQUIRED_VARS LUA_LIBRARY LUA_INCLUDE_DIR VERSION_VAR LUA_VERSION_STRING)
