#!/usr/bin/env python3
"""Compara una captura de CorsixTH con la vista que dibuja th64sim en la misma
posición de cámara, píxel a píxel.

Uso: python3 -I tools/oraculo_mapa/comparar.py <captura.png> <vista.ppm> [diferencias.png]
     [--excluir x0,y0,x1,y1 ...] [--mezcla mezcla.pgm]

Cuenta los píxeles iguales, los que difieren en 2 o menos por componente (el
redondeo de la mezcla de las sombras) y los distintos, y agrupa estos en
rectángulos (componentes conexas) para ver qué los causa: objetos que el motor
aún no dibuja (las puertas), el cursor, etc. Con --excluir se descuentan
rectángulos conocidos. Con --mezcla (la que escribe th64sim vista) dice
cuántas de las diferencias pequeñas caen en píxeles de sombra. Si se le da una ruta, guarda una imagen con los
distintos en rojo sobre la captura atenuada.
"""
import sys

from PIL import Image


def componentes(mascara, w, h):
    """Rectángulos envolventes de las zonas conexas (vecindad 8) de la máscara."""
    visto = bytearray(w * h)
    cajas = []
    for i in range(w * h):
        if not mascara[i] or visto[i]:
            continue
        pila = [i]
        visto[i] = 1
        x0 = x1 = i % w
        y0 = y1 = i // w
        n = 0
        while pila:
            j = pila.pop()
            n += 1
            x, y = j % w, j // w
            x0, x1, y0, y1 = min(x0, x), max(x1, x), min(y0, y), max(y1, y)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h:
                        k = ny * w + nx
                        if mascara[k] and not visto[k]:
                            visto[k] = 1
                            pila.append(k)
        cajas.append((n, x0, y0, x1, y1))
    return sorted(cajas, reverse=True)


def main():
    args = sys.argv[1:]
    excluir = []
    mezcla = None
    if "--mezcla" in args:
        i = args.index("--mezcla")
        mezcla = Image.open(args[i + 1]).convert("L").tobytes()
        del args[i:i + 2]
    while "--excluir" in args:
        i = args.index("--excluir")
        excluir.append(tuple(int(v) for v in args[i + 1].split(",")))
        del args[i:i + 2]
    if len(args) not in (2, 3):
        sys.exit(__doc__)
    a = Image.open(args[0]).convert("RGB")
    b = Image.open(args[1]).convert("RGB")
    if a.size != b.size:
        sys.exit(f"tamaños distintos: {a.size} y {b.size}")
    w, h = a.size
    pa, pb = a.tobytes(), b.tobytes()
    iguales = cercanos = cercanos_sombra = excluidos = 0
    mascara = bytearray(w * h)
    for i in range(w * h):
        x, y = i % w, i // w
        if any(x0 <= x <= x1 and y0 <= y <= y1 for x0, y0, x1, y1 in excluir):
            excluidos += 1
            continue
        d = max(abs(pa[3 * i + k] - pb[3 * i + k]) for k in range(3))
        if d == 0:
            iguales += 1
        elif d <= 2:
            cercanos += 1
            cercanos_sombra += bool(mezcla and mezcla[i])
        else:
            mascara[i] = 1
    distintos = sum(mascara)
    total = w * h - excluidos
    print(f"{w}x{h}: {total} píxeles comparados ({excluidos} excluidos); {iguales} iguales, "
          f"{cercanos} a 2 o menos, {distintos} distintos ({100 * distintos / max(total, 1):.3f} %)")
    if mezcla:
        print(f"  de los que difieren en 2 o menos, {cercanos_sombra} son de sombra "
              f"(de {sum(1 for v in mezcla if v)} píxeles de sombra)")
    for n, x0, y0, x1, y1 in componentes(mascara, w, h)[:12]:
        print(f"  {n:6d} píxeles en ({x0},{y0})-({x1},{y1})")
    if len(args) == 3:
        img = a.point(lambda v: v // 3)
        px = img.load()
        for i in range(w * h):
            if mascara[i]:
                px[i % w, i // w] = (255, 0, 0)
        img.save(args[2])
    return 0 if distintos == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
