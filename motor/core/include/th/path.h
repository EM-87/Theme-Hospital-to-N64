// Búsqueda de caminos: el A* de CorsixTH (basic_pathfinder en
// th_pathfind.cpp), con el mismo montículo y el mismo orden de vecinos, para
// que dé exactamente los mismos caminos. Cambia la memoria: nodos de 8 B
// (128 KiB para el mapa entero, como preveía docs/04_decision.md) y un mapa
// de bits de nodos vistos en lugar de la lista de nodos sucios.
#ifndef TH_PATH_H
#define TH_PATH_H

#include <stdint.h>

#include "th/map.h"

#define TH_PATH_NODES (TH_MAP_WIDTH * TH_MAP_HEIGHT)
#define TH_PATH_NONE 0xFFFFu

typedef struct th_path_node {
    uint16_t prev;     // nodo anterior del camino; TH_PATH_NONE en el origen
    uint16_t distance; // pasos desde el origen
    int16_t guess;     // distancia de Manhattan al destino (0 en las otras búsquedas)
    uint16_t open_idx; // posición en el montículo
} th_path_node;

typedef struct th_pathfinder {
    th_path_node nodes[TH_PATH_NODES];
    uint16_t heap[TH_PATH_NODES];
    uint32_t seen[TH_PATH_NODES / 32];    // prev != self: el nodo ha entrado en el montículo
    uint32_t visited[TH_PATH_NODES / 32]; // path_node::visited
    uint32_t heap_size;
    uint16_t start, dest; // dest = TH_PATH_NONE si no hay camino
    uint16_t dest_x, dest_y;
    uint32_t expanded; // nodos sacados del montículo en la última búsqueda
} th_pathfinder;

// Busca el camino de (sx, sy) a (dx, dy), como World:getPath. Devuelve su
// longitud en pasos o -1 si no hay camino (o el destino no es transitable).
int th_path_find(th_pathfinder *pf, const th_map *m, int sx, int sy, int dx, int dy);

// idle_tile_finder::find_idle_tile (World:getIdleTile sin parcela): la
// n-ésima casilla transitable del hospital donde se puede esperar, buscando
// desde (sx, sy) sin cruzar puertas. Devuelve 1 y la casilla, o 0.
int th_path_idle_tile(th_pathfinder *pf, const th_map *m, int sx, int sy, int n, int *ox, int *oy);

// object_visitor::visit_objects: recorre el mapa desde (sx, sy) sin cruzar
// puertas, hasta max_dist pasos, y llama a fn por cada casilla vecina con el
// objeto thob (x, y, dirección del último paso: 0 norte, 1 este, 2 sur, 3
// oeste, y la distancia del nodo desde el que se ve). Para cuando fn devuelve
// distinto de 0; devuelve eso, o 0.
typedef int (*th_path_visit_fn)(void *ctx, int x, int y, int dir, int dist);
int th_path_visit_objects(th_pathfinder *pf, const th_map *m, int sx, int sy, int thob, int max_dist,
                          th_path_visit_fn fn, void *ctx);

// Copia las casillas del último camino, del origen al destino (longitud + 1),
// en xs e ys. Devuelve cuántas son, o -1 si no hay camino o no caben.
int th_path_get(const th_pathfinder *pf, uint8_t *xs, uint8_t *ys, int max);

#endif
