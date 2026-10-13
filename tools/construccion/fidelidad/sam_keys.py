# Extrae las claves de los .SAM originales (campaña) y comprueba si el Lua de CorsixTH las lee.
import sys, os, re, glob, json, collections
levels_dir, lua_dir, out = sys.argv[1], sys.argv[2], sys.argv[3]
keyre = re.compile(r'^#([A-Za-z_]+)(\[\d+\])?((?:\.[A-Za-z_]+)*)')
fields = collections.defaultdict(set)   # top -> set(fields)
files = sorted(glob.glob(os.path.join(levels_dir, '*.SAM')))
camp = [f for f in files if re.match(r'(FULL|EASY|HARD)\d\d\.SAM$', os.path.basename(f))]
nlines = 0
for f in camp:
    for line in open(f, 'rb').read().decode('latin-1').splitlines():
        m = keyre.match(line.strip())
        if not m: continue
        nlines += 1
        top = m.group(1)
        flds = [x for x in m.group(3).split('.') if x]
        if not flds: flds = ['<value>']
        for x in flds: fields[top].add(x)
# Lua source (sin idiomas ni base_config, que solo declara valores por defecto)
src = []
for root, _, fs in os.walk(lua_dir):
    if '/languages' in root: continue
    for fn in fs:
        if fn.endswith('.lua') and fn != 'base_config.lua':
            src.append(open(os.path.join(root, fn), encoding='utf-8', errors='replace').read())
src = '\n'.join(src)
res = {}
tot = used = 0
for top in sorted(fields):
    top_used = re.search(r'\b' + re.escape(top) + r'\b', src) is not None
    for fl in sorted(fields[top]):
        if fl == '<value>':
            u = top_used
        else:
            u = top_used and re.search(r'\b' + re.escape(fl) + r'\b', src) is not None
        res.setdefault(top, {})[fl] = u
        tot += 1; used += u
json.dump({'campaign_files': len(camp), 'key_lines': nlines, 'fields_total': tot, 'fields_used': used, 'detail': res}, open(out, 'w'), indent=1)
print('ficheros', len(camp), 'lineas_clave', nlines, 'campos', tot, 'usados', used, 'no_usados', tot - used)
for top in sorted(res):
    unused = [k for k, v in res[top].items() if not v]
    if unused: print(top, 'NO USADOS:', ' '.join(unused))
