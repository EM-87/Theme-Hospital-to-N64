// Pruebas del lector de reglas (th/rules.h) con un TRG1 hecho a mano. La
// comparación con el conversor, fichero a fichero, la hace th64sim reglas.
#include <string.h>

#include "th/rules.h"
#include "th_test.h"

static uint8_t b[2048];
static uint32_t n;

static void p32(uint32_t pos, uint32_t v)
{
    b[pos] = (uint8_t)(v >> 24);
    b[pos + 1] = (uint8_t)(v >> 16);
    b[pos + 2] = (uint8_t)(v >> 8);
    b[pos + 3] = (uint8_t)v;
}

static uint32_t reservar(uint32_t tam, uint32_t alin)
{
    while (n % alin)
        b[n++] = 0;
    uint32_t p = n;
    memset(b + n, 0, tam);
    n += tam;
    return p;
}

static uint32_t cadena(const char *s)
{
    uint32_t l = (uint32_t)strlen(s), p = reservar(4 + l + 1, 4);
    p32(p, l);
    memcpy(b + p + 4, s, l + 1);
    return p;
}

static uint32_t real(uint64_t bits)
{
    uint32_t p = reservar(8, 8);
    p32(p, (uint32_t)(bits >> 32));
    p32(p + 4, (uint32_t)bits);
    return p;
}

static void valor(uint32_t en, int tipo, uint32_t dato)
{
    b[en] = (uint8_t)tipo;
    p32(en + 4, dato);
}

static uint32_t coleccion(uint32_t cuantos, uint32_t tam_entrada)
{
    uint32_t p = reservar(4 + tam_entrada * cuantos, 4);
    p32(p, cuantos);
    return p;
}

// { #-1: 7, #3: 2.5, "abc": "hola", "b": [true, false, nil, -0.75, 0.1, 1e12],
//   "objetos": { "bench": { "thob": 4 } } }
static uint32_t fichero_bueno(void)
{
    memset(b, 0, sizeof(b));
    n = 16;
    uint32_t thob = coleccion(1, 16);
    valor(thob + 4, TH_RULES_STRING, cadena("thob"));
    valor(thob + 12, TH_RULES_INT, 4);
    uint32_t bench = coleccion(1, 16);
    valor(bench + 4, TH_RULES_STRING, cadena("bench"));
    valor(bench + 12, TH_RULES_MAP, thob);
    uint32_t lista = coleccion(6, 8);
    valor(lista + 4, TH_RULES_TRUE, 0);
    valor(lista + 12, TH_RULES_FALSE, 0);
    valor(lista + 20, TH_RULES_NIL, 0);
    valor(lista + 28, TH_RULES_REAL, real(0xBFE8000000000000ull));   // -0,75
    valor(lista + 36, TH_RULES_REAL, real(0x3FB999999999999Aull));   // 0,1
    valor(lista + 44, TH_RULES_REAL, real(0x426D1A94A2000000ull));   // 1e12
    uint32_t raiz = coleccion(5, 16);
    valor(raiz + 4, TH_RULES_INT, (uint32_t)-1);
    valor(raiz + 12, TH_RULES_INT, 7);
    valor(raiz + 20, TH_RULES_INT, 3);
    valor(raiz + 28, TH_RULES_REAL, real(0x4004000000000000ull)); // 2,5
    valor(raiz + 36, TH_RULES_STRING, cadena("abc"));
    valor(raiz + 44, TH_RULES_STRING, cadena("hola"));
    valor(raiz + 52, TH_RULES_STRING, cadena("b"));
    valor(raiz + 60, TH_RULES_LIST, lista);
    valor(raiz + 68, TH_RULES_STRING, cadena("objetos"));
    valor(raiz + 76, TH_RULES_MAP, bench);
    uint32_t v = reservar(8, 4);
    valor(v, TH_RULES_MAP, raiz);
    memcpy(b, "TRG1", 4);
    p32(4, n);
    p32(8, v);
    return raiz;
}

static void test_lectura(void)
{
    fichero_bueno();
    th_rules r;
    TH_CHECK_EQ(th_rules_init(&r, b, n), 0);
    TH_CHECK_EQ(th_rules_type(&r, r.root), TH_RULES_MAP);
    TH_CHECK_EQ(th_rules_len(&r, r.root), 5);
    TH_CHECK_EQ(th_rules_int(&r, th_rules_geti(&r, r.root, -1), 0), 7);
    TH_CHECK_EQ(th_rules_geti(&r, r.root, 5), 0);
    TH_CHECK_EQ(th_rules_get(&r, r.root, "ab"), 0);
    TH_CHECK_EQ(th_rules_get(&r, r.root, "abcd"), 0);
    uint32_t largo = 0;
    const char *s = th_rules_str(&r, th_rules_get(&r, r.root, "abc"), &largo);
    TH_CHECK(s && strcmp(s, "hola") == 0);
    TH_CHECK_EQ(largo, 4);
    TH_CHECK_EQ(th_rules_int(&r, th_rules_path(&r, r.root, "objetos.bench.thob"), 0), 4);
    TH_CHECK_EQ(th_rules_path(&r, r.root, "objetos.banco.thob"), 0);
    TH_CHECK_EQ(th_rules_int(&r, th_rules_path(&r, r.root, "#3"), 0), 2); // se trunca
    TH_CHECK_EQ(th_rules_fixed(&r, th_rules_path(&r, r.root, "#3"), 0), 163840);
    TH_CHECK_EQ(th_rules_fixed(&r, th_rules_path(&r, r.root, "#-1"), 0), 7 * 65536);
    th_rnode l = th_rules_get(&r, r.root, "b");
    TH_CHECK_EQ(th_rules_len(&r, l), 6);
    TH_CHECK_EQ(th_rules_bool(&r, th_rules_at(&r, l, 0), 0), 1);
    TH_CHECK_EQ(th_rules_bool(&r, th_rules_at(&r, l, 1), 1), 0);
    TH_CHECK_EQ(th_rules_type(&r, th_rules_at(&r, l, 2)), TH_RULES_NIL);
    TH_CHECK_EQ(th_rules_at(&r, l, 6), 0);
    TH_CHECK_EQ(th_rules_fixed(&r, th_rules_path(&r, r.root, "b.[3]"), 0), -49152);
    TH_CHECK_EQ(th_rules_int(&r, th_rules_path(&r, r.root, "b.[3]"), 9), 0);
    TH_CHECK_EQ(th_rules_fixed(&r, th_rules_at(&r, l, 4), 0), 6554); // 0,1 × 65536 = 6553,6
    TH_CHECK_EQ(th_rules_fixed(&r, th_rules_at(&r, l, 5), 0), INT32_MAX);
    TH_CHECK_EQ(th_rules_int(&r, th_rules_at(&r, l, 5), 0), INT32_MAX);
    TH_CHECK_EQ(th_rules_real_bits(&r, th_rules_at(&r, l, 3)), 0xBFE8000000000000ull);
    TH_CHECK_EQ(th_rules_micro(&r, th_rules_path(&r, r.root, "#3"), 0), 2500000);
    TH_CHECK_EQ(th_rules_micro(&r, th_rules_path(&r, r.root, "#-1"), 0), 7000000);
    TH_CHECK_EQ(th_rules_micro(&r, th_rules_at(&r, l, 3), 0), -750000);
    TH_CHECK_EQ(th_rules_micro(&r, th_rules_at(&r, l, 4), 0), 100000);
    TH_CHECK_EQ(th_rules_micro(&r, th_rules_at(&r, l, 5), 0), INT32_MAX);
    TH_CHECK_EQ(th_rules_micro(&r, th_rules_get(&r, r.root, "abc"), -5), -5);
    TH_CHECK_EQ(th_rules_int(&r, th_rules_get(&r, r.root, "abc"), -5), -5);
    // Entradas en orden: claves enteras primero.
    TH_CHECK_EQ(th_rules_int(&r, th_rules_entry_key(&r, r.root, 0), 0), -1);
    TH_CHECK_EQ(th_rules_int(&r, th_rules_entry_key(&r, r.root, 1), 0), 3);
    TH_CHECK(strcmp(th_rules_str(&r, th_rules_entry_key(&r, r.root, 4), NULL), "objetos") == 0);
    TH_CHECK_EQ(th_rules_entry_value(&r, r.root, 5), 0);
    // La huella cambia si cambia cualquier valor.
    const uint32_t h = th_rules_hash(&r, r.root);
    TH_CHECK(h != 0 && h == th_rules_hash(&r, r.root));
    TH_CHECK(th_rules_hash(&r, l) != h);
    // Un nodo ausente no rompe nada.
    TH_CHECK_EQ(th_rules_len(&r, 0), 0);
    TH_CHECK(th_rules_str(&r, 0, NULL) == NULL);
    TH_CHECK_EQ(th_rules_path(&r, 0, "a.b"), 0);
}

static void test_rechazos(void)
{
    th_rules r;
    uint32_t raiz = fichero_bueno();
    b[0] = 'X';
    TH_CHECK_EQ(th_rules_init(&r, b, n), -1);
    fichero_bueno();
    TH_CHECK_EQ(th_rules_init(&r, b, n - 1), -1);
    // Claves desordenadas: "abc" y "b" intercambiadas.
    raiz = fichero_bueno();
    uint8_t tmp[16];
    memcpy(tmp, b + raiz + 36, 16);
    memcpy(b + raiz + 36, b + raiz + 52, 16);
    memcpy(b + raiz + 52, tmp, 16);
    TH_CHECK_EQ(th_rules_init(&r, b, n), -1);
    // Cadena sin el 0 final.
    raiz = fichero_bueno();
    uint32_t s = (uint32_t)b[raiz + 40] << 24 | (uint32_t)b[raiz + 41] << 16 | (uint32_t)b[raiz + 42] << 8 | b[raiz + 43];
    b[s + 4 + 3] = 'x';
    TH_CHECK_EQ(th_rules_init(&r, b, n), -1);
    // Una lista que dice tener más elementos de los que caben.
    raiz = fichero_bueno();
    p32(raiz, 1000);
    TH_CHECK_EQ(th_rules_init(&r, b, n), -1);
    // Un tipo desconocido y un valor desalineado.
    raiz = fichero_bueno();
    b[raiz + 12] = 9;
    TH_CHECK_EQ(th_rules_init(&r, b, n), -1);
    fichero_bueno();
    p32(8, 18);
    TH_CHECK_EQ(th_rules_init(&r, b, n), -1);
    fichero_bueno();
    TH_CHECK_EQ(th_rules_init(&r, b, n), 0);
}

void th_test_rules(void)
{
    test_lectura();
    test_rechazos();
}
