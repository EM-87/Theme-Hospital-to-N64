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
