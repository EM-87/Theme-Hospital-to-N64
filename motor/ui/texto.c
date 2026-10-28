#include "texto.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

uint32_t th_utf8_next(const char **s, const char *end)
{
    uint8_t c = (uint8_t)*(*s)++;
    int n = 0;
    while (c & 0x80) { // discard_leading_set_bits
        n++;
        c = (uint8_t)(c << 1);
    }
    c = (uint8_t)(c >> n);
    if (n == 1 || n > 4)
        return 0xFFFD;
    uint32_t cp = c;
    for (int i = 1; i < n; i++) {
        if (*s == end)
            return 0xFFFD;
        uint8_t b = (uint8_t)*(*s)++;
        if ((b & 0xC0) != 0x80)
            return 0xFFFD;
        cp = cp << 6 | (b & 0x3F);
    }
    return cp;
}

// unicode_to_cp437_table de th_strings.cpp: de U+00A0 a U+00FF.
static const uint8_t de_a0[0x60] = {
    0xFF, 0xAD, 0x9B, 0x9C, 0x3F, 0x9D, 0x3F, 0x3F, 0x3F, 0x3F, 0xA6, 0xAE, 0xAA, 0x3F, 0x3F, 0x3F,
    0xF8, 0xF1, 0xFD, 0x3F, 0x3F, 0x3F, 0x3F, 0xFA, 0x3F, 0x3F, 0xA7, 0xAF, 0xAC, 0xAB, 0x3F, 0xA8,
    0x3F, 0x3F, 0x3F, 0x3F, 0x8E, 0x8F, 0x3F, 0x80, 0x3F, 0x90, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0xA5, 0x3F, 0x3F, 0x3F, 0x3F, 0x99, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x9A, 0x3F, 0x3F, 0xE1,
    0x85, 0xA0, 0x83, 0x3F, 0x84, 0x86, 0x91, 0x87, 0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B,
    0x3F, 0xA4, 0x95, 0xA2, 0x93, 0x3F, 0x94, 0xF6, 0x3F, 0x97, 0xA3, 0x96, 0x81, 0x3F, 0x3F, 0x98,
};

// El resto de unicode_to_codepage_437: código Unicode y byte de CP437.
static const uint16_t otros[][2] = {
    {0x0192, 0x9F}, {0x0393, 0xE2}, {0x0398, 0xE9}, {0x03A3, 0xE4}, {0x03A6, 0xE8}, {0x03A9, 0xEA},
    {0x03B1, 0xE0}, {0x03B4, 0xEB}, {0x03B5, 0xEE}, {0x03BC, 0xE6}, {0x03C0, 0xE3}, {0x03C3, 0xE5},
    {0x03C4, 0xE7}, {0x03C6, 0xED}, {0x207F, 0xFC}, {0x20A7, 0x9E}, {0x2219, 0xF9}, {0x221A, 0xFB},
    {0x221E, 0xEC}, {0x2229, 0xEF}, {0x2248, 0xF7}, {0x2261, 0xF0}, {0x2264, 0xF3}, {0x2265, 0xF2},
    {0x2310, 0xA9}, {0x2320, 0xF4}, {0x2321, 0xF5}, {0x25A0, 0xFE},
};

unsigned th_unicode_to_cp437(uint32_t cp)
{
    if (cp < 0x80)
        return cp;
    if (cp < 0xA0)
        return '?';
    if (cp < 0x100)
        return de_a0[cp - 0xA0];
    for (size_t i = 0; i < sizeof otros / sizeof otros[0]; i++)
        if (otros[i][0] == cp)
            return otros[i][1];
    return '?';
}

enum { PRIMERO = 31 }; // iFirstASCII

int th_drawlist_fill(th_drawlist *out, int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0)
        return 0;
    if (out->count >= out->capacity)
        return -1;
    out->items[out->count++] = (th_draw){.sprite = color, .sheet = TH_SHEET_FILL, .x = (int16_t)x, .y = (int16_t)y,
                                         .clip_x1 = (int16_t)w, .clip_y1 = (int16_t)h};
    return 0;
}

int th_font_text(const th_font *f, const char *s, size_t n, int x, int y, th_drawlist *out)
{
    const char *end = s + n;
    const unsigned ultimo = f->sheet->count + PRIMERO;
    while (s != end) {
        unsigned c = th_unicode_to_cp437(th_utf8_next(&s, end));
        if (c < PRIMERO || c > ultimo)
            continue;
        int w, h;
        th_sheet_size(f->sheet, (int)(c - PRIMERO), &w, &h);
        if (c == ' ' && f->espacio)
            w = f->espacio;
        else if (out && w) {
            if (out->count >= out->capacity)
                return -1;
            out->items[out->count++] =
                (th_draw){.sprite = (uint16_t)(c - PRIMERO), .sheet = f->sheet_id, .x = (int16_t)x, .y = (int16_t)y};
        }
        x += w + f->sep_x;
    }
    return x;
}

static unsigned car(const char **s, const char *end)
{
    return th_unicode_to_cp437(th_utf8_next(s, end));
}

th_text_layout th_font_wrapped(const th_font *f, const char *msg, size_t n, int x, int y, int width, int max_rows,
                               int skip_rows, int align, th_drawlist *out)
{
    th_text_layout area = {0, 0, 0, 0};
    int saltadas = 0;
    const char *end = msg + n;
    const unsigned ultimo = f->sheet->count + PRIMERO;
    while (n && msg != end && area.row_count < max_rows) {
        const char *corte = end, *buen_corte = end;
        int ancho = -f->sep_x, ancho_corte = ancho, alto = 0;
        int salto = 0;
        const char *s = msg;
        while (s != end) {
            const char *antes = s;
            unsigned c = car(&s, end);
            // iNextChar: el byte siguiente, sin decodificar.
            unsigned sig = th_unicode_to_cp437(s != end ? (uint8_t)*s : 0);
            if ((c == '\n' && sig == '\n') || (c == '/' && sig == '/')) {
                salto = 1;
                ancho_corte = ancho;
                corte = antes;
                break;
            }
            int cw = 0, ch = 0;
            if (c >= PRIMERO && c <= ultimo)
                th_sheet_size(f->sheet, (int)(c - PRIMERO), &cw, &ch);
            if (c == ' ' && f->espacio)
                cw = f->espacio;
            ancho += f->sep_x + cw;
            if (c == ' ') {
                buen_corte = antes;
                ancho_corte = ancho - cw;
            }
            if (ancho > width) {
                corte = buen_corte;
                break;
            }
            if (ch > alto)
                alto = ch;
        }
        if (s == end)
            ancho_corte = ancho;
        if (ancho_corte > area.width)
            area.width = ancho_corte;
        if (saltadas >= skip_rows) {
            if (out) {
                int dx = ancho_corte < width ? (width - ancho_corte) * align / 2 : 0;
                th_font_text(f, msg, (size_t)(corte - msg), x + dx, y, out);
            }
            y += alto + f->sep_y;
            area.end_x = ancho;
            area.row_count++;
            if (salto) {
                y += alto + f->sep_y;
                area.row_count++;
            }
        } else {
            saltadas++;
            if (salto) {
                if (saltadas == skip_rows) {
                    y += alto + f->sep_y;
                    area.row_count++;
                }
                saltadas++;
            }
        }
        msg = corte;
        if (msg != end) {
            th_utf8_next(&msg, end);
            if (salto)
                th_utf8_next(&msg, end);
        }
    }
    area.end_x += x;
    area.end_y = y;
    return area;
}

void th_font_draw(const th_font *f, const char *s, int x, int y, int w, int h, int align, th_drawlist *out, int *end_x,
                  int *end_y)
{
    const size_t n = strlen(s);
    th_text_layout a = th_font_wrapped(f, s, n, 0, 0, INT_MAX, INT_MAX, 0, TH_ALIGN_LEFT, NULL);
    if (w > 0 || h > 0) {
        if (w > a.end_x && align != TH_ALIGN_LEFT)
            x += (w - a.end_x) / (align == TH_ALIGN_CENTER ? 2 : 1);
        if (h > a.end_y)
            y += (h - a.end_y) / 2;
    }
    if (out)
        th_font_text(f, s, n, x, y, out);
    if (end_x)
        *end_x = x + a.end_x;
    if (end_y)
        *end_y = y + a.end_y;
}

int th_font_tooltip(const th_font *f, const char *s, int x, int y, int w, int screen_w, th_drawlist *out)
{
    const size_t n = strlen(s);
    th_text_layout a = th_font_wrapped(f, s, n, x + 2, y + 1, w - 4, INT_MAX, 0, TH_ALIGN_LEFT, NULL);
    const int ultima_x = x + a.width + 3, primera_y = y - (a.end_y - y) - 1;
    const int dx = ultima_x > screen_w ? screen_w - ultima_x : 0;
    const int dy = primera_y < 0 ? -primera_y : 0;
    th_drawlist_fill(out, x + dx, primera_y + dy, a.width + 3, a.end_y - y + 2, 0x0001);         // negro
    th_drawlist_fill(out, x + dx + 1, primera_y + 1 + dy, a.width + 1, a.end_y - y, 0xFFFF);      // blanco
    th_font_wrapped(f, s, n, x + 2 + dx, primera_y + 1 + dy, w - 4, INT_MAX, 0, TH_ALIGN_LEFT, out);
    return a.end_y;
}

const char *th_text(const th_rules *textos, const char *path)
{
    const char *s = textos ? th_rules_str(textos, th_rules_path(textos, textos->root, path), NULL) : NULL;
    return s ? s : "";
}

// Añade n bytes de src a out (cortando si no caben); lleva la longitud total en *len.
static void poner(char *out, size_t cap, size_t *len, const char *src, size_t n)
{
    if (*len + 1 < cap) {
        size_t k = cap - 1 - *len;
        memcpy(out + *len, src, n < k ? n : k);
    }
    *len += n;
}

size_t th_text_format(char *out, size_t cap, const char *fmt, const th_text_arg *args, int nargs,
                      const th_rules *textos)
{
    size_t len = 0;
    int idx = 0; // los %s y %d van por orden
    char num[16];
    for (const char *p = fmt; *p;) {
        if (p[0] != '%') {
            poner(out, cap, &len, p++, 1);
            continue;
        }
        if (p[1] == '%') {
            poner(out, cap, &len, "%", 1);
            p += 2;
            continue;
        }
        if (p[1] == 's' || p[1] == 'd') {
            const th_text_arg *a = idx < nargs ? &args[idx] : NULL;
            idx++;
            if (a && a->str && p[1] == 's') {
                poner(out, cap, &len, a->str, strlen(a->str));
            } else if (a && !a->str) {
                int k = snprintf(num, sizeof num, "%ld", (long)a->num);
                poner(out, cap, &len, num, (size_t)k);
            } else {
                poner(out, cap, &len, p, 2);
            }
            p += 2;
            continue;
        }
        // %n% y %n:tabla%
        if (p[1] >= '1' && p[1] <= '9') {
            const int n = p[1] - '0';
            const char *q = p + 2, *tabla = NULL;
            size_t ltabla = 0;
            if (*q == ':') {
                tabla = ++q;
                while ((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') || (*q >= '0' && *q <= '9') ||
                       *q == '_' || *q == '.')
                    q++;
                ltabla = (size_t)(q - tabla);
                if (!ltabla)
                    tabla = NULL;
            }
            if (*q == '%' && (tabla || q == p + 2)) {
                const size_t literal = (size_t)(q + 1 - p);
                const th_text_arg *a = n <= nargs ? &args[n - 1] : NULL;
                const char *val = NULL;
                if (a && tabla) {
                    // _S.tabla[val]: con un número, el elemento de la lista (desde 1).
                    char ruta[64];
                    if (ltabla < sizeof ruta && textos) {
                        memcpy(ruta, tabla, ltabla);
                        ruta[ltabla] = 0;
                        th_rnode t = th_rules_path(textos, textos->root, ruta);
                        th_rnode e = 0;
                        if (!a->str && th_rules_type(textos, t) == TH_RULES_LIST && a->num >= 1)
                            e = th_rules_at(textos, t, (uint32_t)(a->num - 1));
                        else if (!a->str)
                            e = th_rules_geti(textos, t, a->num);
                        else
                            e = th_rules_get(textos, t, a->str);
                        val = th_rules_str(textos, e, NULL);
                    }
                    if (val)
                        poner(out, cap, &len, val, strlen(val));
                    else
                        poner(out, cap, &len, p, literal);
                } else if (a && a->str) {
                    poner(out, cap, &len, a->str, strlen(a->str));
                } else if (a) {
                    int k = snprintf(num, sizeof num, "%ld", (long)a->num);
                    poner(out, cap, &len, num, (size_t)k);
                } else {
                    poner(out, cap, &len, p, literal);
                }
                p = q + 1;
                continue;
            }
        }
        poner(out, cap, &len, p++, 1);
    }
    if (cap)
        out[len < cap ? len : cap - 1] = 0;
    return len;
}
