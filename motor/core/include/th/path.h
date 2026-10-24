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
    uint16_t guess;    // distancia de Manhattan al destino
    uint16_t open_idx; // posición en el montículo
} th_path_node;

typedef struct th_pathfinder {
    th_path_node nodes[TH_PATH_NODES];
    uint16_t heap[TH_PATH_NODES];
    uint32_t seen[TH_PATH_NODES / 32];
    uint32_t heap_size;
    uint16_t start, dest; // dest = TH_PATH_NONE si no hay camino
    uint16_t dest_x, dest_y;
    uint32_t expanded; // nodos sacados del montículo en la última búsqueda
} th_pathfinder;

// Busca el camino de (sx, sy) a (dx, dy), como World:getPath. Devuelve su
// longitud en pasos o -1 si no hay camino (o el destino no es transitable).
int th_path_find(th_pathfinder *pf, const th_map *m, int sx, int sy, int dx, int dy);

// Copia las casillas del último camino, del origen al destino (longitud + 1),
// en xs e ys. Devuelve cuántas son, o -1 si no hay camino o no caben.
int th_path_get(const th_pathfinder *pf, uint8_t *xs, uint8_t *ys, int max);

#endif
