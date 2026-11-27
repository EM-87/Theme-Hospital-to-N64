// Contratar (UIHireStaff, dialogs/hire_staff.lua) y colocar personal
// (UIPlaceStaff, dialogs/place_staff.lua), a 320 × 240 con las piezas 57–78
// de MUTTON01. Las piezas de contratar están casadas con Req11V solo a ojo
// (mutton01.h), así que lo que va en cada hundido también es a ojo. Medidas:
//   - las pestañas, x = 0, de 29: médicos 57 (43; 58 pulsada) en y = 0,
//     enfermeras 59 (41) en y = 43, bedeles 61 (41) en y = 84 y
//     recepcionistas 63 (42) en y = 125: 167; la caja negra del retrato de la
//     pestaña, filas 29–35 y columnas 8–20: ahí va cuántos hay (CorsixTH los
//     pinta en x + 16, ancho 26, hire_staff.lua:166-173);
//   - el cuerpo, x = 29, de 106: la cabecera 65 (43) en y = 0, la habilidad
//     66 (26) en y = 43, la caja 67 (23) en y = 69, el sueldo 68 (16) en
//     y = 92, nueve tiras 69 (3) desde y = 108 y el pie 70 (8) en y = 135: 143;
//   - el nombre, en el hundido de la 65 (filas 8–16, columnas 8–98); el
//     retrato iría en el recuadro de rejilla de sus columnas 61–98 (FACE01 no
//     está en los assets: el hueco queda, sin cara); la barra blanca de las
//     filas 39–42 (columnas 15–51), bajo el fonendo, el maletín y el gorro, es
//     la marca de junior, médico o consultor (la 296 de CorsixTH): un trazo
//     naranja bajo el tercio que toca;
//   - la barra de habilidad: el hundido de la 66 en las filas 9–11, columnas
//     22–41 (20 píxeles: los 40 de CorsixTH a la mitad, hire_staff.lua:183-194,
//     el sprite 3 repetido), pintada con un rectángulo naranja;
//   - la caja de la 67 (filas 1–17, columnas 26–98), junto a las pastillas:
//     los títulos (cirujano, psiquiatra, investigador: los 292–294 de CorsixTH
//     son iconos; aquí su nombre); el sueldo, en el hundido de la 68 (filas
//     1–9, columnas 26–98); la descripción, en el recuadro que empieza en la
//     fila 15 de la 68 y acaba en la fila 0 de la 70 (columnas 8–98): 3 líneas;
//   - los botones en y = 143: anterior 71 (30) en x = 29, cancelar 73 (17) en
//     x = 59, contratar 75 (32) en x = 76 y siguiente 77 (30) en x = 108 (el
//     orden de MUTTON01: en Req11V contratar va antes que cancelar).
// La ventana, 138 × 167, en (50, 50) (la (100, 100) de CorsixTH a la mitad).
#include <stdio.h>
#include <string.h>

#include "ventanas_int.h"

#define CUERPO_X 29
#define BOTONES_Y 143
#define NARANJA ((27u << 11) | (10u << 6) | 1u) // (221, 83, 0), el naranja de las barras de CorsixTH, en RGBA5551
// (60, 174, 203), el color de los blankers de UIHireStaff (hire_staff.lua:76 y 83), en RGBA5551.
#define TAPADO ((7u << 11) | (21u << 6) | (25u << 1) | 1u)

static const int16_t pestana_y[4] = {0, 43, 84, 125};
static const char *const clases[4] = {"Doctor", "Nurse", "Handyman", "Receptionist"};

// La inicial (World:updateInitialsCache, world.lua:2428-2438): las letras de
// humanoid_name_starts y humanoid_name_ends seguidas, sin las 32 primeras;
// initials[name_seed % n], con la 0 como la última (initials[0] = initials[#initials]).
static char inicial(const th_game_ui *g, unsigned seed)
{
    const th_rules *t = g->env.texts;
    static const char *const tablas[2] = {"humanoid_name_starts", "humanoid_name_ends"};
    char letras[512];
    size_t n = 0;
    for (int k = 0; k < 2; k++) {
        const th_rnode lista = th_rules_path(t, t->root, tablas[k]);
        for (uint32_t i = 0; i < th_rules_len(t, lista); i++) {
            uint32_t len;
            const char *s = th_rules_str(t, th_rules_at(t, lista, i), &len);
            for (uint32_t j = 0; s && j < len && n < sizeof letras; j++)
                letras[n++] = s[j];
        }
    }
    if (n <= 32)
        return 'A';
    const size_t m = n - 32, k = seed % m;
    return letras[32 + (k == 0 ? m - 1 : k - 1)];
}

static const char *de_tabla(const th_game_ui *g, const char *ruta, unsigned rnd)
{
    const th_rules *t = g->env.texts;
    const th_rnode lista = th_rules_path(t, t->root, ruta);
    const uint32_t n = th_rules_len(t, lista);
    const char *s = n ? th_rules_str(t, th_rules_at(t, lista, rnd % n), NULL) : NULL;
    return s ? s : "";
}

// El candidato a la vista: StaffProfile:getFullName, el sueldo, los títulos y
// la descripción (randomiseOrganical, staff_profile.lua:152-176, con los
// índices que guarda el perfil).
static void candidato(th_game_ui *g)
{
    const th_staff_profile *lista;
    for (int k = 0; k < 4; k++) {
        g->n_cand[k] = th_sim_candidates(g->s, TH_HUM_DOCTOR + k, &lista);
        snprintf(g->cuentas[k], sizeof g->cuentas[k], "%d", g->n_cand[k]);
    }
    // Sin categoría (UIHireStaff recién abierta: self.category nil y el
    // complete_blanker a la vista, hire_staff.lua:82-84 y 260-263), sin candidato.
    const int n = g->clase < 0 ? 0 : th_sim_candidates(g->s, TH_HUM_DOCTOR + g->clase, &lista);
    if (g->idx >= n)
        g->idx = n ? n - 1 : 0;
    if (g->idx < 0)
        g->idx = 0;
    g->hay_candidato = n > 0;
    if (!n) {
        g->nombre[0] = g->sueldo[0] = g->titulos[0] = g->descripcion[0] = 0;
        return;
    }
    const th_staff_profile *pf = &lista[g->idx];
    g->perfil = *pf;
    g->habilidad = pf->skill;
    g->titulos_bits = pf->titles;
    g->es_medico = pf->kind == TH_HUM_DOCTOR;
    snprintf(g->nombre, sizeof g->nombre, "%c. %s%s", inicial(g, pf->name_seed),
             de_tabla(g, "humanoid_name_starts", pf->name_rnd[0]), de_tabla(g, "humanoid_name_ends", pf->name_rnd[1]));
    snprintf(g->sueldo, sizeof g->sueldo, "$%ld", (long)pf->wage);
    g->titulos[0] = 0;
    static const struct {
        uint8_t bit;
        const char *ruta;
    } t[3] = {{TH_TITLE_SURGEON, "staff_title.surgeon"},
              {TH_TITLE_PSYCHIATRIST, "staff_title.psychiatrist"},
              {TH_TITLE_RESEARCHER, "staff_title.researcher"}};
    for (int k = 0; k < 3; k++)
        if (pf->titles & t[k].bit) {
            const size_t l = strlen(g->titulos);
            snprintf(g->titulos + l, sizeof g->titulos - l, "%s%s", l ? ", " : "", th_text(g->env.texts, t[k].ruta));
        }
    const char *d1 = de_tabla(g, pf->skill < 550000 ? "staff_descriptions.bad" : "staff_descriptions.good",
                              pf->desc_rnd[0]);
    const char *d2 = de_tabla(g, "staff_descriptions.misc", pf->desc_rnd[1]);
    if (strlen(d1) + strlen(d2) > 96) // «while #our_concat(descs) > 96»: se quita la segunda
        d2 = "";
    if (pf->desc_swap && *d2)
        snprintf(g->descripcion, sizeof g->descripcion, "%s%s", d2, d1);
    else
        snprintf(g->descripcion, sizeof g->descripcion, "%s%s", d1, d2);
}

static int dibujar(const th_ui_window *w, th_drawlist *out, void *ctx)
{
    const th_game_ui *g = ctx;
    const th_ui_fonts *f = g->env.fonts;
    if (th_ui_window_draw(w, out) < 0)
        return -1;
    const int x = w->x, y = w->y, cx = x + CUERPO_X;
    for (int k = 0; k < 4; k++)
        th_font_draw(&f->white, g->cuentas[k], x + 8, y + pestana_y[k] + 28, 13, 0, TH_ALIGN_CENTER, out, NULL, NULL);
    // Sin categoría, el complete_blanker tapa todo lo del candidato
    // (hire_staff.lua:82-83 y 261): aquí, los hundidos del cuerpo (columnas
    // 8–98, de la fila 8 de la cabecera al pie).
    if (g->clase < 0)
        th_drawlist_fill(out, cx + 8, y + 8, 91, 127, (uint16_t)TAPADO);
    if (g->hay_candidato) {
        th_font_draw(&f->white, g->nombre, cx + 9, y + 8, 0, 0, TH_ALIGN_LEFT, out, NULL, NULL);
        const int barra = (int)((g->habilidad * 20 + 500000) / 1000000); // floor(skill · 40 + 0,5) a la mitad
        if (barra > 0)
            th_drawlist_fill(out, cx + 22, y + 43 + 9, barra, 3, (uint16_t)NARANJA);
        if (g->es_medico) {
            const int tercio = (g->titulos_bits & TH_TITLE_JUNIOR)       ? 0
                               : (g->titulos_bits & TH_TITLE_CONSULTANT) ? 2
                                                                         : 1;
            th_drawlist_fill(out, cx + 16 + 12 * tercio, y + 40, 11, 2, (uint16_t)NARANJA);
        }
        th_font_wrapped(&f->white, g->titulos, strlen(g->titulos), cx + 27, y + 69 + 1, 71, 2, 0, TH_ALIGN_LEFT, out);
        th_font_draw(&f->white, g->sueldo, cx + 27, y + 92 + 1, 0, 0, TH_ALIGN_LEFT, out, NULL, NULL);
        th_font_wrapped(&f->white, g->descripcion, strlen(g->descripcion), cx + 9, y + 108, 89, 3, 0, TH_ALIGN_LEFT,
                        out);
    }
    return out->count < out->capacity ? 0 : -1;
}

// La ventana con la categoría de ahora (se rehace al cambiarla).
static void montar(th_game_ui *g)
{
    th_ui_window *w = &g->w;
    th_ui_stack_close(g->env.stack, w);
    th_ui_window_init(w, 138, 167);
    w->x = (int16_t)(g->env.screen_w * 50 / 320);
    w->y = (int16_t)(g->env.screen_h * 50 / 240);
    w->draw = dibujar;
    static const uint8_t pestanas[4][2] = {{TH_MT_CTR_MEDICOS, TH_MT_CTR_MEDICOS_P},
                                           {TH_MT_CTR_ENFERMERAS, TH_MT_CTR_ENFERMERAS_P},
                                           {TH_MT_CTR_BEDELES, TH_MT_CTR_BEDELES_P},
                                           {TH_MT_CTR_RECEPCIONISTAS, TH_MT_CTR_RECEPCIONISTAS_P}};
    static const char *const tips[4] = {"tooltip.hire_staff_window.doctors", "tooltip.hire_staff_window.nurses",
                                        "tooltip.hire_staff_window.handymen",
                                        "tooltip.hire_staff_window.receptionists"};
    for (int k = 0; k < 4; k++) {
        const int b = gw_boton(w, pestanas[k][0], 0, pestana_y[k], 0, 0, 29, k == 0 ? 43 : k == 3 ? 42 : 41,
                               pestanas[k][1], TH_GWB_PESTANA + k);
        th_ui_button_tooltip(w, b, tips[k]);
        // La pestaña elegida, hundida (la categoría de UIHireStaff:setCategory;
        // con g->clase = −1, ninguna).
        if (k == g->clase)
            w->panels[w->buttons[b].panel].sprite = w->buttons[b].sprite_normal = pestanas[k][1];
    }
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_CTR_CABECERA, CUERPO_X, 0);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_CTR_HABILIDAD, CUERPO_X, 43);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_CTR_CAJA, CUERPO_X, 69);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_CTR_SUELDO, CUERPO_X, 92);
    for (int i = 0; i < 9; i++)
        th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_CTR_TEXTO, CUERPO_X, 108 + 3 * i);
    th_ui_add_panel(w, TH_SHEET_MUTTON, TH_MT_CTR_PIE, CUERPO_X, 135);
    int b = gw_boton(w, TH_MT_CTR_ANTERIOR, CUERPO_X, BOTONES_Y, 0, 0, 30, 21, TH_MT_CTR_ANTERIOR_P, TH_GWB_ANTERIOR);
    th_ui_button_tooltip(w, b, "tooltip.hire_staff_window.prev_person");
    b = gw_boton(w, TH_MT_CTR_CANCELAR, CUERPO_X + 30, BOTONES_Y, 0, 0, 17, 21, TH_MT_CTR_CANCELAR_P,
                 TH_GWB_CTR_CANCELAR);
    th_ui_button_tooltip(w, b, "tooltip.hire_staff_window.cancel");
    b = gw_boton(w, TH_MT_CTR_CONTRATAR, CUERPO_X + 47, BOTONES_Y, 0, 0, 32, 21, TH_MT_CTR_CONTRATAR_P,
                 TH_GWB_CONTRATAR);
    th_ui_button_tooltip(w, b, "tooltip.hire_staff_window.hire");
    b = gw_boton(w, TH_MT_CTR_SIGUIENTE, CUERPO_X + 79, BOTONES_Y, 0, 0, 30, 21, TH_MT_CTR_SIGUIENTE_P,
                 TH_GWB_SIGUIENTE);
    th_ui_button_tooltip(w, b, "tooltip.hire_staff_window.next_person");
    // UIHireStaff:updateTooltips (hire_staff.lua:106-108): la del sueldo,
    // solo con una categoría elegida.
    if (g->clase >= 0) {
        th_ui_region_tooltip(w, "tooltip.hire_staff_window.salary", CUERPO_X + 26, 92, CUERPO_X + 99, 102);
        th_ui_region_tooltip(w, "tooltip.hire_staff_window.staff_ability", CUERPO_X + 8, 43, CUERPO_X + 60, 69);
    }
    if (g->s)
        candidato(g);
    gw_abrir_ventana(g, TH_GW_CONTRATAR);
}

// UIHireStaff se abre sin categoría (self.category nil, el complete_blanker a
// la vista y ninguna pestaña hundida: hire_staff.lua:46-55, 82-83 y 260-263),
// cada vez: la de la vez anterior no se recuerda.
void gw_contratar_abrir(th_game_ui *g)
{
    g->clase = -1;
    g->idx = 0;
    montar(g);
}

void gw_contratar_boton(th_game_ui *g, int id, int mouse)
{
    (void)mouse;
    if (id >= TH_GWB_PESTANA && id < TH_GWB_PESTANA + 4) {
        // Las pestañas son de dos estados (makeToggleButton, hire_staff.lua:46-55):
        // una clase sin candidatos no se elige (wrong2.wav) y pulsar la elegida
        // la suelta (setCategory(nil)).
        const int k = id - TH_GWB_PESTANA;
        if (g->n_cand[k] == 0)
            return;
        g->clase = k == g->clase ? -1 : k;
        g->idx = 0; // setCategory: current_index = 1
        montar(g);
        return;
    }
    switch (id) {
    case TH_GWB_ANTERIOR: // moveBy(-1), con el tope en 1
        if (g->idx > 0)
            g->idx--;
        break;
    case TH_GWB_SIGUIENTE: // moveBy(1): sin categoría, nada
        if (g->clase >= 0 && g->idx + 1 < g->n_cand[g->clase])
            g->idx++;
        break;
    case TH_GWB_CTR_CANCELAR: gw_cerrar(g); return;
    case TH_GWB_CONTRATAR:
        // UIHireStaff:hire (hire_staff.lua:126-147): sin perfil, nada; sin
        // dinero, el aviso; si no, a colocarlo (UIPlaceStaff). La orden sale
        // al soltarlo.
        candidato(g);
        if (!g->hay_candidato)
            return;
        if (g->s->hosp.balance < g->perfil.wage) {
            gw_aviso(g, "adviser.warnings.cannot_afford");
            return;
        }
        g->resumen = th_staff_profile_digest(&g->perfil);
        g->esperando_contrato = 0;
        g->sitio.hay = 0;
        memset(g->capas, 0, sizeof g->capas);
        g->capas[5] = g->perfil.layer5; // anim:setLayer(5, profile.layer5)
        th_ui_stack_close(g->env.stack, &g->w); // UIPlaceStaff es invisible
        g->active = TH_GW_COLOCAR_PERSONAL;
        return;
    default: break;
    }
    candidato(g);
}

void gw_contratar_frame(th_game_ui *g)
{
    candidato(g);
}

// UIPlaceStaff: la persona en la mano sigue al puntero (ScreenToWorld(x,
// y + 14): 7 píxeles a la mitad), con idle_east sin espejo y solo la capa 5
// (place_staff.lua:43-50, 127-140), en gris donde _isValidStaffPlacement no
// la deja (th_build_staff_placement, solo con la simulación entre ticks); el
// izquierdo la suelta (TH_ORD_CONTRATAR: dir = clase, qty = índice, def =
// resumen del perfil) y el derecho la cancela: UIPlaceStaff:onMouseUp con el
// derecho solo llama a close() (place_staff.lua:144-148), que devuelve el
// perfil al mercado (place_staff.lua:73-79) sin volver a abrir contratar.
void gw_colocar_personal_frame(th_game_ui *g)
{
    const th_game_ui_input *in = g->in;
    if (in->world_up & (1 << TH_UI_RIGHT)) {
        gw_cerrar(g);
        return;
    }
    if (g->clase < 0)
        return;
    int x, y;
    gw_casilla(g, in->px, in->py + 7, &x, &y);
    if (!g->sobre_mundo || !gw_en_mapa(x, y))
        return;
    const int kind = TH_HUM_DOCTOR + g->clase;
    if (in->sim_idle) {
        g->sitio.ok = th_build_staff_placement(g->s, kind, x, y) == TH_REASON_OK;
        g->sitio.x = x;
        g->sitio.y = y;
        g->sitio.hay = 1;
    }
    const int ok = g->sitio.hay ? g->sitio.ok : 1;
    th_vista_persona_fantasma(th_walk_anims_find(clases[g->clase]), g->env.anims, x, y, TH_DIR_EAST, g->rot,
                              ok ? 0 : TH_ANIM_PAL_GREY, g->capas, &g->top[0]);
    g->n_top = 1;
    if ((in->world_up & (1 << TH_UI_LEFT)) && !g->esperando_contrato) {
        if (!ok) {
            gw_aviso(g, "adviser.placement_info.staff_cannot_place");
            return;
        }
        const th_order o = {.kind = TH_ORD_CONTRATAR, .dir = (uint8_t)kind, .qty = (uint8_t)g->idx, .x = (uint8_t)x,
                            .y = (uint8_t)y, .def = g->resumen};
        if (gw_mandar(g, &o))
            g->esperando_contrato = 1;
    }
}
