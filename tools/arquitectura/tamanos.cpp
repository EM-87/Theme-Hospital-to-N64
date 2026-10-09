// Tamaño en memoria (x86-64) de las estructuras del núcleo C++ de CorsixTH que
// crecen con el mapa o con las entidades. Se compila contra las cabeceras de
// CorsixTH; ver tools/arquitectura/tamanos.sh.
#include <cstdio>

#include "th_gfx.h"
#include "th_map.h"
#include "th_pathfind.h"

int main() {
  std::printf("{\n");
  std::printf(" \"map_tile\": %zu,\n", sizeof(map_tile));
  std::printf(" \"map_tile_flags\": %zu,\n", sizeof(map_tile_flags));
  std::printf(" \"link_list\": %zu,\n", sizeof(link_list));
  std::printf(" \"level_map\": %zu,\n", sizeof(level_map));
  std::printf(" \"animation\": %zu,\n", sizeof(animation));
  std::printf(" \"sprite_render_list\": %zu,\n", sizeof(sprite_render_list));
  std::printf(" \"path_node\": %zu,\n", sizeof(path_node));
  std::printf(" \"pathfinder\": %zu,\n", sizeof(pathfinder));
  std::printf(" \"tiles_128x128_map_tile_x2\": %zu\n", 2 * 128 * 128 * sizeof(map_tile));
  std::printf("}\n");
  return 0;
}
