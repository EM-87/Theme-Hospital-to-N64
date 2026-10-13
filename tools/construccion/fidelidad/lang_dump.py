# Lee LANG-x.DAT (sin RNC): u16 nsecciones, u16 por sección, cadenas terminadas en 0 (CP437).
import sys, struct, re
data = open(sys.argv[1], 'rb').read()
n = struct.unpack_from('<H', data, 0)[0]
counts = struct.unpack_from('<%dH' % n, data, 2)
pos = 2 + 2 * n
S = []
for c in counts:
    sec = []
    for _ in range(c):
        end = data.index(b'\0', pos)
        sec.append(data[pos:end].decode('cp437'))
        pos = end + 1
    S.append(sec)
def get(a, b):  # índices 1-based, como S[a][b] en original_strings.lua
    return S[a - 1][b - 1]
if len(sys.argv) > 2 and sys.argv[2] == 'grep':
    rx = re.compile(sys.argv[3], re.I)
    for i, sec in enumerate(S, 1):
        for j, s in enumerate(sec, 1):
            if rx.search(s): print('S[%d][%d]: %s' % (i, j, s))
else:
    print('secciones', n, 'cadenas', sum(counts))
    for arg in sys.argv[2:]:
        a, b = map(int, arg.split(','))
        print('S[%d][%d]: %s' % (a, b, get(a, b)))
