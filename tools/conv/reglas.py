"""Reglas de CorsixTH al formato TRG1 del motor (fase 5, hito 0).

reglas.lua (con lua5.4) extrae de CorsixTH las definiciones de objetos, salas,
enfermedades y diagnósticos, los marcadores, las duraciones fijadas a mano,
las animaciones de caminar, base_config.lua y la configuración de los 36
niveles de la campaña (3 dificultades × 12), en un JSON canónico. Aquí:
  * se resuelven las duraciones que dependen de otra animación
    (getAnimLength(N)) con las tablas de animación;
  * el aspecto de cada enfermedad (todas las variantes de initPatient, con su
    peso) se factoriza en tipo y capas independientes cuando es exacto;
  * se escribe en TRG1 y se comprueba la ida y vuelta.

TRG1 (big-endian): un árbol de valores como el de Lua.
  cabecera (16 B): 'TRG1', u32 tamaño del fichero, u32 posición del valor
    raíz, u32 0
  valor (8 B): u8 tipo, u8 0, u16 0, u32 dato
    tipo 0 nil, 1 falso, 2 verdadero;
    3 entero: dato = i32;
    4 real: dato = posición de un double IEEE 754 (alineado a 8);
    5 cadena: dato = posición de [u32 longitud][bytes][0] (sin repetir);
    6 lista: dato = posición de [u32 n][n valores];
    7 mapa: dato = posición de [u32 n][n × (clave, valor)], con las claves
      enteras primero, en orden creciente, y después las de texto, por bytes.
Las claves enteras de Lua van en el JSON como "#n" y en TRG1 como enteros.
"""
import json
import os
import struct
import subprocess
import tempfile
from fractions import Fraction

MAGIA = b"TRG1"
NIL, FALSO, VERDADERO, ENTERO, REAL, CADENA, LISTA, MAPA = range(8)
AQUI = os.path.dirname(os.path.abspath(__file__))


def extraer(lua_dir, levels_dir):
    """Ejecuta reglas.lua y devuelve el JSON canónico y su salida de texto."""
    with tempfile.TemporaryDirectory() as tmp:
        ruta = os.path.join(tmp, "reglas.json")
        r = subprocess.run(["lua5.4", os.path.join(AQUI, "reglas.lua"), lua_dir, levels_dir, ruta],
                           capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError("reglas.lua: " + r.stderr.strip())
        return json.load(open(ruta)), r.stdout.strip()


def extraer_textos(lua_dir, data_dir, idiomas):
    """Ejecuta textos.lua y devuelve {idioma: tabla _S} en JSON canónico."""
    with tempfile.TemporaryDirectory() as tmp:
        ruta = os.path.join(tmp, "textos.json")
        r = subprocess.run(["lua5.4", os.path.join(AQUI, "textos.lua"), lua_dir, data_dir, ruta, *idiomas],
                           capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError("textos.lua: " + r.stderr.strip())
        return json.load(open(ruta, encoding="utf-8"))


def resolver_duraciones(doc, duraciones_anim):
    """Las duraciones fijadas como getAnimLength(N) pasan a un número."""
    for d in doc["duraciones"]:
        v = d["duracion"]
        if isinstance(v, dict) and v.get("$ref", "").startswith("getAnimLength("):
            d["duracion"] = duraciones_anim[int(v["$ref"][len("getAnimLength("):-1])]


def factorizar_aspecto(a):
    """Factoriza el aspecto de una enfermedad: el tipo de paciente y, dentro de
    cada tipo, bloques de capas independientes entre sí (las capas que dependen
    unas de otras van juntas en un bloque). Los bloques salen de la dependencia
    por pares, y al final se comprueba con fracciones que el producto de las
    probabilidades da exactamente la de cada variante. Si no, None."""
    total = a["total"]
    tipos = {}
    for v in a["variantes"]:
        tipos.setdefault(v["tipo"], []).append(v)
    out = []
    for nombre in sorted(tipos):
        vs = tipos[nombre]
        peso = sum(v["peso"] for v in vs)
        capas = sorted({c for v in vs for c in v["capas"]}, key=lambda c: int(c[1:]))
        if any(set(v["capas"]) != set(capas) for v in vs):
            return None

        def marginal(cs):
            m = {}
            for v in vs:
                k = tuple(v["capas"][c] for c in cs)
                m[k] = m.get(k, 0) + v["peso"]
            return m

        # Dependencia por pares: P(a, b) != P(a) P(b) para algún par de valores.
        padre = {c: c for c in capas}

        def raiz(c):
            while padre[c] != c:
                c = padre[c]
            return c

        for i, ca in enumerate(capas):
            for cb in capas[i + 1:]:
                ma, mb, mab = marginal([ca]), marginal([cb]), marginal([ca, cb])
                indep = all(Fraction(mab.get((x[0], y[0]), 0), peso) ==
                            Fraction(ma[x], peso) * Fraction(mb[y], peso) for x in ma for y in mb)
                if not indep:
                    padre[raiz(cb)] = raiz(ca)
        bloques = {}
        for c in capas:
            bloques.setdefault(raiz(c), []).append(c)
        bloques = sorted(bloques.values(), key=lambda b: int(b[0][1:]))
        margs = [marginal(b) for b in bloques]
        combinaciones = 1
        for m in margs:
            combinaciones *= len(m)
        if combinaciones != len(vs):
            return None
        for v in vs:
            p = Fraction(peso, total)
            for b, m in zip(bloques, margs):
                p *= Fraction(m[tuple(v["capas"][c] for c in b)], peso)
            if p != Fraction(v["peso"], total):
                return None
        out.append({"tipo": nombre, "peso": peso, "bloques": [
            {"capas": [int(c[1:]) for c in b], "valores": [[list(k), w] for k, w in sorted(m.items())]}
            for b, m in zip(bloques, margs)]})
    return {"tipos": out, "total": total}


def preparar(doc, duraciones_anim):
    """Resuelve y factoriza; devuelve un resumen."""
    resolver_duraciones(doc, duraciones_anim)
    fact = 0
    for e in doc["enfermedades"].values():
        f = factorizar_aspecto(e["aspecto"])
        if f:
            e["aspecto"] = f
            fact += 1
    # Los marcadores salen en el orden de pairs(): se ordenan, y se comprueba
    # que ninguna animación recibe dos marcadores distintos del mismo tipo.
    clave = lambda m: (m["tipo"], m["animacion"], json.dumps(m["args"], sort_keys=True))
    doc["marcadores"].sort(key=clave)
    vistos = {}
    for m in doc["marcadores"]:
        k = (m["tipo"], m["animacion"])
        if k in vistos and vistos[k] != m["args"]:
            raise RuntimeError(f"marcadores distintos para la animación {k}")
        vistos[k] = m["args"]
    return {"aspectos_factorizados": fact, "aspectos": len(doc["enfermedades"]), "marcadores": len(doc["marcadores"])}


# TRG1 ---------------------------------------------------------------------------

def _clave(k):
    return (0, int(k[1:]), b"") if k.startswith("#") else (1, 0, k.encode())


def escribir(arbol):
    datos = bytearray(16)
    cadenas = {}

    def alinear(n):
        while len(datos) % n:
            datos.append(0)

    def cadena(s):
        b = s.encode()
        if b not in cadenas:
            alinear(4)
            cadenas[b] = len(datos)
            datos.extend(struct.pack(">I", len(b)) + b + b"\0")
        return cadenas[b]

    def valor(v):
        """Devuelve los 8 B del valor; lo que cuelga de él va al final."""
        if v is None:
            return struct.pack(">BBHI", NIL, 0, 0, 0)
        if v is True or v is False:
            return struct.pack(">BBHI", VERDADERO if v else FALSO, 0, 0, 0)
        if isinstance(v, int):
            if not -2**31 <= v < 2**31:
                raise ValueError(f"entero fuera de i32: {v}")
            return struct.pack(">BBHi", ENTERO, 0, 0, v)
        if isinstance(v, float):
            alinear(8)
            pos = len(datos)
            datos.extend(struct.pack(">d", v))
            return struct.pack(">BBHI", REAL, 0, 0, pos)
        if isinstance(v, str):
            return struct.pack(">BBHI", CADENA, 0, 0, cadena(v))
        if isinstance(v, list):
            hijos = [valor(x) for x in v]
            alinear(4)
            pos = len(datos)
            datos.extend(struct.pack(">I", len(hijos)) + b"".join(hijos))
            return struct.pack(">BBHI", LISTA, 0, 0, pos)
        if isinstance(v, dict):
            pares = []
            for k in sorted(v, key=_clave):
                kv = struct.pack(">BBHi", ENTERO, 0, 0, int(k[1:])) if k.startswith("#") else \
                    struct.pack(">BBHI", CADENA, 0, 0, cadena(k))
                pares.append(kv + valor(v[k]))
            alinear(4)
            pos = len(datos)
            datos.extend(struct.pack(">I", len(pares)) + b"".join(pares))
            return struct.pack(">BBHI", MAPA, 0, 0, pos)
        raise TypeError(type(v))

    raiz = valor(arbol)
    alinear(4)
    pos = len(datos)
    datos.extend(raiz)
    datos[0:16] = MAGIA + struct.pack(">III", len(datos), pos, 0)
    return bytes(datos)


def leer(datos):
    if datos[:4] != MAGIA:
        raise ValueError("no es TRG1")
    tam, raiz, _ = struct.unpack_from(">III", datos, 4)
    if tam != len(datos):
        raise ValueError("tamaño incorrecto")

    def cadena(pos):
        n = struct.unpack_from(">I", datos, pos)[0]
        return datos[pos + 4:pos + 4 + n].decode()

    def valor(pos):
        tipo, _, _, dato = struct.unpack_from(">BBHI", datos, pos)
        if tipo == NIL:
            return None
        if tipo in (FALSO, VERDADERO):
            return tipo == VERDADERO
        if tipo == ENTERO:
            return struct.unpack_from(">i", datos, pos + 4)[0]
        if tipo == REAL:
            return struct.unpack_from(">d", datos, dato)[0]
        if tipo == CADENA:
            return cadena(dato)
        n = struct.unpack_from(">I", datos, dato)[0]
        if tipo == LISTA:
            return [valor(dato + 4 + 8 * i) for i in range(n)]
        if tipo == MAPA:
            out = {}
            for i in range(n):
                k = valor(dato + 4 + 16 * i)
                out["#%d" % k if isinstance(k, int) else k] = valor(dato + 4 + 16 * i + 8)
            return out
        raise ValueError(f"tipo desconocido {tipo}")

    return valor(raiz)


def igual(a, b):
    """Igualdad estricta: 1 y 1.0 son distintos, como en Lua."""
    if type(a) is not type(b):
        return False
    if isinstance(a, dict):
        return a.keys() == b.keys() and all(igual(a[k], b[k]) for k in a)
    if isinstance(a, list):
        return len(a) == len(b) and all(igual(x, y) for x, y in zip(a, b))
    return a == b


def ficheros(doc):
    """Reparte el documento: las definiciones en un fichero y cada nivel en
    otro (solo se carga el de la partida). Devuelve {ruta relativa: árbol}."""
    out = {"reglas/definiciones.trg": {k: v for k, v in doc.items() if k != "niveles"}}
    for dif, lista in doc["niveles"].items():
        for n, cfg in enumerate(lista, 1):
            out[f"reglas/{dif}{n:02d}.trg"] = cfg
    return out


def comprobar_c(th64sim, dir_assets):
    """Compara el lector del núcleo (th64sim reglas) con leer() en cada TRG1
    de dir_assets/reglas y dir_assets/textos. Devuelve (ficheros, distintos)."""
    nombres = []
    for sub in ("reglas", "textos"):
        d = os.path.join(dir_assets, sub)
        if os.path.isdir(d):
            nombres += [os.path.join(sub, f) for f in sorted(os.listdir(d)) if f.endswith(".trg")]
    distintos = []
    for f in nombres:
        ruta = os.path.join(dir_assets, f)
        r = subprocess.run([th64sim, "reglas", ruta], capture_output=True, text=True)
        if r.returncode != 0 or not igual(json.loads(r.stdout), leer(open(ruta, "rb").read())):
            distintos.append(f)
    return len(nombres), distintos


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 4 or sys.argv[1] != "comprobar":
        sys.exit("uso: python3 -I tools/conv/reglas.py comprobar <th64sim> <dir assets>")
    total, mal = comprobar_c(sys.argv[2], sys.argv[3])
    print(f"reglas y textos: {total - len(mal)} de {total} ficheros iguales en el núcleo y en el conversor"
          + (f"; distintos: {', '.join(mal)}" if mal else ""))
    sys.exit(1 if mal else 0)
