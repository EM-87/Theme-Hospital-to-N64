#!/usr/bin/env python3
"""Conversor de los datos de Theme Hospital para el motor de la N64 (fase 5).

Lee la copia del juego de cada usuario ($TH_DATA_DIR) y escribe los assets del
motor en motor/assets/ (sin comprimir; el Makefile del motor los comprime al
meterlos en la ROM). Nada de lo que genera va al repositorio.

Uso:
  python3 -I tools/conv/conv.py [--datos DIR] [--salida DIR] [--vistas DIR] [pasos...]
Pasos (por defecto, todos): graficos animaciones niveles reglas textos fuentes sonido
Los pasos reglas, textos, fuentes y sonido necesitan además el código de
CorsixTH ($CORSIXTH_DIR); reglas y textos, también lua5.4; sonido, g++ y el
SoundFont TimGM6mb (paquete timgm6mb-soundfont, que instala tools/setup.sh).
"""
import argparse
import json
import os
import struct
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import anim  # noqa: E402
import fuentes  # noqa: E402
import hojas  # noqa: E402
import niveles  # noqa: E402
import paleta  # noqa: E402
import reglas  # noqa: E402
import rnc  # noqa: E402
import sonidos  # noqa: E402

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


def paso_animaciones(d, salida, vistas, resumen, oraculo=None):
    tablas = [d.leer("DATAM", f"M{t}-1.ANI") for t in ("START", "FRA", "LIST", "ELE")]
    prim, fot, lista, ele = anim.leer(*tablas)
    # El encadenamiento de DATAM tiene que ser el de DATA, que es el que usa CorsixTH
    # para medir el tiempo (fase 2: START idéntico y mismos fotogramas).
    v = [d.leer("DATA", f"V{t}-1.ANI") for t in ("START", "FRA", "LIST", "ELE")]
    vprim, vfot, _, _ = anim.leer(*v)
    if vprim != prim or [f[1:] for f in vfot] != [f[1:] for f in fot]:
        sys.exit("ERROR: las animaciones de DATAM no se encadenan como las de DATA")
    tan = anim.escribir(prim, fot, lista, ele)
    dur = anim.duraciones(prim, fot)
    if anim.leer_tan(tan) != (prim, dur, [(f[0], f[1], f[2], f[3]) for f in fot], lista, ele):
        sys.exit("ERROR: las animaciones no se leen igual que se escribieron")
    r = {"fichero": "datam/animaciones.tan", "animaciones": len(prim), "fotogramas": len(fot),
         "lista": len(lista), "elementos": len(ele), "bytes": escribir(salida, "datam/animaciones.tan", tan)}
    if oraculo:
        # Volcado de AnimationManager:getAnimLength de CorsixTH: «animación duración» por línea
        # (tools/construccion/metodo/oraculo/sonda_anim.lua).
        ref = {int(a): int(b) for a, b in (l.split() for l in open(oraculo) if l.strip())}
        distintas = [(i, dur[i], ref.get(i)) for i in range(len(dur)) if ref.get(i) != dur[i]]
        r["oraculo"] = {"iguales": len(dur) - len(distintas), "distintas": distintas}
    resumen["animaciones"] = r


def paso_niveles(d, salida, vistas, resumen):
    resumen["niveles"] = []
    for n in range(1, 13):
        tmp, r = niveles.convertir(d.leer("LEVELS", f"LEVEL.L{n}"))
        rel = f"niveles/nivel{n:02d}.tmp"
        r.update({"nivel": n, "fichero": rel, "bytes": escribir(salida, rel, tmp)})
        resumen["niveles"].append(r)


def paso_reglas(d, salida, vistas, resumen):
    lua = os.path.join(os.environ.get("CORSIXTH_DIR", ""), "CorsixTH", "Lua")
    if not os.path.isfile(os.path.join(lua, "base_config.lua")):
        sys.exit(f"no encuentro el código de CorsixTH en {lua} (CORSIXTH_DIR)")
    doc, salida_lua = reglas.extraer(lua, d.ruta("LEVELS"))
    tablas = [d.leer("DATAM", f"M{t}-1.ANI") for t in ("START", "FRA", "LIST", "ELE")]
    prim, fot, _, _ = anim.leer(*tablas)
    r = reglas.preparar(doc, anim.duraciones(prim, fot))
    r["extractor"] = salida_lua
    r["ficheros"] = {}
    for rel, arbol in reglas.ficheros(doc).items():
        trg = reglas.escribir(arbol)
        # Comprobación de ida y vuelta, con igualdad estricta (1 no es 1.0).
        if not reglas.igual(reglas.leer(trg), arbol):
            sys.exit(f"ERROR: {rel} no se lee igual que se escribió")
        r["ficheros"][rel] = escribir(salida, rel, trg)
    r["duraciones"] = {str(x["animacion"]): x["duracion"] for x in doc["duraciones"]}
    if vistas:
        os.makedirs(vistas, exist_ok=True)
        with open(os.path.join(vistas, "reglas.json"), "w") as f:
            json.dump(doc, f, indent=1, sort_keys=True, ensure_ascii=False)
    resumen["reglas"] = r


IDIOMAS = ("english", "spanish")


def paso_textos(d, salida, vistas, resumen):
    lua = os.path.join(os.environ.get("CORSIXTH_DIR", ""), "CorsixTH", "Lua")
    doc = reglas.extraer_textos(lua, d.ruta("DATA"), IDIOMAS)
    r = {"ficheros": {}}
    for idioma in IDIOMAS:
        rel = f"textos/{idioma}.trg"
        trg = reglas.escribir(doc[idioma])
        if not reglas.igual(reglas.leer(trg), doc[idioma]):
            sys.exit(f"ERROR: {rel} no se lee igual que se escribió")
        r["ficheros"][rel] = escribir(salida, rel, trg)
    if vistas:
        os.makedirs(vistas, exist_ok=True)
        with open(os.path.join(vistas, "textos.json"), "w") as f:
            json.dump(doc, f, indent=1, sort_keys=True, ensure_ascii=False)
    resumen["textos"] = r


def paso_fuentes(d, salida, vistas, resumen):
    tabla = fuentes.tabla_cp437(os.environ.get("CORSIXTH_DIR", ""))
    rgb = paleta.colores_rgb8(d.leer("DATA", "MPALETTE.DAT"))
    # Cobertura: los caracteres de los textos ya convertidos (paso textos).
    textos = {}
    for idioma in IDIOMAS:
        ruta = os.path.join(salida, "textos", f"{idioma}.trg")
        if os.path.exists(ruta):
            textos[idioma] = reglas.leer(open(ruta, "rb").read())
    r = {"fuentes": []}
    for nombre in fuentes.FUENTES:
        sprites = hojas.leer(d.leer("QDATAM", nombre + ".TAB"), d.leer("QDATAM", nombre + ".DAT"),
                             hojas.es_compleja(nombre))
        fnt, png, glifos = fuentes.convertir(nombre, sprites, rgb, tabla)
        base = f"fuentes/{nombre.lower()}"
        f = {"fuente": f"QDATAM/{nombre}", "glifos": len(glifos), "fnt": escribir(salida, base + ".fnt", fnt.encode()),
             "png": escribir(salida, base + ".png", png)}
        if len(glifos) > 60:
            f["faltan"] = {i: "".join(sorted(fuentes.cobertura(t, glifos))) for i, t in textos.items()}
        r["fuentes"].append(f)
    resumen["fuentes"] = r


def paso_sonido(d, salida, vistas, resumen):
    # Efectos y locutor: WAV sin repetir y una tabla posición → fichero por idioma.
    ficheros, tablas, r = sonidos.sonidos(lambda n: d.leer("SOUND", "DATA", n))
    r["wav_bytes"] = sum(escribir(salida, rel, wav) for rel, wav in ficheros.items())
    r["wav"] = len(ficheros)
    for idioma, t in tablas.items():
        trg = reglas.escribir(t)
        if not reglas.igual(reglas.leer(trg), t):
            sys.exit(f"ERROR: la tabla de sonidos de {idioma} no se lee igual que se escribió")
        escribir(salida, f"sonido/{idioma}.trg", trg)
    # Música: XMI a MIDI con el código de CorsixTH, y el SoundFont recortado.
    cache = os.path.join(os.environ.get("TH64_WORK", os.path.expanduser("~/th64-work")), "cache", "xmi2mid")
    exe = sonidos.compilar_xmi2mid(os.environ.get("CORSIXTH_DIR", ""), cache)
    midi = d.ruta("SOUND", "MIDI")
    usados, piezas = set(), []
    for f in sorted(os.listdir(midi)):
        if f.upper().endswith(".XMI"):
            mid = sonidos.xmi_a_midi(exe, d.leer("SOUND", "MIDI", f))
            escribir(salida, f"musica/{f[:-4].lower()}.mid", mid)
            usados |= sonidos.programas(mid)
            piezas.append(f[:-4].lower())
    sf2 = open(sonidos.SOUNDFONT, "rb").read()
    presentes = sonidos.presets_sf2(sf2)
    # Un preset que no esté en el banco se toca con el programa 0 de su banco.
    quedan = {p if p in presentes else (p[0], 0) for p in usados}
    banco, rs = sonidos.recortar_sf2(sf2, quedan)
    if sonidos.presets_sf2(banco) != quedan:
        sys.exit("ERROR: el SoundFont recortado no tiene los presets pedidos")
    rs.update({"bytes": escribir(salida, "musica/banco.sf2", banco), "bytes_original": len(sf2),
               "usados": sorted(usados), "sin_preset": sorted(usados - presentes)})
    r["musica"] = {"piezas": piezas, "banco": rs}
    resumen["sonido"] = r


PASOS = {"graficos": paso_graficos, "animaciones": paso_animaciones, "niveles": paso_niveles,
         "reglas": paso_reglas, "textos": paso_textos, "fuentes": paso_fuentes, "sonido": paso_sonido}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--datos", default=os.environ.get("TH_DATA_DIR"), help="copia del juego (TH_DATA_DIR)")
    ap.add_argument("--salida", default=os.path.join(REPO, "motor", "assets"))
    ap.add_argument("--vistas", help="directorio para vistas previas en PNG (opcional)")
    ap.add_argument("--oraculo-anim", help="duraciones de CorsixTH para comprobar las animaciones (opcional)")
    ap.add_argument("pasos", nargs="*", default=list(PASOS))
    a = ap.parse_args()
    if not a.datos:
        sys.exit("falta --datos o TH_DATA_DIR")
    d = Datos(a.datos)
    previo = os.path.join(a.salida, "conversion.json")
    resumen = json.load(open(previo)) if os.path.exists(previo) else {}
    for p in a.pasos:
        if p not in PASOS:
            sys.exit(f"paso desconocido: {p}")
        if p == "animaciones":
            PASOS[p](d, a.salida, a.vistas, resumen, a.oraculo_anim)
        else:
            PASOS[p](d, a.salida, a.vistas, resumen)
    escribir(a.salida, "conversion.json", json.dumps(resumen, indent=1, ensure_ascii=False).encode())
    for h in (resumen.get("hojas", []) if "graficos" in a.pasos else []):
        print(f"{h['hoja']:<18} {h['sprites']:5d} sprites  {h['bytes']:9d} B  -> {h['fichero']}")
    if "animaciones" in a.pasos:
        r = resumen["animaciones"]
        print(f"animaciones       {r['animaciones']} animaciones, {r['fotogramas']} fotogramas, "
              f"{r['elementos']} elementos  {r['bytes']} B  -> {r['fichero']}")
        if "oraculo" in r:
            o = r["oraculo"]
            print(f"  duraciones frente a CorsixTH: {o['iguales']} iguales, {len(o['distintas'])} distintas "
                  f"{[(i, a, b) for i, a, b in o['distintas']][:5]}")
    if "niveles" in a.pasos:
        for r in resumen["niveles"]:
            print(f"nivel {r['nivel']:2d}          {r['parcelas']:2d} parcelas, {r['objetos']:3d} objetos, {r['divisorias']:3d} paredes divisorias, "
                  f"cámara {tuple(r['camara'])}  {r['bytes']} B  -> {r['fichero']}")
    if "reglas" in a.pasos:
        r = resumen["reglas"]
        f = r["ficheros"]
        niv = [v for k, v in f.items() if k != "reglas/definiciones.trg"]
        print(f"reglas            {r['extractor']}")
        print(f"                  definiciones {f['reglas/definiciones.trg']} B, {len(niv)} niveles de "
              f"{min(niv)} a {max(niv)} B; aspecto factorizado en {r['aspectos_factorizados']} de {r['aspectos']} "
              f"enfermedades; duraciones fijadas {r['duraciones']}")
    if "textos" in a.pasos:
        for rel, n in resumen["textos"]["ficheros"].items():
            print(f"textos            {n} B  -> {rel}")
    if "fuentes" in a.pasos:
        for f in resumen["fuentes"]["fuentes"]:
            print(f"{f['fuente']:<17} {f['glifos']:4d} glifos  -> fuentes/; faltan: "
                  + (", ".join(f"{i}: {c!r}" for i, c in f.get("faltan", {}).items()) or "(solo cifras)"))
    if "sonido" in a.pasos:
        r = resumen["sonido"]
        print(f"sonido            {r['wav']} WAV ({r['wav_bytes']} B): {r['comunes']} comunes, propios {r['propios']}; "
              f"entradas por idioma {r['entradas']}, sin PCM {r['sin_pcm']}")
        b = r["musica"]["banco"]
        print(f"música            {len(r['musica']['piezas'])} piezas; banco recortado {b['presets']} de "
              f"{b['presets_original']} presets, {b['instrumentos']} instrumentos, {b['muestras']} de "
              f"{b['muestras_original']} muestras, {b['bytes']} de {b['bytes_original']} B; sin preset: {b['sin_preset']}")
    if "graficos" in a.pasos:
        print(f"paleta             {len(resumen['paleta']['variantes'])} variantes  "
              f"{resumen['paleta']['bytes']:5d} B  -> paleta.tpl")


if __name__ == "__main__":
    main()
