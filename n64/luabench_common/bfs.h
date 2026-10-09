// Búsqueda en anchura en una rejilla de 128x128 con muros, en C. Mismo código
// en el host (tools/luabench_host.sh) y en la N64 (n64/luabench), para medir
// el factor de CPU del código nativo, como el pathfinding de CorsixTH.
#ifndef TH64_BFS_H
#define TH64_BFS_H

#define TH64_W 128
#define TH64_H 128

static long th64_bfs_bench(void)
{
    static unsigned char wall[TH64_W * TH64_H];
    static short dist[TH64_W * TH64_H];
    static unsigned short queue[TH64_W * TH64_H];
    for (int y = 0; y < TH64_H; y++)
        for (int x = 0; x < TH64_W; x++)
            wall[y * TH64_W + x] = (x % 8 == 4 && y % 16 != 0) || (y % 8 == 4 && x % 16 != 8);
    long sum = 0;
    for (int run = 0; run < 40; run++) {
        for (int i = 0; i < TH64_W * TH64_H; i++) dist[i] = -1;
        int start = run % 8, goal = (TH64_H - 1 - run % 8) * TH64_W + (TH64_W - 1);
        int head = 0, tail = 0;
        queue[tail++] = start;
        dist[start] = 0;
        while (head < tail) {
            int c = queue[head++];
            if (c == goal) break;
            int x = c % TH64_W, y = c / TH64_W, d = dist[c] + 1;
            int n[4] = {x > 0 ? c - 1 : -1, x < TH64_W - 1 ? c + 1 : -1,
                        y > 0 ? c - TH64_W : -1, y < TH64_H - 1 ? c + TH64_W : -1};
            for (int k = 0; k < 4; k++)
                if (n[k] >= 0 && !wall[n[k]] && dist[n[k]] < 0) {
                    dist[n[k]] = d;
                    queue[tail++] = n[k];
                }
        }
        sum += dist[goal] + tail;
    }
    return sum;
}

#endif
