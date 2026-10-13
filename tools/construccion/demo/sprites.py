"""Fase 4b: sprites y animaciones de DATAM que necesita el nivel 1 frente al total.

Animaciones de partida (estimación estática a partir del Lua de CorsixTH):
  - objetos: tablas *anim* de cada definición (anims.tsv, sacado con anims.lua),
    filtradas por tipo de humanoide cuando la tabla va por tipo;
  - humanoides: llamadas walk_anims/die_anims/... de entities/humanoid.lua por tipo,
    más los números de vip.lua, inspector.lua y grim_reaper.lua;
  - números fijos en humanoid_actions/*.lua y en el código de cada habitación.
Luego: animación -> fotogramas (MSTART, MFRA, encadenados) -> lista (MLIST) ->
elementos (MELE) -> sprites de MSPR-0 -> bytes en CI8 (filas a 8 B, como graficos.py).
Uso: python3 -I sprites.py <DATAM> <CorsixTH/Lua> <anims.tsv>
"""
import os
import re
import struct
import sys

DATAM, LUA, TSV = sys.argv[1:4]


def rd(name):
    return open(os.path.join(DATAM, name), "rb").read()


start, fra, lst, ele, tab = (rd(n) for n in ("MSTART-1.ANI", "MFRA-1.ANI", "MLIST-1.ANI", "MELE-1.ANI", "MSPR-0.TAB"))
n_anim = len(start) // 4
n_frames = len(fra) // 10
n_ele = len(ele) // 6
n_spr = len(tab) // 6
first = [struct.unpack_from("<H", start, 4 * i)[0] for i in range(n_anim)]
frames = [struct.unpack_from("<IBBBBH", fra, 10 * i) for i in range(n_frames)]
lst16 = struct.unpack("<%dH" % (len(lst) // 2), lst)
spr_wh = [(tab[6 * i + 4], tab[6 * i + 5]) for i in range(n_spr)]


def ci8(i):
    w, h = spr_wh[i]
    return ((w + 7) // 8) * 8 * h


def anim_frames(a):
    if a >= n_anim:
        return set()
    seen = set()
    f = first[a]
    while f < n_frames and f not in seen:
        seen.add(f)
        f = frames[f][5]
    return seen


def frame_sprites(f):
    out = set()
    li = frames[f][0]
    if li >= len(lst16):
        li = 0
    while li < len(lst16) and lst16[li] < n_ele:
        e = lst16[li]
        pos = struct.unpack_from("<H", ele, 6 * e)[0]
        s = pos // 6
        if s < n_spr:
            out.add(s)
        li += 1
    return out


def cover(anims):
    fr = set()
    for a in anims:
        fr |= anim_frames(a)
    sp = set()
    for f in fr:
        sp |= frame_sprites(f)
    return fr, sp


# --- conjuntos del nivel 1 (salida de niveles.lua) ---
ROOMS1 = set("gp psych ward pharmacy inflation staff_room general_diag toilets".split())
TYPES1 = {"Doctor", "Nurse", "Handyman", "Receptionist", "Standard Male Patient", "Standard Female Patient",
          "Invisible Patient", "Stripped Male Patient", "Stripped Female Patient", "Stripped Male Patient 2",
          "Stripped Female Patient 2", "Stripped Male Patient 3", "Stripped Female Patient 3", "VIP", "Inspector",
          "Grim Reaper"}

obj_anims = {}   # id -> [(types, anim)]
room_anims = {}
room_objs = {}
avail1 = {}
thob_of = {}
for line in open(TSV):
    p = line.rstrip("\n").split("\t")
    if p[0] == "OBJ":
        obj_anims.setdefault(p[1], []).append((p[3], int(p[4])))
        thob_of[p[1]] = p[2]
    elif p[0] == "ROOM":
        room_anims.setdefault(p[1], set()).add(int(p[2]))
    elif p[0] == "ROOMOBJ":
        room_objs[p[1]] = (set(filter(None, p[2].split(","))), set(filter(None, p[3].split(","))))
    elif p[0] == "AVAIL1":
        avail1[p[1]] = p[2]

room_only = set()
for r, (need, add) in room_objs.items():
    room_only |= need
demo_objs = set()
for r in ROOMS1:
    demo_objs |= room_objs[r][0] | room_objs[r][1]
for o, th in thob_of.items():
    if o in room_only:
        continue
    if avail1.get(th) == "1":
        demo_objs.add(o)
# Puertas y objetos que no se compran (puertas, basura y ratas aparecen en cualquier nivel)
demo_objs |= {"door", "entrance_left_door", "entrance_right_door", "litter", "rathole", "gates_to_hell"}
# Partes secundarias de habitaciones que no están en el nivel 1 (quirófano, rayos X, sala con puertas batientes)
demo_objs -= {"op_sink1", "op_sink2", "operating_table_b", "radiation_shield_b", "swing_door_left", "swing_door_right", "surgeon_screen"}
for o in ("helicopter", "gates_to_hell"):  # emergencias y muerte de pacientes en el infierno: ver nota
    pass


def types_ok(tstr, types):
    if tstr == "*":
        return True
    for part in tstr.split("|"):
        for t in part.split(" - "):
            if t.strip() not in types:
                return False
    return True


def object_anims(objs, types):
    s = set()
    for o in objs:
        for t, a in obj_anims.get(o, []):
            if types_ok(t, types):
                s.add(a)
    return s


hum_src = open(os.path.join(LUA, "entities/humanoid.lua"), encoding="latin-1").read()
hum_calls = re.findall(r'^\s*(\w+_anims?|moods)\("([^"]+)",\s*([^)]*)\)', hum_src, re.M)


def humanoid_anims(types):
    s = set()
    for fn, t, args in hum_calls:
        if fn == "moods" or t not in types:
            continue
        for x in re.findall(r"\b\d+\b", args.split("--")[0]):
            v = int(x)
            if 0 < v < n_anim:
                s.add(v)
    for f, t in (("vip.lua", "VIP"), ("inspector.lua", "Inspector"), ("grim_reaper.lua", "Grim Reaper")):
        if t in types:
            for line in open(os.path.join(LUA, "entities/humanoids", f), encoding="latin-1"):
                line = line.split("--")[0]
                if re.search(r"anim|walk|idle|=", line):
                    for x in re.findall(r"\b\d{2,4}\b", line):
                        v = int(x)
                        if 0 < v < n_anim:
                            s.add(v)
    return s


def code_anims(paths):
    s = set()
    for p in paths:
        for line in open(p, encoding="latin-1"):
            line = line.split("--")[0]
            for m in re.finditer(r"(?:setAnimation\(|anim\w*\s*=\s*)(\d{2,4})", line):
                v = int(m.group(1))
                if 0 < v < n_anim:
                    s.add(v)
    return s


ha = [os.path.join(LUA, "humanoid_actions", f) for f in os.listdir(os.path.join(LUA, "humanoid_actions")) if f.endswith(".lua")]
room_file = {}
for f in os.listdir(os.path.join(LUA, "rooms")):
    m = re.search(r'room\.id = "(\w+)"', open(os.path.join(LUA, "rooms", f), encoding="latin-1").read())
    if m:
        room_file[m.group(1)] = os.path.join(LUA, "rooms", f)

ALLTYPES = {t for _, t, _ in hum_calls} | {"VIP", "Inspector", "Grim Reaper"}
for lst_ in obj_anims.values():
    for t, _ in lst_:
        if t != "*":
            for part in t.split("|"):
                for x in part.split(" - "):
                    ALLTYPES.add(x.strip())


def level_set(rooms, objs, types):
    a = object_anims(objs, types) | humanoid_anims(types) | code_anims(ha + [room_file[r] for r in rooms])
    for r in rooms:
        a |= room_anims.get(r, set())
    return a


a1 = level_set(ROOMS1, demo_objs, TYPES1)
aall = level_set(set(room_file), set(obj_anims), ALLTYPES)
for name, a in (("nivel 1", a1), ("todo lo que referencia CorsixTH", aall), ("las 5.186 animaciones", set(range(n_anim)))):
    fr, sp = cover(a)
    print("%-32s animaciones %5d  fotogramas %6d  sprites %5d  CI8 %8d B (%.2f MiB)" % (
        name, len(a), len(fr), len(sp), sum(ci8(s) for s in sp), sum(ci8(s) for s in sp) / 1048576))
print("%-32s sprites %5d  CI8 %8d B (%.2f MiB)" % ("hoja MSPR-0 entera", n_spr, sum(ci8(i) for i in range(n_spr)),
                                                    sum(ci8(i) for i in range(n_spr)) / 1048576))
blk = rd("MBLK-0.TAB")
nb = len(blk) // 6
bb = sum(((blk[6 * i + 4] + 7) // 8) * 8 * blk[6 * i + 5] for i in range(nb))
print("%-32s bloques %5d  CI8 %8d B (%.2f MiB)" % ("hoja MBLK-0 (suelos y paredes)", nb, bb, bb / 1048576))
print("objetos del nivel 1:", " ".join(sorted(demo_objs)))
# Tablas de animación (MFRA+MLIST+MELE) que tocan los fotogramas del nivel 1, en bytes del formato original
fr1, sp1 = cover(a1)
eles = set()
lists = 0
for f in fr1:
    li = frames[f][0]
    while li < len(lst16) and lst16[li] < n_ele:
        eles.add(lst16[li])
        li += 1
        lists += 1
print("tablas de animación del nivel 1 (formato original): fotogramas %d x 10 B + listas %d x 2 B + elementos %d x 6 B = %d B" % (
    len(fr1), lists, len(eles), len(fr1) * 10 + lists * 2 + len(eles) * 6))
print("tablas enteras: %d B" % (len(start) + len(fra) + len(lst) + len(ele)))
