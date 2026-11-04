-- El estado de una partida de CorsixTH tal como lo carga el motor (fase 5,
-- hito 4), para bench/scenarios/exportar_escenario.lua y para la batería de
-- construir del hito 6.3 (bench/scenarios/construir.lua):
--   * celdas.tsv: las 16.384 casillas, con sus capas tal cual (con las
--     banderas de dibujo en el byte alto), todas las banderas de
--     getCellFlags, la sala, la parcela y el objeto;
--   * escenario.json (JSON canónico de tools/conv/json_canonico.lua): la
--     fecha, el mundo (llegadas, puntos de aparición, enfermedades), el
--     hospital (dinero, políticas, casebook, seguros), los objetos, las salas
--     y el personal con su perfil. De cada objeto, además, su sitio en la
--     lista world.objects de su casilla («orden», desde 0), que puede no ser el
--     de world.entities; y en «listas», las casillas (y * ancho + x) que tienen
--     esa lista, aunque se haya quedado vacía: Patient:setTile no tira basura
--     en ellas. También el dueño de cada parcela («parcelas», de la 1 en
--     adelante), la basura del suelo («basura»: casilla, animación, banderas
--     y sitio en world.objects) y, en los objetos cogidos (picked_up),
--     «recogido». El motor aún no carga ni la basura ni los objetos cogidos:
--     rechaza esos escenarios.
-- Coordenadas desde 0, como en el motor.
local th64 = th64
local canonico = dofile(th64.bench_dir .. "/../tools/conv/json_canonico.lua")

local E = {}

-- Casillas ------------------------------------------------------------------------
E.campos = {"passable", "hospital", "buildable", "room", "doorWest", "doorNorth", "tallWest", "tallNorth",
  "travelNorth", "travelEast", "travelSouth", "travelWest", "doNotIdle", "buildableNorth", "buildableEast",
  "buildableSouth", "buildableWest"}
E.cabecera_celdas = "x\ty\tsuelo\tnorte\toeste\t" .. table.concat(E.campos, "\t") .. "\tsala\tparcela\tthob"

--! La fila de celdas.tsv de la casilla (x, y), desde 1 como en CorsixTH, sin
--! el salto de línea. flags es una tabla que se reutiliza.
function E.fila_celda(th, x, y, flags)
  local suelo, norte, oeste = th:getCell(x, y)
  th:getCellFlags(x, y, flags)
  local fila = {x - 1, y - 1, suelo, norte, oeste}
  for _, c in ipairs(E.campos) do fila[#fila + 1] = flags[c] and 1 or 0 end
  fila[#fila + 1] = flags.roomId
  fila[#fila + 1] = flags.parcelId
  fila[#fila + 1] = flags.thob
  return table.concat(fila, "\t")
end

--! Escribe celdas.tsv entero en la ruta dada.
function E.escribir_celdas(ruta, th)
  local f = assert(io.open(ruta, "w"))
  f:write(E.cabecera_celdas, "\n")
  local w, hh = th:size()
  local flags = {}
  for y = 1, hh do
    for x = 1, w do
      f:write(E.fila_celda(th, x, y, flags), "\n")
    end
  end
  f:close()
end

-- Entidades --------------------------------------------------------------------------
local function sala_de(e)
  local r = e.tile_x and e:getRoom()
  return r and r.id or 0
end
E.sala_de = sala_de

--! Sitio del objeto en la lista world.objects de su casilla, desde 0 (nil si
--! no está en ella).
function E.orden_en_casilla(world, e)
  for k, o in ipairs(world:getObjects(e.tile_x, e.tile_y) or {}) do
    if o == e then return k - 1 end
  end
end

--! El documento de escenario.json y un resumen para el registro.
function E.documento(world)
  local h = world.hospitals[1]
  local objetos, salas, personal, basura = {}, {}, {}, {}
  for _, e in ipairs(world.entities) do
    if class.is(e, Object) then
      objetos[#objetos + 1] = {id = e.object_type.id, x = e.tile_x - 1, y = e.tile_y - 1, direccion = e.direction,
        sala = sala_de(e), orden = E.orden_en_casilla(world, e), recogido = e.picked_up and true or nil}
    elseif class.is(e, Litter) and e.tile_x then
      basura[#basura + 1] = {x = e.tile_x - 1, y = e.tile_y - 1, animacion = e.animation_idx,
        banderas = e.animation_flags, orden = E.orden_en_casilla(world, e)}
    end
  end
  -- El dueño de cada parcela (Map:setPlotOwner), de la 1 en adelante.
  local parcelas = {}
  for p = 1, world.map.th:getPlotCount() do parcelas[p] = world.map.th:getPlotOwner(p) end
  -- world.objects[(y - 1) * ancho + x], desde 1: la casilla y * ancho + x desde 0.
  local listas = {}
  for indice in pairs(world.objects) do listas[#listas + 1] = indice - 1 end
  table.sort(listas)
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

  -- Mundo y hospital ----------------------------------------------------------------
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
    listas = listas,
    parcelas = parcelas,
    basura = basura,
    salas = salas,
    personal = personal,
  }
  local resumen = ("%d objetos, %d listas de world.objects, %d basuras, %d salas, %d en plantilla, " ..
    "%d puntos de aparición, fecha %s"):format(#objetos, #listas, #basura, #salas, #personal, #puntos, d:tostring())
  return doc, resumen
end

--! Escribe celdas.tsv y escenario.json en las rutas dadas. Devuelve el
--! resumen para el registro.
function E.exportar(world, ruta_celdas, ruta_json)
  E.escribir_celdas(ruta_celdas, world.map.th)
  local doc, resumen = E.documento(world)
  local f = assert(io.open(ruta_json, "w"))
  f:write(canonico.escribir(doc), "\n")
  f:close()
  return resumen
end

return E
