// Módulo de interfaz ficticio: código que solo hace falta en una pantalla concreta.
#include <libdragon.h>
#include <stdio.h>
static char buf[4096];
int menu_dibuja(int n)
{
    int len = 0;
    for (int i = 0; i < n; i++)
        len += snprintf(buf + (len % 2048), 2048, "Opcion %d: %s precio %d.%02d\n", i, (i & 1) ? "Consulta" : "Farmacia", i * 1000 / 7, i % 100);
    return len;
}
