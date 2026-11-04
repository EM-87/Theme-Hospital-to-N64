// Un TRG1 (th/rules.h) a partir de un JSON pequeño, para escribir en las
// pruebas definiciones legibles: lo mismo que escribir() de
// tools/conv/reglas.py. Admite mapas, listas, enteros, true, false, null y
// cadenas sin escapes; las claves "#n" son enteras.
#include <string.h>

#include "th/rules.h"
#include "th_test.h"

#define NODOS 1024
#define HIJOS 64

typedef struct jnodo {
    uint8_t tipo; // TH_RULES_*
    int32_t entero;
    const char *s, *clave; // cadena y clave (sin comillas), en el JSON
    uint16_t largo, largo_clave;
    int16_t primero, siguiente; // hijos
} jnodo;

static jnodo nodos[NODOS];
static int n_nodos;
static const char *p;

static void blancos(void)
{
    while (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r')
        p++;
}

static int cadena_json(const char **s, uint16_t *largo)
{
    if (*p != '"')
        return 0;
    const char *ini = ++p;
    while (*p && *p != '"')
        p++;
    if (!*p)
        return 0;
    *s = ini;
    *largo = (uint16_t)(p - ini);
    p++;
    return 1;
}

static int valor_json(void)
{
    blancos();
    if (n_nodos >= NODOS)
        return -1;
    const int i = n_nodos++;
    jnodo *v = &nodos[i];
    memset(v, 0, sizeof *v);
    v->primero = v->siguiente = -1;
    if (*p == '{' || *p == '[') {
        const int mapa = *p++ == '{';
        v->tipo = mapa ? TH_RULES_MAP : TH_RULES_LIST;
        int ultimo = -1;
        blancos();
        if (*p == (mapa ? '}' : ']')) {
            p++;
            return i;
        }
        for (;;) {
            const char *clave = NULL;
            uint16_t largo = 0;
            blancos();
            if (mapa) {
                if (!cadena_json(&clave, &largo))
                    return -1;
                blancos();
                if (*p++ != ':')
                    return -1;
            }
            const int h = valor_json();
            if (h < 0)
                return -1;
            nodos[h].clave = clave;
            nodos[h].largo_clave = largo;
            if (ultimo < 0)
                nodos[i].primero = (int16_t)h;
            else
                nodos[ultimo].siguiente = (int16_t)h;
            ultimo = h;
            blancos();
            if (*p == ',') {
                p++;
                continue;
            }
            if (*p++ != (mapa ? '}' : ']'))
                return -1;
            return i;
        }
    }
    if (*p == '"') {
        v->tipo = TH_RULES_STRING;
        return cadena_json(&v->s, &v->largo) ? i : -1;
    }
    if (!strncmp(p, "true", 4) || !strncmp(p, "false", 5) || !strncmp(p, "null", 4)) {
        v->tipo = *p == 't' ? TH_RULES_TRUE : *p == 'f' ? TH_RULES_FALSE : TH_RULES_NIL;
        p += *p == 'f' ? 5 : 4;
        return i;
    }
    int signo = 1;
    if (*p == '-') {
        signo = -1;
        p++;
    }
    if (*p < '0' || *p > '9')
        return -1;
    v->tipo = TH_RULES_INT;
    while (*p >= '0' && *p <= '9')
        v->entero = v->entero * 10 + (*p++ - '0');
    v->entero *= signo;
    return i;
}

// La salida.
static uint8_t *out;
static size_t cap, n_out;
static int lleno;

static void p32(size_t pos, uint32_t v)
{
    out[pos] = (uint8_t)(v >> 24);
    out[pos + 1] = (uint8_t)(v >> 16);
    out[pos + 2] = (uint8_t)(v >> 8);
    out[pos + 3] = (uint8_t)v;
}

static size_t reservar(size_t tam, size_t alin)
{
    while (n_out % alin && n_out < cap)
        out[n_out++] = 0;
    if (n_out + tam > cap) {
        lleno = 1;
        return 0;
    }
    const size_t pos = n_out;
    memset(out + pos, 0, tam);
    n_out += tam;
    return pos;
}

// Las cadenas no se repiten (como en el conversor).
typedef struct vista {
    const char *s;
    uint16_t largo;
    uint32_t pos;
} vista;
static vista vistas[NODOS];
static int n_vistas;

static uint32_t cadena(const char *s, uint16_t largo)
{
    for (int i = 0; i < n_vistas; i++)
        if (vistas[i].largo == largo && !memcmp(vistas[i].s, s, largo))
            return vistas[i].pos;
    const size_t pos = reservar(4u + largo + 1, 4);
    if (lleno)
        return 0;
    p32(pos, largo);
    memcpy(out + pos + 4, s, largo);
    if (n_vistas < NODOS)
        vistas[n_vistas++] = (vista){s, largo, (uint32_t)pos};
    return (uint32_t)pos;
}

static void ocho(uint8_t v[8], int tipo, uint32_t dato)
{
    memset(v, 0, 8);
    v[0] = (uint8_t)tipo;
    v[4] = (uint8_t)(dato >> 24);
    v[5] = (uint8_t)(dato >> 16);
    v[6] = (uint8_t)(dato >> 8);
    v[7] = (uint8_t)dato;
}

static int clave_entera(const jnodo *h)
{
    return h->largo_clave > 1 && h->clave[0] == '#';
}

static int32_t numero_clave(const jnodo *h)
{
    int32_t v = 0, signo = 1;
    int k = 1;
    if (h->clave[k] == '-') {
        signo = -1;
        k++;
    }
    for (; k < h->largo_clave; k++)
        v = v * 10 + (h->clave[k] - '0');
    return v * signo;
}

// Orden de _clave en reglas.py: enteras primero, en orden; después texto, por bytes.
static int antes(const jnodo *a, const jnodo *b)
{
    const int ea = clave_entera(a), eb = clave_entera(b);
    if (ea != eb)
        return ea;
    if (ea)
        return numero_clave(a) < numero_clave(b);
    const int c = memcmp(a->clave, b->clave, a->largo_clave < b->largo_clave ? a->largo_clave : b->largo_clave);
    return c ? c < 0 : a->largo_clave < b->largo_clave;
}

static void escribir(int i, uint8_t v[8])
{
    const jnodo *x = &nodos[i];
    switch (x->tipo) {
    case TH_RULES_INT: ocho(v, TH_RULES_INT, (uint32_t)x->entero); return;
    case TH_RULES_STRING: ocho(v, TH_RULES_STRING, cadena(x->s, x->largo)); return;
    case TH_RULES_LIST:
    case TH_RULES_MAP: break;
    default: ocho(v, x->tipo, 0); return;
    }
    int hijos[HIJOS], n = 0;
    for (int h = x->primero; h >= 0 && n < HIJOS; h = nodos[h].siguiente) {
        int k = n++;
        while (x->tipo == TH_RULES_MAP && k > 0 && antes(&nodos[h], &nodos[hijos[k - 1]])) {
            hijos[k] = hijos[k - 1];
            k--;
        }
        hijos[k] = h;
    }
    const int ancho = x->tipo == TH_RULES_MAP ? 16 : 8;
    uint8_t bloque[HIJOS * 16];
    for (int k = 0; k < n; k++) {
        const jnodo *h = &nodos[hijos[k]];
        if (x->tipo == TH_RULES_MAP) {
            if (clave_entera(h))
                ocho(bloque + 16 * k, TH_RULES_INT, (uint32_t)numero_clave(h));
            else
                ocho(bloque + 16 * k, TH_RULES_STRING, cadena(h->clave, h->largo_clave));
        }
        escribir(hijos[k], bloque + ancho * k + (ancho - 8));
    }
    const size_t pos = reservar(4 + (size_t)ancho * n, 4);
    if (lleno)
        return;
    p32(pos, (uint32_t)n);
    memcpy(out + pos + 4, bloque, (size_t)ancho * n);
    ocho(v, x->tipo, (uint32_t)pos);
}

size_t th_test_trg(const char *json, uint8_t *salida, size_t tam)
{
    n_nodos = n_vistas = 0;
    p = json;
    const int raiz = valor_json();
    blancos();
    if (raiz < 0 || *p)
        return 0;
    out = salida;
    cap = tam;
    n_out = 0;
    lleno = 0;
    reservar(16, 4);
    uint8_t v[8];
    escribir(raiz, v);
    const size_t pos = reservar(8, 4);
    if (lleno)
        return 0;
    memcpy(out + pos, v, 8);
    memcpy(out, "TRG1", 4);
    p32(4, (uint32_t)n_out);
    p32(8, (uint32_t)pos);
    p32(12, 0);
    return n_out;
}
