"""Tablas de animación de TH (START/FRA/LIST/ELE) al formato TAN1 de la N64.

Lectura como animation_manager::load_from_th_file (CorsixTH th_gfx.cpp):
  START: 4 B por animación; u16 LE primer fotograma (si pasa del número de
         fotogramas, 0).
  FRA:   10 B por fotograma; u32 LE índice en LIST, 2 B (ancho y alto, sin
         usar), u8 sonido, u8 flags, u16 LE siguiente fotograma (fuera de
         rango, 0).
  LIST:  u16 LE por entrada, índice de elemento; uno fuera de rango termina la
         lista del fotograma (0xFFFF). Se añade un 0xFFFF final.
  ELE:   6 B por elemento; u16 LE posición en el .TAB (/6 = sprite), u8 x,
         u8 y, u8 capa (nibble alto) y flags (nibble bajo), u8 id de capa.

Formato TAN1 (big-endian):
  0   'TAN1'
  4   u16 animaciones, u16 0
  8   u32 fotogramas, u32 entradas de lista, u32 elementos
  20  u32 desplazamiento de cada tabla: animaciones, fotogramas, lista, elementos
  36  animaciones: u16 primer fotograma, u16 duración en fotogramas (como
      AnimationManager:getAnimLength de CorsixTH, sin los ajustes de su Lua)
      fotogramas: u32 índice en la lista, u16 siguiente, u8 sonido, u8 flags
      lista: u16 índice de elemento, 0xFFFF = fin de la lista del fotograma
      elementos: u16 sprite, u8 x, u8 y, u8 capa << 4 | flags, u8 id de capa
Los desplazamientos x e y se guardan crudos: en alta resolución CorsixTH les
resta 141 y 186; en DATAM se decidirá al dibujar (hito 3).
"""
import struct

MAGIA = b"TAN1"


def leer(start, fra, lst, ele):
    n_anim, n_fot = len(start) // 4, len(fra) // 10
    n_lista, n_ele = len(lst) // 2, len(ele) // 6
    primeros = []
    for i in range(n_anim):
        f = struct.unpack_from("<H", start, 4 * i)[0]
        primeros.append(f if f <= n_fot else 0)
    fotogramas = []
    for i in range(n_fot):
        li, son, flags, sig = struct.unpack_from("<I2xBBH", fra, 10 * i)
        fotogramas.append((li if li < n_lista else 0, sig if sig < n_fot else 0, son, flags))
    lista = []
    for i in range(n_lista):
        e = struct.unpack_from("<H", lst, 2 * i)[0]
        lista.append(e if e < n_ele else 0xFFFF)
    lista.append(0xFFFF)
    elementos = []
    for i in range(n_ele):
        pos, x, y, capa_flags, capa_id = struct.unpack_from("<HBBBB", ele, 6 * i)
        elementos.append((pos // 6, x, y, capa_flags >> 4, capa_flags & 0xF, capa_id))
    return primeros, fotogramas, lista, elementos


def duraciones(primeros, fotogramas):
    """AnimationManager:getAnimLength (graphics.lua): fotogramas hasta repetir."""
    out = []
    for f in primeros:
        vistos = set()
        while f not in vistos:
            vistos.add(f)
            f = fotogramas[f][1] if f < len(fotogramas) else f
        out.append(len(vistos))
    return out


def escribir(primeros, fotogramas, lista, elementos):
    dur = duraciones(primeros, fotogramas)
    t_anim = b"".join(struct.pack(">HH", f, d) for f, d in zip(primeros, dur))
    t_fot = b"".join(struct.pack(">IHBB", li, sig, son, fl) for li, sig, son, fl in fotogramas)
    t_lista = struct.pack(f">{len(lista)}H", *lista)
    t_ele = b"".join(struct.pack(">HBBBB", s, x, y, (capa << 4) | fl, cid) for s, x, y, capa, fl, cid in elementos)
    def al4(n): return (n + 3) // 4 * 4
    o_anim = 36
    o_fot = al4(o_anim + len(t_anim))
    o_lista = al4(o_fot + len(t_fot))
    o_ele = al4(o_lista + len(t_lista))
    cab = MAGIA + struct.pack(">HHIIIIIII", len(primeros), 0, len(fotogramas), len(lista), len(elementos),
                              o_anim, o_fot, o_lista, o_ele)
    out = bytearray(cab)
    for off, t in ((o_anim, t_anim), (o_fot, t_fot), (o_lista, t_lista), (o_ele, t_ele)):
        out += b"\0" * (off - len(out)) + t
    return bytes(out)


def leer_tan(datos):
    """Lector de TAN1 (para las comprobaciones)."""
    if datos[:4] != MAGIA:
        raise ValueError("no es TAN1")
    n_anim, _, n_fot, n_lista, n_ele, o_anim, o_fot, o_lista, o_ele = struct.unpack_from(">HHIIIIIII", datos, 4)
    anims = [struct.unpack_from(">HH", datos, o_anim + 4 * i) for i in range(n_anim)]
    fot = [struct.unpack_from(">IHBB", datos, o_fot + 8 * i) for i in range(n_fot)]
    lista = list(struct.unpack_from(f">{n_lista}H", datos, o_lista))
    ele = []
    for i in range(n_ele):
        s, x, y, cf, cid = struct.unpack_from(">HBBBB", datos, o_ele + 6 * i)
        ele.append((s, x, y, cf >> 4, cf & 0xF, cid))
    return [a[0] for a in anims], [a[1] for a in anims], fot, lista, ele
