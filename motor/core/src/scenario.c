#include "th/scenario.h"

#define SOMBRAS (TH_TILE_SHADOW_HALF | TH_TILE_SHADOW_FULL | TH_TILE_SHADOW_WALL)

int th_scenario_apply_cells(th_map *m, const th_rules *r, th_rnode cells)
{
    if (th_rules_type(r, cells) != TH_RULES_LIST)
        return -1;
    const uint32_t n = th_rules_len(r, cells);
    for (uint32_t i = 0; i < n; i++) {
        const th_rnode c = th_rules_at(r, cells, i);
        if (th_rules_len(r, c) != 8)
            return -1;
        int32_t v[8];
        for (int k = 0; k < 8; k++)
            v[k] = th_rules_int(r, th_rules_at(r, c, (uint32_t)k), -1);
        if (v[0] < 0 || v[0] >= TH_MAP_WIDTH || v[1] < 0 || v[1] >= TH_MAP_HEIGHT || v[2] < 0 || v[2] > 255 ||
            v[3] < 0 || v[3] > 255 || v[4] < 0 || v[4] > 255 || v[6] < 0 || v[6] > 65535 || v[7] < 0 || v[7] > 255)
            return -1;
        th_tile *t = th_map_tile(m, v[0], v[1]);
        t->floor = (uint8_t)v[2];
        t->wall_n = (uint8_t)v[3];
        t->wall_w = (uint8_t)v[4];
        t->flags = (t->flags & SOMBRAS) | ((uint32_t)v[5] & ~SOMBRAS);
        t->room = (uint16_t)v[6];
        t->object = (uint8_t)v[7];
    }
    th_map_update_shadows(m);
    return (int)n;
}
