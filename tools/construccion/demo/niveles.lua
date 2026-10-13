-- Fase 4b (modo demo): qué trae cada nivel de la campaña.
-- Carga la configuración igual que CorsixTH (base_config.lua + FULL00.SAM +
-- FULLxx.SAM + originalXX.level; map.lua Map:loadMapConfig y
-- Map:_loadOriginalCampaignLevel), calcula habitaciones disponibles
-- (World:getAvailableRooms) y enfermedades (World:initLevel), y cuenta las
-- casillas del mapa (level_map::load_from_th_file, th_map.cpp).
-- Uso: lua5.4 niveles.lua <CorsixTH/Lua> <CorsixTH/Levels> <dir .SAM> <dir mapas descomprimidos>
local LUA, LEVELS, SAMDIR, MAPDIR = arg[1], arg[2], arg[3], arg[4]

-- Objeto comodín para que los ficheros de definición carguen sin el juego.
local magic_mt = {}
local function magic() return setmetatable({}, magic_mt) end
magic_mt.__index = function() return magic() end
magic_mt.__call = function() return magic() end
for _, k in ipairs{"__add","__sub","__mul","__div","__mod","__unm","__concat","__idiv"} do
  magic_mt[k] = function() return magic() end
end
magic_mt.__lt = function() return false end
magic_mt.__le = function() return false end
magic_mt.__len = function() return 0 end

local function sandbox()
  local env = setmetatable({}, {__index = function(_, k)
    if _G[k] ~= nil and k ~= "_G" then return _G[k] end
    return magic()
  end})
  env._G = env
  env.class = function() return function() end end
  env.permanent = function() return function(x) return x end end
  env.math = math; env.table = table; env.string = string; env.pairs = pairs; env.ipairs = ipairs
  return env
end

local function load_def(path)
  local env = sandbox()
  local f = assert(loadfile(path, "t", env))
  local ok, res = pcall(f)
  if not ok then io.stderr:write("aviso: ", path, ": ", tostring(res), "\n"); return nil end
  return res
end

local function listdir(d)
  local t = {}
  local p = io.popen('ls "' .. d .. '"')
  for l in p:lines() do t[#t + 1] = l end
  p:close()
  return t
end

-- Map:loadMapConfig (map.lua:393), solo la parte que lee líneas "#..."
local function loadMapConfig(path, config)
  local fh = io.open(path, "rb")
  if not fh then return false end
  local data = fh:read("a"); fh:close()
  for line in data:gmatch("[^\r\n]+") do
    if line:sub(1, 1) == "#" then
      local parts = {}
      local nkeys = 0
      for part in line:gmatch("%.?[-?a-zA-Z0-9%[_%]]+") do
        if part:sub(1, 1) == "." and #parts == nkeys + 1 then nkeys = nkeys + 1 end
        parts[#parts + 1] = part
      end
      if nkeys == 0 then parts[3] = parts[2]; parts[2] = ".Value"; nkeys = 1 end
      for i = 2, nkeys + 1 do
        local key = parts[1] .. parts[i]
        local t, n
        for match in key:gmatch("[^.%[%]]+") do
          local name = tonumber(match) or match
          if t then
            if not t[n] then t[n] = {} end
            t = t[n]
          else
            t = config
          end
          n = name
        end
        t[n] = tonumber(parts[nkeys + i]) or parts[nkeys + i]
      end
    end
  end
  return true
end

-- Definiciones de objetos, habitaciones y enfermedades de CorsixTH
local objects = {}
for _, sub in ipairs{"objects", "objects/machines", "objects/doors"} do
  for _, f in ipairs(listdir(LUA .. "/" .. sub)) do
    if f:match("%.lua$") then
      local o = load_def(LUA .. "/" .. sub .. "/" .. f)
      if type(o) == "table" and rawget(o, "id") then objects[o.id] = o end
    end
  end
end
local rooms = {}
for _, f in ipairs(listdir(LUA .. "/rooms")) do
  if f:match("%.lua$") then
    local r = load_def(LUA .. "/rooms/" .. f)
    if type(r) == "table" and rawget(r, "id") then rooms[#rooms + 1] = r end
  end
end
table.sort(rooms, function(a, b) return a.level_config_id < b.level_config_id end)
local diseases = {}
for _, f in ipairs(listdir(LUA .. "/diseases")) do
  if f:match("%.lua$") then
    local d = load_def(LUA .. "/diseases/" .. f)
    if type(d) == "table" and rawget(d, "id") then diseases[#diseases + 1] = d end
  end
end
table.sort(diseases, function(a, b) return (a.expertise_id or 0) < (b.expertise_id or 0) end)

-- Tipos de paciente de cada enfermedad: se llama a initPatient con un paciente falso
local function patient_types(d)
  local types = {}
  if type(rawget(d, "initPatient")) ~= "function" then return types end
  for _ = 1, 200 do
    local p = setmetatable({}, {__index = function(_, k)
      if k == "setType" then return function(_, ty) types[ty] = true end end
      return function() end
    end})
    pcall(d.initPatient, p)
  end
  return types
end

-- Casillas del mapa (th_map.cpp: load_from_th_file)
local function map_stats(path)
  local fh = assert(io.open(path, "rb")); local s = fh:read("a"); fh:close()
  assert(#s >= 163948)
  local players = math.max(1, s:byte(1))
  local parcel = {}
  local hosp, build = 0, 0
  for i = 0, 128 * 128 - 1 do
    local o = 34 + i * 8
    local b5, b7 = s:byte(o + 6), s:byte(o + 8)
    local pid = s:byte(131106 + i * 2 + 1) + 256 * s:byte(131106 + i * 2 + 2)
    local p = parcel[pid] or {tiles = 0, hospital = 0, buildable = 0}
    parcel[pid] = p
    p.tiles = p.tiles + 1
    if (b5 & 1) == 0 and (b7 & 16) == 0 then
      p.hospital = p.hospital + 1
      if (b5 & 2) == 0 then p.buildable = p.buildable + 1 end
    end
  end
  return players, parcel
end

local function sorted_keys(t)
  local k = {}
  for x in pairs(t) do k[#k + 1] = x end
  table.sort(k, function(a, b) return tostring(a) < tostring(b) end)
  return k
end

local additional = {[5] = true, [7] = true, [8] = true, [11] = true, [12] = true}
local out = {}
local function emit(s) out[#out + 1] = s end

for lvl = 1, 12 do
  local base = dofile(LUA .. "/base_config.lua")
  assert(loadMapConfig(SAMDIR .. "/FULL00.SAM", base))
  local num = lvl < 10 and ("0" .. lvl) or tostring(lvl)
  assert(loadMapConfig(SAMDIR .. "/FULL" .. num .. ".SAM", base))
  if additional[lvl] then assert(loadMapConfig(LEVELS .. "/original" .. num .. ".level", base)) end
  local cfg = base

  -- World:getAvailableRooms
  local avail, avail_set = {}, {}
  for _, room in ipairs(rooms) do
    local available, discovered = true, true
    for name in pairs(room.objects_needed) do
      local spec = cfg.objects[objects[name].thob]
      if spec.AvailableForLevel == 0 then available = false
      elseif spec.StartAvail == 0 then discovered = false end
    end
    if available then
      avail[#avail + 1] = room.id .. (discovered and "" or "*")
      avail_set[room.id] = true
    end
  end
  -- World:initLevel
  local dis, ptypes = {}, {}
  for _, d in ipairs(diseases) do
    if not rawget(d, "pseudo") then
      local vis = 1
      local vid, nid = rawget(d, "visuals_id"), rawget(d, "non_visuals_id")
      if cfg.visuals and (cfg.visuals[vid] or cfg.non_visuals[nid]) then
        vis = vid and cfg.visuals[vid].Value or cfg.non_visuals[nid].Value
      end
      if vis ~= 0 then
        for _, r in ipairs(d.treatment_rooms) do if not avail_set[r] then vis = 0; break end end
      end
      if vis ~= 0 then
        dis[#dis + 1] = d.id .. (vid and "(v)" or "")
        for ty in pairs(patient_types(d)) do ptypes[ty] = true end
      end
    end
  end
  local win = {}
  local names = {"reputacion", "saldo", "%curados", "curados", "%muertos", "valor"}
  for i = 0, 9 do
    local w = cfg.win_criteria and cfg.win_criteria[i]
    if w and w.Criteria and w.Criteria ~= 0 then win[#win + 1] = names[w.Criteria] .. ">=" .. w.Value end
  end
  local lose = {}
  for i = 0, 9 do
    local w = cfg.lose_criteria and cfg.lose_criteria[i]
    if w and w.Criteria and w.Criteria ~= 0 then lose[#lose + 1] = names[w.Criteria] .. (w.MaxMin == 1 and ">=" or "<=") .. w.Value end
  end
  local players, parcel = map_stats(MAPDIR .. "/LEVEL.L" .. lvl)
  local p1 = parcel[1] or {tiles = 0, hospital = 0, buildable = 0}
  local other_b, other_n = 0, 0
  for pid, p in pairs(parcel) do
    if pid > players and p.buildable > 0 then other_b = other_b + p.buildable; other_n = other_n + 1 end
  end
  local emerg = 0
  if cfg.emergency_control then for i = 0, 99 do if cfg.emergency_control[i] and (cfg.emergency_control[i].Random or cfg.emergency_control[i].StartMonth) then emerg = emerg + 1 end end end
  emit(string.format("NIVEL\t%d\tcaja=%s\tparcela1_edificables=%d\tparcela1_hospital=%d\tparcelas_compra=%d\tcompra_edificables=%d\thabitaciones=%d\tenfermedades=%d",
    lvl, tostring(cfg.town.StartCash), p1.buildable, p1.hospital, other_n, other_b, #avail, #dis))
  emit("  habitaciones: " .. table.concat(avail, " "))
  emit("  enfermedades: " .. table.concat(dis, " "))
  emit("  pacientes: " .. table.concat(sorted_keys(ptypes), ", "))
  emit("  ganar: " .. table.concat(win, ", "))
  emit("  perder: " .. table.concat(lose, ", "))
end
print(table.concat(out, "\n"))
