"""Lectura de las animaciones y los sprites de alta resolución de TH (DATA)
para las herramientas de arte. Rutas: TH_DATA_DIR y CORSIXTH_DIR."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "conv"))
import anim  # noqa: E402
import rnc  # noqa: E402

TH = os.environ.get("TH_DATA_DIR", os.path.expanduser("~/th64-work/data/HOSP"))
CORSIXTH = os.environ.get("CORSIXTH_DIR", os.path.expanduser("~/th64-work/src/CorsixTH"))
LUA = os.path.join(CORSIXTH, "CorsixTH", "Lua")


def leer(carpeta, nombre):
    datos = open(os.path.join(TH, carpeta, nombre), "rb").read()
    return rnc.descomprimir(datos) if rnc.es_rnc(datos) else datos


class Animaciones:
    """VSTART, VFRA, VLIST y VELE de DATA."""

    def __init__(self):
        self.primeros, self.fotogramas, self.lista, self.elementos = anim.leer(
            leer("DATA", "VSTART-1.ANI"), leer("DATA", "VFRA-1.ANI"), leer("DATA", "VLIST-1.ANI"),
            leer("DATA", "VELE-1.ANI"))

    def __len__(self):
        return len(self.primeros)

    def cadena(self, a):
        """Los fotogramas de la animación a, hasta que se repite."""
        f, vistos = self.primeros[a], []
        while f not in vistos:
            vistos.append(f)
            f = self.fotogramas[f][1]
        return vistos

    def elementos_de(self, f):
        i, out = self.fotogramas[f][0], []
        while self.lista[i] != 0xFFFF:
            out.append(self.elementos[self.lista[i]])
            i += 1
        return out

    def sprites(self, fotogramas):
        return {e[0] for f in fotogramas for e in self.elementos_de(f)}

    def vacia(self, a):
        """Sin fotograma propio (apunta al 0) o sin elementos."""
        return self.primeros[a] == 0 or all(not self.elementos_de(f) for f in self.cadena(a))
