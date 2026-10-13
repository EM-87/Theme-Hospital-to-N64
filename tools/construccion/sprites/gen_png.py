"""PNG indexados sintéticos para probar mksprite."""
import random
from PIL import Image
random.seed(64)
# Paleta tipo TH: 256 entradas, colores distintos y deliberadamente "desordenados"
pal = []
for i in range(256):
    pal += [(i * 37) % 256, (i * 91 + 13) % 256, (255 - i * 53) % 256]

def make(name, w, h, choices, trns_255=True):
    im = Image.new("P", (w, h))
    im.putpalette(pal)
    px = [random.choice(choices) for _ in range(w * h)]
    im.putdata(px)
    kw = {}
    if trns_255:
        kw["transparency"] = bytes([255] * 255 + [0])  # solo el 255 transparente
    im.save(name, **kw)
    open(name + ".idx", "wb").write(bytes(px))

# 1) CI8: índices altos y bajos mezclados + transparente 255, ancho impar
make("ci8_impar.png", 13, 7, [0, 1, 7, 100, 200, 254, 255, 255])
# 2) solo índices 0..14 y el 255 transparente (como un sprite TH de pocos colores)
make("pocos_col_255.png", 16, 8, list(range(15)) + [255])
# 3) índices 0..15 con el 15 transparente (lo que tendría que generar el conversor para CI4)
pal_save = pal
make("ci4_15.png", 10, 4, list(range(16)), trns_255=False)
