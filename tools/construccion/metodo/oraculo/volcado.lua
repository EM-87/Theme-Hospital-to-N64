-- Oráculo: vuelca el estado de la simulación tras cada hora de juego, sobre una
-- partida cargada con --load=<fichero>. Guion para tools/bench_run.sh.
--
-- Opciones (--th64-<nombre>=valor):
--   hours   horas de juego (por defecto 500 = 10 días)
--   batch   ticks de mundo por evento de temporizador (por defecto 30)
--   frames  0 = no dibujar (por defecto 1)
--   seed    si se da, math.randomseed(seed) justo después de cargar
--   speed   velocidad (por defecto Normal)
--
-- Escribe volcado.jsonl: una línea JSON por hora, con agregados del hospital
-- (nombres exactos de hospital.lua/world.lua), colas y dos huellas:
--   rng: FNV-1a del estado del Mersenne Twister (math.randomdump())
--   pos: FNV-1a de (clase, casilla, acción actual) de cada entidad, en el
--        orden de world.entities
local th64 = th64
local o = th64.opts
local hours = tonumber(o.hours or "500")
local batch = tonumber(o.batch or "30")
th64.skip_frames = o.frames == "0"

local world = assert(TheApp.world, "no hay partida cargada (falta --load=)")
if o.seed then
  math.randomseed(tonumber(o.seed))
end

local function fnv(s, h)
  h = h or 2166136261
  for i = 1, #s do
    h = ((h ~ s:byte(i)) * 16777619) & 0xffffffff
  end
  return h
end

local function snapshot()
  local h = world.hospitals[1]
  local queues, queue_rooms, rooms = 0, 0, 0
  for _, r in pairs(world.rooms) do
    if r and not r.crashed then
      rooms = rooms + 1
      if r.door and r.door.queue then
        queues = queues + r.door.queue:size()
        queue_rooms = queue_rooms + 1
      end
    end
  end
  local pos = 2166136261
  local n_patients, n_staff, n_objects, n_litter = 0, 0, 0, 0
  local happiness, warmth, thirst, nh = 0, 0, 0, 0
  for _, e in ipairs(world.entities) do
    local cls = class.type(e) or "?"
    local a = e.action_queue and e.action_queue[1]
    pos = fnv(string.format("%s|%s|%s|%s;", cls, tostring(e.tile_x), tostring(e.tile_y),
      a and a.name or ""), pos)
    if class.is(e, Patient) then
      n_patients = n_patients + 1
      if e.attributes then
        happiness = happiness + (e.attributes.happiness or 0)
        warmth = warmth + (e.attributes.warmth or 0)
        thirst = thirst + (e.attributes.thirst or 0)
        nh = nh + 1
      end
    elseif class.is(e, Staff) then
      n_staff = n_staff + 1
    elseif class.is(e, Litter) then
      n_litter = n_litter + 1
    elseif class.is(e, Object) then
      n_objects = n_objects + 1
    end
  end
  return {
    hora = th64.hours_done(),
    fecha = world.game_date:tostring(),
    balance = h.balance,
    reputation = h.reputation,
    value = h.value,
    loan = h.loan,
    num_visitors = h.num_visitors,
    num_cured = h.num_cured,
    not_cured = h.not_cured,
    num_deaths = h.num_deaths,
    num_explosions = h.num_explosions,
    percentage_cured = h.percentage_cured,
    money_in = h.money_in,
    money_out = h.money_out,
    sodas_sold = h.sodas_sold,
    patients = #h.patients,
    staff = #h.staff,
    entities = #world.entities,
    ent_patients = n_patients, ent_staff = n_staff, ent_objects = n_objects, ent_litter = n_litter,
    rooms = rooms,
    queue_total = queues,
    happiness_mean = nh > 0 and happiness / nh or nil,
    warmth_mean = nh > 0 and warmth / nh or nil,
    thirst_mean = nh > 0 and thirst / nh or nil,
    rng = fnv(math.randomdump()),
    pos = pos,
  }
end

local out = assert(io.open(th64.out("volcado.jsonl"), "w"))
local function emit()
  out:write((th64.to_json(snapshot()):gsub("\n%s*", " ")), "\n")
end

th64.log("volcado: %s, %d h, batch %d, frames %s, seed %s", tostring(o.save or "?"), hours, batch,
  tostring(not th64.skip_frames), tostring(o.seed))
emit()
th64.set_sampler(function() emit() end)
th64.autorun_speed = o.speed or "Normal"
th64.run_hours(hours, batch)
th64.set_sampler(nil)
out:close()
th64.log("decisiones: %s", th64.to_json(th64.decisions):gsub("\n%s*", " "))
th64.quit(0)
