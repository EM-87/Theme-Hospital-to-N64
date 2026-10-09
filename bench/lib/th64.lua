-- Planificador de guiones dentro de CorsixTH.
--
-- El guion (--th64-script) se ejecuta como corrutina. Cada vez que cede el
-- control pide algo al planificador:
--   th64.wait_ticks(n)    deja pasar n eventos de temporizador (18 ms reales cada uno)
--   th64.run_hours(n, b)  avanza n horas de juego, ejecutando hasta b ticks de
--                         mundo por evento de temporizador (b > 1 = avance rápido)
-- Mientras tanto el juego sigue su bucle normal: el render, la UI y la
-- simulación son los de CorsixTH, sin cambios.
--
-- Mediciones:
--   * Zonas de Tracy "th64_hora" (evento de temporizador que procesa una hora de
--     juego: la simulación), "th64_tick" (evento sin hora nueva) y "th64_frame"
--     (dibujo de un frame).
--   * collectgarbage("count") tras cada hora simulada, y conteo de entidades.

local M = {}

-- En los builds sin Tracy, CorsixTH crea un sustituto (utility.lua) después de
-- cargar este módulo; se toma la referencia en M.start().
local tracy
local opts, out_dir, log_fh
local script_co, req
local hours_done = 0
local sampler = nil
M.autorun_speed = nil
M.zone_suffix = "" -- p. ej. "_calentamiento" para separar fases en la traza

function M.init(o, bench_dir)
  opts = o
  M.opts = o
  M.bench_dir = bench_dir
  out_dir = o.out or "."
  os.execute('mkdir -p "' .. out_dir .. '"')
  log_fh = assert(io.open(out_dir .. "/th64.log", "w"))
end

function M.out(name)
  return out_dir .. "/" .. name
end

function M.log(fmt, ...)
  local s = select("#", ...) > 0 and fmt:format(...) or tostring(fmt)
  io.stdout:write("[th64] ", s, "\n")
  io.stdout:flush()
  log_fh:write(s, "\n")
  log_fh:flush()
end

function M.hours_done()
  return hours_done
end

-- Peticiones del guion ------------------------------------------------------

function M.wait_ticks(n)
  coroutine.yield({kind = "ticks", n = n or 1})
end

function M.run_hours(n, batch)
  coroutine.yield({kind = "hours", n = n, batch = batch or 1})
end

--! Llama a fn(app) después de cada hora simulada (nil para quitarlo).
function M.set_sampler(fn)
  sampler = fn
end

function M.quit(code)
  M.log("fin (código %d)", code or 0)
  log_fh:close()
  if code and code ~= 0 then
    os.exit(code, true)
  end
  -- Un segundo más de bucle para que Tracy envíe lo pendiente antes de salir.
  if coroutine.isyieldable() then
    M.autorun_speed = nil
    for _ = 1, 55 do coroutine.yield({kind = "ticks", n = 1}) end
  end
  TheApp:abandon()
end

-- Bucle ---------------------------------------------------------------------

local function sim_hour_pending(app)
  local w = app.world
  return w and not app.moviePlayer.playing and w.map.level_number ~= "MAP EDITOR"
    and w.tick_timer == 0 and w.hours_per_tick > 0
end

local function timed_timer(app, orig, ...)
  local hour = sim_hour_pending(app)
  local hours = hour and app.world.hours_per_tick or 0
  tracy.ZoneBeginN((hour and "th64_hora" or "th64_tick") .. M.zone_suffix)
  local r = orig(app, ...)
  tracy.ZoneEnd()
  if hour then
    hours_done = hours_done + hours
    if sampler then sampler(app) end
  end
  return r
end

-- Respuesta a los faxes, por orden de preferencia: lo que haría un jugador que
-- quiere un hospital grande y activo (aceptar emergencias y VIP, encubrir la
-- epidemia para que siga activa, arriesgar tratamiento si el diagnóstico es dudoso).
local FAX_CHOICES = {"accept_emergency", "accept_vip", "cover_up_epidemic",
  "guess_cure", "wait", "stay_on_level"}
M.decisions = {}

local function answer_window(win)
  local name = class.type(win) or "?"
  local choice = "cerrar"
  if class.is(win, UIStaffRise) then
    choice = "subir sueldo"
    win:increaseSalary()
  elseif class.is(win, UIFax) and win.message and win.message.choices then
    local idx
    for _, want in ipairs(FAX_CHOICES) do
      for i, c in ipairs(win.message.choices) do
        if c.choice == want and c.enabled ~= false then idx = i break end
      end
      if idx then break end
    end
    if idx then
      choice = win.message.choices[idx].choice
      win:choice(idx)
    else
      win:close()
    end
  else
    win:close()
  end
  local k = name .. ": " .. choice
  M.decisions[k] = (M.decisions[k] or 0) + 1
end

-- Si el guion quiere que el juego corra, responde a lo que lo pausa (faxes,
-- subidas de sueldo, informes anuales) y restablece la velocidad.
local function keep_running(app)
  local w = app.world
  if not (M.autorun_speed and w) then return end
  if w:mustPause() then
    for _, win in ipairs({table.unpack(app.ui.windows)}) do
      if win:mustPause() then
        answer_window(win)
      end
    end
  end
  if not w:isCurrentSpeed(M.autorun_speed) then
    w:setSpeed(M.autorun_speed)
  end
end

local function resume()
  local ok, r = coroutine.resume(script_co)
  if not ok then
    error(debug.traceback(script_co, r), 0)
  end
  if coroutine.status(script_co) == "dead" then
    req = {kind = "done"}
  else
    req = r
    if req.kind == "hours" then
      req.target = hours_done + req.n
    end
  end
end

local function step(app, orig, ...)
  if not req then
    resume()
  end
  keep_running(app)
  if req.kind == "ticks" then
    local r = timed_timer(app, orig, ...)
    req.n = req.n - 1
    if req.n <= 0 then req = nil end
    return r
  elseif req.kind == "hours" then
    local r = true
    for _ = 1, req.batch do
      r = timed_timer(app, orig, ...)
      if hours_done >= req.target then
        req = nil
        break
      end
    end
    return r
  end
  return timed_timer(app, orig, ...)
end

function M.start()
  tracy = rawget(_G, "tracy")
  local app = TheApp
  local script = assert(loadfile(opts.script))
  script_co = coroutine.create(script)

  local orig_timer = app.eventHandlers.timer
  app.eventHandlers.timer = function(self, ...)
    local ok, res = xpcall(step, debug.traceback, self, orig_timer, ...)
    if not ok then
      M.log("ERROR en el guion:\n%s", tostring(res))
      M.quit(3)
    end
    return res
  end

  local orig_frame = app.eventHandlers.frame
  app.eventHandlers.frame = function(self, ...)
    tracy.ZoneBeginN("th64_frame" .. M.zone_suffix)
    local r = orig_frame(self, ...)
    tracy.ZoneEnd()
    return r
  end
  M.log("arnés listo: %s (%s)", opts.script, _VERSION)
end

-- Utilidades para los guiones -----------------------------------------------

--! Cuenta entidades del mundo por tipo.
function M.count_entities(world)
  local c = {patients = 0, staff = 0, doctors = 0, nurses = 0, handymen = 0,
    receptionists = 0, other_humanoids = 0, objects = 0, other = 0, total = 0, by_class = {}}
  for _, e in ipairs(world.entities) do
    c.total = c.total + 1
    local cname = class.type(e) or "?"
    c.by_class[cname] = (c.by_class[cname] or 0) + 1
    if class.is(e, Patient) then
      c.patients = c.patients + 1
    elseif class.is(e, Staff) then
      c.staff = c.staff + 1
      local hc = e.humanoid_class
      if hc == "Doctor" or hc == "Surgeon" then c.doctors = c.doctors + 1
      elseif hc == "Nurse" then c.nurses = c.nurses + 1
      elseif hc == "Handyman" then c.handymen = c.handymen + 1
      elseif hc == "Receptionist" then c.receptionists = c.receptionists + 1
      end
    elseif class.is(e, Humanoid) then
      c.other_humanoids = c.other_humanoids + 1
    elseif class.is(e, Object) then
      c.objects = c.objects + 1
    else
      c.other = c.other + 1
    end
  end
  local rooms = 0
  for _, r in pairs(world.rooms) do
    if r and not r.crashed then rooms = rooms + 1 end
  end
  c.rooms = rooms
  return c
end

--! Serializa una tabla Lua simple (números, cadenas, booleanos, tablas) a JSON.
local function to_json(v, indent)
  indent = indent or ""
  local t = type(v)
  if t == "table" then
    local is_array = #v > 0 or next(v) == nil
    local parts, ni = {}, indent .. "  "
    if is_array then
      for _, x in ipairs(v) do parts[#parts + 1] = ni .. to_json(x, ni) end
      return "[\n" .. table.concat(parts, ",\n") .. "\n" .. indent .. "]"
    end
    local keys = {}
    for k in pairs(v) do keys[#keys + 1] = tostring(k) end
    table.sort(keys)
    for _, k in ipairs(keys) do
      parts[#parts + 1] = ni .. string.format("%q", k) .. ": " .. to_json(v[k] == nil and v[tonumber(k)] or v[k], ni)
    end
    return "{\n" .. table.concat(parts, ",\n") .. "\n" .. indent .. "}"
  elseif t == "number" then
    if v ~= v or v == math.huge or v == -math.huge then return "null" end
    return (math.type and math.type(v) == "integer") and tostring(v) or string.format("%.6g", v)
  elseif t == "boolean" then
    return tostring(v)
  elseif v == nil then
    return "null"
  end
  return string.format("%q", tostring(v))
end
M.to_json = to_json

function M.write_json(name, data)
  local f = assert(io.open(M.out(name), "w"))
  f:write(to_json(data), "\n")
  f:close()
end

return M
