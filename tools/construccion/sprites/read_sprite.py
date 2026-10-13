"""Lee un .sprite (sin comprimir) según sprite_internal.h y lo compara con el PNG fuente."""
import struct, sys
from PIL import Image
FMT = {0: "NONE", 2: "RGBA16", 3: "RGBA32", 6: "YUV16", 8: "CI4", 9: "CI8", 14: "IA4", 15: "IA8", 13: "IA16", 16: "I4", 17: "I8"}
def conv5551(r, g, b, a):
    return ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (1 if a >= 128 else 0)
def check(spr, png, idxfile):
    d = open(spr, "rb").read()
    w, h, _dep, flags, hs, vs = struct.unpack_from(">HHBBBB", d, 0)
    fmt = flags & 0x1F
    bpp = {8: 4, 9: 8}[fmt]
    stride = (w * bpp + 7) // 8
    pxbytes = stride * h
    px = d[8:8 + pxbytes]
    ext = 8 + ((pxbytes + 7) // 8) * 8
    size, ver, palpos = struct.unpack_from(">HHI", d, ext)
    xflags, used, _ = struct.unpack_from(">HBB", d, ext + 8 + 7 * 8)
    pal = d[palpos:]
    npal = len(pal) // 2
    src = open(idxfile, "rb").read()
    if bpp == 8:
        got = list(px)
    else:
        got = []
        for y in range(h):
            row = px[y * stride:(y + 1) * stride]
            nib = [v for b in row for v in (b >> 4, b & 15)]
            got += nib[:w]
    im = Image.open(png); p = im.getpalette(); trns = im.info.get("transparency")
    exp_pal = [conv5551(p[3*i], p[3*i+1], p[3*i+2], (trns[i] if isinstance(trns, bytes) and i < len(trns) else (0 if trns == i else 255))) for i in range(256)]
    got_pal = [struct.unpack_from(">H", pal, 2*i)[0] for i in range(npal)]
    print(f"{spr}: {len(d)} B, {w}x{h} {FMT.get(fmt)} flags=0x{flags:02x} ext v{ver} size {size}, "
          f"cabecera 8 + pixeles {pxbytes} (stride {stride}, sin alinear filas) + ext 128, paleta en +{palpos} "
          f"con {npal} colores ({npal*2} B), pal_used_colors={used or 256}, fits_tmem={(xflags>>5)&1}")
    print("   indices identicos al PNG:", got == list(src),
          "| paleta RGBA5551 identica:", got_pal == exp_pal[:npal],
          "| alfa de la 255:", (got_pal[255] & 1) if npal > 255 else None,
          "| alfa de la 0:", got_pal[0] & 1)
for spr, png in [("auto/ci8_impar.sprite", "ci8_impar.png"), ("auto/pocos_col_255.sprite", "pocos_col_255.png"),
                 ("auto/ci4_15.sprite", "ci4_15.png"), ("ci8/ci4_15.sprite", "ci4_15.png")]:
    check(spr, png, png + ".idx")
