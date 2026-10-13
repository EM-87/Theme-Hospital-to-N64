"""Convierte una fuente de TH (hoja .TAB/.DAT, carácter c = sprite c-31, CP437)
en BMFont de texto (.fnt + atlas PNG RGBA) para mkfont. También mide la
cobertura de los caracteres que usa un LANG-x.DAT.
Uso: python3 -I th2bmfont.py <TAB> <DAT> <MPALETTE.DAT> <salida_base> [LANG.DAT]"""
import collections, os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "datos"))
import graficos
from PIL import Image
tabf, datf, palf, outbase = sys.argv[1:5]
lang = sys.argv[5] if len(sys.argv) > 5 else None
tab = open(tabf, "rb").read(); dat = open(datf, "rb").read(); p = open(palf, "rb").read()
pal = [tuple(min(255, 4 * p[3 * i + k]) for k in range(3)) for i in range(256)]
glyphs = {}
for s in range(len(tab) // 6):
    off, w, h = struct.unpack_from("<IBB", tab, 6 * s)
    c = s + 31
    if c < 32 or c > 255:
        continue
    if not w or not h:
        continue
    glyphs[c] = (w, h, graficos.decode(dat[off:], w, h, False))
# Atlas simple en filas
maxh = max(g[1] for g in glyphs.values())
W = 256; x = y = 0; rows = []
place = {}
for c, (w, h, px) in sorted(glyphs.items()):
    if x + w > W:
        x = 0; y += maxh + 1
    place[c] = (x, y); x += w + 1
H = y + maxh + 1
img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
colors = set()
for c, (w, h, px) in glyphs.items():
    gx, gy = place[c]
    for j in range(h):
        for i in range(w):
            v = px[j * w + i]
            if v != 0xFF:
                img.putpixel((gx + i, gy + j), pal[v] + (255,)); colors.add(v)
img.save(outbase + ".png")
lines = [f'info face="{outbase}" size={maxh} bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=0 aa=1 padding=0,0,0,0 spacing=1,1',
         f"common lineHeight={maxh + 1} base={maxh} scaleW={W} scaleH={H} pages=1 packed=0",
         f'page id=0 file="{outbase.split("/")[-1]}.png"',
         f"chars count={len(glyphs)}"]
uni = {}
for c, (w, h, px) in sorted(glyphs.items()):
    u = ord(bytes([c]).decode("cp437"))
    uni[c] = u
    gx, gy = place[c]
    lines.append(f"char id={u} x={gx} y={gy} width={w} height={h} xoffset=0 yoffset=0 xadvance={w + 1} page=0 chnl=15")
open(outbase + ".fnt", "w").write("\n".join(lines) + "\n")
print(f"{outbase}: {len(glyphs)} glifos, atlas {W}x{H}, {len(colors)} colores de paleta, alto max {maxh}")
es = "áéíóúñÑüÜ¿¡ÁÉÍÓÚ"
have = {chr(u) for u in uni.values()}
print("  español presentes:", "".join(ch for ch in es if ch in have), "| ausentes:", "".join(ch for ch in es if ch not in have))
if lang:
    d = open(lang, "rb").read()
    nsec = struct.unpack_from("<H", d, 0)[0]
    pos = 2 + 2 * nsec
    used = collections.Counter(b for b in d[pos:] if b)
    missing = {b: n for b, n in used.items() if b >= 32 and b not in glyphs}
    ctrl = {b: n for b, n in used.items() if b < 32}
    hi = sorted(b for b in used if b >= 128)
    print(f"  {lang.split('/')[-1]}: {len(used)} códigos distintos; >=128: {[bytes([b]).decode('cp437') for b in hi]}")
    print(f"  códigos sin glifo: {[(hex(b), bytes([b]).decode('cp437'), n) for b, n in sorted(missing.items())]}; control <32: {ctrl}")
