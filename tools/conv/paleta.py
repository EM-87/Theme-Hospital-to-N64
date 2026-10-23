"""Paletas para la N64: MPALETTE.DAT de TH a RGBA5551, con sus variantes.

Cada paleta son 256 entradas RGBA5551 big-endian (512 B), lista para subirla
a la TLUT con rdpq_tex_upload_tlut. La entrada 255 es transparente (alfa 0),
como en CorsixTH (graphics.lua: _loadPalette con transparent_255).

Variantes (las mismas que usa CorsixTH):
  0  normal
  1  fantasma gris: cada color se cambia por el de la paleta más cercano a su
     gris (graphics.lua: makeGreyscaleGhost); al colocar objetos y personal
  2  fantasma rojo: tabla 6 de QDATA/GHOST1.DAT (dialogs/edit_room.lua), para
     paredes, puertas y ventanas en una posición no válida
"""

VARIANTES = ("normal", "fantasma_gris", "fantasma_rojo")


def colores_6bit(datos):
    """256 tuplas (r, g, b) con los valores de 6 bits del fichero .DAT/.PAL."""
    if len(datos) < 768:
        raise ValueError("paleta de menos de 768 bytes")
    return [tuple(datos[3 * i + k] & 0x3F for k in range(3)) for i in range(256)]


def a_8bit(c):
    # th_gfx_sdl.cpp: convert_6bit_to_8bit_colour_component (redondeo de lround)
    return int(c * 255 / 63 + 0.5)


def rgba5551(r8, g8, b8, a):
    return ((r8 >> 3) << 11) | ((g8 >> 3) << 6) | ((b8 >> 3) << 1) | (1 if a else 0)


def remapeo_gris(col6):
    """graphics.lua: makeGreyscaleGhost, con la misma aritmética y desempates."""
    remap = []
    for r, g, b in col6:
        gris = r * 0.299 + g * 0.587 + b * 0.114
        mejor, dist = 0, 100000
        for j, (r2, g2, b2) in enumerate(col6):
            d = (r2 - gris) ** 2 + (g2 - gris) ** 2 + (b2 - gris) ** 2
            if d < dist:
                dist, mejor = d, j
        remap.append(mejor)
    return remap


def tabla_tlut(col6, remap=None):
    """256 colores RGBA5551 (lista de enteros); remap[i] = índice de color a usar."""
    out = []
    for i in range(256):
        if i == 255:
            out.append(0)  # transparente
            continue
        r, g, b = col6[remap[i] if remap else i]
        out.append(rgba5551(a_8bit(r), a_8bit(g), a_8bit(b), True))
    return out


def variantes(mpalette, ghost1):
    """Las tres paletas de VARIANTES, a partir de MPALETTE.DAT y GHOST1.DAT."""
    col6 = colores_6bit(mpalette)
    rojo = list(ghost1[6 * 256:7 * 256])
    if len(rojo) != 256:
        raise ValueError("GHOST1.DAT no tiene la tabla 6")
    return [tabla_tlut(col6), tabla_tlut(col6, remapeo_gris(col6)), tabla_tlut(col6, rojo)]


def colores_rgb8(mpalette):
    """Para las vistas previas en PNG: 256 tuplas (r, g, b) de 8 bits."""
    return [tuple(a_8bit(c) for c in rgb) for rgb in colores_6bit(mpalette)]
