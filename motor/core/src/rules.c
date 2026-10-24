#include "th/rules.h"

#include <string.h>

#define CABECERA 16
#define PROFUNDIDAD_MAX 64

static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static int tipo_en(const th_rules *r, th_rnode n)
{
    return r->data[n];
}

static uint32_t dato(const th_rules *r, th_rnode n)
{
    return u32(r->data + n + 4);
}

// Validación ------------------------------------------------------------------

typedef struct {
    const th_rules *r;
    uint32_t visitas, max_visitas;
} validacion;

// ¿Cabe [pos, pos + n) en el fichero, después de la cabecera?
static int cabe(const th_rules *r, uint32_t pos, uint64_t n)
{
    return pos >= CABECERA && (uint64_t)pos + n <= r->size;
}

static int validar_cadena(const th_rules *r, uint32_t pos)
{
    if ((pos & 3) || !cabe(r, pos, 4))
        return 0;
    const uint32_t n = u32(r->data + pos);
    return cabe(r, pos, 4ull + n + 1) && r->data[pos + 4 + n] == 0;
}

// Orden de las claves de un mapa: enteras primero, en orden creciente;
// después las de texto, por bytes. Devuelve <0, 0 o >0.
static int comparar_claves(const th_rules *r, th_rnode a, th_rnode b)
{
    const int ta = tipo_en(r, a), tb = tipo_en(r, b);
    if (ta != tb)
        return ta == TH_RULES_INT ? -1 : 1;
    if (ta == TH_RULES_INT) {
        const int32_t x = (int32_t)dato(r, a), y = (int32_t)dato(r, b);
        return x < y ? -1 : x > y;
    }
    const uint32_t pa = dato(r, a), pb = dato(r, b);
    const uint32_t na = u32(r->data + pa), nb = u32(r->data + pb);
    const int c = memcmp(r->data + pa + 4, r->data + pb + 4, na < nb ? na : nb);
    return c ? c : (na < nb ? -1 : na > nb);
}

static int validar(validacion *v, th_rnode n, int profundidad)
{
    const th_rules *r = v->r;
    if (profundidad > PROFUNDIDAD_MAX || ++v->visitas > v->max_visitas)
        return 0;
    if ((n & 3) || !cabe(r, n, 8))
        return 0;
    const uint32_t d = dato(r, n);
    switch (tipo_en(r, n)) {
    case TH_RULES_NIL:
    case TH_RULES_FALSE:
    case TH_RULES_TRUE:
    case TH_RULES_INT: return 1;
    case TH_RULES_REAL: return !(d & 7) && cabe(r, d, 8);
    case TH_RULES_STRING: return validar_cadena(r, d);
    case TH_RULES_LIST: {
        if ((d & 3) || !cabe(r, d, 4))
            return 0;
        const uint32_t cuantos = u32(r->data + d);
        if (!cabe(r, d, 4 + 8ull * cuantos))
            return 0;
        for (uint32_t i = 0; i < cuantos; i++)
            if (!validar(v, d + 4 + 8 * i, profundidad + 1))
                return 0;
        return 1;
    }
    case TH_RULES_MAP: {
        if ((d & 3) || !cabe(r, d, 4))
            return 0;
        const uint32_t cuantos = u32(r->data + d);
        if (!cabe(r, d, 4 + 16ull * cuantos))
            return 0;
        for (uint32_t i = 0; i < cuantos; i++) {
            const th_rnode k = d + 4 + 16 * i;
            const int tk = tipo_en(r, k);
            if (tk != TH_RULES_INT && (tk != TH_RULES_STRING || !validar_cadena(r, dato(r, k))))
                return 0;
            if (i > 0 && comparar_claves(r, k - 16, k) >= 0)
                return 0;
            if (!validar(v, k + 8, profundidad + 1))
                return 0;
        }
        return 1;
    }
    default: return 0;
    }
}

int th_rules_init(th_rules *r, const uint8_t *data, size_t len)
{
    memset(r, 0, sizeof(*r));
    if (len < CABECERA + 8 || len > 0x7FFFFFFF || memcmp(data, "TRG1", 4) != 0 || u32(data + 4) != len)
        return -1;
    r->data = data;
    r->size = (uint32_t)len;
    r->root = u32(data + 8);
    // Cada valor ocupa sus propios 8 B en el fichero (solo se comparten las
    // cadenas), así que un fichero bien hecho no tiene más valores que
    // tamaño / 8: el límite evita que uno mal hecho haga crecer el recorrido.
    validacion v = {r, 0, r->size / 8};
    if (!validar(&v, r->root, 0)) {
        memset(r, 0, sizeof(*r));
        return -1;
    }
    return 0;
}

// Lectura -----------------------------------------------------------------------

int th_rules_type(const th_rules *r, th_rnode n)
{
    return n ? tipo_en(r, n) : TH_RULES_NIL;
}

uint64_t th_rules_real_bits(const th_rules *r, th_rnode n)
{
    if (th_rules_type(r, n) != TH_RULES_REAL)
        return 0;
    const uint32_t d = dato(r, n);
    return (uint64_t)u32(r->data + d) << 32 | u32(r->data + d + 4);
}

// Valor absoluto de un double finito multiplicado por 2^escala, redondeado al
// más cercano (con 0 para los subnormales). Satura en INT32_MAX.
static uint32_t magnitud(uint64_t b, int escala, int redondear)
{
    const int e = (int)(b >> 52 & 0x7FF);
    if (e == 0)
        return 0;
    if (e == 0x7FF)
        return 0x7FFFFFFF;
    const uint64_t m = (b & ((1ull << 52) - 1)) | 1ull << 52;
    const int sh = e - 1075 + escala; // valor × 2^escala = m × 2^sh
    if (sh >= 0)
        return 0x7FFFFFFF; // m >= 2^52
    if (sh < -60)
        return 0;
    uint64_t q = m >> -sh;
    if (redondear && (m >> (-sh - 1) & 1))
        q++;
    return q > 0x7FFFFFFF ? 0x7FFFFFFF : (uint32_t)q;
}

int32_t th_rules_int(const th_rules *r, th_rnode n, int32_t def)
{
    switch (th_rules_type(r, n)) {
    case TH_RULES_INT: return (int32_t)dato(r, n);
    case TH_RULES_REAL: {
        const uint64_t b = th_rules_real_bits(r, n);
        const int32_t m = (int32_t)magnitud(b, 0, 0);
        return b >> 63 ? -m : m;
    }
    default: return def;
    }
}

int32_t th_rules_micro(const th_rules *r, th_rnode n, int32_t def)
{
    switch (th_rules_type(r, n)) {
    case TH_RULES_INT: {
        const int32_t i = (int32_t)dato(r, n);
        if (i > 2147 || i < -2147)
            return i > 0 ? INT32_MAX : -INT32_MAX;
        return i * 1000000;
    }
    case TH_RULES_REAL: {
        // valor = m × 2^(e − 1075); se toman los 31 bits altos de m para
        // que m × 10^6 quepa en 64 bits.
        const uint64_t b = th_rules_real_bits(r, n);
        const int e = (int)(b >> 52 & 0x7FF);
        if (e == 0)
            return 0;
        const uint64_t m = ((b & ((1ull << 52) - 1)) | 1ull << 52) >> 22;
        const int sh = e - 1075 + 22;
        uint64_t q;
        if (sh >= 0) {
            if (e == 0x7FF || sh > 10)
                return b >> 63 ? -INT32_MAX : INT32_MAX;
            q = (m * 1000000u) << sh;
        } else if (sh < -60) {
            q = 0;
        } else {
            const uint64_t p = m * 1000000u;
            q = p >> -sh;
            if (p >> (-sh - 1) & 1)
                q++;
        }
        if (q > INT32_MAX)
            q = INT32_MAX;
        return b >> 63 ? -(int32_t)q : (int32_t)q;
    }
    default: return def;
    }
}

fx32 th_rules_fixed(const th_rules *r, th_rnode n, fx32 def)
{
    switch (th_rules_type(r, n)) {
    case TH_RULES_INT: {
        const int32_t i = (int32_t)dato(r, n);
        if (i > 32767 || i < -32768)
            return i > 0 ? INT32_MAX : -INT32_MAX;
        return fx_from_int(i);
    }
    case TH_RULES_REAL: {
        const uint64_t b = th_rules_real_bits(r, n);
        const int32_t m = (int32_t)magnitud(b, FX_FRAC_BITS, 1);
        return b >> 63 ? -m : m;
    }
    default: return def;
    }
}

int th_rules_bool(const th_rules *r, th_rnode n, int def)
{
    switch (th_rules_type(r, n)) {
    case TH_RULES_TRUE: return 1;
    case TH_RULES_FALSE:
    case TH_RULES_NIL: return 0;
    default: return def;
    }
}

const char *th_rules_str(const th_rules *r, th_rnode n, uint32_t *len)
{
    if (th_rules_type(r, n) != TH_RULES_STRING)
        return NULL;
    const uint32_t d = dato(r, n);
    if (len)
        *len = u32(r->data + d);
    return (const char *)r->data + d + 4;
}

uint32_t th_rules_len(const th_rules *r, th_rnode n)
{
    const int t = th_rules_type(r, n);
    return t == TH_RULES_LIST || t == TH_RULES_MAP ? u32(r->data + dato(r, n)) : 0;
}

th_rnode th_rules_at(const th_rules *r, th_rnode list, uint32_t i)
{
    if (th_rules_type(r, list) != TH_RULES_LIST || i >= th_rules_len(r, list))
        return 0;
    return dato(r, list) + 4 + 8 * i;
}

th_rnode th_rules_entry_key(const th_rules *r, th_rnode map, uint32_t i)
{
    if (th_rules_type(r, map) != TH_RULES_MAP || i >= th_rules_len(r, map))
        return 0;
    return dato(r, map) + 4 + 16 * i;
}

th_rnode th_rules_entry_value(const th_rules *r, th_rnode map, uint32_t i)
{
    const th_rnode k = th_rules_entry_key(r, map, i);
    return k ? k + 8 : 0;
}

// Búsqueda binaria de una clave entera (texto == NULL) o de texto.
static th_rnode buscar(const th_rules *r, th_rnode map, int32_t entero, const char *texto)
{
    if (th_rules_type(r, map) != TH_RULES_MAP)
        return 0;
    const size_t n_texto = texto ? strlen(texto) : 0;
    uint32_t lo = 0, hi = th_rules_len(r, map);
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        const th_rnode k = dato(r, map) + 4 + 16 * mid;
        int c; // clave del mapa frente a la buscada
        if (tipo_en(r, k) == TH_RULES_INT) {
            const int32_t x = (int32_t)dato(r, k);
            c = texto ? -1 : (x < entero ? -1 : x > entero);
        } else if (!texto) {
            c = 1;
        } else {
            const uint32_t p = dato(r, k), nk = u32(r->data + p);
            c = memcmp(r->data + p + 4, texto, nk < n_texto ? nk : n_texto);
            if (!c)
                c = nk < n_texto ? -1 : nk > n_texto;
        }
        if (c == 0)
            return k + 8;
        if (c < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return 0;
}

th_rnode th_rules_get(const th_rules *r, th_rnode map, const char *key)
{
    return buscar(r, map, 0, key);
}

th_rnode th_rules_geti(const th_rules *r, th_rnode map, int32_t key)
{
    return buscar(r, map, key, NULL);
}

static int32_t numero(const char *s, size_t n, int *ok)
{
    int32_t v = 0;
    int neg = n > 0 && s[0] == '-';
    *ok = n > (size_t)neg && n - neg <= 9;
    for (size_t i = neg; i < n && *ok; i++) {
        if (s[i] < '0' || s[i] > '9')
            *ok = 0;
        else
            v = v * 10 + (s[i] - '0');
    }
    return neg ? -v : v;
}

th_rnode th_rules_path(const th_rules *r, th_rnode n, const char *path)
{
    char clave[128];
    while (n && *path) {
        const char *fin = strchr(path, '.');
        const size_t largo = fin ? (size_t)(fin - path) : strlen(path);
        int ok;
        if (largo >= 2 && path[0] == '[' && path[largo - 1] == ']') {
            const int32_t i = numero(path + 1, largo - 2, &ok);
            n = ok && i >= 0 ? th_rules_at(r, n, (uint32_t)i) : 0;
        } else if (largo >= 2 && path[0] == '#') {
            const int32_t i = numero(path + 1, largo - 1, &ok);
            n = ok ? th_rules_geti(r, n, i) : 0;
        } else {
            if (largo >= sizeof(clave))
                return 0;
            memcpy(clave, path, largo);
            clave[largo] = 0;
            n = th_rules_get(r, n, clave);
        }
        path = fin ? fin + 1 : path + largo;
    }
    return n;
}

static uint32_t fnv(uint32_t h, const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

static uint32_t huella(const th_rules *r, th_rnode n, uint32_t h)
{
    const uint8_t tipo = (uint8_t)th_rules_type(r, n);
    h = fnv(h, &tipo, 1);
    if (!n)
        return h;
    const uint32_t d = dato(r, n);
    switch (tipo) {
    case TH_RULES_INT: return fnv(h, r->data + n + 4, 4);
    case TH_RULES_REAL: return fnv(h, r->data + d, 8);
    case TH_RULES_STRING: return fnv(h, r->data + d, 4 + u32(r->data + d));
    case TH_RULES_LIST:
    case TH_RULES_MAP: {
        const uint32_t cuantos = u32(r->data + d);
        h = fnv(h, r->data + d, 4);
        for (uint32_t i = 0; i < cuantos; i++) {
            if (tipo == TH_RULES_LIST) {
                h = huella(r, d + 4 + 8 * i, h);
            } else {
                h = huella(r, d + 4 + 16 * i, h);
                h = huella(r, d + 4 + 16 * i + 8, h);
            }
        }
        return h;
    }
    default: return h;
    }
}

uint32_t th_rules_hash(const th_rules *r, th_rnode n)
{
    return huella(r, n, 2166136261u);
}
