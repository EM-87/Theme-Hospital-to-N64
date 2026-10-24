-- Oráculo del pathfinding (hito 3 de la fase 5): carga un nivel de la campaña
-- y pide a CorsixTH (World:getPath, su A*) caminos entre pares de casillas
-- transitables elegidos con un generador propio, para que el motor pueda
-- repetir los mismos pares. Escribe caminos_nivelN.tsv con una línea por par:
-- x1 y1 x2 y2 longitud casillas ("x,y x,y ..."; coordenadas desde 0), o
-- longitud -1 si no hay camino.
-- Opciones: --th64-nivel=N (por defecto 1), --th64-pares=P (por defecto 1000).
local th64 = th64
local nivel = tonumber(th64.opts.nivel or "1")
local pares = tonumber(th64.opts.pares or "1000")
assert(TheApp:loadLevel(nivel, "full"), "no se pudo cargar el nivel " .. nivel)
th64.wait_ticks(1)
local world = TheApp.world
local th = TheApp.map.th
local ancho, alto = th:size()
local transitables = {}
local flags = {}
for y = 1, alto do
  for x = 1, ancho do
    th:getCellFlags(x, y, flags)
    if flags.passable then transitables[#transitables + 1] = {x, y} end
  end
end
-- Generador congruencial (el mismo en tests del motor si hiciera falta).
local semilla = 12345 + nivel
local function azar(n)
  semilla = (1103515245 * semilla + 12345) % 2147483648
  return semilla % n + 1
end
local f = assert(io.open(th64.out(("caminos_nivel%d.tsv"):format(nivel)), "w"))
local sin_camino = 0
for _ = 1, pares do
  local a = transitables[azar(#transitables)]
  local b = transitables[azar(#transitables)]
  local px, py = world:getPath(a[1], a[2], b[1], b[2])
  if px then
    local c = {}
    for i = 1, #px do c[i] = (px[i] - 1) .. "," .. (py[i] - 1) end
    f:write(("%d\t%d\t%d\t%d\t%d\t%s\n"):format(a[1] - 1, a[2] - 1, b[1] - 1, b[2] - 1, #px - 1, table.concat(c, " ")))
  else
    sin_camino = sin_camino + 1
    f:write(("%d\t%d\t%d\t%d\t-1\t\n"):format(a[1] - 1, a[2] - 1, b[1] - 1, b[2] - 1))
  end
end
f:close()
th64.log("nivel %d: %d casillas transitables, %d pares, %d sin camino", nivel, #transitables, pares, sin_camino)
th64.quit(0)
