-- Sonda: orden de pairs() dentro del proceso de CorsixTH, sobre tablas reales
-- del juego con claves de cadena (disease_casebook, available_rooms) y con
-- claves tabla (un conjunto de entidades). Si cambia entre ejecuciones, la
-- semilla de hash de Lua 5.4 (luai_makeseed) o las direcciones de memoria
-- deciden el orden de esos recorridos.
local th64 = th64
local world = assert(TheApp.world, "falta --load=")
local h = world.hospitals[1]
local function order(t, f)
  local s = {}
  for k, v in pairs(t) do s[#s + 1] = f(k, v) end
  return table.concat(s, ",")
end
th64.log("casebook: %s", order(h.disease_casebook, function(k) return tostring(k):sub(1, 6) end))
th64.log("available_rooms: %s", order(world.available_rooms, function(k) return tostring(k):sub(1, 6) end))
local set, idx = {}, {}
for i, e in ipairs(world.entities) do
  if i > 40 then break end
  set[e] = true
  idx[e] = i
end
th64.log("entidades como clave: %s", order(set, function(k) return tostring(idx[k]) end))
th64.quit(0)
