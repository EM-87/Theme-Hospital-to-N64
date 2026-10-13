// Misma secuencia que src/main.c, en el host, para comprobar el checksum y medir en x86.
#include <stdio.h>
#include <time.h>
#include "../src/sim.h"
static uint16_t camino[PATH_SLOT];
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void){
  sim_init_map(); sim_init_ents();
  uint32_t seed=12345,total=0; int n0=sim_nodes_expanded; double a=now();
  for(int i=0;i<20;i++){ seed=seed*1103515245u+12345u;
    int sx=1+(seed>>8)%6, sy=1+(seed>>12)%6, tx=120+(seed>>16)%6, ty=120+(seed>>20)%6;
    int len=sim_astar(sx,sy,tx,ty,camino,PATH_SLOT); if(len>0) total+=len; }
  double b=now();
  printf("astar: %u pasos, %d nodos, %.3f ms\n",total,sim_nodes_expanded-n0,(b-a)*1e3);
  for(int i=0;i<361;i++) sim_spawn(i<304?0:1+(i%3));
  a=now();
  for(int t=0;t<200;t++){ sim_tick(); if(sim_live()<300) sim_spawn(0); }
  b=now();
  printf("ticks: %.4f ms/tick, vivos %d, caminos %u, fallidos %u\n",(b-a)*1e3/200,sim_live(),sim_stats[0],sim_stats[1]);
  printf("checksum %08x\n",sim_checksum());
}
