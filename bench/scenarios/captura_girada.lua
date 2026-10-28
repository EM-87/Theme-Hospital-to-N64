-- El hospital de una partida guardada, girado, para comparar las vistas
-- giradas del motor con CorsixTH (fase 6, hito 6.2). CorsixTH no gira la
-- cámara, así que se gira el hospital: se carga la partida, se quitan las
-- personas y los objetos del mapa, se vuelven a poner las casillas giradas y
-- se crean los objetos con la orientación girada, y se hace una captura de
-- 640 × 480 (sin ventanas ni cursor).
--
-- Lo que se gira aquí, con una lógica distinta de la del motor
-- (motor/vista/girar.c y objetos.c), que se comprueba con esto:
--   - las casillas: cada pieza del mundo se lleva a su sitio girado (el motor
--     hace lo contrario: busca, para cada casilla de la vista, lo que le toca);
--   - las paredes: con las tablas de walls/*.lua de CorsixTH
--     (getWallSetFromBlockId y compañía), la cara y la mitad que tocan;
--   - la luz de las paredes (hito 6.2b): el sol no gira, así que la pared cuya
--     cara se ve está al sol o a la sombra según la vista; si la pieza tiene
--     la otra luz, va la de su pareja (el mismo nombre en el otro borde, con
--     la mitad cambiada) en espejo;
--   - los objetos: World:newObject en la casilla y con la orientación girada
--     (la opuesta si no se puede, el sustituto); CorsixTH los dibuja solo.
-- Sin sombras: CorsixTH las calcularía con las paredes de la vista, con la
-- luz girada. Las del motor se comprueban aparte (motor/tests/test_vista.c),
-- y el motor se compara con estas capturas con th64sim --sin-sombras.
-- Lo que no se puede sacar de CorsixTH viene de la tabla del motor
-- (th64sim tabla_giro): la pieza girada de los suelos y del decorado, y los
-- pilares de las paredes de fuera y su pareja.
--
-- Opciones: --load=partida.sav, --th64-png=ruta, --th64-giro=0..3,
--           --th64-tabla=tabla_giro.tsv, --th64-centro="x,y" (casilla del
--           mundo, desde 0; por defecto 62,74).
-- Escribe ruta.txt con «0 desplazamiento_x desplazamiento_y» y cuántas
-- casillas tienen dos piezas que CorsixTH no puede poner en la misma capa.
local th64 = th64
local world = assert(TheApp.world, "falta --load=")
local png = assert(th64.opts.png, "falta --th64-png=")
local giro = tonumber(th64.opts.giro or "0") % 4
local cx, cy = (th64.opts.centro or "62,74"):match("(%d+),(%d+)")
cx, cy = tonumber(cx), tonumber(cy)
local th = world.map.th
local W = th:size()
local TILE, FLIP = 0x200, 0x100

world:setSpeed("Pause")
th64.autorun_speed = nil
TheApp.video:setBlueFilterActive(false)

-- La tabla del motor: suelo[b] y capa[b] para este giro, y la pareja en
-- espejo de cada pared.
local suelo, capa, pareja = {}, {}, {}
for b = 0, 255 do
  suelo[b] = b
  capa[b] = (b >= 82 and b <= 164) and b or (b | TILE)
  pareja[b] = b
end
if giro ~= 0 then
  local f = assert(io.open(assert(th64.opts.tabla, "falta --th64-tabla="), "r"))
  for linea in f:read("a"):gmatch("[^\n]+") do
    local r, b, s, c, p = linea:match("^(%d+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)$")
    b = tonumber(b)
    if tonumber(r) == giro then
      suelo[b], capa[b] = tonumber(s), tonumber(c)
    end
    pareja[b] = tonumber(p)
  end
  f:close()
end

local function girar_casilla(x, y) -- desde 0
  if giro == 0 then return x, y
  elseif giro == 1 then return W - 1 - y, x
  elseif giro == 2 then return W - 1 - x, W - 1 - y
  else return y, W - 1 - x end
end

-- Las paredes por las tablas de CorsixTH.
local por_bloque = {}
for id, tipo in pairs(TheApp.walls) do
  for juego, piezas in pairs(tipo) do
    if type(piezas) == "table" then
      for nombre, b in pairs(piezas) do por_bloque[b] = {tipo = id, juego = juego, nombre = nombre} end
    end
  end
end
local otra_cara = {inside_tiles = "outside_tiles", outside_tiles = "inside_tiles", window_tiles = "window_tiles",
  swing_inside_tiles = "swing_outside_tiles", swing_outside_tiles = "swing_inside_tiles"}
local N, E, S, O = 0, 1, 2, 3

-- La pared b del borde d0 (N u O) con la cámara girada: el borde girado, si
-- la casilla que lo tiene es la vecina (y la cámara la ve por la otra
-- cara) y el bloque.
local function girar_pared(b, d0)
  local e = (d0 + giro) % 4
  local otro = e == E or e == S
  local D = otro and (e + 2) % 4 or e
  local p = por_bloque[b]
  if not p then return e, capa[b] & 0xFF end -- los pilares, por la tabla del motor
  local base, resto = p.nombre:match("^(%a+)(.*)$")
  local juego = otro and otra_cara[p.juego] or p.juego
  -- _1 y _left son la pieza de la izquierda en pantalla; _2 y _right, la de la derecha.
  local lado = (resto == "_window_1" or resto == "_left") and -1 or (resto == "_window_2" or resto == "_right") and 1 or 0
  if lado ~= 0 then
    local dx, dy
    if base == "north" then dx, dy = lado, 0 else dx, dy = 0, -lado end
    for _ = 1, giro do dx, dy = -dy, dx end
    local izquierda = (D == N and dx < 0) or (D == O and dy > 0)
    if resto:find("window") then resto = izquierda and "_window_1" or "_window_2"
    else resto = izquierda and "_left" or "_right" end
  end
  local nombre_d = D == N and "north" or "west"
  local t = TheApp.walls[p.tipo][juego]
  return e, t[nombre_d .. resto] or t[nombre_d] or b
end

-- La luz (hito 6.2b): la cara que se ve de una pared de borde norte de la
-- vista mira al sur de la vista, y la de una de borde oeste, al este; en el
-- mundo, a esa dirección menos el giro. Con el sol al suroeste, están al sol
-- las que miran al sur o al oeste. La pieza norte está pintada al sol y la
-- oeste a la sombra: si no casa, va la pareja en espejo (bandera 1 de dibujo,
-- en el byte alto de la capa).
local function con_luz(b, D)
  local cara = ((D == N and S or E) - giro) % 4
  local al_sol = cara == S or cara == O
  if al_sol == (D == N) then return b end
  local p = por_bloque[b]
  local otra = pareja[b]
  if p then
    local base, resto = p.nombre:match("^(%a+)(.*)$")
    local cambio = {_window_1 = "_window_2", _window_2 = "_window_1", _left = "_right", _right = "_left"}
    otra = TheApp.walls[p.tipo][p.juego][(base == "north" and "west" or "north") .. (cambio[resto] or resto)]
    assert(otra, "sin pareja: " .. p.nombre)
  end
  return otra | 256
end

-- 1. Las casillas giradas.
local viejas = {}
for y = 0, W - 1 do
  for x = 0, W - 1 do
    local f, n, o = th:getCell(x + 1, y + 1)
    viejas[y * W + x] = {f & 0xFF, n & 0xFF, o & 0xFF}
  end
end
local nf, nn, no, de_pared = {}, {}, {}, {}
for k = 0, W * W - 1 do nf[k], nn[k], no[k] = 0, 0, 0 end
local choques = 0
local function poner(t, k, valor, pared)
  if t[k] ~= 0 then
    choques = choques + 1
    if de_pared[t] and de_pared[t][k] and not pared then return end
  end
  t[k] = valor
  if pared then
    de_pared[t] = de_pared[t] or {}
    de_pared[t][k] = true
  end
end
for y = 0, W - 1 do
  for x = 0, W - 1 do
    local c = viejas[y * W + x]
    local vx, vy = girar_casilla(x, y)
    local k = vy * W + vx
    local s = suelo[c[1]]
    nf[k] = (s & 0xFF) | ((s & FLIP) ~= 0 and 256 or 0)
    for capa_mundo = 2, 3 do
      local b = c[capa_mundo]
      if b ~= 0 then
        local r = capa[b]
        if (r & TILE) ~= 0 then
          -- Decorado de la casilla: en la misma capa.
          poner(capa_mundo == 2 and nn or no, k, (r & 0xFF) | ((r & FLIP) ~= 0 and 256 or 0), false)
        else
          local e, b2 = girar_pared(b, capa_mundo == 2 and N or O)
          local tx, ty = vx, vy
          if e == S then ty = ty + 1 elseif e == E then tx = tx + 1 end
          if tx < W and ty < W then
            local D = (e == N or e == S) and N or O
            poner(D == N and nn or no, ty * W + tx, con_luz(b2, D), true)
          end
        end
      end
    end
  end
end

-- 2. Fuera las personas y los objetos (solo del mapa: su lógica no se toca).
local objetos = {}
for _, e in ipairs(world.entities) do
  if class.is(e, Humanoid) then
    e.th:setTile(nil)
  elseif class.is(e, Object) and e.tile_x then
    objetos[#objetos + 1] = {tipo = e.object_type, x = e.tile_x - 1, y = e.tile_y - 1, dir = e.direction}
    e.th:setTile(nil)
    for _, parte in ipairs(e.split_anims or {}) do parte:setTile(nil) end -- los partidos en columnas
  end
end
local sin_banderas = {tallNorth = false, tallWest = false, doorNorth = false, doorWest = false}
for y = 0, W - 1 do
  for x = 0, W - 1 do
    local k = y * W + x
    th:setCell(x + 1, y + 1, nf[k], nn[k], no[k], 0)
    th:setCellFlags(x + 1, y + 1, sin_banderas)
  end
end

-- 3. Los objetos girados.
local nombres = {"north", "east", "south", "west"}
local indice = {north = 0, east = 1, south = 2, west = 3}
local function caja(tipo, dir)
  local spec = tipo.orientations and tipo.orientations[dir]
  local x0, y0, x1, y1
  for _, p in ipairs(spec and spec.footprint or {{0, 0}}) do
    if not p.only_passable then
      x0 = x0 and math.min(x0, p[1]) or p[1]
      y0 = y0 and math.min(y0, p[2]) or p[2]
      x1 = x1 and math.max(x1, p[1]) or p[1]
      y1 = y1 and math.max(y1, p[2]) or p[2]
    end
  end
  return x0 or 0, y0 or 0, x1 or 0, y1 or 0
end
local sustitutos = 0
for _, o in ipairs(objetos) do
  local d = (indice[o.dir] + giro) % 4
  local vx, vy = girar_casilla(o.x, o.y)
  if not o.tipo.orientations then -- puertas: en el borde girado
    if d == S then vy, d = vy + 1, N elseif d == E then vx, d = vx + 1, O end
  else
    if not o.tipo.orientations[nombres[d + 1]] then
      d = (d + 2) % 4
      sustitutos = sustitutos + 1
      if not o.tipo.orientations[nombres[d + 1]] then d, sustitutos = indice[o.dir], sustitutos - 1 end
    end
    local ax0, ay0, ax1, ay1 = caja(o.tipo, o.dir)
    local px0, py0 = girar_casilla(o.x + ax0, o.y + ay0)
    local px1, py1 = girar_casilla(o.x + ax1, o.y + ay1)
    local bx0, by0 = caja(o.tipo, nombres[d + 1])
    vx, vy = math.min(px0, px1) - bx0, math.min(py0, py1) - by0
  end
  world:newObject(o.tipo.id, vx + 1, vy + 1, nombres[d + 1])
  th64.log("objeto %s (%d,%d) %s -> (%d,%d) %s", o.tipo.id, o.x, o.y, o.dir, vx, vy, nombres[d + 1])
end
-- Sin sombras: las puertas las recalculan al crearse. Se quitan las paredes y
-- lo alto, se recalculan (ninguna) y se vuelven a poner, sin recalcular.
local guardadas = {}
for y = 0, W - 1 do
  for x = 0, W - 1 do
    local f, n, o = th:getCell(x + 1, y + 1)
    local fl = th:getCellFlags(x + 1, y + 1)
    guardadas[y * W + x] = {f, n, o, fl.tallNorth, fl.tallWest}
    th:setCell(x + 1, y + 1, f, 0, 0, 0)
    th:setCellFlags(x + 1, y + 1, {tallNorth = false, tallWest = false})
  end
end
th:updateShadows()
for y = 0, W - 1 do
  for x = 0, W - 1 do
    local c = guardadas[y * W + x]
    th:setCell(x + 1, y + 1, c[1], c[2], c[3], 0)
    th:setCellFlags(x + 1, y + 1, {tallNorth = c[4], tallWest = c[5]})
  end
end

-- 4. La captura, con el mismo punto del mundo en el centro.
local function girar_punto(a, b)
  if giro == 0 then return a, b
  elseif giro == 1 then return W - b, a
  elseif giro == 2 then return W - a, W - b
  else return b, W - a end
end
local a, b = girar_punto(cx, cy)
local ui = TheApp.ui
ui.windows, ui.simulated_cursor = {}, nil
ui.screen_offset_x = 32 * (a - b) - TheApp.config.width // 2
ui.screen_offset_y = 16 * (a + b) - TheApp.config.height // 2
ui:scrollMap(0, 0)
th64.wait_ticks(3)
local ok, err = TheApp.video:takeScreenshot(png)
assert(ok, err)
local f = assert(io.open(png .. ".txt", "w"))
f:write(("0\t%d\t%d\t%d\t%d\n"):format(ui.screen_offset_x, ui.screen_offset_y, choques, sustitutos))
f:close()
th64.log("giro %d: captura %s, desplazamiento (%d, %d), %d objetos (%d sustitutos), %d casillas con dos piezas",
  giro, png, ui.screen_offset_x, ui.screen_offset_y, #objetos, sustitutos, choques)
th64.quit(0)
