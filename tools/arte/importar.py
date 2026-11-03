#!/usr/bin/env python3
"""El importador de arte nuevo: vistas de objetos que TH no dibujó (o dibujó en
espejo), hechas a mano o con IA, convertidas en sprites y animaciones del juego.

El arte nuevo no va al repositorio: se deriva del arte de TH. Vive en
TH64_ARTE_DIR (por defecto $TH64_WORK/arte), con un manifiesto arte.json:

    {"objetos": [
      {"objeto": "desk", "orientacion": "south",
       "alta": "desk/sur_alta.png", "baja": "desk/sur_baja.png",
       "origen": "PixelLab, rotación de 8 vistas (v3)"}
    ]}

- objeto: el id del objeto en CorsixTH (objects/*.lua);
- orientacion: north, east, south o west;
- alta y baja: PNG con fondo transparente, a la escala de DATA (640 × 480) y de
  DATAM (320 × 240). Si falta la baja, sale de la alta reducida a la mitad.

Cada vista se convierte así, en cada resolución:
  1. la referencia es lo que el juego dibuja hoy en esa orientación: la animación
     de reposo de la orientación o, si no la hay, la de su espejo
     (Object:initOrientation de CorsixTH), con sus elementos opacos y sus sombras
     semitransparentes por separado;
  2. el dibujo nuevo se pasa a los colores de los sprites opacos del objeto;
  3. la sombra que el dibujo trae pintada (opaca) se quita: los píxeles dentro de
     la huella de la sombra de la referencia cuyo color más cercano es el de la
     sombra. La sombra de verdad es la de la referencia, semitransparente;
  4. se coloca donde su silueta casa mejor con la de la referencia (el
     desplazamiento con más intersección sobre unión);
  5. la animación nueva es un fotograma con las sombras de la referencia (ya sin
     espejo) y el sprite nuevo, que se añaden al final de la hoja de sprites y de
     las tablas de animación.

Uso como herramienta: python3 -I tools/arte/importar.py [--vistas dir]
  Lee el manifiesto y TH_DATA_DIR, y escribe en dir, si se da, una vista previa por
  vista nueva (la referencia de hoy junto a la nueva, en las dos resoluciones).
  conv.py y alta.py usan las mismas funciones al convertir.
"""
import json
import os
import sys

import numpy as np
from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, "..", "conv"))
import paleta  # noqa: E402

ORIENTACIONES = ("north", "east", "south", "west")
# Object:initOrientation: si una orientación no tiene animación, la de su espejo.
ESPEJO = {"north": "west", "west": "north", "east": "south", "south": "east"}
ORIGEN_X, ORIGEN_Y = 141, 186  # origen de los desplazamientos crudos de los elementos
FLIP_H, ALFA = 1, 0xC          # banderas de elemento: espejo y semitransparencia (thdf_alpha_50/75)
TRANSPARENTE = 255


def dir_arte():
    w = os.environ.get("TH64_WORK", os.path.expanduser("~/th64-work"))
    return os.environ.get("TH64_ARTE_DIR", os.path.join(w, "arte"))


def manifiesto(directorio=None):
    """Las entradas del manifiesto, o [] si no hay arte nuevo."""
    directorio = directorio or dir_arte()
    ruta = os.path.join(directorio, "arte.json")
    if not os.path.isfile(ruta):
        return []
    doc = json.load(open(ruta))
    entradas = []
    for e in doc.get("objetos", []):
        if e.get("orientacion") not in ORIENTACIONES:
            raise ValueError(f"orientación desconocida en {ruta}: {e}")
        e = dict(e)
        for res in ("alta", "baja"):
            if e.get(res):
                e[res] = os.path.join(directorio, e[res])
        entradas.append(e)
    return entradas


def animaciones_de_reposo(doc):
    """{objeto: {orientación: animación}} de las reglas extraídas de CorsixTH (reglas.extraer)."""
    return {i: dict(o.get("idle_animations") or {}) for i, o in doc["objetos"].items()}


def animacion_de_referencia(idle, orientacion):
    """(animación, espejo) que el juego dibuja hoy: la de la orientación o la de su espejo."""
    if idle.get(orientacion) is not None:
        return idle[orientacion], False
    if idle.get(ESPEJO[orientacion]) is not None:
        return idle[ESPEJO[orientacion]], True
    raise ValueError(f"sin animación de reposo para {orientacion} ni para su espejo")


class Escala:
    """Una resolución: tablas de animación (anim.leer), sprites (hojas.leer), unidad y paleta RGB."""

    def __init__(self, tablas, sprites, unidad, rgb):
        self.prim, self.fot, self.lista, self.ele = (list(t) for t in tablas)
        self.sprites = list(sprites)
        self.u = unidad
        self.rgb = np.array(rgb, int)

    # Desplazamiento en píxeles de un elemento, como vista.c: ((x - 141) * u / 32), truncando hacia 0.
    def px(self, crudo, origen):
        v = (crudo - origen) * self.u
        return int(v / 32) if v >= 0 else -int(-v / 32)

    def crudo(self, px, origen):
        c = origen + px * 32 // self.u
        if not 0 <= c <= 255 or self.px(c, origen) != px:
            raise ValueError(f"desplazamiento {px} fuera de lo que cabe en un elemento (crudo {c})")
        return c

    def elementos(self, a):
        i = self.fot[self.prim[a]][0]
        out = []
        while self.lista[i] != 0xFFFF:
            out.append(self.ele[self.lista[i]])
            i += 1
        return out

    def sin_espejo(self, el, espejo):
        """El elemento como se dibuja con la entidad en espejo (vista.c: px = x - ex - w y se invierte su
        espejo), pero escrito sin espejo de entidad."""
        s, x, y, capa, fl, cid = el
        if not espejo:
            return el
        w = self.sprites[s][0]
        return (s, self.crudo(-self.px(x, ORIGEN_X) - w, ORIGEN_X), y, capa, fl ^ FLIP_H, cid)

    def componer(self, elementos, lado=1024, ox=512, oy=640):
        """Índices opacos (-1 = nada) y máscara de sombra, con el vértice norte de la casilla en (ox, oy)."""
        opaco = np.full((lado, lado), -1, np.int16)
        sombra = np.zeros((lado, lado), bool)
        for s, x, y, capa, fl, cid in elementos:
            w, h, px = self.sprites[s]
            if not w:
                continue
            m = np.frombuffer(px, np.uint8).reshape(h, w).astype(np.int16)
            if fl & FLIP_H:
                m = m[:, ::-1]
            x0, y0 = ox + self.px(x, ORIGEN_X), oy + self.px(y, ORIGEN_Y)
            k = m != TRANSPARENTE
            if fl & ALFA:
                sombra[y0:y0 + h, x0:x0 + w] |= k
            else:
                opaco[y0:y0 + h, x0:x0 + w][k] = m[k]
        return opaco, sombra


def colores_opacos(esc, anims):
    """Índices de paleta de los sprites opacos de unas animaciones."""
    usados = set()
    for a in anims:
        for s, x, y, capa, fl, cid in esc.elementos(a):
            if fl & ALFA:
                continue
            q = np.frombuffer(esc.sprites[s][2], np.uint8)
            usados |= set(np.unique(q[q != TRANSPARENTE]).tolist())
    return sorted(usados)


def colores_sombra(esc, elementos):
    usados = set()
    for s, x, y, capa, fl, cid in elementos:
        if fl & ALFA:
            q = np.frombuffer(esc.sprites[s][2], np.uint8)
            usados |= set(np.unique(q[q != TRANSPARENTE]).tolist())
    return sorted(usados)


def convertir(esc, imagen, anims_objeto, anim_ref, espejo_ref, buscar=12, lado=1024, ox=512, oy=640):
    """Una vista nueva en una resolución. imagen: PIL RGBA. Devuelve (sprite, elementos, informe):
    sprite = (ancho, alto, píxeles CI8 sin relleno) y elementos = los del fotograma nuevo, sin el índice
    del sprite nuevo (None, que pone anadir)."""
    ref = [esc.sin_espejo(e, espejo_ref) for e in esc.elementos(anim_ref)]
    opaco, sombra = esc.componer(ref, lado, ox, oy)
    usados = colores_opacos(esc, anims_objeto)
    grises = colores_sombra(esc, ref)
    a = np.asarray(imagen.convert("RGBA")).astype(int)
    alfa = a[:, :, 3] > 127
    ys, xs = np.where(alfa)
    if not len(ys):
        raise ValueError("la imagen está vacía")
    a, alfa = a[ys.min():ys.max() + 1, xs.min():xs.max() + 1], alfa[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    h, w = alfa.shape
    # 2. Colores: los opacos del objeto, más los de la sombra para reconocer la pintada.
    cand = np.array([esc.rgb[i] for i in usados + grises])
    idx = np.argmin(((a[:, :, None, :3] - cand[None, None]) ** 2).sum(3), 2)
    # 4. Registro: con la silueta sin la sombra pintada que caiga fuera de lo opaco de la referencia.
    ref_m = opaco >= 0
    ry, rx = np.where(ref_m)
    base_y, base_x = ry.max() + 1 - h, (rx.min() + rx.max() + 1 - w) // 2  # pie y centro de la referencia
    mejor = None
    for dy in range(-buscar, buscar + 1):
        for dx in range(-buscar, buscar + 1):
            y0, x0 = base_y + dy, base_x + dx
            m = np.zeros_like(ref_m)
            m[y0:y0 + h, x0:x0 + w] = alfa
            union = (m | ref_m).sum()
            iou = (m & ref_m).sum() / union if union else 0
            if mejor is None or iou > mejor[0]:
                mejor = (iou, y0, x0)
    iou, y0, x0 = mejor
    # 3. La sombra pintada: dentro de la huella de la sombra de la referencia y de color de sombra.
    en_sombra = sombra[y0:y0 + h, x0:x0 + w]
    pintada = alfa & en_sombra & (idx >= len(usados))
    # Lo que queda, en los colores opacos.
    idx_op = np.argmin(((a[:, :, None, :3] - np.array([esc.rgb[i] for i in usados])[None, None]) ** 2).sum(3), 2)
    ci8 = np.where(alfa & ~pintada, np.array(usados, np.uint8)[idx_op], TRANSPARENTE).astype(np.uint8)
    # Recorte final a lo que queda.
    ys, xs = np.where(ci8 != TRANSPARENTE)
    ci8 = ci8[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    y0, x0 = y0 + ys.min(), x0 + xs.min()
    sprite = (ci8.shape[1], ci8.shape[0], ci8.tobytes())
    nuevo = (None, esc.crudo(x0 - ox, ORIGEN_X), esc.crudo(y0 - oy, ORIGEN_Y), 0, 0, 0)
    sombras = [e for e in ref if e[4] & ALFA]
    informe = {"silueta": round(float(iou), 3), "sprite": [sprite[0], sprite[1]], "colores": len(usados),
               "sombra_pintada_quitada": int(pintada.sum()), "sombras": len(sombras),
               "referencia": {"animacion": anim_ref, "espejo": espejo_ref}}
    return sprite, sombras + [nuevo], informe


def anadir(esc, sprite, elementos):
    """Añade el sprite y una animación de un fotograma al final de las tablas. Devuelve (animación, sprite)."""
    n_spr = len(esc.sprites)
    esc.sprites.append(sprite)
    li = len(esc.lista)
    for el in elementos:
        el = (n_spr,) + tuple(el[1:]) if el[0] is None else el
        esc.lista.append(len(esc.ele))
        esc.ele.append(el)
    esc.lista.append(0xFFFF)
    f = len(esc.fot)
    esc.fot.append((li, f, 0, 0))  # se repite a sí mismo: una animación quieta
    esc.prim.append(f)
    if len(esc.prim) > 0xFFFF or len(esc.sprites) > 0xFFFF:
        raise ValueError("demasiadas animaciones o sprites para 16 bits")
    return len(esc.prim) - 1, n_spr


def imagen_de(entrada, res):
    """La imagen de una resolución; la baja, si falta, la alta reducida a la mitad."""
    if entrada.get(res):
        return Image.open(entrada[res]).convert("RGBA")
    if res == "baja" and entrada.get("alta"):
        im = Image.open(entrada["alta"]).convert("RGBA")
        return im.resize((max(1, round(im.width / 2)), max(1, round(im.height / 2))), Image.LANCZOS)
    raise ValueError(f"{entrada['objeto']} {entrada['orientacion']}: falta la imagen {res}")


def rgb_paleta(mpalette):
    return [tuple(paleta.a_8bit(c) for c in col) for col in paleta.colores_6bit(mpalette)]


def vista_previa(esc, anim_a, espejo_a, anim_b, ruta, k=4):
    """La referencia de hoy (anim_a con su espejo) junto a la nueva (anim_b), sobre gris, con la sombra al 50 %."""
    imgs = []
    for a, esp in ((anim_a, espejo_a), (anim_b, False)):
        op, so = esc.componer([esc.sin_espejo(e, esp) for e in esc.elementos(a)])
        im = np.full(op.shape + (3,), 150.0)
        im[so] *= 0.5
        im[op >= 0] = esc.rgb[op[op >= 0]]
        imgs.append((im, (op >= 0) | so))
    m = imgs[0][1] | imgs[1][1]
    ys, xs = np.where(m)
    c = (slice(ys.min() - 4, ys.max() + 5), slice(xs.min() - 4, xs.max() + 5))
    sep = np.full((imgs[0][0][c].shape[0], 6, 3), 60.0)
    out = np.concatenate([imgs[0][0][c], sep, imgs[1][0][c]], 1).astype(np.uint8)
    Image.fromarray(out).resize((out.shape[1] * k, out.shape[0] * k), Image.NEAREST).save(ruta)


_CACHE = {}


def _doc(d):
    """Las reglas extraídas de CorsixTH (una vez por proceso)."""
    if "doc" not in _CACHE:
        import reglas
        lua = os.path.join(os.environ.get("CORSIXTH_DIR", ""), "CorsixTH", "Lua")
        _CACHE["doc"], _ = reglas.extraer(lua, d.ruta("LEVELS"))
    return _CACHE["doc"]


TMEM_TEXELES = 2048  # motor/platform/n64/mapa_n64.c: más grande, va por rdpq_tex_blit, que no recorta


def partida(doc, objeto, orientacion):
    """¿Se dibuja esa orientación partida en columnas? (render_attach_position con columnas)."""
    spec = ((doc["objetos"][objeto].get("orientations") or {}).get(orientacion)) or {}
    rap = spec.get("render_attach_position")
    return isinstance(rap, list) and bool(rap) and isinstance(rap[0], dict)


def aplicar(d, res, entradas=None):
    """El arte nuevo del manifiesto en una resolución ('alta': DATA; 'baja': DATAM), sobre los datos del juego
    d (conv.Datos). Devuelve (esc, nuevas): esc con las tablas y la hoja de sprites ya ampliadas, y nuevas =
    [{objeto, orientacion, animacion, sprite, informe}], en el orden del manifiesto. Sin arte, nuevas = [].
    Los índices de animación y de sprite nuevos son los mismos en las dos resoluciones: las tablas de DATA y
    DATAM tienen las mismas animaciones y la misma hoja de sprites (en número)."""
    import anim
    import hojas
    entradas = manifiesto() if entradas is None else entradas
    clave = (res, json.dumps(entradas, sort_keys=True))
    if clave in _CACHE:
        return _CACHE[clave]
    carpeta, p, u = {"alta": ("DATA", "V", 32), "baja": ("DATAM", "M", 16)}[res]
    tablas = anim.leer(*[d.leer(carpeta, f"{p}{t}-1.ANI") for t in ("START", "FRA", "LIST", "ELE")])
    sprites = hojas.leer(d.leer(carpeta, f"{p}SPR-0.TAB"), d.leer(carpeta, f"{p}SPR-0.DAT"),
                         hojas.es_compleja(f"{p}SPR-0.DAT"))
    esc = Escala(tablas, sprites, u, rgb_paleta(d.leer("DATA", "MPALETTE.DAT")))
    nuevas = []
    doc = _doc(d) if entradas else {"objetos": {}}
    idle = animaciones_de_reposo(doc)
    for e in entradas:
        if e["objeto"] not in idle:
            raise ValueError(f"objeto desconocido en el manifiesto: {e['objeto']}")
        id_anims = idle[e["objeto"]]
        ref, esp = animacion_de_referencia(id_anims, e["orientacion"])
        sprite, elementos, inf = convertir(esc, imagen_de(e, res), sorted(set(id_anims.values())), ref, esp)
        # Un objeto partido en columnas se dibuja recortado; en la N64, un sprite que no cabe en la TMEM
        # va por rdpq_tex_blit, que no recorta, y saldría entero en cada columna.
        if partida(doc, e["objeto"], e["orientacion"]) and res == "baja" and \
                (sprite[0] + 7) // 8 * 8 * sprite[1] > TMEM_TEXELES:
            raise ValueError(f"{e['objeto']} {e['orientacion']}: se dibuja partido en columnas y su sprite "
                             f"({sprite[0]} x {sprite[1]}) no cabe en la TMEM de la N64; hay que partirlo")
        nueva, n_spr = anadir(esc, sprite, elementos)
        nuevas.append({"objeto": e["objeto"], "orientacion": e["orientacion"], "animacion": nueva,
                       "sprite": n_spr, "informe": inf})
    _CACHE[clave] = (esc, nuevas)
    return esc, nuevas


def por_objeto(nuevas):
    """{objeto: {orientación: animación}}: lo que va en las reglas, en la clave «arte» de cada objeto."""
    out = {}
    for n in nuevas:
        out.setdefault(n["objeto"], {})[n["orientacion"]] = n["animacion"]
    return out


def main():
    import argparse
    import conv
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--vistas", help="directorio para las vistas previas")
    a = ap.parse_args()
    if not manifiesto():
        sys.exit(f"no hay arte nuevo en {dir_arte()} (falta arte.json)")
    d = conv.Datos(os.environ.get("TH_DATA_DIR", ""))
    for res in ("alta", "baja"):
        esc, nuevas = aplicar(d, res)
        for n in nuevas:
            inf = n["informe"]
            print(f"{res}: {n['objeto']} {n['orientacion']} -> animación {n['animacion']}, sprite {n['sprite']}: "
                  f"{json.dumps(inf, ensure_ascii=False)}")
            if a.vistas:
                os.makedirs(a.vistas, exist_ok=True)
                ref = inf["referencia"]
                vista_previa(esc, ref["animacion"], ref["espejo"], n["animacion"],
                             os.path.join(a.vistas, f"{n['objeto']}_{n['orientacion']}_{res}.png"))


if __name__ == "__main__":
    main()
