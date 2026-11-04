-- Las 16.384 casillas de un nivel recién cargado (hito 6.3), en el formato de
-- celdas.tsv de bench/scenarios/exportar_escenario.lua (todas las banderas de
-- getCellFlags, la sala, la parcela y el primer objeto), más owner y la capa
-- 4 (la de la interfaz), para compararlas con th64sim mapa --comparar.
-- Opción: --th64-nivel=N (por defecto 1).
local th64 = th64
assert(TheApp:loadLevel(tonumber(th64.opts.nivel or "1"), "full"))
th64.wait_ticks(1)
local th = TheApp.world.map.th
local campos = {"passable", "hospital", "buildable", "room", "doorWest", "doorNorth", "tallWest", "tallNorth",
  "travelNorth", "travelEast", "travelSouth", "travelWest", "doNotIdle", "buildableNorth", "buildableEast",
  "buildableSouth", "buildableWest"}
local f = assert(io.open(th64.out("celdas.tsv"), "w"))
f:write("x\ty\tsuelo\tnorte\toeste\t", table.concat(campos, "\t"), "\tsala\tparcela\tthob\towner\tui\n")
local w, hh = th:size()
local flags = {}
for y = 1, hh do
  for x = 1, w do
    local suelo, norte, oeste, ui = th:getCell(x, y)
    th:getCellFlags(x, y, flags)
    local fila = {x - 1, y - 1, suelo, norte, oeste}
    for _, c in ipairs(campos) do fila[#fila + 1] = flags[c] and 1 or 0 end
    fila[#fila + 1] = flags.roomId
    fila[#fila + 1] = flags.parcelId
    fila[#fila + 1] = flags.thob
    fila[#fila + 1] = flags.owner
    fila[#fila + 1] = ui
    f:write(table.concat(fila, "\t"), "\n")
  end
end
f:close()
local h = TheApp.world.hospitals[1]
th64.log("saldo %s valor %s; objetos en el mapa: %d", tostring(h.balance), tostring(h.value), (function()
  local n = 0
  for _, e in ipairs(TheApp.world.entities) do if class.is(e, Object) then n = n + 1 end end
  return n end)())
th64.quit(0)
