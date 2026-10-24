"""Sonido y música de Theme Hospital para el motor (fase 5, hito 0).

Efectos y locutor (SOUND/DATA/SOUND-n.DAT, uno por idioma: el número es el
de su LANG-n.DAT, 0 el inglés y 4 el español):
  * formato de sound_archive::load_from_th_file (th_sound.cpp de CorsixTH):
    los 4 últimos bytes dan la posición de una cabecera de 234 B; en ella,
    +50 es la posición de la tabla y +58 su longitud; la tabla tiene entradas
    de 32 B (nombre de 18 B, posición u32 en +18, longitud u32 en +26) y cada
    entrada es un WAV completo. La entrada 0 es el propio índice;
  * CorsixTH reproduce los sonidos de los fotogramas por su posición en el
    archivo del idioma cargado (sound_player::play_at), y el orden cambia
    entre idiomas, así que cada idioma lleva su tabla posición → fichero;
  * los sonidos con el mismo PCM se guardan una vez: en sonido/comun/ si
    están en varios idiomas y en sonido/<idioma>/ si solo en uno.

Música (SOUND/MIDI/*.XMI): a MIDI estándar con el mismo código que CorsixTH
(Src/xmi2mid.cpp, compilado aquí con g++). Se toca con un SoundFont General
MIDI (TimGM6mb, como en la fase 4), recortado a los programas que usan las
piezas: un SF2 nuevo con esos presets, sus instrumentos y sus muestras.

El Makefile del motor pasa los WAV a wav64 (VADPCM sin Huffman), los MIDI a
mid64 y el SF2 a sf64 con audioconv64.
"""
import hashlib
import os
import struct
import subprocess
import tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
IDIOMAS = {"english": 0, "spanish": 4}
SOUNDFONT = "/usr/share/sounds/sf2/TimGM6mb.sf2"


# Archivos de sonido -------------------------------------------------------------

def leer_archivo(datos):
    """Entradas 1.. del archivo: [(nombre, wav)]; la posición en la lista + 1
    es el índice de CorsixTH."""
    hp = struct.unpack_from("<I", datos, len(datos) - 4)[0]
    tpos, tlen = struct.unpack_from("<I", datos, hp + 50)[0], struct.unpack_from("<I", datos, hp + 58)[0]
    out = []
    for i in range(1, tlen // 32):
        e = tpos + i * 32
        nombre = datos[e:e + 18].split(b"\0")[0].decode("latin-1")
        pos, ln = struct.unpack_from("<I", datos, e + 18)[0], struct.unpack_from("<I", datos, e + 26)[0]
        out.append((nombre, datos[pos:pos + ln]))
    return out


def pcm(wav):
    """(formato, canales, hz, bits) y los datos PCM de un WAV; None si no lo es."""
    if wav[:4] != b"RIFF" or wav[8:12] != b"WAVE":
        return None
    fmt, datos, i = None, None, 12
    while i + 8 <= len(wav):
        cid, cl = wav[i:i + 4], struct.unpack_from("<I", wav, i + 4)[0]
        if cid == b"fmt ":
            tag, ch, hz, _, _, bits = struct.unpack_from("<HHIIHH", wav, i + 8)
            fmt = (tag, ch, hz, bits)
        elif cid == b"data":
            datos = wav[i + 8:i + 8 + cl]
        i += 8 + cl + (cl & 1)
    return (fmt, datos) if fmt and datos is not None and fmt[0] == 1 else None


def wav_limpio(fmt, datos):
    """WAV mínimo (fmt y data) con el mismo PCM."""
    _, ch, hz, bits = fmt
    blk = ch * bits // 8
    cab = struct.pack("<4sI4s4sIHHIIHH4sI", b"RIFF", 36 + len(datos), b"WAVE", b"fmt ", 16, 1, ch, hz, hz * blk, blk,
                      bits, b"data", len(datos))
    return cab + datos + (b"\0" if len(datos) & 1 else b"")


def sonidos(leer):
    """leer(nombre) devuelve los bytes de SOUND/DATA/<nombre>. Devuelve
    ({ruta: wav}, {idioma: [[nombre, ruta o None], ...]}, resumen)."""
    archivos = {i: leer_archivo(leer(f"SOUND-{n}.DAT")) for i, n in IDIOMAS.items()}
    por_pcm = {}  # hash -> {idiomas, nombre, fmt, datos}
    for idioma, entradas in archivos.items():
        for nombre, wav in entradas:
            p = pcm(wav)
            if not p:
                continue
            h = hashlib.sha256(repr(p[0]).encode() + p[1]).hexdigest()
            r = por_pcm.setdefault(h, {"idiomas": set(), "nombre": nombre, "fmt": p[0], "datos": p[1]})
            r["idiomas"].add(idioma)
    ficheros, ruta_de, usados = {}, {}, set()
    for h, r in sorted(por_pcm.items(), key=lambda kv: kv[1]["nombre"]):
        carpeta = "comun" if len(r["idiomas"]) > 1 else next(iter(r["idiomas"]))
        base = os.path.splitext(r["nombre"])[0].lower()
        ruta, n = f"sonido/{carpeta}/{base}", 2
        while ruta in usados:  # mismo nombre con distinto PCM
            ruta, n = f"sonido/{carpeta}/{base}_{n}", n + 1
        usados.add(ruta)
        ruta_de[h] = ruta
        ficheros[ruta + ".wav"] = wav_limpio(r["fmt"], r["datos"])
    tablas = {}
    for idioma, entradas in archivos.items():
        t = []
        for nombre, wav in entradas:
            p = pcm(wav)
            h = p and hashlib.sha256(repr(p[0]).encode() + p[1]).hexdigest()
            t.append([nombre, ruta_de[h] if p else None])
        tablas[idioma] = t
    resumen = {"entradas": {i: len(t) for i, t in tablas.items()},
               "comunes": sum(1 for r in por_pcm.values() if len(r["idiomas"]) > 1),
               "propios": {i: sum(1 for r in por_pcm.values() if r["idiomas"] == {i}) for i in IDIOMAS},
               "sin_pcm": {i: sum(1 for x in t if x[1] is None) for i, t in tablas.items()}}
    return ficheros, tablas, resumen


# Música ---------------------------------------------------------------------------

def compilar_xmi2mid(corsixth_dir, destino):
    """Compila tools/datos/xmi2mid.cpp con el xmi2mid.cpp de CorsixTH. De su
    config.h (que genera CMake) solo hacen falta las cabeceras estándar que
    incluye."""
    if os.path.exists(destino):
        return destino
    src = os.path.join(corsixth_dir, "CorsixTH", "Src")
    with tempfile.TemporaryDirectory() as tmp:
        with open(os.path.join(tmp, "config.h"), "w") as f:
            f.write("#include <cstddef>\n#include <cstdint>\n")
        os.makedirs(os.path.dirname(destino), exist_ok=True)
        subprocess.run(["g++", "-std=c++17", "-O2", "-I", src, "-I", tmp, "-o", destino,
                        os.path.join(AQUI, "..", "datos", "xmi2mid.cpp"), os.path.join(src, "xmi2mid.cpp")],
                       check=True)
    return destino


def xmi_a_midi(exe, xmi):
    with tempfile.TemporaryDirectory() as tmp:
        a, b = os.path.join(tmp, "a.xmi"), os.path.join(tmp, "a.mid")
        open(a, "wb").write(xmi)
        subprocess.run([exe, a, b], check=True)
        return open(b, "rb").read()


def programas(mid):
    """Pares (banco, programa) que suenan en un MIDI: el programa de cada
    canal cuando toca una nota; el canal 10 (9 desde 0) es percusión, banco 128."""
    assert mid[:4] == b"MThd"
    ntrk = struct.unpack(">H", mid[10:12])[0]
    pos = 8 + struct.unpack(">I", mid[4:8])[0]
    usados = set()
    prog = [0] * 16
    for _ in range(ntrk):
        assert mid[pos:pos + 4] == b"MTrk"
        p, fin = pos + 8, pos + 8 + struct.unpack(">I", mid[pos + 4:pos + 8])[0]
        estado = 0
        while p < fin:
            while mid[p] & 0x80:  # delta
                p += 1
            p += 1
            b = mid[p]
            if b == 0xFF:
                q, n = p + 2, 0
                while True:
                    n = (n << 7) | (mid[q] & 0x7F)
                    q += 1
                    if not mid[q - 1] & 0x80:
                        break
                p = q + n
                continue
            if b in (0xF0, 0xF7):
                q, n = p + 1, 0
                while True:
                    n = (n << 7) | (mid[q] & 0x7F)
                    q += 1
                    if not mid[q - 1] & 0x80:
                        break
                p = q + n
                continue
            if b & 0x80:
                estado, p = b, p + 1
            tipo, canal = estado & 0xF0, estado & 0x0F
            if tipo == 0xC0:
                prog[canal] = mid[p]
                p += 1
            elif tipo == 0xD0:
                p += 1
            else:
                if tipo == 0x90 and mid[p + 1] > 0:
                    usados.add((128, prog[canal]) if canal == 9 else (0, prog[canal]))
                p += 2
        pos = fin
    return usados


# SoundFont ------------------------------------------------------------------------

def _trozos(d, ini, fin):
    out = {}
    i = ini
    while i < fin:
        cid, ln = d[i:i + 4], struct.unpack_from("<I", d, i + 4)[0]
        if cid == b"LIST":
            out[d[i + 8:i + 12]] = (i + 12, i + 8 + ln)
        else:
            out[cid] = (i + 8, i + 8 + ln)
        i += 8 + ln + (ln & 1)
    return out


def _registros(d, rango, tam):
    a, b = rango
    return [d[a + i:a + i + tam] for i in range(0, b - a, tam)]


def recortar_sf2(sf2, usados):
    """SF2 nuevo con solo los presets (banco, programa) de usados, sus
    instrumentos y sus muestras (y las que estas enlazan en estéreo)."""
    assert sf2[:4] == b"RIFF" and sf2[8:12] == b"sfbk"
    top = _trozos(sf2, 12, 8 + struct.unpack_from("<I", sf2, 4)[0])
    info, sdta, pdta = top[b"INFO"], top[b"sdta"], top[b"pdta"]
    smpl = _trozos(sf2, *sdta)[b"smpl"]
    p = _trozos(sf2, *pdta)
    phdr = _registros(sf2, p[b"phdr"], 38)
    pbag = _registros(sf2, p[b"pbag"], 4)
    pmod = _registros(sf2, p[b"pmod"], 10)
    pgen = _registros(sf2, p[b"pgen"], 4)
    inst = _registros(sf2, p[b"inst"], 22)
    ibag = _registros(sf2, p[b"ibag"], 4)
    imod = _registros(sf2, p[b"imod"], 10)
    igen = _registros(sf2, p[b"igen"], 4)
    shdr = _registros(sf2, p[b"shdr"], 46)
    u16 = lambda r, o: struct.unpack_from("<H", r, o)[0]

    def bags(lista, i):
        return range(u16(lista[i], 20 if len(lista[i]) == 22 else 24), u16(lista[i + 1], 20 if len(lista[i]) == 22 else 24))

    presets = [i for i in range(len(phdr) - 1) if (u16(phdr[i], 22), u16(phdr[i], 20)) in usados]
    instrumentos, muestras = [], []
    for i in presets:
        for b in bags(phdr, i):
            for g in range(u16(pbag[b], 0), u16(pbag[b + 1], 0)):
                if u16(pgen[g], 0) == 41 and u16(pgen[g], 2) not in instrumentos:
                    instrumentos.append(u16(pgen[g], 2))
    for i in instrumentos:
        for b in bags(inst, i):
            for g in range(u16(ibag[b], 0), u16(ibag[b + 1], 0)):
                if u16(igen[g], 0) == 53 and u16(igen[g], 2) not in muestras:
                    muestras.append(u16(igen[g], 2))
    k = 0
    while k < len(muestras):  # muestras enlazadas (estéreo)
        s = muestras[k]
        enlace, tipo = u16(shdr[s], 42), u16(shdr[s], 44)
        if tipo & 0x0E and enlace < len(shdr) - 1 and enlace not in muestras:
            muestras.append(enlace)
        k += 1
    muestras.sort()
    nuevo_inst = {v: n for n, v in enumerate(instrumentos)}
    nueva_muestra = {v: n for n, v in enumerate(muestras)}

    # Muestras: sus datos seguidos de 46 ceros, como pide la especificación.
    datos = bytearray()
    n_shdr = []
    for s in muestras:
        r = shdr[s]
        ini, fin, li, lf = struct.unpack_from("<IIII", r, 20)
        base = len(datos) // 2
        datos += sf2[smpl[0] + 2 * ini:smpl[0] + 2 * fin] + b"\0" * 92
        enlace = u16(r, 42)
        enlace = nueva_muestra.get(enlace, 0)
        n_shdr.append(r[:20] + struct.pack("<IIII", base, base + fin - ini, base + li - ini, base + lf - ini) +
                      r[36:42] + struct.pack("<HH", enlace, u16(r, 44)))
    n_shdr.append(shdr[-1])

    def copiar(cabeceras, indices, bag, mod, gen, gen_ref, remap, tam_nombre_bag):
        n_cab, n_bag, n_mod, n_gen = [], [], [], []
        for i in indices:
            r = cabeceras[i]
            n_cab.append(r[:tam_nombre_bag] + struct.pack("<H", len(n_bag)) + r[tam_nombre_bag + 2:])
            for b in range(u16(r, tam_nombre_bag), u16(cabeceras[i + 1], tam_nombre_bag)):
                n_bag.append(struct.pack("<HH", len(n_gen), len(n_mod)))
                for m in range(u16(bag[b], 2), u16(bag[b + 1], 2)):
                    n_mod.append(mod[m])
                for g in range(u16(bag[b], 0), u16(bag[b + 1], 0)):
                    r2 = gen[g]
                    if u16(r2, 0) == gen_ref:
                        r2 = r2[:2] + struct.pack("<H", remap[u16(r2, 2)])
                    n_gen.append(r2)
        fin = cabeceras[-1]
        n_cab.append(fin[:tam_nombre_bag] + struct.pack("<H", len(n_bag)) + fin[tam_nombre_bag + 2:])
        n_bag.append(struct.pack("<HH", len(n_gen), len(n_mod)))
        n_mod.append(mod[-1])
        n_gen.append(gen[-1])
        return n_cab, n_bag, n_mod, n_gen

    n_phdr, n_pbag, n_pmod, n_pgen = copiar(phdr, presets, pbag, pmod, pgen, 41, nuevo_inst, 24)
    n_inst, n_ibag, n_imod, n_igen = copiar(inst, instrumentos, ibag, imod, igen, 53, nueva_muestra, 20)

    def trozo(cid, cuerpo):
        cuerpo = bytes(cuerpo)
        return cid + struct.pack("<I", len(cuerpo)) + cuerpo + (b"\0" if len(cuerpo) & 1 else b"")

    def lista(tipo, cuerpo):
        return trozo(b"LIST", tipo + cuerpo)

    pdta_n = b"".join(trozo(c, b"".join(r)) for c, r in (
        (b"phdr", n_phdr), (b"pbag", n_pbag), (b"pmod", n_pmod), (b"pgen", n_pgen), (b"inst", n_inst),
        (b"ibag", n_ibag), (b"imod", n_imod), (b"igen", n_igen), (b"shdr", n_shdr)))
    cuerpo = b"sfbk" + lista(b"INFO", sf2[info[0]:info[1]]) + lista(b"sdta", trozo(b"smpl", datos)) + \
        lista(b"pdta", pdta_n)
    resumen = {"presets": len(presets), "instrumentos": len(instrumentos), "muestras": len(muestras),
               "presets_original": len(phdr) - 1, "muestras_original": len(shdr) - 1}
    return b"RIFF" + struct.pack("<I", len(cuerpo)) + cuerpo, resumen


def presets_sf2(sf2):
    """Pares (banco, programa) de los presets de un SF2."""
    top = _trozos(sf2, 12, 8 + struct.unpack_from("<I", sf2, 4)[0])
    p = _trozos(sf2, *top[b"pdta"])
    phdr = _registros(sf2, p[b"phdr"], 38)
    return {(struct.unpack_from("<H", r, 22)[0], struct.unpack_from("<H", r, 20)[0]) for r in phdr[:-1]}
