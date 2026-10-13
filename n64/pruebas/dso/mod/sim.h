#ifndef SIM_H
#define SIM_H
#include <stdint.h>

#define MAP_W 128
#define MAP_H 128
#define MAP_N (MAP_W * MAP_H)
#define MAX_ENTS 384
#define PATH_SLOT 255

void sim_init_map(void);
void sim_init_ents(void);
int  sim_spawn(int kind);
void sim_tick(void);
int  sim_live(void);
int  sim_astar(int sx, int sy, int tx, int ty, uint16_t *out, int max_out);
uint32_t sim_checksum(void);
extern int sim_nodes_expanded;
extern uint32_t sim_stats[8];

#endif
