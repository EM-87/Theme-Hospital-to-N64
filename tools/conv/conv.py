#!/usr/bin/env python3
"""Conversor de los datos de Theme Hospital para el motor de la N64 (fase 5).

Lee la copia del juego de cada usuario ($TH_DATA_DIR) y escribe los assets del
motor en motor/assets/ (sin comprimir; el Makefile del motor los comprime al
meterlos en la ROM). Nada de lo que genera va al repositorio.

Uso:
  python3 -I tools/conv/conv.py [--datos DIR] [--salida DIR] [--vistas DIR] [pasos...]
Pasos (por defecto, todos): graficos
"""
import argparse
import json
import os
import struct
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import hojas  # noqa: E402
import paleta  # noqa: E402
import rnc  # noqa: E402

REPO = os.path.normpath(os.path.join(AQUI, "..", ".."))


class Datos:
    """Acceso a los ficheros del juego sin distinguir mayúsculas, con RNC."""

    def __init__(self, raiz):
        if not os.path.isdir(raiz):
            sys.exit(f"no existe el directorio de datos: {raiz}")
        self.raiz = raiz

    def ruta(self, *partes):
        actual = self.raiz
        for p in partes:
            nombres = {n.upper(): n for n in os.listdir(actual)}
            if p.upper() not in nombres:
                raise FileNotFoundError(os.path.join(actual, p))
            actual = os.path.join(actual, nombres[p.upper()])
        return actual

    def leer(self, *partes):
        datos = open(self.ruta(*partes), "rb").read()
        return rnc.descomprimir(datos) if rnc.es_rnc(datos) else datos

    def listar(self, directorio):
        return sorted(os.listdir(self.ruta(directorio)))


def escribir(salida, relativa, datos):
    p = os.path.join(salida, relativa)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "wb") as f:
        f.write(datos)
    return len(datos)


def paso_graficos(d, salida, vistas, resumen):
    # Paletas: TPL1 = 'TPL1', u16 número de paletas, u16 0, y 512 B por paleta.
    pals = paleta.variantes(d.leer("DATA", "MPALETTE.DAT"), d.leer("QDATA", "GHOST1.DAT"))
    blob = b"TPL1" + struct.pack(">HH", len(pals), 0)
    for p in pals:
        blob += struct.pack(">256H", *p)
    resumen["paleta"] = {"fichero": "paleta.tpl", "variantes": list(paleta.VARIANTES),
                         "bytes": escribir(salida, "paleta.tpl", blob)}
    # Hojas de DATAM: todas las que tienen .TAB y .DAT.
    nombres = sorted({f[:-4].upper() for f in d.listar("DATAM") if f.upper().endswith(".TAB")})
    resumen["hojas"] = []
    for n in nombres:
        try:
            tab, dat = d.leer("DATAM", n + ".TAB"), d.leer("DATAM", n + ".DAT")
        except FileNotFoundError:
            continue
        sprites = hojas.leer(tab, dat, hojas.es_compleja(n))
        tsh = hojas.escribir(sprites)
        # Comprobación de ida y vuelta: el fichero escrito devuelve los mismos píxeles.
        if hojas.leer_tsh(tsh) != sprites:
            sys.exit(f"ERROR: la hoja {n} no se lee igual que se escribió")
        rel = f"datam/{n.lower()}.tsh"
        resumen["hojas"].append({"hoja": f"DATAM/{n}", "fichero": rel, "sprites": len(sprites),
                                 "vacios": sum(1 for s in sprites if not s[0]),
                                 "bytes": escribir(salida, rel, tsh)})
        if vistas:
            vista_hoja(sprites, paleta.colores_rgb8(d.leer("DATA", "MPALETTE.DAT")),
                       os.path.join(vistas, f"{n.lower()}.png"))


def vista_hoja(sprites, rgb, ruta, ancho=512):
    """PNG con los sprites de una hoja en fila, sobre fondo magenta (transparente)."""
    from PIL import Image
    x = y = alto_fila = 0
    sitios = []
    for w, h, _ in sprites:
        if not w:
            sitios.append(None)
            continue
        if x + w > ancho:
            x, y, alto_fila = 0, y + alto_fila + 1, 0
        sitios.append((x, y))
        x += w + 1
        alto_fila = max(alto_fila, h)
    img = Image.new("RGB", (ancho, max(1, y + alto_fila)), (255, 0, 255))
    for (w, h, px), sitio in zip(sprites, sitios):
        if not sitio:
            continue
        for j in range(h):
            for i in range(w):
                c = px[j * w + i]
                if c != 255:
                    img.putpixel((sitio[0] + i, sitio[1] + j), rgb[c])
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    img.save(ruta)


PASOS = {"graficos": paso_graficos}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--datos", default=os.environ.get("TH_DATA_DIR"), help="copia del juego (TH_DATA_DIR)")
    ap.add_argument("--salida", default=os.path.join(REPO, "motor", "assets"))
    ap.add_argument("--vistas", help="directorio para vistas previas en PNG (opcional)")
    ap.add_argument("pasos", nargs="*", default=list(PASOS))
    a = ap.parse_args()
    if not a.datos:
        sys.exit("falta --datos o TH_DATA_DIR")
    d = Datos(a.datos)
    resumen = {}
    for p in a.pasos:
        if p not in PASOS:
            sys.exit(f"paso desconocido: {p}")
        PASOS[p](d, a.salida, a.vistas, resumen)
    escribir(a.salida, "conversion.json", json.dumps(resumen, indent=1, ensure_ascii=False).encode())
    for h in resumen.get("hojas", []):
        print(f"{h['hoja']:<18} {h['sprites']:5d} sprites  {h['bytes']:9d} B  -> {h['fichero']}")
    if "paleta" in resumen:
        print(f"paleta             {len(resumen['paleta']['variantes'])} variantes  "
              f"{resumen['paleta']['bytes']:5d} B  -> paleta.tpl")


if __name__ == "__main__":
    main()
