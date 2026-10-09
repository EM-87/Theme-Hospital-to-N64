// Tamaño de las estructuras del mapa y del pathfinding de CorsixTH compiladas
// para la N64 (mips64-elf, ABI o64 de libdragon: punteros de 32 bits). Solo se
// compila (-c): los tamaños quedan en símbolos que se leen con nm.
// Ver tools/arquitectura/tamanos.sh.
#include "th_map.h"
#include "th_pathfind.h"

extern "C" {
extern const char sz_map_tile[sizeof(map_tile)];
const char sz_map_tile[sizeof(map_tile)] = {};
extern const char sz_link_list[sizeof(link_list)];
const char sz_link_list[sizeof(link_list)] = {};
extern const char sz_level_map[sizeof(level_map)];
const char sz_level_map[sizeof(level_map)] = {};
extern const char sz_path_node[sizeof(path_node)];
const char sz_path_node[sizeof(path_node)] = {};
extern const char sz_pathfinder[sizeof(pathfinder)];
const char sz_pathfinder[sizeof(pathfinder)] = {};
}
