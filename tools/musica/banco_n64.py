#!/usr/bin/env python3
"""Copia de un SoundFont (SF2) sin lo que no hace el sintetizador SF64 de
libdragon, para oír con fluidsynth qué aporta cada cosa: los LFO de vibrato y
de modulación, el filtro y la envolvente de modulación aplicada al filtro
(audioconv64 los ignora al convertir). La envolvente de modulación aplicada al
tono sí la hace la N64 y se deja.

Pone a cero (o en el valor neutro) estos generadores, en los instrumentos y en
los presets: modLfoToPitch (5), vibLfoToPitch (6), initialFilterFc (8: 13500
cents en el instrumento, 0 en el preset), initialFilterQ (9),
modLfoToFilterFc (10), modEnvToFilterFc (11) y modLfoToVolume (13). Los
moduladores por defecto de SF2 (la velocidad sobre el filtro, la rueda de
modulación sobre el vibrato) los sigue aplicando fluidsynth.

Uso: python3 -I tools/musica/banco_n64.py <entrada.sf2> <salida.sf2>
"""
import struct
import sys

NEUTRO_INSTRUMENTO = {5: 0, 6: 0, 8: 13500, 9: 0, 10: 0, 11: 0, 13: 0}
NEUTRO_PRESET = {5: 0, 6: 0, 8: 0, 9: 0, 10: 0, 11: 0, 13: 0}
NOMBRES = {5: "modLfoToPitch", 6: "vibLfoToPitch", 8: "initialFilterFc", 9: "initialFilterQ",
           10: "modLfoToFilterFc", 11: "modEnvToFilterFc", 13: "modLfoToVolume"}


def trozos(datos, ini, fin):
    """Subtrozos RIFF (id, desplazamiento de los datos, tamaño) entre ini y fin."""
    i = ini
    while i + 8 <= fin:
        cid, tam = datos[i:i + 4], struct.unpack_from("<I", datos, i + 4)[0]
        yield cid, i + 8, tam
        i += 8 + tam + (tam & 1)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    datos = bytearray(open(sys.argv[1], "rb").read())
    if datos[:4] != b"RIFF" or datos[8:12] != b"sfbk":
        sys.exit("no es un SF2")
    cambios = {}
    for cid, off, tam in trozos(datos, 12, len(datos)):
        if cid != b"LIST" or datos[off:off + 4] != b"pdta":
            continue
        for sid, soff, stam in trozos(datos, off + 4, off + tam):
            if sid not in (b"pgen", b"igen"):
                continue
            neutro = NEUTRO_PRESET if sid == b"pgen" else NEUTRO_INSTRUMENTO
            for k in range(stam // 4):
                p = soff + 4 * k
                oper, valor = struct.unpack_from("<Hh", datos, p)
                if oper in neutro and valor != neutro[oper]:
                    struct.pack_into("<Hh", datos, p, oper, neutro[oper])
                    clave = (sid.decode(), NOMBRES[oper])
                    cambios[clave] = cambios.get(clave, 0) + 1
    open(sys.argv[2], "wb").write(datos)
    for (nivel, nombre), n in sorted(cambios.items()):
        print(f"{nivel} {nombre}: {n}")


if __name__ == "__main__":
    main()
