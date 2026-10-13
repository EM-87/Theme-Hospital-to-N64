"""Líneas de código Lua de CorsixTH (sin blancos ni comentarios de línea) por
parte, separando lo que usa el nivel 1 de lo que solo usan los niveles 2-12."""
import os, re, sys
L = sys.argv[1]
def loc(path):
    n = 0; inblock = False
    for line in open(path, encoding="latin-1"):
        s = line.strip()
        if inblock:
            if "]]" in s: inblock = False
            continue
        if s.startswith("--[[") or s.startswith("--[=["):
            if "]]" not in s: inblock = True
            continue
        if not s or s.startswith("--"): continue
        n += 1
    return n
def ids(d, pat):
    r = {}
    for f in sorted(os.listdir(os.path.join(L, d))):
        if f.endswith(".lua"):
            p = os.path.join(L, d, f); txt = open(p, encoding="latin-1").read()
            m = re.search(pat, txt); r[m.group(1) if m else f] = loc(p)
    return r
demo_rooms = set("gp psych ward pharmacy inflation staff_room general_diag toilets".split())
demo_dis = set("bloaty_head invisibility uncommon_cold tv_personalities the_squits heaped_piles".split())
rooms = ids("rooms", r'room\.id = "(\w+)"')
dis = ids("diseases", r'disease\.id = "(\w+)"')
print("habitaciones: total %d, las 8 del nivel 1 %d" % (sum(rooms.values()), sum(v for k, v in rooms.items() if k in demo_rooms)))
print("enfermedades: total %d, las 6 del nivel 1 %d" % (sum(dis.values()), sum(v for k, v in dis.items() if k in demo_dis)))
for d in ["objects", "objects/machines", "objects/doors", "humanoid_actions", "entities", "entities/humanoids", "entities/humanoids/staff", "dialogs", "dialogs/resizables", "dialogs/fullscreen", "dialogs/menu_list_dialogs", "hospitals"]:
    p = os.path.join(L, d)
    if os.path.isdir(p):
        fs = [f for f in os.listdir(p) if f.endswith(".lua")]
        print("%-28s %3d ficheros %6d líneas" % (d, len(fs), sum(loc(os.path.join(p, f)) for f in fs)))
for f in ["research_department.lua", "epidemic.lua", "earthquake.lua", "endconditions.lua", "world.lua", "hospital.lua", "map.lua", "room.lua", "staff_profile.lua", "calls_dispatcher.lua", "game_ui.lua", "ui.lua", "window.lua", "graphics.lua"]:
    p = os.path.join(L, f)
    if os.path.exists(p): print("%-28s %6d líneas" % (f, loc(p)))
for f in sorted(os.listdir(os.path.join(L, "dialogs/fullscreen"))):
    print("  fullscreen/%-22s %5d" % (f, loc(os.path.join(L, "dialogs/fullscreen", f))))
