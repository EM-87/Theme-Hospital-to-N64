"""RAM de los sprites de DATAM como .sprite individuales (mksprite) frente a un
formato propio por hoja. Uso: python3 -I overhead.py <TH_DATA_DIR>"""
import os, struct, sys
r8 = lambda x: (x + 7) // 8 * 8
tot_sprite = tot_sprite_ci4pal = tot_own = tot_own_nopad = n = 0
for name in ("MSPR-0", "MBLK-0"):
    tab = open(os.path.join(sys.argv[1], "DATAM", name + ".TAB"), "rb").read()
    for k in range(0, len(tab) - 5, 6):
        off, w, h = struct.unpack_from("<IBB", tab, k)
        if not w or not h:
            continue
        n += 1
        # mksprite: cabecera 8 + píxeles sin alinear filas (redondeado a 8) + ext 128 + paleta 256x2 (sprite.c, mksprite.c:1462-1690)
        tot_sprite += 8 + r8(w * h) + 128 + 512
        # formato propio: entrada de tabla de 8 B + filas alineadas a 8 B (LOAD_BLOCK posible)
        tot_own += 8 + r8(w) * h
        tot_own_nopad += 8 + w * h
pal = 512
print(f"sprites {n}")
print(f".sprite individuales: {tot_sprite} B = {tot_sprite/2**20:.2f} MiB (de ellos {n*(8+128+512)/2**20:.2f} MiB de cabeceras y paletas)")
print(f"formato propio, filas a 8 B: {tot_own + pal} B = {(tot_own+pal)/2**20:.2f} MiB")
print(f"formato propio, filas sin rellenar: {tot_own_nopad + pal} B = {(tot_own_nopad+pal)/2**20:.2f} MiB")
