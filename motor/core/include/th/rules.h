// Reglas del juego en el formato TRG1 del conversor (tools/conv/reglas.py):
// las definiciones de CorsixTH (objetos, salas, enfermedades, diagnósticos,
// marcadores, base_config) y la configuración de cada nivel, como un árbol
// de valores igual que el de Lua. Solo lectura, sobre el fichero cargado.
//
// th_rules_init valida el fichero entero (posiciones, tipos, orden de las
// claves), así que después ningún acceso sale de él. Un nodo es la posición
// de un valor de 8 B; 0 es «no está», y todas las funciones lo aceptan.
// El núcleo lee lo que necesita al cargar y lo pasa a sus propias
// estructuras; los reales se leen en Q16.16, sin coma flotante.
#ifndef TH_RULES_H
#define TH_RULES_H

#include <stddef.h>
#include <stdint.h>

#include "th/fixed.h"

enum {
    TH_RULES_NIL,
    TH_RULES_FALSE,
    TH_RULES_TRUE,
    TH_RULES_INT,
    TH_RULES_REAL,
    TH_RULES_STRING,
    TH_RULES_LIST,
    TH_RULES_MAP,
};

typedef uint32_t th_rnode;

typedef struct th_rules {
    const uint8_t *data;
    uint32_t size;
    th_rnode root;
} th_rules;

int th_rules_init(th_rules *r, const uint8_t *data, size_t len); // 0 = bien

// Tipo del nodo; un nodo ausente es TH_RULES_NIL.
int th_rules_type(const th_rules *r, th_rnode n);
// Entero; un real se trunca hacia cero. Si no es un número, def.
int32_t th_rules_int(const th_rules *r, th_rnode n, int32_t def);
// Número en Q16.16, redondeado al más cercano. Si no es un número, def.
fx32 th_rules_fixed(const th_rules *r, th_rnode n, fx32 def);
// Verdadero para true; falso para false, nil o ausente; def para lo demás.
int th_rules_bool(const th_rules *r, th_rnode n, int def);
// Cadena (terminada en 0) y su longitud; NULL si no es una cadena.
const char *th_rules_str(const th_rules *r, th_rnode n, uint32_t *len);
// Bits del double IEEE 754 de un real (para volcarlo tal cual); 0 si no lo es.
uint64_t th_rules_real_bits(const th_rules *r, th_rnode n);

// Elementos de una lista o entradas de un mapa; 0 para lo demás.
uint32_t th_rules_len(const th_rules *r, th_rnode n);
// Elemento i (desde 0) de una lista: el elemento i + 1 de la tabla de Lua.
th_rnode th_rules_at(const th_rules *r, th_rnode list, uint32_t i);
// Entrada i de un mapa, en orden (las claves enteras primero): su clave y su valor.
th_rnode th_rules_entry_key(const th_rules *r, th_rnode map, uint32_t i);
th_rnode th_rules_entry_value(const th_rules *r, th_rnode map, uint32_t i);
// Valor de la clave de texto o entera de un mapa (búsqueda binaria).
th_rnode th_rules_get(const th_rules *r, th_rnode map, const char *key);
th_rnode th_rules_geti(const th_rules *r, th_rnode map, int32_t key);
// Ruta separada por puntos desde n: "objetos.bench.orientations.north";
// "#3" es la clave entera 3 de un mapa y "[2]" el elemento 2 de una lista.
th_rnode th_rules_path(const th_rules *r, th_rnode n, const char *path);

// Huella (FNV-1a de 32 bits) del subárbol de n, recorrido en orden: sirve para
// comprobar que el PC y la N64 leen lo mismo.
uint32_t th_rules_hash(const th_rules *r, th_rnode n);

#endif
