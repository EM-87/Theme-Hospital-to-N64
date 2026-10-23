-- Oráculo del hito 2 de la fase 5: carga un nivel de la campaña y vuelca, para
-- cada una de las 16.384 casillas, sus capas (suelo, pared norte, pared oeste)
-- y las banderas que da TheApp.map.th:getCellFlags, tal como quedan nada más
-- cargar el nivel en CorsixTH (con los objetos del mapa ya colocados).
-- Opción: --th64-nivel=N (por defecto 1). Escribe mapa_nivelN.tsv.
local th64 = th64
local nivel = tonumber(th64.opts.nivel or "1")
assert(TheApp:loadLevel(nivel, "full"), "no se pudo cargar el nivel " .. nivel)
th64.wait_ticks(1)
local mapa = TheApp.map
local th = mapa.th
local ancho, alto = th:size()
local campos = {"passable", "hospital", "buildable", "buildableNorth", "buildableEast", "buildableSouth",
                "buildableWest", "travelNorth", "travelEast", "travelSouth", "travelWest", "tallNorth", "tallWest"}
local f = assert(io.open(th64.out(("mapa_nivel%d.tsv"):format(nivel)), "w"))
f:write("x\ty\tsuelo\tnorte\toeste\t", table.concat(campos, "\t"), "\tparcela\tthob\n")
local flags = {}
for y = 1, alto do
  for x = 1, ancho do
    local suelo, norte, oeste = th:getCell(x, y)
    th:getCellFlags(x, y, flags)
    local fila = {x - 1, y - 1, suelo, norte, oeste}
    for _, c in ipairs(campos) do fila[#fila + 1] = flags[c] and 1 or 0 end
    fila[#fila + 1] = flags.parcelId
    fila[#fila + 1] = flags.thob
    f:write(table.concat(fila, "\t"), "\n")
  end
end
f:close()
th64.log("volcado el mapa del nivel %d: %dx%d casillas", nivel, ancho, alto)
th64.quit(0)
