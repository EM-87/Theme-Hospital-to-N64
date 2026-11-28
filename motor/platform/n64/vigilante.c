// El vigilante de la ROM de diagnóstico: vigilante.h.
#include "vigilante.h"
#ifdef TH_VIGILANTE
#include <libdragon.h>
#include <stdio.h>

enum { ANILLO = 10, CADA_MS = 20 };

static volatile const char *fase_nombre; // NULL: desarmado
static volatile uint32_t fase_marca, fase_limite;
static struct {
    const char *nombre;
    uint32_t ticks;
} anillo[ANILLO];
static volatile int anillo_n;
static volatile struct {
    uint32_t fotograma, hora;
    int velocidad, pausa, personas, ventana;
} ctx;

static uint32_t ms_de(uint32_t ticks)
{
    return ticks / (TICKS_PER_SECOND / 1000);
}

void vigilante_fase(const char *nombre, int limite_ms)
{
    const uint32_t t = TICKS_READ();
    fase_nombre = NULL; // primero desarmado: el temporizador no ve una marca a medias
    fase_marca = t;
    fase_limite = TICKS_FROM_MS(limite_ms);
    if (nombre) {
        anillo[anillo_n % ANILLO].nombre = nombre;
        anillo[anillo_n % ANILLO].ticks = t;
        anillo_n++;
    }
    fase_nombre = nombre;
}

void vigilante_contexto(uint32_t fotograma, uint32_t hora, int velocidad, int pausa, int personas, int ventana)
{
    ctx.fotograma = fotograma;
    ctx.hora = hora;
    ctx.velocidad = velocidad;
    ctx.pausa = pausa;
    ctx.personas = personas;
    ctx.ventana = ventana;
}

// Cada 20 ms, en la interrupción del temporizador: si la fase lleva más de su
// límite, el assert. El texto empieza con dos líneas en blanco para que el
// borde de la tele no se lo coma; sin tildes, que la fuente del inspector no
// las tiene. Después del texto, libdragon pinta el rastreo de la pila del
// punto interrumpido: la función de libdragon en la que esperaba la CPU.
static void tic(int ovfl)
{
    (void)ovfl;
    const char *n = (const char *)fase_nombre;
    if (!n)
        return;
    const uint32_t ahora = TICKS_READ(), edad = ahora - fase_marca;
    if (edad < fase_limite)
        return;
    fase_nombre = NULL;
    volatile uint32_t *const SP = (volatile uint32_t *)0xA4040000, *const DP = (volatile uint32_t *)0xA4100000,
                      *const SP_PCR = (volatile uint32_t *)0xA4080000;
    const uint32_t sp_status = SP[4], sp_pc = *SP_PCR, dp_status = DP[3], dp_cur = DP[2], dp_end = DP[1];
    static char msg[700];
    int k = 0;
    k += snprintf(msg + k, sizeof msg - k, "\n\nVIGILANTE: la CPU lleva %lu ms en '%s'\n", (unsigned long)ms_de(edad), n);
    k += snprintf(msg + k, sizeof msg - k, "SP_STATUS %04lx PC %03lx | DP_STATUS %03lx cur %08lx end %08lx\n",
                  (unsigned long)sp_status, (unsigned long)(sp_pc & 0xFFF), (unsigned long)dp_status,
                  (unsigned long)dp_cur, (unsigned long)dp_end);
    k += snprintf(msg + k, sizeof msg - k, "fotograma %lu hora %lu x%d%s %d personas ventana %d\n",
                  (unsigned long)ctx.fotograma, (unsigned long)ctx.hora, ctx.velocidad, ctx.pausa ? " pausa" : "",
                  ctx.personas, ctx.ventana);
    k += snprintf(msg + k, sizeof msg - k, "antes (ms):");
    const int total = anillo_n, desde = total > ANILLO ? total - ANILLO : 0;
    for (int i = desde; i < total && k < (int)sizeof msg - 40; i++) {
        const uint32_t t0 = anillo[i % ANILLO].ticks, t1 = i + 1 < total ? anillo[(i + 1) % ANILLO].ticks : ahora;
        k += snprintf(msg + k, sizeof msg - k, " %s %lu", anillo[i % ANILLO].nombre, (unsigned long)ms_de(t1 - t0));
    }
    assertf(0, "%s", msg);
}

void vigilante_iniciar(void)
{
    timer_init();
    new_timer(TICKS_FROM_MS(CADA_MS), TF_CONTINUOUS, tic);
    debugf("TH64 vigilante: en marcha, cada %d ms\n", CADA_MS);
}
#endif
