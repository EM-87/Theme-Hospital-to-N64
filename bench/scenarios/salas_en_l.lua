-- Salas en L con las piezas de pared de CorsixTH (fase 6): ¿hacen falta
-- piezas nuevas para las esquinas interiores? Busca en el nivel 1 el mayor
-- cuadrado libre dentro del hospital, levanta las paredes de una sala de
-- n × n casillas sin uno de sus cuadrantes y hace una captura sin ventanas.
-- Las paredes van por bordes, como en UIEditRoom:finishRoom: cara interior en
-- los bordes norte y oeste de las casillas de la sala, exterior en los de las
-- casillas vecinas del sur y del este.
-- Opciones: --th64-quitar=no|ne|so|se (cuadrante que falta, por defecto no)
--           --th64-n=N (lado, por defecto 4) --th64-pared=blue|green|white|yellow
local th64 = th64
assert(TheApp:loadLevel(1, "full"), "no se pudo cargar el nivel 1")
th64.wait_ticks(2)
local ui, map = TheApp.ui, TheApp.map.th
local W, H = map:size()
local function libre(x, y)
  if x < 1 or y < 1 or x > W or y > H then return false end
  local f = map:getCellFlags(x, y)
  return f.hospital and f.buildable and f.passable and map:getCell(x, y, 2) == 0 and map:getCell(x, y, 3) == 0
end
local best, bx, by = 0, 0, 0
for y = 1, H do
  for x = 1, W do
    local s = 0
    while true do
      local ok = true
      for i = 0, s do
        if not (libre(x + s, y + i) and libre(x + i, y + s)) then ok = false break end
      end
      if not ok then break end
      s = s + 1
    end
    if s > best then best, bx, by = s, x, y end
  end
end
local n, q = tonumber(th64.opts.n or "4"), th64.opts.quitar or "no"
assert(best >= n + 2, "no hay sitio")
local wall = assert(TheApp.walls[th64.opts.pared or "blue"])
local dentro, m = {}, n // 2
local function d(x, y) return dentro[x .. "," .. y] end
for y = 0, n - 1 do
  for x = 0, n - 1 do
    local fuera = (q == "no" and x < m and y < m) or (q == "ne" and x >= m and y < m)
      or (q == "so" and x < m and y >= m) or (q == "se" and x >= m and y >= m)
    if not fuera then dentro[(bx + 1 + x) .. "," .. (by + 1 + y)] = true end
  end
end
for k in pairs(dentro) do
  local x, y = k:match("(%d+),(%d+)")
  x, y = tonumber(x), tonumber(y)
  if not d(x, y - 1) then map:setCell(x, y, 2, wall.inside_tiles.north) end
  if not d(x - 1, y) then map:setCell(x, y, 3, wall.inside_tiles.west) end
  if not d(x, y + 1) then map:setCell(x, y + 1, 2, wall.outside_tiles.north) end
  if not d(x + 1, y) then map:setCell(x + 1, y, 3, wall.outside_tiles.west) end
end
if map.updateShadows then map:updateShadows() end
ui.windows, ui.simulated_cursor = {}, nil
local cx, cy = bx + n / 2, by + n / 2
ui.screen_offset_x = 32 * (cx - cy) - TheApp.config.width / 2
ui.screen_offset_y = 16 * (cx + cy) + 16 - TheApp.config.height / 2
ui:scrollMap(0, 0)
th64.wait_ticks(3)
local png = th64.out(("sala_l_%s_%s.png"):format(q, th64.opts.pared or "blue"))
assert(TheApp.video:takeScreenshot(png))
th64.log("sala en L sin el cuadrante %s en %d,%d: %s", q, bx + 1, by + 1, png)
th64.quit(0)
