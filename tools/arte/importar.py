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
  DATAM (320 × 240). Hace falta una de las dos: si falta la baja, sale de la alta
  reducida a la mitad; si falta la alta, de la baja ampliada al doble.

Cada vista se convierte así, en cada resolución:
  1. la referencia es lo que el juego dibuja hoy en esa orientación: la animación
     de reposo de la orientación o, si no la hay, la de su espejo
     (Object:initOrientation de CorsixTH), con sus elementos opacos y sus sombras
     semitransparentes por separado;
  2. lo que tiene color de sombra: los píxeles más cerca del gris de la sombra de
     la referencia que de cualquier color opaco del objeto. Si el objeto usa ese
     mismo gris (sombra ambigua en el informe), ese color no cuenta como opaco;
  3. se coloca donde su silueta, sin lo que tiene color de sombra, casa mejor con
     la de la referencia (el desplazamiento con más intersección sobre unión);
  4. la sombra que el dibujo trae pintada (opaca) se quita: las manchas de color de
     sombra que tocan la huella de la sombra de la referencia, enteras, y lo que
     queda suelto junto a ellas (manchas de hasta MIGAS píxeles: el borde, en otro
     gris). Con la sombra ambigua, lo que cae sobre la silueta de la referencia se
     queda (puede ser del objeto). La sombra de verdad es la de la referencia,
     semitransparente. Si la referencia no tiene sombra semitransparente, no se
     quita nada: lo pintado se queda en los colores del objeto;
  5. el resto pasa a los colores de los sprites opacos del objeto;
  6. la animación nueva es un fotograma con las sombras de la referencia (ya sin
     espejo) y el sprite nuevo, que se añaden al final de la hoja de sprites y de
     las tablas de animación.

Uso como herramienta: python3 -I tools/arte/importar.py [--vistas dir] [--comprobar]
  Lee el manifiesto, TH_DATA_DIR (DATA y DATAM) y las reglas de CorsixTH
  (CORSIXTH_DIR, con lua5.4), y escribe en dir, si se da, una vista previa por vista
  nueva (la referencia de hoy junto a la nueva, en las dos resoluciones). conv.py y
  alta.py usan las mismas funciones al convertir. Necesita numpy y Pillow; sin
  manifiesto, el resto de herramientas no.
  --comprobar: la vuelta completa con el arte de TH, sin el arte nuevo. Cada
  orientación de cada objeto, con su sombra pintada en opaco, tiene que salir igual
  que el juego la dibuja (ver comprobar). Sale con 1 si alguna no sale igual.
"""
import json
import os
import sys

try:  # solo hacen falta si hay arte nuevo: conv.py y alta.py importan este módulo siempre
    import numpy as np
    from PIL import Image
except ImportError:
    np = Image = None

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
        if not (e.get("alta") or e.get("baja")):
            raise ValueError(f"entrada sin imagen (alta o baja) en {ruta}: {e}")
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
            # Como el motor dibuja los objetos: con las capas a 0, un elemento con id de capa no sale (la luz
            # del visor de rayos X, que se enciende durante la operación).
            if not w or cid:
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


MIGAS = 3  # píxeles: lo que queda suelto junto a la sombra pintada, hasta este tamaño, se quita con ella


def cercanos(rgb, colores):
    """Para cada píxel, la distancia RGB al cuadrado al más cercano de unos colores (filas RGB) y cuál es
    (el primero, si empatan). Color a color, para no gastar memoria con imágenes grandes."""
    mejor = np.full(rgb.shape[:2], np.iinfo(np.int64).max)
    cual = np.zeros(rgb.shape[:2], int)
    for i, c in enumerate(colores):
        d = ((rgb - c) ** 2).sum(2)
        menor = d < mejor
        mejor[menor], cual[menor] = d[menor], i
    return mejor, cual


def crecer(m):
    """La máscara m crecida un píxel, también en diagonal."""
    c = m.copy()
    c[1:] |= m[:-1]
    c[:-1] |= m[1:]
    out = c.copy()
    out[:, 1:] |= c[:, :-1]
    out[:, :-1] |= c[:, 1:]
    return out


def rellenar(semilla, mascara):
    """Las manchas de la máscara (conexas, también en diagonal) que tocan la semilla, enteras."""
    m = semilla & mascara
    while True:
        n = crecer(m) & mascara
        if (n == m).all():
            return m
        m = n


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
    ry, rx = np.where((opaco >= 0) | sombra)
    rh, rw = ry.max() + 1 - ry.min(), rx.max() + 1 - rx.min()
    if h > 2 * rh + 2 * buscar or w > 2 * rw + 2 * buscar:
        raise ValueError(f"el dibujo ({w} x {h}) es mucho mayor que lo que el juego dibuja hoy ({rw} x {rh}): "
                         f"¿está a la escala de DATA (alta, 640 x 480) o de DATAM (baja, 320 x 240)?")
    rgb = a[:, :, :3]
    # 2. Lo que tiene color de sombra: más cerca del gris de la sombra de la referencia que de cualquier color
    # opaco del objeto. Si el objeto usa ese mismo gris (sombra ambigua), ese color no cuenta aquí como opaco:
    # más abajo se protege lo que cae sobre la silueta de la referencia, y el informe lo avisa.
    rgb_sombra = {tuple(esc.rgb[i]) for i in grises}
    distintos = [i for i in usados if tuple(esc.rgb[i]) not in rgb_sombra] or usados
    ambigua = len(distintos) < len(usados)
    gris = alfa & (cercanos(rgb, esc.rgb[grises])[0] < cercanos(rgb, esc.rgb[distintos])[0]) if grises \
        else np.zeros_like(alfa)
    # 3. Registro: la silueta sin lo que tiene color de sombra (la sombra pintada tiraría del dibujo hacia
    # ella), desde su pie en el de la referencia y con los centros alineados.
    silueta = alfa & ~gris if (alfa & ~gris).any() else alfa
    ref_m = opaco >= 0
    ry, rx = np.where(ref_m)
    sy, sx = np.where(silueta)
    base_y = ry.max() - sy.max()
    base_x = (rx.min() + rx.max() + 1) // 2 - (sx.min() + sx.max() + 1) // 2
    n_sil, n_ref = int(silueta.sum()), int(ref_m.sum())
    mejor = None
    for dy in range(-buscar, buscar + 1):
        for dx in range(-buscar, buscar + 1):
            y0, x0 = base_y + dy, base_x + dx
            if y0 < 0 or x0 < 0 or y0 + h > lado or x0 + w > lado:
                continue
            inter = int((ref_m[y0:y0 + h, x0:x0 + w] & silueta).sum())
            iou = inter / (n_sil + n_ref - inter)
            if mejor is None or iou > mejor[0]:
                mejor = (iou, y0, x0)
    if mejor is None:
        raise ValueError(f"el dibujo ({w} x {h}) no cabe en el lienzo de {lado}")
    iou, y0, x0 = mejor
    # 4. La sombra pintada: las manchas de color de sombra que tocan la huella de la sombra de la referencia,
    # enteras (la pintada suele salirse un poco de la huella). Con la sombra ambigua, sin lo que cae sobre la
    # silueta de la referencia, que puede ser del objeto.
    huella = sombra[y0:y0 + h, x0:x0 + w]
    candidata = gris & ~ref_m[y0:y0 + h, x0:x0 + w] if ambigua else gris
    pintada = rellenar(huella, candidata)
    # Y las migas: lo que queda suelto junto a la sombra pintada, en manchas de hasta MIGAS píxeles, es su
    # borde en otro gris (el dibujo de la IA la suaviza).
    resto = alfa & ~pintada
    pendiente = resto & crecer(pintada)
    migas = np.zeros_like(alfa)
    while pendiente.any():
        ys, xs = np.where(pendiente)
        semilla = np.zeros_like(alfa)
        semilla[ys[0], xs[0]] = True
        mancha = rellenar(semilla, resto)
        if mancha.sum() <= MIGAS:
            migas |= mancha
        pendiente &= ~mancha
    # 5. Lo que queda, en los colores opacos.
    idx_op = cercanos(rgb, esc.rgb[usados])[1]
    ci8 = np.where(resto & ~migas, np.array(usados, np.uint8)[idx_op], TRANSPARENTE).astype(np.uint8)
    # Recorte final a lo que queda.
    ys, xs = np.where(ci8 != TRANSPARENTE)
    if not len(ys):
        raise ValueError("no queda nada del dibujo tras quitar la sombra pintada")
    ci8 = ci8[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    y0, x0 = y0 + ys.min(), x0 + xs.min()
    sprite = (ci8.shape[1], ci8.shape[0], ci8.tobytes())
    nuevo = (None, esc.crudo(x0 - ox, ORIGEN_X), esc.crudo(y0 - oy, ORIGEN_Y), 0, 0, 0)
    sombras = [e for e in ref if e[4] & ALFA]
    informe = {"silueta": round(float(iou), 3), "sprite": [sprite[0], sprite[1]], "colores": len(usados),
               "sombra_pintada_quitada": int(pintada.sum()), "fuera_de_la_huella": int((pintada & ~huella).sum()),
               "migas": int(migas.sum()), "sombra_ambigua": ambigua, "sombras": len(sombras),
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
    """La imagen de una resolución. Si falta, sale de la otra: la baja, de la alta reducida a la mitad; la
    alta, de la baja ampliada al doble (sin suavizar)."""
    if entrada.get(res):
        return Image.open(entrada[res]).convert("RGBA")
    if res == "baja" and entrada.get("alta"):
        im = Image.open(entrada["alta"]).convert("RGBA")
        return im.resize((max(1, round(im.width / 2)), max(1, round(im.height / 2))), Image.LANCZOS)
    if res == "alta" and entrada.get("baja"):
        im = Image.open(entrada["baja"]).convert("RGBA")
        return im.resize((im.width * 2, im.height * 2), Image.NEAREST)
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
        if not os.path.isfile(os.path.join(lua, "base_config.lua")):
            raise SystemExit(f"el arte nuevo necesita el código de CorsixTH en {lua} (CORSIXTH_DIR) y lua5.4: "
                             "de ahí sale qué animación dibuja hoy cada orientación")
        _CACHE["doc"], _ = reglas.extraer(lua, d.ruta("LEVELS"))
    return _CACHE["doc"]


def necesita_numpy():
    if np is None:
        raise SystemExit("el arte nuevo necesita numpy y Pillow (python3-numpy y python3-pil)")


def escala(d, res):
    """Las tablas de animación, la hoja de sprites y la paleta del juego en una resolución ('alta': DATA;
    'baja': DATAM)."""
    import anim
    import hojas
    carpeta, p, u = {"alta": ("DATA", "V", 32), "baja": ("DATAM", "M", 16)}[res]
    tablas = anim.leer(*[d.leer(carpeta, f"{p}{t}-1.ANI") for t in ("START", "FRA", "LIST", "ELE")])
    sprites = hojas.leer(d.leer(carpeta, f"{p}SPR-0.TAB"), d.leer(carpeta, f"{p}SPR-0.DAT"),
                         hojas.es_compleja(f"{p}SPR-0.DAT"))
    return Escala(tablas, sprites, u, rgb_paleta(d.leer("DATA", "MPALETTE.DAT")))


TMEM_TEXELES = 2048  # motor/platform/n64/mapa_n64.c: más grande, va por rdpq_tex_blit, que no recorta


def partida(doc, objeto, orientacion):
    """¿Se dibuja esa orientación partida en columnas? (render_attach_position con columnas)."""
    spec = ((doc["objetos"][objeto].get("orientations") or {}).get(orientacion)) or {}
    rap = spec.get("render_attach_position")
    return isinstance(rap, list) and bool(rap) and isinstance(rap[0], dict)


def aplicar(d, res, entradas=None):
    """El arte nuevo del manifiesto en una resolución ('alta': DATA; 'baja': DATAM), sobre los datos del juego
    d (conv.Datos). Devuelve (esc, nuevas): esc con las tablas y la hoja de sprites ya ampliadas, y nuevas =
    [{objeto, orientacion, animacion, sprite, informe}], en el orden del manifiesto. Sin arte, (None, []):
    no lee nada ni necesita numpy, Pillow ni CorsixTH.
    Los índices de animación y de sprite nuevos son los mismos en las dos resoluciones: las tablas de DATA y
    DATAM tienen las mismas animaciones y la misma hoja de sprites (en número). Con arte, hace falta el código
    de CorsixTH ($CORSIXTH_DIR) y lua5.4, para saber qué dibuja hoy cada orientación (idle_animations)."""
    entradas = manifiesto() if entradas is None else entradas
    if not entradas:
        return None, []
    necesita_numpy()
    vistas = [(e["objeto"], e["orientacion"]) for e in entradas]
    repetidas = sorted({v for v in vistas if vistas.count(v) > 1})
    if repetidas:
        raise ValueError(f"vistas repetidas en el manifiesto: {repetidas}")
    clave = (res, json.dumps(entradas, sort_keys=True))
    if clave in _CACHE:
        return _CACHE[clave]
    esc = escala(d, res)
    nuevas = []
    doc = _doc(d)
    idle = animaciones_de_reposo(doc)
    for e in entradas:
        if e["objeto"] not in idle:
            raise ValueError(f"objeto desconocido en el manifiesto: {e['objeto']}")
        id_anims = idle[e["objeto"]]
        ref, esp = animacion_de_referencia(id_anims, e["orientacion"])
        try:
            sprite, elementos, inf = convertir(esc, imagen_de(e, res),
                                               sorted({a for a in id_anims.values() if a is not None}), ref, esp)
        except ValueError as ex:
            raise ValueError(f"{e['objeto']} {e['orientacion']} ({res}): {ex}") from ex
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


def comprobar(d):
    """La vuelta completa con el arte de TH, sin arte nuevo: cada orientación que el juego dibuja hoy (la
    suya o la de su espejo), con su sombra pintada en opaco, como si fuera el dibujo nuevo de esa misma
    orientación. Tiene que salir el mismo sprite en el mismo sitio, sin la sombra pintada. Devuelve
    [(res, objeto, orientación, informe, distintos)]: distintos = píxeles que no salen iguales (0 = la
    vuelta es exacta; -1 = el sprite sale de otro tamaño o en otro sitio)."""
    necesita_numpy()
    idle = animaciones_de_reposo(_doc(d))
    out = []
    for res in ("alta", "baja"):
        esc = escala(d, res)
        for objeto in sorted(idle):
            anims = sorted({a for a in idle[objeto].values() if a is not None})
            for ori in ORIENTACIONES:
                try:
                    a, esp = animacion_de_referencia(idle[objeto], ori)
                except ValueError:
                    continue
                ref = [esc.sin_espejo(e, esp) for e in esc.elementos(a)]
                opaco, _ = esc.componer(ref)
                if not (opaco >= 0).any():
                    continue
                # La sombra pintada: los sprites de sombra con sus colores, opacos, debajo del objeto.
                pintura, _ = esc.componer([(s, x, y, c, fl & ~ALFA, cid) for s, x, y, c, fl, cid in ref if fl & ALFA])
                todo = np.where(opaco >= 0, opaco, pintura)
                img = np.zeros(todo.shape + (4,), np.uint8)
                img[todo >= 0, :3] = esc.rgb[todo[todo >= 0]]
                img[todo >= 0, 3] = 255
                sprite, elementos, inf = convertir(esc, Image.fromarray(img, "RGBA"), anims, a, esp)
                ys, xs = np.where(opaco >= 0)
                esperado = np.where(opaco >= 0, opaco, TRANSPARENTE)[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
                w, h, px = sprite
                sitio = (esc.px(elementos[-1][1], ORIGEN_X), esc.px(elementos[-1][2], ORIGEN_Y))
                if esperado.shape == (h, w) and sitio == (xs.min() - 512, ys.min() - 640):
                    distintos = int((np.frombuffer(px, np.uint8).reshape(h, w) != esperado).sum())
                else:
                    distintos = -1
                out.append((res, objeto, ori, inf, distintos))
    return out


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
    ap.add_argument("--comprobar", action="store_true",
                    help="la vuelta completa con el arte de TH en todos los objetos (no usa el arte nuevo)")
    a = ap.parse_args()
    d = conv.Datos(os.environ.get("TH_DATA_DIR", ""))
    if a.comprobar:
        filas = comprobar(d)
        for res in ("alta", "baja"):
            f = [x for x in filas if x[0] == res]
            mal = [x for x in f if x[4]]
            print(f"{res}: {len(f) - len(mal)} de {len(f)} orientaciones salen iguales "
                  f"({sum(1 for x in f if x[3]['sombras'])} con sombra, "
                  f"{sum(1 for x in f if x[3]['sombra_ambigua'])} con la sombra ambigua; "
                  f"sombra pintada quitada: {sum(x[3]['sombra_pintada_quitada'] for x in f)} píxeles)")
            for x in mal:
                print(f"  {x[1]} {x[2]}: {'otro tamaño o sitio' if x[4] < 0 else f'{x[4]} píxeles distintos'}; "
                      f"{json.dumps(x[3], ensure_ascii=False)}")
        sys.exit(1 if any(x[4] for x in filas) else 0)
    if not manifiesto():
        sys.exit(f"no hay arte nuevo en {dir_arte()} (falta arte.json)")
    for res in ("alta", "baja"):
        try:
            esc, nuevas = aplicar(d, res)
        except ValueError as ex:
            sys.exit(f"ERROR: {ex}")
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
