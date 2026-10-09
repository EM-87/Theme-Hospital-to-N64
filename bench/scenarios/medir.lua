-- Medición sobre una partida cargada con --load=<fichero>.
--
-- Opciones (--th64-<nombre>=valor):
--   warmup  horas de juego de calentamiento antes de medir (por defecto 100)
--   hours   horas de juego medidas (por defecto 500)
--   speed   velocidad del juego durante la medición (por defecto Normal)
--   batch   ticks de mundo por evento de temporizador (1 = tiempo real)
--
-- Escribe medida.json con: heap de Lua (collectgarbage("count") tras cada
-- hora: mínimo, máximo, media, percentiles; y heap vivo tras un GC completo al
-- empezar y al acabar), entidades (al empezar, cada 50 horas y al acabar) y
-- metadatos. Los tiempos por hora salen de la traza de Tracy: las zonas
-- "th64_hora" son la medición; las del calentamiento llevan "_calentamiento".
local th64 = th64
local o = th64.opts
local warmup = tonumber(o.warmup or "100")
local hours = tonumber(o.hours or "500")
local speed = o.speed or "Normal"
local batch = tonumber(o.batch or "1")

local world = assert(TheApp.world, "no hay partida cargada (falta --load=)")
th64.log("medir: %s, %s, calentamiento %d h, medición %d h a %s", tostring(o.save or "?"),
  _VERSION, warmup, hours, speed)

local function live_kb()
  collectgarbage("collect")
  collectgarbage("collect")
  return collectgarbage("count")
end

local function percentile(sorted, p)
  if #sorted == 0 then return nil end
  local i = math.max(1, math.min(#sorted, math.ceil(p / 100 * #sorted)))
  return sorted[i]
end

th64.autorun_speed = speed
th64.zone_suffix = "_calentamiento"
th64.run_hours(warmup, batch)
th64.zone_suffix = ""

local start = {
  date = world.game_date:tostring(),
  lua_live_kb = live_kb(),
  entities = th64.count_entities(world),
}

local kb, ent_series = {}, {}
local h0 = th64.hours_done()
th64.set_sampler(function()
  local n = th64.hours_done() - h0
  kb[#kb + 1] = collectgarbage("count")
  if n % 50 == 0 then
    local c = th64.count_entities(world)
    ent_series[#ent_series + 1] = {hour = n, patients = c.patients, staff = c.staff,
      objects = c.objects, total = c.total}
  end
end)
tracy.Message("th64 medicion inicio")
th64.run_hours(hours, batch)
tracy.Message("th64 medicion fin")
th64.zone_suffix = "_fin"
th64.set_sampler(nil)

local finish = {
  date = world.game_date:tostring(),
  entities = th64.count_entities(world),
  lua_count_kb = collectgarbage("count"),
}
finish.lua_live_kb = live_kb()

local sorted, sum = {}, 0
for i, v in ipairs(kb) do sorted[i] = v sum = sum + v end
table.sort(sorted)
local mean_patients, n = 0, 0
for _, e in ipairs(ent_series) do mean_patients = mean_patients + e.patients n = n + 1 end

th64.write_json("medida.json", {
  save = o.save, lua = _VERSION, speed = speed, batch = batch,
  warmup_hours = warmup, hours = #kb,
  start = start, finish = finish,
  lua_heap_kb = {
    min = sorted[1], max = sorted[#sorted], mean = sum / math.max(1, #kb),
    p50 = percentile(sorted, 50), p95 = percentile(sorted, 95), p99 = percentile(sorted, 99),
  },
  mean_patients = n > 0 and mean_patients / n or nil,
  entities_series = ent_series,
  lua_heap_series_kb = kb,
})
th64.log("heap Lua: vivo %.0f -> %.0f KB, muestreado máx %.0f KB, media %.0f KB",
  start.lua_live_kb, finish.lua_live_kb, sorted[#sorted], sum / math.max(1, #kb))
th64.quit(0)
