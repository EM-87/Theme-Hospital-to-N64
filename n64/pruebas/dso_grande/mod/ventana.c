// Módulo pequeño de prueba (como una ventana de gestión): un poco de código y una tabla.
#include <stdint.h>
#include <string.h>
static const uint16_t tabla[512] = { 1, 2, 3 };
static uint8_t estado[2048];
int ventana_dibuja(int n)
{
    int s = 0;
    for (int i = 0; i < n && i < 512; i++) { estado[i & 2047] = (uint8_t)i; s += tabla[i] + estado[(i * 7) & 2047]; }
    memset(estado, 0, 16);
    return s;
}
