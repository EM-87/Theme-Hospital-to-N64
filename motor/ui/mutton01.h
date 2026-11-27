// Catálogo de DATAM/MUTTON01 (TH_SHEET_MUTTON, rom:/datam/mutton01.tsh): las
// piezas de las ventanas del juego de PC a 320 × 200. CorsixTH no usa esta
// hoja (no aparece en su Lua ni en su C++): sus ventanas de 640 salen de las
// hojas QDATA/Req0?V, que comparten una tabla de 415 entradas con cada
// ventana en su tramo. MUTTON01 trae las mismas piezas de cada ventana
// empaquetadas y renumeradas seguidas, a la mitad aproximada pero con sus
// propias proporciones; por eso cada ventana necesita su disposición medida
// sobre estas piezas, como la «baja» de panel_inferior.c.
//
// 412 entradas; 190 piezas reales (1–48, 50–185 y 406–411); las demás son
// relleno de 1 × 1 (el color 119), y la 0 está vacía. Medido con
// tools/conv/hojas.leer_tsh sobre motor/assets/datam/mutton01.tsh y mirado a
// ojo en los PNG del volcado (x1 y x4 por pieza y hojas de contacto).
//
// Columnas de los comentarios: índice, ancho × alto, qué es y el sprite de 640
// de CorsixTH con el que casa («Req09V 210» = el sprite 210 de QData/Req09V).
// «_P» es el sprite pulsado (rojo) del botón de la línea anterior. Grado de
// certeza: sin marca, casado por tramo y comprobado a ojo contra el sprite de
// 640; «(a ojo)», identificado solo por el dibujo; «(?)», dudoso.
//
// TH_MUTTON01_PIEZAS(X) da las piezas de las cuatro ventanas de la demo
// (colocar, construir, amueblar y contratar) como X(nombre, índice, ancho,
// alto), para disposiciones y pruebas; el enum TH_MT_* tiene los índices.
#ifndef TH_UI_MUTTON01_H
#define TH_UI_MUTTON01_H

// ---------------------------------------------------------------------------
// UIPlaceObjects (dialogs/place_objects.lua:44-163), QData/Req05V: 1–19.
// La ventana de 93 de ancho (186 en 640); a la derecha (0.9, 0.1).
//   1  93×30  cabecera (título)                           Req05V 112
//   2  93×3   tira de la caja de texto (se repite)         Req05V 113
//   3  93×11  medio, sobre los botones                     Req05V 114
//   4  26×27  botón cancelar (✗), verde                    Req05V 115
//   5  26×27  cancelar_P                                   Req05V 116
//   6  22×27  botón comprar (caja registradora)            Req05V 117
//   7  22×27  comprar_P                                    Req05V 118
//   8  22×27  botón coger (mano), de dos estados           Req05V 119
//   9  22×27  coger_P                                      Req05V 120
//  10  26×27  botón confirmar (✓)                          Req05V 121
//  11  26×27  confirmar_P                                  Req05V 122
//  12  26×27  cancelar en gris (CorsixTH no lo usa)        Req05V 126 (?)
//  13  22×27  comprar deshabilitado (gris)                 Req05V 127
//  14  22×27  coger deshabilitado                          Req05V 128
//  15  26×27  confirmar deshabilitado                      Req05V 129
//  16  93×6   cabecera de la lista (oculta sin objetos)    Req05V 123
//  17  93×17  fila de la lista                             Req05V 124
//  18  93×17  fila_P (la activa)                           Req05V 125
//  19  93×11  pie de la lista                              Req05V 156
//
// UIBuildRoom (dialogs/build_room.lua:62-91), QData/Req09V 210–224: 20–34.
// Columna izquierda de 79 (categorías y rejilla de la vista previa) y
// derecha de 116 (lista de 10 filas y cerrar): 195 × 175 montada.
//  20  79×23  categoría diagnóstico (lila)                 Req09V 210
//  21  79×23  diagnóstico_P                                Req09V 211
//  22  79×18  categoría tratamiento (verde)                Req09V 212
//  23  79×18  tratamiento_P                                Req09V 213
//  24  79×18  categoría clínicas (ocre)                    Req09V 214
//  25  79×18  clínicas_P                                   Req09V 215
//  26  79×22  categoría instalaciones (rosa)               Req09V 216
//  27  79×22  instalaciones_P                              Req09V 217
//  28  79×19  rejilla de la vista previa, arriba           Req09V 218
//  29  79×7   rejilla, tramo central (se repite: 8 veces)  Req09V 219
//  30  79×19  rejilla, abajo                               Req09V 220
//  31  116×19 cabecera de la lista (título)                Req09V 221
//  32  116×11 fila de sala (×10)                           Req09V 222
//  33  116×11 fila_P / bajo el puntero                     Req09V 223
//  34  116×42 caja del coste con el botón cerrar (✗)       Req09V 224
//  35  108×19 botón cerrar (✗) solo, verde                 sin pareja: CorsixTH pinta el hundido
//  36  108×19 cerrar_P                                     con Bitmap/aux_ui (build_room.lua:84-91)
//  37  69×18  rectángulo gris claro con brillo             sin casar (?)
//  38  69×17  ídem, 1 px más bajo                          sin casar (?)
//
// UIFurnishCorridor (dialogs/furnish_corridor.lua:45-123), QData/Req10V
// 228–247: 39–56. Izquierda de 79 (vista previa, precio, total, cerrar) y
// derecha de 136 (lista de 10 filas, pie y confirmar): 215 × 175 montada.
//  39  79×19  rejilla de la vista previa, arriba           Req10V 228
//  40  79×7   rejilla, tramo central (se repite)           Req10V 229
//  41  79×19  rejilla, abajo                               Req10V 230
//  42  79×22  caja del precio, arriba                      Req10V 231
//  43  79×15  caja del precio, medio                       Req10V 232
//  44  79×21  caja del total, abajo                        Req10V 233
//  45  79×23  botón cerrar (✗) con su fondo                Req10V 242 (y el fondo 234)
//  46  79×23  cerrar_P                                     Req10V 243
//  47  136×19 cabecera de la lista (título)                Req10V 235
//  48  136×11 fila con «−» y «+» dibujados (×10)           Req10V 239 (sin el pulsado 240/241)
//  49  1×1    relleno
//  50  136×19 pie de la lista                              Req10V 236
//  51  136×23 botón confirmar (hospital ← camión)          Req10V 237
//  52  136×23 confirmar_P                                  Req10V 238
//  53  9×9    botón «−» (repetido)                         Req10V 244
//  54  9×9    menos_P                                      Req10V 245
//  55  9×9    botón «+» (repetido)                         Req10V 246
//  56  9×9    más_P                                        Req10V 247
//
// UIHireStaff (dialogs/hire_staff.lua:26-146), QData/Req11V 253–279: 57–78.
// 106 de ancho más las pestañas de 29 (242 × 323 en 640).
//  57  29×43  pestaña médicos (retrato)                    Req11V 264
//  58  29×43  médicos_P                                    Req11V 265
//  59  29×41  pestaña enfermeras                           Req11V 266
//  60  29×41  enfermeras_P                                 Req11V 267
//  61  29×41  pestaña bedeles                              Req11V 268
//  62  29×41  bedeles_P                                    Req11V 269
//  63  29×42  pestaña recepcionistas                       Req11V 270
//  64  29×42  recepcionistas_P                             Req11V 271
//  65  106×43 cabecera: cara, títulos (fonendo, maletín,   Req11V 256 (a ojo)
//             gorro de cirujano) y rejilla
//  66  106×26 habilidad: barra y ordenador (investigador)  Req11V 259/260 (a ojo)
//  67  106×23 caja con pastillas (psiquiatra?)             (a ojo)
//  68  106×16 sueldo (el cheque)                           Req11V 261 (a ojo)
//  69  106×3  tira de la descripción (se repite)           Req11V 262 (a ojo)
//  70  106×8  pie                                          Req11V 263 (a ojo)
//  71  30×21  botón anterior (←)                           Req11V 272
//  72  30×21  anterior_P                                   Req11V 273
//  73  17×21  botón cancelar (✗)                           Req11V 276
//  74  17×21  cancelar_P                                   Req11V 277
//  75  32×21  botón contratar (el contrato)                Req11V 274
//  76  32×21  contratar_P                                  Req11V 275
//  77  30×21  botón siguiente (→)                          Req11V 278
//  78  30×21  siguiente_P                                  Req11V 279
//  (en MUTTON01 el orden es anterior, cancelar, contratar, siguiente; en
//  Req11V, anterior, contratar, cancelar, siguiente)
//
// ---------------------------------------------------------------------------
// El resto, fuera de la demo, identificado a ojo (los Req de CorsixTH:
// Req01V staff_dialog, Req02V patient, Req03V machine, Req04V confirm_dialog,
// Req06V queue_dialog, Req12V staff_rise, Req13V jukebox):
//  79–83  106×29/6/30/36/19  más piezas azules de contratar: barra de
//         habilidad, rejilla, títulos y reloj (?) (Req11V 292–296 (?))
//  84–86  107×40/34/39  pergamino (la descripción del candidato o un mensaje) (?)
//  87/88  54×30  subir sueldo (bolsa con «+»), y _P     UIStaff (a ojo)
//  89/90  54×30  despedir (mano en la puerta), y _P     UIStaff (a ojo)
//  91–95  112×41, 112×25, 112×32, 120×32, 120×25  ventana del empleado
//         (Req01V): cabecera con ✗ y felicidad, cansancio y habilidades,
//         títulos, el ojo de buey de la vista (a ojo)
//  96/97  13×13  cerrar (✗) y _P                        UIStaff (a ojo)
//  98/99  26×27  coger (mano en la puerta) y _P         UIStaff (a ojo)
// 100/101 23×27  péndulo (¿llamar o investigar?) y _P   UIStaff (?)
// 102     112×49 prioridades del bedel: limpiar, regar y reparar con sus barras
// 103–108 28×28  los tres botones del bedel y sus _P   UIStaff (a ojo)
// 109–112 101×36, 101×33, 101×47, 109×50  ventana del paciente (Req02V):
//         felicidad, la gráfica de salud (0–7), sed y calor, el ojo de buey
//         con la casa (a ojo)
// 113/114 13×13  cerrar (✗) y _P                        UIPatient (a ojo)
// 115/116 28×20  adivinar cura (libro y maletín) y _P   UIPatient (a ojo)
// 117/118 28×29  mandar a casa y _P                     UIPatient (a ojo)
// 119     40×35  el hospital con 1 2 3 (?)
// 120–125 12×13  botones 1, 2, 3 y sus _P (?)
// 126–128 107×38/34/42  ventana de la máquina (Req03V), naranja: cabecera con
//         ✗, resistencia (barra), llamar al bedel y sustituir (a ojo)
// 129/130 13×13  cerrar (✗) y _P                        UIMachine (a ojo)
// 131/132 32×32  llamar al bedel y _P                   UIMachine (a ojo)
// 133/134 32×32  sustituir (el reloj con chispas) y _P  UIMachine (a ojo)
// 135–137 103×18/9/17  diálogo de confirmar (Req04V), verde: arriba, tira y abajo
// 138/139 52×24  cancelar (✗) y _P                      UIConfirmDialog (a ojo)
// 140/141 52×24  aceptar (✓) y _P                       UIConfirmDialog (a ojo)
// 142–145 18/81/15/39×66  ventana de la cola (Req06V), gris, con la puerta
// 146/147 13×13  cerrar (✗) y _P                        UIQueue (a ojo)
// 148/149 11×11  «+» y _P; 150/151 «−» y _P             UIQueue (a ojo)
// 152–155 148×20/29/15/11  máquina de discos (Req13V), ocre: cabecera con ✗,
//         mandos, abajo con ✓, fila (a ojo)
// 156/157 14×14  cerrar (✗) y _P                        UIJukebox (a ojo)
// 158/159 13×13  ✓ y ✗ pequeños (?)
// 160/161 28×13  reproducir y _P; 162–169 15×13: anterior, siguiente, parar y
//         repetir con sus _P                            UIJukebox (a ojo)
// 170–175 2×3    puntas de barra en seis colores (?); 176 5×4 (?)
// 177–179 6×15, 12×11, 15×12  iconos pequeños (?)
// 180–185 99×34, 50×34 (×2), 99×20 (×2), 99×9  dos botones con imagen
//         (mostrador y casa) y una fila con el hospital, azules y _P (?)
// 406/407 28×20  frascos (medicina) y _P, lila           UIPatient (?)
// 408/409 13×15  salida (letrero) y _P                   (?)
// 410/411 88×11  fila azul verdosa y _P                  (?)
//
// Memoria: la hoja entera son 270.840 B (16 de cabecera, 3.296 de tabla y
// 267.528 de píxeles con las filas a 8 bytes); las piezas 1–78 (las cuatro
// ventanas de la demo) son 105.968 B de píxeles y 1–56 (sin contratar), 76.912.
// El conversor deja solo las 1–78 (PIEZAS_USADAS de tools/conv/conv.py; las
// demás, 0 × 0 con los mismos índices): 109.280 B el fichero.

#define TH_MUTTON01_PIEZAS(X)                                                                                         \
    X(COL_CABECERA, 1, 93, 30)                                                                                         \
    X(COL_TEXTO, 2, 93, 3)                                                                                             \
    X(COL_MEDIO, 3, 93, 11)                                                                                            \
    X(COL_CANCELAR, 4, 26, 27)                                                                                         \
    X(COL_CANCELAR_P, 5, 26, 27)                                                                                       \
    X(COL_COMPRAR, 6, 22, 27)                                                                                          \
    X(COL_COMPRAR_P, 7, 22, 27)                                                                                        \
    X(COL_COGER, 8, 22, 27)                                                                                            \
    X(COL_COGER_P, 9, 22, 27)                                                                                          \
    X(COL_CONFIRMAR, 10, 26, 27)                                                                                       \
    X(COL_CONFIRMAR_P, 11, 26, 27)                                                                                     \
    X(COL_CANCELAR_GRIS, 12, 26, 27)                                                                                   \
    X(COL_COMPRAR_GRIS, 13, 22, 27)                                                                                    \
    X(COL_COGER_GRIS, 14, 22, 27)                                                                                      \
    X(COL_CONFIRMAR_GRIS, 15, 26, 27)                                                                                  \
    X(COL_LISTA_CABECERA, 16, 93, 6)                                                                                   \
    X(COL_FILA, 17, 93, 17)                                                                                            \
    X(COL_FILA_P, 18, 93, 17)                                                                                          \
    X(COL_PIE, 19, 93, 11)                                                                                             \
    X(CON_DIAGNOSTICO, 20, 79, 23)                                                                                     \
    X(CON_DIAGNOSTICO_P, 21, 79, 23)                                                                                   \
    X(CON_TRATAMIENTO, 22, 79, 18)                                                                                     \
    X(CON_TRATAMIENTO_P, 23, 79, 18)                                                                                   \
    X(CON_CLINICAS, 24, 79, 18)                                                                                        \
    X(CON_CLINICAS_P, 25, 79, 18)                                                                                      \
    X(CON_INSTALACIONES, 26, 79, 22)                                                                                   \
    X(CON_INSTALACIONES_P, 27, 79, 22)                                                                                 \
    X(CON_REJILLA_ARRIBA, 28, 79, 19)                                                                                  \
    X(CON_REJILLA, 29, 79, 7)                                                                                          \
    X(CON_REJILLA_ABAJO, 30, 79, 19)                                                                                   \
    X(CON_LISTA_CABECERA, 31, 116, 19)                                                                                 \
    X(CON_FILA, 32, 116, 11)                                                                                           \
    X(CON_FILA_P, 33, 116, 11)                                                                                         \
    X(CON_CERRAR_FONDO, 34, 116, 42)                                                                                   \
    X(CON_CERRAR, 35, 108, 19)                                                                                         \
    X(CON_CERRAR_P, 36, 108, 19)                                                                                       \
    X(AMU_REJILLA_ARRIBA, 39, 79, 19)                                                                                  \
    X(AMU_REJILLA, 40, 79, 7)                                                                                          \
    X(AMU_REJILLA_ABAJO, 41, 79, 19)                                                                                   \
    X(AMU_PRECIO_ARRIBA, 42, 79, 22)                                                                                   \
    X(AMU_PRECIO, 43, 79, 15)                                                                                          \
    X(AMU_TOTAL, 44, 79, 21)                                                                                           \
    X(AMU_CERRAR, 45, 79, 23)                                                                                          \
    X(AMU_CERRAR_P, 46, 79, 23)                                                                                        \
    X(AMU_LISTA_CABECERA, 47, 136, 19)                                                                                 \
    X(AMU_FILA, 48, 136, 11)                                                                                           \
    X(AMU_PIE, 50, 136, 19)                                                                                            \
    X(AMU_CONFIRMAR, 51, 136, 23)                                                                                      \
    X(AMU_CONFIRMAR_P, 52, 136, 23)                                                                                    \
    X(AMU_MENOS, 53, 9, 9)                                                                                             \
    X(AMU_MENOS_P, 54, 9, 9)                                                                                           \
    X(AMU_MAS, 55, 9, 9)                                                                                               \
    X(AMU_MAS_P, 56, 9, 9)                                                                                             \
    X(CTR_MEDICOS, 57, 29, 43)                                                                                         \
    X(CTR_MEDICOS_P, 58, 29, 43)                                                                                       \
    X(CTR_ENFERMERAS, 59, 29, 41)                                                                                      \
    X(CTR_ENFERMERAS_P, 60, 29, 41)                                                                                    \
    X(CTR_BEDELES, 61, 29, 41)                                                                                         \
    X(CTR_BEDELES_P, 62, 29, 41)                                                                                       \
    X(CTR_RECEPCIONISTAS, 63, 29, 42)                                                                                  \
    X(CTR_RECEPCIONISTAS_P, 64, 29, 42)                                                                                \
    X(CTR_CABECERA, 65, 106, 43)                                                                                       \
    X(CTR_HABILIDAD, 66, 106, 26)                                                                                      \
    X(CTR_CAJA, 67, 106, 23)                                                                                        \
    X(CTR_SUELDO, 68, 106, 16)                                                                                         \
    X(CTR_TEXTO, 69, 106, 3)                                                                                           \
    X(CTR_PIE, 70, 106, 8)                                                                                             \
    X(CTR_ANTERIOR, 71, 30, 21)                                                                                        \
    X(CTR_ANTERIOR_P, 72, 30, 21)                                                                                      \
    X(CTR_CANCELAR, 73, 17, 21)                                                                                        \
    X(CTR_CANCELAR_P, 74, 17, 21)                                                                                      \
    X(CTR_CONTRATAR, 75, 32, 21)                                                                                       \
    X(CTR_CONTRATAR_P, 76, 32, 21)                                                                                     \
    X(CTR_SIGUIENTE, 77, 30, 21)                                                                                       \
    X(CTR_SIGUIENTE_P, 78, 30, 21)

#define TH_MT_ENUM_(nombre, indice, ancho, alto) TH_MT_##nombre = indice,
enum { TH_MUTTON01_PIEZAS(TH_MT_ENUM_) TH_MT_ENTRADAS = 412 };
#undef TH_MT_ENUM_

#endif
