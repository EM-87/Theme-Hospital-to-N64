-- Hito 6.3, paso 1: las rarezas de ocupar y desocupar casillas que fijan las
-- pruebas del núcleo (motor/tests/test_objects.c), vistas en CorsixTH. Coloca
-- y quita objetos con world:newObject y obj:setTile, sin la interfaz, en el
-- nivel 1, y escribe las banderas en th64.log:
--   A. resetSideObjects con dos objetos de borde en una casilla;
--   B. dos radiadores en el mismo borde, uno a cada lado;
--   C. dos bancos con la misma casilla de uso;
--   D. un radiador que tapa al banco con el que comparte origen.
-- Uso: tools/bench_run.sh bench/scenarios/rarezas_objetos.lua <dir_salida>
local th64 = th64
assert(TheApp:loadLevel(1, "full"), "nivel 1")
th64.wait_ticks(2)
local world = TheApp.world
local map = TheApp.map.th
local function f(x, y) return map:getCellFlags(x, y) end
local function b(v) return v and 1 or 0 end
local function note(fmt, ...) th64.log(fmt, ...) end
-- Casillas libres: un cuadrado de 7 x 7 de pasillo del hospital, construible,
-- pasable y con paso en las cuatro direcciones.
local function libre(cx, cy)
  for y = cy - 3, cy + 3 do
    for x = cx - 3, cx + 3 do
      local g = f(x, y)
      if not (g.hospital and g.roomId == 0 and g.buildable and g.passable and g.travelNorth and g.travelEast
          and g.travelSouth and g.travelWest and g.thob == 0) then return false end
    end
  end
  return true
end
local sitios, usados = {}, {}
local w, h = map:size()
for y = 4, h - 4 do
  for x = 4, w - 4 do
    if #sitios < 4 and libre(x, y) then
      local lejos = true
      for _, s in ipairs(sitios) do
        if math.abs(s[1] - x) < 8 and math.abs(s[2] - y) < 8 then lejos = false end
      end
      if lejos then sitios[#sitios + 1] = {x, y} end
    end
  end
end
assert(#sitios == 4, "faltan sitios libres: " .. #sitios)
for i, s in ipairs(sitios) do note("sitio %d: (%d,%d)", i, s[1] - 1, s[2] - 1) end

-- A: radiador norte y extintor oeste en la misma casilla.
local x, y = sitios[1][1], sitios[1][2]
local r = world:newObject("radiator", x, y, "north")
local e = world:newObject("extinguisher", x, y, "west")
local function estado_a(cuando)
  note("A %s: thob=%d tN=%d tW=%d (arriba tS=%d, izquierda tE=%d) objetos=%s,%s", cuando, f(x, y).thob,
    b(f(x, y).travelNorth), b(f(x, y).travelWest), b(f(x, y - 1).travelSouth), b(f(x - 1, y).travelEast),
    world:getObjects(x, y)[1].object_type.id, world:getObjects(x, y)[2].object_type.id)
end
estado_a("colocados")
world:resetSideObjects()
estado_a("tras resetSideObjects")
map:updatePathfinding()
world:resetSideObjects()
estado_a("tras updatePathfinding y resetSideObjects")

-- B: dos radiadores en el mismo borde, uno a cada lado; se quita el de arriba.
x, y = sitios[2][1], sitios[2][2]
local ra = world:newObject("radiator", x, y, "south")
local rb = world:newObject("radiator", x, y + 1, "north")
note("B colocados: abajo tN=%d, arriba tS=%d; set_passable_flags a=%s b=%s", b(f(x, y + 1).travelNorth),
  b(f(x, y).travelSouth), tostring(ra.set_passable_flags), tostring(rb.set_passable_flags))
ra:setTile(nil)
note("B quitado el de arriba: abajo tN=%d, arriba tS=%d", b(f(x, y + 1).travelNorth), b(f(x, y).travelSouth))
world:resetSideObjects()
note("B tras resetSideObjects: abajo tN=%d, arriba tS=%d", b(f(x, y + 1).travelNorth), b(f(x, y).travelSouth))

-- C: dos bancos con la misma casilla de uso.
x, y = sitios[3][1], sitios[3][2]
local b1 = world:newObject("bench", x, y, "north")
local b2 = world:newObject("bench", x, y - 2, "south")
note("C colocados: uso buildable=%d bN=%d bS=%d", b(f(x, y - 1).buildable), b(f(x, y - 1).buildableNorth),
  b(f(x, y - 1).buildableSouth))
b1:setTile(nil)
note("C quitado el norte: uso buildable=%d bN=%d bS=%d", b(f(x, y - 1).buildable), b(f(x, y - 1).buildableNorth),
  b(f(x, y - 1).buildableSouth))
b2:setTile(nil)
note("C quitado el sur: uso buildable=%d bN=%d bS=%d", b(f(x, y - 1).buildable), b(f(x, y - 1).buildableNorth),
  b(f(x, y - 1).buildableSouth))

-- D: un radiador y un banco con el mismo origen, y otro banco con la misma
-- casilla de uso que el primero; se quita el segundo banco.
x, y = sitios[4][1], sitios[4][2]
world:newObject("radiator", x, y, "north")
world:newObject("bench", x, y, "north")
local b4 = world:newObject("bench", x, y - 2, "south")
note("D colocados: primero en el origen=%s; uso buildable=%d", world:getObject(x, y).object_type.id,
  b(f(x, y - 1).buildable))
b4:setTile(nil)
note("D quitado el banco sur: uso buildable=%d", b(f(x, y - 1).buildable))
th64.quit(0)
