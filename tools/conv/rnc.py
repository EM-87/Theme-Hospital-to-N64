"""Descompresor RNC ProPack (método 1), el formato de muchos ficheros de TH.

Port a Python de libs/rnc/rnc.cpp de CorsixTH v0.70.1. Aviso del original:

    Copyright (c) 1997 Simon Tatham

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.

Uso: datos = rnc.descomprimir(bytes) si rnc.es_rnc(bytes).
"""

FIRMA = b"RNC\x01"
CABECERA = 18


def _tabla_crc():
    t = []
    for i in range(256):
        v = i
        for _ in range(8):
            v = (v >> 1) ^ 0xA001 if v & 1 else v >> 1
        t.append(v)
    return t


_CRC = _tabla_crc()


def _crc(datos):
    v = 0
    for b in datos:
        v ^= b
        v = (v >> 8) ^ _CRC[v & 0xFF]
    return v


def es_rnc(datos):
    return datos[:4] == FIRMA


class ErrorRNC(ValueError):
    pass


class _Bits:
    """Flujo de bits como el de rnc.cpp: palabras de 16 bits little-endian."""

    def __init__(self, datos, ini, fin):
        self.d, self.p, self.fin = datos, ini, fin
        self.buf = datos[ini] | (datos[ini + 1] << 8)
        self.n = 16

    def _cargar(self):
        if self.p < self.fin - 1:
            self.buf |= (self.d[self.p] | (self.d[self.p + 1] << 8)) << self.n
            self.n += 16
        elif self.p < self.fin:
            self.buf |= self.d[self.p] << self.n
            self.n += 16

    def mirar(self, mascara):
        return self.buf & mascara

    def avanzar(self, n):
        self.buf >>= n
        self.n -= n
        if self.n < 16:
            self.p += 2
            self._cargar()

    def leer(self, mascara, n):
        v = self.buf & mascara
        self.avanzar(n)
        return v

    def arreglar(self):
        # Tras copiar literales moviendo p: se quitan los 16 bits de arriba y se
        # recarga desde la nueva posición.
        self.n = max(self.n - 16, 0)
        self.buf &= (1 << self.n) - 1
        if self.p < self.fin - 1:
            self.buf |= (self.d[self.p] | (self.d[self.p + 1] << 8)) << self.n
            self.n += 16
        elif self.p == self.fin - 1:
            self.buf |= self.d[self.p] << self.n
            self.n += 16


def _espejo(x, n):
    arriba, abajo = 1 << (n - 1), 1
    while arriba > abajo:
        m = arriba | abajo
        if x & m not in (0, m):
            x ^= m
        arriba >>= 1
        abajo <<= 1
    return x


def _tabla_huffman(bits):
    num = bits.leer(0x1F, 5)
    if num == 0:
        return None
    long_hoja = [bits.leer(0x0F, 4) for _ in range(num)]
    tabla, codigo = [], 0
    for lon in range(1, max(max(long_hoja), 1) + 1):
        for valor, l in enumerate(long_hoja):
            if l == lon:
                tabla.append((_espejo(codigo, lon), lon, valor))
                codigo += 1
        codigo <<= 1
    return tabla


def _leer_huffman(tabla, bits):
    for codigo, lon, valor in tabla:
        if bits.mirar((1 << lon) - 1) == codigo:
            bits.avanzar(lon)
            if valor >= 2:
                v = 1 << (valor - 1)
                return v | bits.leer(v - 1, valor - 1)
            return valor
    raise ErrorRNC("código Huffman no válido")


def descomprimir(datos):
    """Devuelve los datos descomprimidos; lanza ErrorRNC si algo no cuadra."""
    if not es_rnc(datos):
        raise ErrorRNC("no es un fichero RNC")
    tam_salida = int.from_bytes(datos[4:8], "big")
    tam_entrada = int.from_bytes(datos[8:12], "big")
    crc_salida = int.from_bytes(datos[12:14], "big")
    crc_entrada = int.from_bytes(datos[14:16], "big")
    ini, fin = CABECERA, CABECERA + tam_entrada
    if fin > len(datos):
        raise ErrorRNC("fichero truncado")
    if _crc(datos[ini:fin]) != crc_entrada:
        raise ErrorRNC("CRC de los datos comprimidos incorrecto")
    salida = bytearray()
    bits = _Bits(datos, ini, fin)
    bits.avanzar(2)
    tabla_raw = tabla_dist = tabla_len = None
    while len(salida) < tam_salida:
        # Como en rnc.cpp, una tabla vacía (num == 0) conserva la anterior.
        tabla_raw = _tabla_huffman(bits) or tabla_raw
        tabla_dist = _tabla_huffman(bits) or tabla_dist
        tabla_len = _tabla_huffman(bits) or tabla_len
        trozos = bits.leer(0xFFFF, 16)
        while True:
            lon = _leer_huffman(tabla_raw, bits)
            if lon:
                salida += datos[bits.p:bits.p + lon]
                bits.p += lon
                bits.arreglar()
            trozos -= 1
            if trozos <= 0:
                break
            dist = _leer_huffman(tabla_dist, bits) + 1
            lon = _leer_huffman(tabla_len, bits) + 2
            for _ in range(lon):
                salida.append(salida[-dist])
    if len(salida) != tam_salida:
        raise ErrorRNC("tamaño descomprimido incorrecto")
    if _crc(salida) != crc_salida:
        raise ErrorRNC("CRC de los datos descomprimidos incorrecto")
    return bytes(salida)
