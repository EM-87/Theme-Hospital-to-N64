#include "partida.h"

#include <libdragon.h>
#include <stdio.h>
#include <string.h>

#include "simulacion.h"
#include "th/rules.h"

// Los nombres de las órdenes en el registro, por TH_ORD_*: los de
// bench/lib/ordenes.lua, copiados de th64sim (platform/pc/construir.c,
// orden_nombres y th64sim_nombre_orden), que no se enlaza en la N64.
static const char *const orden_nombres[] = {
    [TH_ORD_NONE] = "?",
    [TH_ORD_COMPRAR] = "comprar",
    [TH_ORD_COLOCAR] = "colocar",
    [TH_ORD_CERRAR] = "cerrar",
    [TH_ORD_RECOGER] = "recoger",
    [TH_ORD_VENDER] = "vender",
    [TH_ORD_DESHACER] = "deshacer",
    [TH_ORD_SALA] = "sala",
    [TH_ORD_PUERTA] = "puerta",
    [TH_ORD_VENTANA] = "ventana",
    [TH_ORD_QUITAR_VENTANA] = "quitar_ventana",
    [TH_ORD_FIN_VENTANAS] = "fin_ventanas",
    [TH_ORD_COMPRAR_SALA] = "comprar_sala",
    [TH_ORD_FIN_SALA] = "fin_sala",
    [TH_ORD_ATRAS] = "atras",
    [TH_ORD_CONTRATAR] = "contratar",
    [TH_ORD_DEVOLVER] = "devolver",
};

const char *partida_nombre_orden(int kind)
{
    const int n = (int)(sizeof orden_nombres / sizeof orden_nombres[0]);
    return kind > 0 && kind < n && orden_nombres[kind] ? orden_nombres[kind] : "?";
}

// th64sim_id_de (construir.c): el id dentro de objetos o salas de las
// definiciones, o «-».
const char *partida_id_de(const th_sim *s, th_rnode def)
{
    static const char *const grupos[] = {"objetos", "salas"};
    if (!def)
        return "-";
    for (int g = 0; g < 2; g++) {
        const th_rnode m = th_rules_get(s->defs, s->defs->root, grupos[g]);
        for (uint32_t i = 0; i < th_rules_len(s->defs, m); i++)
            if (th_rules_entry_value(s->defs, m, i) == def)
                return th_rules_str(s->defs, th_rules_entry_key(s->defs, m, i), NULL);
    }
    return "-";
}

// El hueco propio: las órdenes que esperan a que la simulación deje el tick
// a medias o a que la cola tenga sitio. Un clic da como mucho una orden por
// fotograma (TH_GAME_UI_ORDERS 4): 8 son unos cuantos fotogramas de margen.
#define ESPERAN 8
static th_order esperan[ESPERAN];
static int n_esperan;
static th_game_ui *ventanas;
static int aplicadas, aceptadas, rechazo, motivo_rechazo;
static uint32_t hora_ultimo;
static int en_ultimo;
static char nombres_ultimo[64];
static char mensaje[64]; // el texto de la barra (partida_texto)

// El gancho th_sim_events.order: corre dentro de la corrutina, en
// th_sim_apply_orders, al empezar el tick. La línea es la que escribe th64sim
// partida en ordenes_rom.log (al_aplicar, platform/pc/hito4.c): el tick en
// que se aplica, los campos de th_order en decimal, el id de la definición
// o, en contratar y devolver, el resumen del perfil (th_order.def,
// th_staff_profile_digest) en 8 cifras hexadecimales, y el motivo.
static void al_aplicar(void *ctx, const th_sim *s, const th_order *o, const th_order_result *r)
{
    (void)ctx;
    const int ok = r->reason == TH_REASON_OK;
    char id[12];
    const char *def_id = id;
    if (o->kind == TH_ORD_CONTRATAR || o->kind == TH_ORD_DEVOLVER)
        snprintf(id, sizeof id, "%08lx", (unsigned long)o->def);
    else
        def_id = partida_id_de(s, o->def);
    debugf("TH64 orden\t%lu\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%s\t%s\n", (unsigned long)s->hours,
           partida_nombre_orden(o->kind), o->x, o->y, o->dir, o->qty, o->w, o->h, def_id,
           ok ? "-" : th_build_reason_name(r->reason));
    aplicadas++;
    aceptadas += ok;
    if (!en_ultimo || hora_ultimo != s->hours) {
        hora_ultimo = s->hours;
        en_ultimo = 0;
        nombres_ultimo[0] = 0;
    }
    const size_t largo = strlen(nombres_ultimo);
    snprintf(nombres_ultimo + largo, sizeof nombres_ultimo - largo, "%s%s", en_ultimo ? "," : "",
             partida_nombre_orden(o->kind));
    en_ultimo++;
    if (!ok) {
        rechazo = 1;
        motivo_rechazo = r->reason;
    }
    // Para que la ventana reaccione (el objeto sigue en la mano, la de
    // colocar personal se cierra...). El sonido y el texto los decide la
    // ventana (reject_sound, ventanas.h) en el bucle principal, fuera de la
    // corrutina; partida_rechazo solo lo apunta en debugf.
    if (ventanas)
        th_game_ui_order_result(ventanas, s, o, r);
}

void partida_empezar(th_sim *s, th_game_ui *g, const char *escenario, uint32_t semilla)
{
    ventanas = g;
    n_esperan = aplicadas = aceptadas = rechazo = en_ultimo = 0;
    nombres_ultimo[0] = 0;
    mensaje[0] = 0; // sin el texto de la partida anterior (partida_texto)
    s->ev.order = al_aplicar;
    s->ev.ctx = NULL;
    if (escenario)
        debugf("TH64 partida nueva: escenario %s, semilla %lu\n", escenario, (unsigned long)semilla);
}

// Empujar con la corrutina suspendida a medio tick no rompe la cola
// (th_sim_push_order solo añade al final y th_sim_apply_orders vuelve a mirar
// n_orders en cada vuelta), pero no es lo que pide el contrato: th_build_apply
// busca caminos (el bloqueo de zonas, would_break_pathfinding, build.c:149-200)
// y path.c tiene puntos de pausa, así que la corrutina puede estar dentro de
// th_sim_apply_orders. Por sencillez, las órdenes entran solo con el tick
// acabado o sin empezar.
void partida_vaciar(th_sim *s)
{
    int k = 0;
    if (!simulacion_en_tick())
        while (k < n_esperan) {
            esperan[k].tick = s->hours;
            if (th_sim_push_order(s, &esperan[k]) != 0)
                break;
            k++;
        }
    if (k) {
        memmove(esperan, esperan + k, (size_t)(n_esperan - k) * sizeof esperan[0]);
        n_esperan -= k;
    }
}

int partida_mandar_orden(th_sim *s, const th_order *o)
{
    if (n_esperan == ESPERAN) {
        partida_vaciar(s);
        if (n_esperan == ESPERAN) {
            debugf("TH64 partida: la orden %s no cabe (%d esperan): se descarta\n", partida_nombre_orden(o->kind),
                   n_esperan);
            return -1;
        }
    }
    esperan[n_esperan++] = *o;
    partida_vaciar(s);
    return 0;
}

int partida_pendientes(const th_sim *s)
{
    return n_esperan + s->n_orders;
}

int partida_rechazo(int *motivo)
{
    if (!rechazo)
        return 0;
    rechazo = 0;
    *motivo = motivo_rechazo;
    return 1;
}

int partida_aplicadas(int *n_aceptadas)
{
    if (n_aceptadas)
        *n_aceptadas = aceptadas;
    return aplicadas;
}

const char *partida_ultimo_tick(uint32_t *hora, int *cuantas)
{
    *hora = hora_ultimo;
    *cuantas = en_ultimo;
    return nombres_ultimo;
}

// El aviso en la barra: unos 3 s.
#define MS_AVISO 3000
static uint32_t mensaje_hasta;

static wav64_t *efecto;
static bool audio;

void partida_texto(const char *texto)
{
    if (!texto)
        return;
    snprintf(mensaje, sizeof mensaje, "%s", texto);
    mensaje_hasta = get_ticks_ms() + MS_AVISO;
}

void partida_aviso(const char *texto)
{
    partida_texto(texto);
    if (efecto)
        wav64_play(efecto, 0);
}

const char *partida_mensaje(void)
{
    if (!mensaje[0])
        return NULL;
    if ((int32_t)(get_ticks_ms() - mensaje_hasta) >= 0) {
        mensaje[0] = 0;
        return NULL;
    }
    return mensaje;
}

bool partida_sonido_iniciar(void)
{
    static const char ruta[] = "rom:/sonido/comun/wrong2.wav64";
    FILE *f = fopen(ruta, "rb");
    if (!f)
        return false;
    fclose(f);
    audio_init(22050, AUDIO_DEFAULT_LATENCY);
    mixer_init(4);
    audio = true;
    efecto = wav64_load(ruta, NULL);
    return efecto != NULL;
}

void partida_sonido_fotograma(void)
{
    if (audio)
        mixer_try_play();
}

void partida_sonido_cerrar(void)
{
    if (!audio)
        return;
    mixer_ch_stop(0);
    if (efecto)
        wav64_close(efecto);
    efecto = NULL;
    mixer_close();
    audio_close();
    audio = false;
}

void partida_terminar(th_sim *s)
{
    const int descartadas = n_esperan + s->n_orders;
    if (descartadas)
        debugf("TH64 partida: %d ordenes sin aplicar al salir (%d esperaban en la ROM y %d en la cola del nucleo): "
               "se descartan\n",
               descartadas, n_esperan, s->n_orders);
    n_esperan = 0;
    s->n_orders = 0;
    s->ev.order = NULL;
    ventanas = NULL;
    mensaje[0] = 0;
    debugf("TH64 partida: %lu horas, huella %08lx, saldo %ld\n", (unsigned long)s->hours,
           (unsigned long)th_sim_hash(s), (long)s->hosp.balance);
}
