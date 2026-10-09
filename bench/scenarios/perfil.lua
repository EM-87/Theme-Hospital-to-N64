-- Perfil de la simulación por fichero y función Lua, sobre una partida cargada
-- con --load=<fichero>.
--
-- Opciones (--th64-<nombre>=valor):
--   warmup  horas de juego de calentamiento (por defecto 50)
--   hours   horas de juego medidas (por defecto 300)
--   batch   ticks de mundo por evento de temporizador (por defecto 30, para
--           que el dibujo de frames pese poco frente a la simulación)
--   every   instrucciones de la VM entre muestras (por defecto 1000; 0 = sin
--           muestreo, solo las marcas para callgrind)
--   frames  0 = no dibujar durante la medición (por defecto 1: se dibuja). Sin
--           dibujo, callgrind mide solo la simulación y no el render por software
--
-- Muestreo: un hook de conteo (debug.sethook con count) en el hilo de eventos
-- del juego. Cada muestra se apunta a la función Lua que se está ejecutando
-- (propio) y a todos los ficheros y funciones de la pila (inclusivo), separando
-- si estaba en el manejador del temporizador (simulación y UI.onTick) o en el
-- del dibujo. Cuenta instrucciones de la VM, no tiempo: lo que pasa dentro de
-- funciones C (pathfinding, mapa, render) no se ve aquí; para eso está la
-- pasada con callgrind (tools/bench_run.sh --callgrind).
--
-- Escribe perfil.json.
local th64 = th64
local o = th64.opts
local warmup = tonumber(o.warmup or "50")
local hours = tonumber(o.hours or "300")
local batch = tonumber(o.batch or "30")
local every = tonumber(o.every or "1000")
local frames = o.frames ~= "0"

local world = assert(TheApp.world, "no hay partida cargada (falta --load=)")
th64.log("perfil: %s, %s, calentamiento %d h, medición %d h, batch %d, muestra cada %d instrucciones",
  tostring(o.save or "?"), _VERSION, warmup, hours, batch, every)

th64.autorun_speed = o.speed or "Normal"
th64.run_hours(warmup, batch)

local getinfo = debug.getinfo
local total, by_phase = 0, {}
local self_file, incl_file, self_fn, incl_fn, names = {}, {}, {}, {}, {}

local function src_of(info)
  local s = info.source or "?"
  return (s:gsub("^@", ""):gsub("^.*/CorsixTH/Lua/", ""):gsub("^.*/bench/", "bench/"))
end

local function hook()
  local info = getinfo(2, "Sn")
  if not info then return end
  total = total + 1
  local phase = th64.phase or "otro"
  by_phase[phase] = (by_phase[phase] or 0) + 1
  local src = src_of(info)
  local key = src .. ":" .. info.linedefined
  self_file[src] = (self_file[src] or 0) + 1
  self_fn[key] = (self_fn[key] or 0) + 1
  if info.name and not names[key] then names[key] = info.name end
  local seen_f, seen_k = {}, {}
  local lvl = 2
  while true do
    local i = getinfo(lvl, "Sn")
    if not i then break end
    if i.what ~= "C" then
      local s = src_of(i)
      local k = s .. ":" .. i.linedefined
      if not seen_f[s] then
        seen_f[s] = true
        incl_file[s] = (incl_file[s] or 0) + 1
      end
      if not seen_k[k] then
        seen_k[k] = true
        incl_fn[k] = (incl_fn[k] or 0) + 1
        if i.name and not names[k] then names[k] = i.name end
      end
    end
    lvl = lvl + 1
  end
end

local start = {date = world.game_date:tostring(), entities = th64.count_entities(world)}
-- Antes y después de medir se deja un margen para que el lanzador active y
-- desactive callgrind. Durante esos márgenes el juego está en pausa y, si se
-- pide, sin dibujar, para que no se cuelen horas ni frames fuera de la medida.
local speed = th64.autorun_speed
th64.skip_frames = not frames
th64.autorun_speed = "Pause"
th64.log("perfil: medición inicio")
th64.wait_ticks(60)
th64.autorun_speed = speed
if every > 0 then th64.set_profiler({hook = hook, count = every}) end
local c0 = os.clock()
th64.run_hours(hours, batch)
local cpu = os.clock() - c0
th64.set_profiler(nil)
th64.autorun_speed = "Pause"
th64.wait_ticks(2)
th64.log("perfil: medición fin")
th64.wait_ticks(60)
th64.skip_frames = false

local function top(t, n)
  local arr = {}
  for k, v in pairs(t) do arr[#arr + 1] = {k, v} end
  table.sort(arr, function(a, b) return a[2] > b[2] end)
  local out = {}
  for i = 1, math.min(n, #arr) do
    out[i] = {clave = arr[i][1], muestras = arr[i][2], nombre = names[arr[i][1]]}
  end
  return out
end

th64.write_json("perfil.json", {
  save = o.save, lua = _VERSION, warmup_hours = warmup, hours = hours, batch = batch,
  every = every, frames = frames, cpu_s = cpu, start = start,
  finish = {date = world.game_date:tostring(), entities = th64.count_entities(world)},
  muestras = total, por_fase = by_phase,
  ficheros_propio = top(self_file, 500), ficheros_inclusivo = top(incl_file, 500),
  funciones_propio = top(self_fn, 150), funciones_inclusivo = top(incl_fn, 150),
})
th64.log("perfil: %d muestras en %.1f s de CPU", total, cpu)
th64.quit(0)
