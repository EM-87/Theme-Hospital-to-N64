"""Mapas de los niveles (LEVELS/LEVEL.Lx) al formato TMP1 de la N64.

Lectura como level_map::load_from_th_file de CorsixTH (th_map.cpp):
  - byte 0: número de jugadores; cámara inicial en el desplazamiento 163876 y
    helipuerto en 163884 (índice de casilla u16 LE por jugador);
  - desde el byte 34, 128 × 128 casillas de 8 B: [0] dirección del objeto,
    [1] tipo de objeto (thob), [2] suelo, [3] pared norte, [4] pared oeste,
    [5] y [7] banderas;
  - desde el byte 131106, la parcela de cada casilla (u16 LE).
Los bloques pasan por la misma tabla gs_iTHMapBlockLUT. Como en CorsixTH, cada
casilla tiene dos versiones de sus paredes: la actual, sin las «paredes
divisorias» entre parcelas, y la original, con ellas (original_cells). Las
plantas del estanque (suelos 71–73) pasan a la pared norte con el suelo 69.

El conversor no aplica nada más. Lo que hace CorsixTH después (parcelas sin
dueño convertidas en césped, paredes divisorias, Map:_fixTiles) es lógica del
juego y está en el núcleo (motor/core/src/map.c).

Formato TMP1 (big-endian):
  0   'TMP1'
  4   u16 ancho, u16 alto, u8 jugadores, u8 parcelas, u16 tamaño de casilla (10)
  12  u16 cámara x, u16 cámara y, u16 helipuerto x, u16 helipuerto y (jugador 1)
  20  12 B a cero
  32  casillas, 10 B cada una, por filas (y, luego x):
      u8 suelo, u8 pared norte, u8 pared oeste, u8 banderas,
      u8 pared norte original, u8 pared oeste original,
      u8 objeto (thob), u8 dirección del objeto, u8 parcela, u8 0
  banderas: 1 transitable, 2 hospital, 4 edificable, 8/16/32/64 edificable
            por el lado norte/este/sur/oeste (tal como vienen en el fichero)
"""
import struct

ANCHO = ALTO = 128
MAGIA = b"TMP1"

# th_map.cpp: gs_iTHMapBlockLUT
LUT = bytes([
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C,
    0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x52, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61,
    0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D,
    0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x84, 0x85, 0x88, 0x89,
    0x8C, 0x8D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8E, 0x8F, 0x00, 0x00,
    0x00, 0x00, 0x8E, 0x8F, 0xD5, 0xD6, 0x9C, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0,
    0xD1, 0xD2, 0xD3, 0xD4, 0xB3, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5,
    0xB6, 0xB7, 0xB8, 0xB9, 0xB3, 0xB3, 0xB4, 0xB4, 0xBA, 0xBB, 0xBC, 0xBD,
    0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9,
    0xCA, 0xCB, 0x00, 0x82, 0x83, 0x86, 0x87, 0x8A, 0x8B, 0x92, 0x93, 0x94,
    0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x00, 0x9D, 0x9E, 0x9F, 0xA0,
    0xA1, 0xA2, 0xA3, 0xA4, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE0, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00])
assert len(LUT) == 256

OFF_CASILLAS, OFF_PARCELAS, OFF_CAMARA, OFF_HELIPUERTO = 34, 131106, 163876, 163884


TAM_CASILLA = 10


def es_divisoria(b):
    return (b >> 1) == 70


def convertir(datos):
    """Devuelve (fichero TMP1, resumen)."""
    if len(datos) < 163948:
        raise ValueError("mapa demasiado corto")
    jugadores = max(1, min(datos[0], 4))

    def indice(off):
        i = datos[off] | (datos[off + 1] << 8)
        return i % ANCHO, i // ANCHO

    cam, heli = indice(OFF_CAMARA), indice(OFF_HELIPUERTO)
    casillas = bytearray()
    parcelas = divisorias = objetos = 0
    for n in range(ANCHO * ALTO):
        r = datos[OFF_CASILLAS + 8 * n:OFF_CASILLAS + 8 * n + 8]
        parcela = datos[OFF_PARCELAS + 2 * n] | (datos[OFF_PARCELAS + 2 * n + 1] << 8)
        if parcela > 255:
            raise ValueError("parcela fuera de rango")
        parcelas = max(parcelas, parcela + 1)
        suelo = LUT[r[2]]
        norte = oeste = 0
        if r[3] == 0 or es_divisoria(r[3]):
            if 71 <= suelo <= 73:
                norte, suelo = suelo, 69
        else:
            norte = LUT[r[3]]
        if r[4] != 0 and not es_divisoria(r[4]):
            oeste = LUT[r[4]]
        norte_orig = LUT[r[3]] if es_divisoria(r[3]) else norte
        oeste_orig = LUT[r[4]] if es_divisoria(r[4]) else oeste
        divisorias += es_divisoria(r[3]) + es_divisoria(r[4])
        b = 0
        if not (r[5] & 1):
            b |= 1
            if not (r[7] & 16):
                b |= 2
                if not (r[5] & 2):
                    b |= 4
                for bit, lado in ((4, 8), (8, 16), (16, 32), (32, 64)):
                    if not (r[5] & bit) or r[1] == 0:
                        b |= lado
        objetos += r[1] != 0
        casillas += bytes([suelo, norte, oeste, b, norte_orig, oeste_orig, r[1], r[0], parcela, 0])
    cab = MAGIA + struct.pack(">HHBBH", ANCHO, ALTO, jugadores, parcelas, TAM_CASILLA)
    cab += struct.pack(">HHHH", cam[0], cam[1], heli[0], heli[1]) + bytes(12)
    resumen = {"jugadores": jugadores, "parcelas": parcelas, "camara": cam, "helipuerto": heli,
               "objetos": objetos, "divisorias": divisorias}
    return bytes(cab + casillas), resumen
