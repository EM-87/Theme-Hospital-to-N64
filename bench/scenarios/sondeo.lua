-- Sondeo de los 12 niveles de la campaña: tamaño del mapa, superficie de
-- hospital construible, parcelas, llegada de pacientes y habitaciones
-- disponibles al empezar. Sirve para elegir niveles para las partidas.
local th64 = th64
local levels = {}

for level = 1, 12 do
  assert(TheApp:loadLevel(level, "full"), "no se pudo cargar el nivel " .. level)
  th64.wait_ticks(3)
  local world, map = TheApp.world, TheApp.map
  local hosp = world.hospitals[1]
  local tiles = {hospital = 0, buildable = 0, owned_buildable = 0}
  for y = 1, map.height do
    for x = 1, map.width do
      local f = map.th:getCellFlags(x, y)
      if f.hospital then tiles.hospital = tiles.hospital + 1 end
      if f.buildable then tiles.buildable = tiles.buildable + 1 end
      if f.buildable and f.hospital and f.owner == 1 then
        tiles.owned_buildable = tiles.owned_buildable + 1
      end
    end
  end
  local popn = {}
  for i = 0, 20 do
    local p = map.level_config.popn and map.level_config.popn[i]
    if p then popn[#popn + 1] = {month = p.Month, change = p.Change} end
  end
  local rooms = {}
  for _, disc in pairs(hosp.room_discoveries) do
    if disc.is_discovered and disc.room.class then rooms[#rooms + 1] = disc.room.id end
  end
  table.sort(rooms)
  local parcels = {}
  for p, n in pairs(map.parcelTileCounts) do
    parcels[#parcels + 1] = {parcel = p, tiles = n, owner = map.th:getPlotOwner(p)}
  end
  table.sort(parcels, function(a, b) return a.parcel < b.parcel end)
  levels[#levels + 1] = {
    level = level, width = map.width, height = map.height, tiles = tiles,
    parcels = parcels, popn = popn, rooms_available = rooms, balance = hosp.balance,
    spawn_rate = world.spawn_rate,
  }
  th64.log("nivel %d: %dx%d, hospital %d casillas (%d propias construibles), %d habitaciones, saldo %d",
    level, map.width, map.height, tiles.hospital, tiles.owned_buildable, #rooms, hosp.balance)
end

th64.write_json("sondeo.json", levels)
th64.quit(0)
