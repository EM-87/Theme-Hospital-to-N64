// Pool de entidades de tamaño fijo con asas (índice + generación).
// Sin malloc: el almacenamiento lo pone quien llama (un array estático).
// Recorrido siempre en orden de índice, así que es determinista (a diferencia
// de pairs() de Lua, cuyo orden depende de la semilla de hash y de direcciones).
#ifndef TH_POOL_H
#define TH_POOL_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t th_handle; // (generación << 16) | índice; 0 = nulo

#define TH_HANDLE_NULL ((th_handle)0)
#define TH_POOL_MAX 65535u

typedef struct th_pool {
    uint8_t *items;      // capacidad * tam_item bytes
    uint16_t *gen;       // generación por hueco; impar = ocupado
    uint16_t *next_free; // lista libre (índices)
    uint16_t capacity;
    uint16_t item_size;
    uint16_t free_head;  // capacity = lista vacía
    uint16_t count;
} th_pool;

// Declara el almacenamiento estático de un pool de 'cap' elementos de 'type'.
#define TH_POOL_STORAGE(name, type, cap)          \
    static type name##_items[(cap)];              \
    static uint16_t name##_gen[(cap)];            \
    static uint16_t name##_next[(cap)]

#define TH_POOL_INIT(pool, name, type, cap) \
    th_pool_init((pool), name##_items, name##_gen, name##_next, (uint16_t)(cap), (uint16_t)sizeof(type))

void th_pool_init(th_pool *p, void *items, uint16_t *gen, uint16_t *next_free,
                  uint16_t capacity, uint16_t item_size);
th_handle th_pool_alloc(th_pool *p);              // TH_HANDLE_NULL si está lleno
int th_pool_free(th_pool *p, th_handle h);        // 1 = liberado, 0 = asa no válida
void *th_pool_get(const th_pool *p, th_handle h); // NULL si el asa caducó
// Recorrido determinista: siguiente hueco ocupado a partir de *cursor.
void *th_pool_next(const th_pool *p, uint16_t *cursor, th_handle *out);

#endif
