-- Exporta una partida guardada de CorsixTH para que el motor la cargue (fase
-- 5, hito 4): lo que el jugador ha construido y el estado de la simulación
-- que el motor necesita para seguir desde ahí.
--   * celdas.tsv: las 16.384 casillas, con sus capas tal cual (con las
--     banderas de dibujo en el byte alto), todas las banderas de
--     getCellFlags, la sala, la parcela y el objeto;
--   * escenario.json (JSON canónico de tools/conv/json_canonico.lua): la
--     fecha, el mundo (llegadas, puntos de aparición, enfermedades), el
--     hospital (dinero, políticas, casebook, seguros), los objetos, las salas
--     y el personal con su perfil.
-- Coordenadas desde 0, como en el motor.
-- Opción: --load=partida.sav (obligatoria)
local th64 = th64
local canonico = dofile(th64.bench_dir .. "/../tools/conv/json_canonico.lua")
local world = assert(TheApp.world, "falta --load=")
local h = world.hospitals[1]
local th = world.map.th

-- Casillas ------------------------------------------------------------------------
local campos = {"passable", "hospital", "buildable", "room", "doorWest", "doorNorth", "tallWest", "tallNorth",
  "travelNorth", "travelEast", "travelSouth", "travelWest", "doNotIdle", "buildableNorth", "buildableEast",
  "buildableSouth", "buildableWest"}
local f = assert(io.open(th64.out("celdas.tsv"), "w"))
f:write("x\ty\tsuelo\tnorte\toeste\t", table.concat(campos, "\t"), "\tsala\tparcela\tthob\n")
local w, hh = th:size()
local flags = {}
for y = 1, hh do
  for x = 1, w do
    local suelo, norte, oeste = th:getCell(x, y)
    th:getCellFlags(x, y, flags)
    local fila = {x - 1, y - 1, suelo, norte, oeste}
    for _, c in ipairs(campos) do fila[#fila + 1] = flags[c] and 1 or 0 end
    fila[#fila + 1] = flags.roomId
    fila[#fila + 1] = flags.parcelId
    fila[#fila + 1] = flags.thob
    f:write(table.concat(fila, "\t"), "\n")
  end
end
f:close()

-- Entidades --------------------------------------------------------------------------
local function sala_de(e)
  local r = e.tile_x and e:getRoom()
  return r and r.id or 0
end
local objetos, salas, personal = {}, {}, {}
for _, e in ipairs(world.entities) do
  if class.is(e, Object) then
    objetos[#objetos + 1] = {id = e.object_type.id, x = e.tile_x - 1, y = e.tile_y - 1, direccion = e.direction,
      sala = sala_de(e)}
  end
end
for _, r in pairs(world.rooms) do
  if r then
    salas[#salas + 1] = {num = r.id, id = r.room_info.id, x = r.x - 1, y = r.y - 1, ancho = r.width, alto = r.height,
      puerta = {x = r.door.tile_x - 1, y = r.door.tile_y - 1, direccion = r.door.direction}, activa = r.is_active}
  end
end
table.sort(salas, function(a, b) return a.num < b.num end)
for _, s in ipairs(h.staff) do
  local p = s.profile
  local capas = {}
  for k, v in pairs(s.layers or {}) do capas["#" .. k] = v end
  personal[#personal + 1] = {clase = s.humanoid_class, x = s.tile_x - 1, y = s.tile_y - 1, sala = sala_de(s),
    perfil = {nombre = p.name, habilidad = p.skill, atencion = p.attention_to_detail, sueldo = p.wage,
      consultor = p.is_consultant and true or false, junior = p.is_junior and true or false,
      cirujano = p.is_surgeon, psiquiatra = p.is_psychiatrist, investigador = p.is_researcher,
      capas = p.layer5 and {layer5 = p.layer5} or nil},
    atributos = {fatiga = s.attributes.fatigue, felicidad = s.attributes.happiness},
    capas = s.layers}
end

-- Mundo y hospital ------------------------------------------------------------------
local d = world.game_date
local enfermedades = {}
for i, dis in ipairs(world.available_diseases) do enfermedades[i] = dis.id end
local puntos = {}
for i, p in ipairs(world.spawn_points) do puntos[i] = {x = p.x - 1, y = p.y - 1, direccion = p.direction} end
local casebook = {}
for id, c in pairs(h.disease_casebook) do
  casebook[id] = {precio = c.price, descubierta = c.discovered and true or false, eficacia = c.cure_effectiveness,
    pseudo = c.pseudo and true or false, reputacion = c.reputation}
end
local doc = {
  version = 1,
  nivel = world.map.level_number,
  dificultad = world.map.difficulty,
  fecha = {anio = d:year(), mes = d:monthOfYear(), dia = d:dayOfMonth(), hora = d:hourOfDay()},
  mundo = {spawn_rate = world.spawn_rate, monthly_spawn_increase = world.monthly_spawn_increase,
    popn = world.map.level_config.popn, spawn_dates = world.spawn_dates, spawn_hours = world.spawn_hours,
    puntos_aparicion = puntos, enfermedades = enfermedades, hours_per_tick = world.hours_per_tick,
    tick_rate = world.tick_rate},
  hospital = {saldo = h.balance, valor = h.value, reputacion = h.reputation, poblacion = h.population,
    abierto = h.opened and true or false, politicas = h.policies, casebook = casebook,
    seguros = h.insurance, saldo_seguros = h.insurance_balance, entrada = {x = h.x and h.x - 1 or nil}},
  objetos = objetos,
  salas = salas,
  personal = personal,
}
f = assert(io.open(th64.out("escenario.json"), "w"))
f:write(canonico.escribir(doc), "\n")
f:close()
th64.log("escenario exportado: %d objetos, %d salas, %d en plantilla, %d puntos de aparición, fecha %s", #objetos,
  #salas, #personal, #puntos, d:tostring())
th64.quit(0)
