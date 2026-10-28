"""Hojas de sprites de TH (.TAB + .DAT) al formato TSH1 de la N64.

Formato TSH1 (big-endian), pensado para dibujar en CI8 con el RDP:
  0   'TSH1'
  4   u16 número de sprites
  6   u16 reservado (0)
  8   u32 desplazamiento de la tabla (16)
  12  u32 desplazamiento de los píxeles (múltiplo de 16)
  16  tabla: por sprite, u32 desplazamiento de sus píxeles (desde el inicio
      de los píxeles), u16 ancho, u16 alto. Ancho 0 = sprite vacío.
  ... píxeles: por sprite, alto filas de ROUND_UP(ancho, 8) bytes; el relleno
      de cada fila es el índice 255 (transparente). Cada sprite empieza en un
      múltiplo de 8, como pide LOAD_BLOCK.

El índice de cada sprite es el mismo que en el .TAB original, porque las
animaciones lo usan.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "datos"))
import graficos  # noqa: E402  (decodificador de la fase 2)

MAGIA = b"TSH1"
CABECERA = 16


def leer(tab, dat, compleja):
    """Lista de (ancho, alto, píxeles CI8 sin relleno) en el orden del .TAB."""
    sprites = []
    for k in range(0, len(tab) - 5, 6):
        off, w, h = struct.unpack_from("<IBB", tab, k)
        if not w or not h:
            sprites.append((0, 0, b""))
            continue
        sprites.append((w, h, graficos.decode(dat[off:], w, h, compleja)))
    return sprites


# Bloques de la hoja del mapa (MBLK-0 / VBLK-0) para las vistas giradas.
SOMBRA_PARED = 156  # la sombra de una pared oeste sobre la cara sur de la pared norte
SOMBRA_PARED_OESTE = 226  # generado: la misma sombra sobre la cara este de una pared oeste


def con_sombra_oeste(sprites, u):
    """La hoja de bloques del mapa con el bloque 226 al final: la sombra del
    156 sobre la cara de una pared oeste, para la vista con la cámara girada
    3 veces (motor/vista/girar.h), donde la cara sur de una pared norte del
    mundo se ve como pared oeste. El 156 ocupa las columnas u a 2u − 1 (la
    cara de la pared norte, de la esquina hacia la derecha, con el pie
    bajando media fila por columna); la cara de la pared oeste va de la
    columna 0 a la u − 1, con el pie subiendo media fila por columna y la
    esquina a la izquierda. Cada columna d de la sombra pasa de la u + d a la
    d y baja u/2 − d filas: la misma altura sobre el pie y la misma distancia
    a la esquina. u es 16 en DATAM y 32 en DATA."""
    if len(sprites) != SOMBRA_PARED_OESTE:
        raise ValueError(f"la hoja de bloques tiene {len(sprites)} sprites y se esperaban {SOMBRA_PARED_OESTE}")
    w, h, px = sprites[SOMBRA_PARED]
    if w != 2 * u:
        raise ValueError(f"el bloque {SOMBRA_PARED} mide {w} de ancho y se esperaba {2 * u}")
    alto = h + u // 2
    out = bytearray([255]) * (w * alto)
    for y in range(h):
        for x in range(w):
            c = px[y * w + x]
            if c == 255:
                continue
            if x < u:
                raise ValueError(f"el bloque {SOMBRA_PARED} tiene píxeles a la izquierda de la esquina")
            d = x - u
            out[(y + u // 2 - d) * w + d] = c
    return list(sprites) + [(w, alto, bytes(out))]


def paso(w):
    return (w + 7) // 8 * 8


def escribir(sprites):
    n = len(sprites)
    tabla = bytearray()
    pix = bytearray()
    for w, h, px in sprites:
        if not w:
            tabla += struct.pack(">IHH", 0, 0, 0)
            continue
        p = paso(w)
        tabla += struct.pack(">IHH", len(pix), w, h)
        for y in range(h):
            pix += px[y * w:(y + 1) * w] + b"\xff" * (p - w)
    ini_pix = (CABECERA + len(tabla) + 15) // 16 * 16
    cab = MAGIA + struct.pack(">HHII", n, 0, CABECERA, ini_pix)
    return bytes(cab + tabla + b"\0" * (ini_pix - CABECERA - len(tabla)) + pix)


def leer_tsh(datos):
    """Lector de TSH1 (para las comprobaciones): lista de (ancho, alto, píxeles)."""
    if datos[:4] != MAGIA:
        raise ValueError("no es TSH1")
    n, _, ini_tabla, ini_pix = struct.unpack_from(">HHII", datos, 4)
    out = []
    for i in range(n):
        off, w, h = struct.unpack_from(">IHH", datos, ini_tabla + 8 * i)
        if not w:
            out.append((0, 0, b""))
            continue
        p = paso(w)
        base = ini_pix + off
        filas = [datos[base + y * p:base + y * p + w] for y in range(h)]
        out.append((w, h, b"".join(filas)))
    return out


def es_compleja(nombre):
    return bool(graficos.COMPLEX.match(nombre))
