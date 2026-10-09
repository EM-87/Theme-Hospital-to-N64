# Se inyecta con -DCMAKE_PROJECT_INCLUDE=... al configurar CorsixTH contra Lua 5.5.
# El FindLua.cmake de CMake 3.28 solo conoce hasta Lua 5.4 y descarta la ruta
# de 5.5; este directorio aporta un FindLua mínimo que usa la ruta dada.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
