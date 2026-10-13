# Claves de base_config.lua de CorsixTH que no aparecen en ningún .SAM original.
import sys, re, glob, os
base = open(sys.argv[1], encoding='utf-8').read()
# quitar comentarios Lua de línea
base_nc = '\n'.join(l.split('--')[0] for l in base.splitlines())
keys = set(re.findall(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*=', base_nc))
sam = ''.join(open(f, 'rb').read().decode('latin-1') for f in glob.glob(os.path.join(sys.argv[2], '*.SAM')))
samkeys = set(re.findall(r'[#.]([A-Za-z_][A-Za-z0-9_]*)', sam))
missing = sorted(k for k in keys if k not in samkeys and k not in ('local', 'configuration'))
print('claves en base_config', len(keys), 'ausentes en los .SAM', len(missing))
print(' '.join(missing))
