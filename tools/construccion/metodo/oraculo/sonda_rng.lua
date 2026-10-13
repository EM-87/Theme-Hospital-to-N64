-- Sonda: ¿quién llama a math.random y en qué fase (timer = simulación + UI:onTick,
-- frame = dibujo)? Guion para tools/bench_run.sh sobre una partida cargada.
-- Opciones: hours (por defecto 200), batch (30), frames (1).
local th64 = th64
local o = th64.opts
local hours = tonumber(o.hours or "200")
local batch = tonumber(o.batch or "30")
th64.skip_frames = o.frames == "0"
local counts = {}
local orig = math.random
local getinfo = debug.getinfo
math.random = function(...)
  local info = getinfo(2, "Sl")
  local src = (info and info.short_src or "?"):gsub("^.*/CorsixTH/Lua/", "")
  local caller = info and (src .. ":" .. (info.currentline or 0)) or "?"
  -- math.t_random/n_random/p_random están en app.lua: subir un nivel más
  if caller:match("^app%.lua") then
    local i2 = getinfo(3, "Sl")
    if i2 then caller = caller .. " <- " .. i2.short_src:gsub("^.*/CorsixTH/Lua/", "") .. ":" .. i2.currentline end
  end
  local k = (th64.phase or "otro") .. " " .. caller
  counts[k] = (counts[k] or 0) + 1
  return orig(...)
end
th64.autorun_speed = o.speed or "Normal"
th64.run_hours(hours, batch)
local list = {}
for k, v in pairs(counts) do list[#list + 1] = {k, v} end
table.sort(list, function(a, b) return a[2] > b[2] end)
local total, by_phase = 0, {}
for _, kv in ipairs(list) do
  total = total + kv[2]
  local ph = kv[1]:match("^(%S+)")
  by_phase[ph] = (by_phase[ph] or 0) + kv[2]
end
th64.log("llamadas a math.random en %d h: %d; por fase: %s", hours, total,
  th64.to_json(by_phase):gsub("\n%s*", " "))
for i = 1, math.min(40, #list) do th64.log("  %6d  %s", list[i][2], list[i][1]) end
th64.quit(0)
