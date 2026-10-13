#include "th/pool.h"

#include <string.h>

static th_handle make_handle(uint16_t gen, uint16_t idx)
{
    return ((th_handle)gen << 16) | (th_handle)(idx + 1u);
}

static int split_handle(const th_pool *p, th_handle h, uint16_t *idx)
{
    uint32_t i = (h & 0xffffu);
    if (i == 0 || i > p->capacity)
        return 0;
    *idx = (uint16_t)(i - 1u);
    return p->gen[*idx] == (uint16_t)(h >> 16) && (p->gen[*idx] & 1u);
}

void th_pool_init(th_pool *p, void *items, uint16_t *gen, uint16_t *next_free,
                  uint16_t capacity, uint16_t item_size)
{
    p->items = items;
    p->gen = gen;
    p->next_free = next_free;
    p->capacity = capacity;
    p->item_size = item_size;
    p->count = 0;
    memset(items, 0, (size_t)capacity * item_size);
    for (uint16_t i = 0; i < capacity; i++) {
        gen[i] = 0;
        next_free[i] = (uint16_t)(i + 1u);
    }
    p->free_head = 0;
}

th_handle th_pool_alloc(th_pool *p)
{
    if (p->free_head >= p->capacity)
        return TH_HANDLE_NULL;
    uint16_t idx = p->free_head;
    p->free_head = p->next_free[idx];
    p->gen[idx]++; // par -> impar: ocupado
    p->count++;
    memset(p->items + (size_t)idx * p->item_size, 0, p->item_size);
    return make_handle(p->gen[idx], idx);
}

int th_pool_free(th_pool *p, th_handle h)
{
    uint16_t idx;
    if (!split_handle(p, h, &idx))
        return 0;
    p->gen[idx]++; // impar -> par: libre; las asas antiguas caducan
    p->next_free[idx] = p->free_head;
    p->free_head = idx;
    p->count--;
    return 1;
}

void *th_pool_get(const th_pool *p, th_handle h)
{
    uint16_t idx;
    if (!split_handle(p, h, &idx))
        return NULL;
    return p->items + (size_t)idx * p->item_size;
}

void *th_pool_next(const th_pool *p, uint16_t *cursor, th_handle *out)
{
    for (uint16_t i = *cursor; i < p->capacity; i++) {
        if (p->gen[i] & 1u) {
            *cursor = (uint16_t)(i + 1u);
            if (out)
                *out = make_handle(p->gen[i], i);
            return p->items + (size_t)i * p->item_size;
        }
    }
    *cursor = p->capacity;
    return NULL;
}
