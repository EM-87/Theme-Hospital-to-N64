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


-- Animaciones de cada objeto: hojas enteras bajo campos *anim*, con los tipos de humanoide del camino.
local htypes = {}
local function walk(t, path, emit, seen)
  seen = seen or {}
  if seen[t] then return end
  seen[t] = true
  for k, v in pairs(t) do
    local p = path
    if type(k) == "string" and (k:match("Patient") or k:match("Doctor") or k:match("Nurse") or k:match("Handyman") or k:match("Receptionist") or k:match("Surgeon") or k:match("VIP") or k:match("Inspector") or k:match("Reaper")) then
      p = (path == "" and k) or (path .. "|" .. k)
    end
    if type(v) == "number" and math.type(v) == "integer" and v >= 0 and v < 5186 then emit(p, v)
    elseif type(v) == "table" and getmetatable(v) ~= magic_mt then walk(v, p, emit, seen) end
  end
end
for id, o in pairs(objects) do
  for field, v in pairs(o) do
    if type(field) == "string" and field:match("anim") then
      if type(v) == "number" then print(string.format("OBJ\t%s\t%s\t*\t%d", id, tostring(rawget(o, "thob")), v))
      elseif type(v) == "table" then
        walk(v, "", function(p, a) print(string.format("OBJ\t%s\t%s\t%s\t%d", id, tostring(rawget(o, "thob")), p == "" and "*" or p, a)) end)
      end
    end
  end
end
for _, r in ipairs(rooms) do
  for field, v in pairs(r) do
    if type(field) == "string" and field:match("anim") and type(v) == "number" then print(string.format("ROOM\t%s\t%d", r.id, v)) end
  end
  local need = {}
  for name in pairs(r.objects_needed) do need[#need + 1] = name end
  local add = {}
  if type(rawget(r, "objects_additional")) == "table" then for _, n in ipairs(r.objects_additional) do add[#add + 1] = n end end
  print(string.format("ROOMOBJ\t%s\t%s\t%s", r.id, table.concat(need, ","), table.concat(add, ",")))
end
-- Disponibilidad de objetos (thob) en el nivel 1
local base = dofile(LUA .. "/base_config.lua")
assert(loadMapConfig(SAMDIR .. "/FULL00.SAM", base))
assert(loadMapConfig(SAMDIR .. "/FULL01.SAM", base))
for thob, spec in pairs(base.objects) do
  print(string.format("AVAIL1\t%d\t%s\t%s", thob, tostring(spec.AvailableForLevel), tostring(spec.StartAvail)))
end
