-- Carga las definiciones de CorsixTH (objects, rooms, diseases, diagnosis) con
-- lua5.4 de serie y stubs mínimos, y las vuelca a JSON.
--
-- Uso: lua5.4 extraer.lua <dir Lua de CorsixTH> <dir salida>
--
-- Stubs (todo lo demás es Lua estándar o código de CorsixTH sin cambios):
--   * strict_declare_global / destrict: los usa class.lua (de strict.lua).
--   * _S, _A: cadenas de idioma; devuelven una referencia "_S.object.bench".
--   * TheApp: animation_manager (registra setStaffMarker/setPatientMarker),
--     config (tabla vacía que anota qué claves se leen), gfx/ui/world (stub).
--   * corsixth.require: no carga nada salvo lo que se pide aquí a mano.
--   * require("TH"): stub.
--   * permanent"x"(f): devuelve f (es para la persistencia de partidas).
-- Las clases base (Entity, Object, Machine, Room) se declaran con el class.lua
-- real de CorsixTH; Object.processTypeDefinition se toma del object.lua real.

local base, outdir = arg[1], arg[2]
assert(base and outdir, "uso: lua5.4 extraer.lua <dir Lua> <dir salida>")

local report = {missing_globals = {}, config_reads = {}, markers = {}, stub_calls = {},
  requires = {}}

-- Stub automático: cualquier acceso devuelve otro stub con la ruta.
local Stub = {}
local function stub(path)
  return setmetatable({__stub = path}, Stub)
end
Stub.__index = function(t, k)
  if k == "__stub" then return rawget(t, "__stub") end
  return stub(rawget(t, "__stub") .. "." .. tostring(k))
end
Stub.__call = function(t, ...)
  local p = rawget(t, "__stub")
  report.stub_calls[p] = (report.stub_calls[p] or 0) + 1
  return stub(p .. "()")
end
Stub.__tostring = function(t) return rawget(t, "__stub") end
Stub.__concat = function(a, b) return tostring(a) .. tostring(b) end

-- Entorno compartido para todos los ficheros (como _G en CorsixTH).
local SAFE = {"assert", "error", "ipairs", "next", "pairs", "pcall", "print", "rawequal",
  "rawget", "rawlen", "rawset", "select", "setmetatable", "getmetatable", "tonumber",
  "tostring", "type", "xpcall", "math", "string", "table", "utf8"}
local env = {}
for _, k in ipairs(SAFE) do env[k] = _G[k] end
env._G = env
env.unpack = table.unpack -- CorsixTH lo define en utility.lua
env.strict_declare_global = function() end
env.destrict = function(f) return f end
env._S = stub("_S")
env._A = stub("_A")
env.permanent = function(_) return function(f) return f end end
env.corsixth = {require = function(name)
  report.requires[name] = (report.requires[name] or 0) + 1
end}
env.require = function(name)
  report.requires["require:" .. name] = (report.requires["require:" .. name] or 0) + 1
  return stub(name)
end
local config = setmetatable({}, {__index = function(_, k)
  report.config_reads[k] = (report.config_reads[k] or 0) + 1
  return nil
end})
local current_file
local anim_mgr = {}
function anim_mgr:setStaffMarker(anim, ...)
  report.markers[#report.markers + 1] = {file = current_file, kind = "staff", anim = anim, args = {...}}
end
function anim_mgr:setPatientMarker(anim, ...)
  report.markers[#report.markers + 1] = {file = current_file, kind = "patient", anim = anim, args = {...}}
end
function anim_mgr:setMarker(anim, ...)
  report.markers[#report.markers + 1] = {file = current_file, kind = "marker", anim = anim, args = {...}}
end
report.anim_length_overrides = {}
function anim_mgr:setAnimLength(anim, len)
  report.anim_length_overrides[#report.anim_length_overrides + 1] = {file = current_file, anim = anim, length = len}
end
function anim_mgr:getAnimLength(anim)
  return stub("getAnimLength(" .. tostring(anim) .. ")")
end
env.TheApp = setmetatable({animation_manager = anim_mgr, config = config},
  {__index = function(_, k) return stub("TheApp." .. k) end})
setmetatable(env, {__index = function(_, k)
  report.missing_globals[k] = (report.missing_globals[k] or 0) + 1
  return nil
end})

local function run(path)
  current_file = path:sub(#base + 2)
  local chunk, err = loadfile(path, "t", env)
  if not chunk then return false, err end
  return pcall(chunk)
end

-- class.lua real de CorsixTH
assert(run(base .. "/class.lua"))
local class = env.class
-- Ficheros reales de CorsixTH que las definiciones necesitan al cargar:
-- utility.lua (AnimationEffect, unpack...), entity.lua, humanoid.lua
-- (Humanoid.getIdleAnimation), object.lua (Object.processTypeDefinition),
-- machine.lua, map.lua (Map:WorldToScreen), room.lua. Si alguno falla, se
-- declara la clase vacía y se anota.
report.base_files = {}
local fallback = {
  ["entity.lua"] = function() class "Entity" end,
  ["entities/humanoid.lua"] = function() class "Humanoid" (env.Entity) end,
  ["entities/object.lua"] = function() class "Object" (env.Entity) end,
  ["entities/machine.lua"] = function() class "Machine" (env.Object) end,
  ["map.lua"] = function() class "Map" end,
  ["room.lua"] = function() class "Room" end,
}
for _, f in ipairs({"utility.lua", "entity.lua", "entities/humanoid.lua", "entities/object.lua",
    "entities/machine.lua", "map.lua", "room.lua"}) do
  local ok, err = run(base .. "/" .. f)
  report.base_files[f] = ok and "ok" or tostring(err)
  if not ok and fallback[f] then fallback[f]() end
end

local function list_lua(dir)
  local p = io.popen('ls "' .. dir .. '"')
  local files = {}
  for f in p:lines() do
    if f:match("%.lua$") then files[#files + 1] = f end
  end
  p:close()
  table.sort(files)
  return files
end

local results = {}
local function load_folder(kind, dir)
  results[kind] = results[kind] or {ok = {}, failed = {}, defs = {}}
  local r = results[kind]
  for _, f in ipairs(list_lua(base .. "/" .. dir)) do
    local ok, val = run(base .. "/" .. dir .. "/" .. f)
    if ok and type(val) == "table" and val.id then
      r.ok[#r.ok + 1] = dir .. "/" .. f
      r.defs[#r.defs + 1] = val
    else
      r.failed[#r.failed + 1] = {file = dir .. "/" .. f, error = tostring(val)}
    end
  end
end

load_folder("objects", "objects")
load_folder("objects", "objects/machines")
load_folder("objects", "objects/doors")
load_folder("rooms", "rooms")
load_folder("diseases", "diseases")
load_folder("diagnosis", "diagnosis")

-- Volcado a JSON ----------------------------------------------------------------
local function is_array(t)
  local n = 0
  for k in pairs(t) do
    if type(k) ~= "number" or k < 1 or math.type(k) ~= "integer" then return false end
    n = n + 1
  end
  return n == #t
end

local function to_json(v, seen, indent)
  indent = indent or ""
  local t = type(v)
  if t == "table" then
    if getmetatable(v) == Stub then
      return string.format('{"$ref": %q}', rawget(v, "__stub"))
    end
    if seen[v] then return '{"$cycle": true}' end
    seen[v] = true
    local ni = indent .. " "
    local parts = {}
    if is_array(v) and next(v) ~= nil then
      for _, x in ipairs(v) do parts[#parts + 1] = ni .. to_json(x, seen, ni) end
      seen[v] = nil
      return "[\n" .. table.concat(parts, ",\n") .. "\n" .. indent .. "]"
    end
    local keys = {}
    for k in pairs(v) do keys[#keys + 1] = k end
    table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)
    for _, k in ipairs(keys) do
      parts[#parts + 1] = ni .. string.format("%q", tostring(k)) .. ": " .. to_json(v[k], seen, ni)
    end
    seen[v] = nil
    if #parts == 0 then return "{}" end
    return "{\n" .. table.concat(parts, ",\n") .. "\n" .. indent .. "}"
  elseif t == "function" then
    local info = debug.getinfo(v, "S")
    return string.format('{"$fn": "%s:%d-%d"}', info.source:sub(#base + 3), info.linedefined,
      info.lastlinedefined)
  elseif t == "number" then
    if math.type(v) == "integer" then return tostring(v) end
    if v ~= v or v == math.huge or v == -math.huge then return "null" end
    return string.format("%.17g", v)
  elseif t == "boolean" then
    return tostring(v)
  elseif t == "string" then
    return (string.format("%q", v):gsub("\\\n", "\\n"))
  end
  return "null"
end

local function deepcopy(v, seen)
  if type(v) ~= "table" or getmetatable(v) == Stub then return v end
  seen = seen or {}
  if seen[v] then return seen[v] end
  local c = {}
  seen[v] = c
  for k, x in pairs(v) do c[deepcopy(k, seen)] = deepcopy(x, seen) end
  return c
end

local function write(name, data)
  local f = assert(io.open(outdir .. "/" .. name, "w"))
  f:write(to_json(data, {}), "\n")
  f:close()
end

-- Definiciones en bruto (tal como las devuelve cada fichero)
local summary = {}
for kind, r in pairs(results) do
  local defs = {}
  local fields = {}
  for _, d in ipairs(r.defs) do
    defs[d.id] = d
    for k, x in pairs(d) do
      local ty = type(x)
      if ty == "table" and getmetatable(x) == Stub then ty = "texto(_S/_A)" end
      fields[k] = fields[k] or {count = 0, types = {}}
      fields[k].count = fields[k].count + 1
      fields[k].types[ty] = true
    end
  end
  local flist = {}
  for k, f in pairs(fields) do
    local tys = {}
    for ty in pairs(f.types) do tys[#tys + 1] = ty end
    table.sort(tys)
    flist[#flist + 1] = {field = k, count = f.count, types = table.concat(tys, "|")}
  end
  table.sort(flist, function(a, b)
    if a.count ~= b.count then return a.count > b.count end
    return a.field < b.field
  end)
  summary[kind] = {ok = #r.ok, failed = r.failed, fields = flist}
  write(kind .. ".json", defs)
end

-- Posproceso de CorsixTH (app.lua: slave_type y Object.processTypeDefinition)
local processed = {ok = 0, failed = {}}
if env.Object and env.Object.processTypeDefinition then
  local objs = deepcopy(results.objects.defs)
  local by_id = {}
  for _, v in ipairs(objs) do by_id[v.id] = v end
  for _, v in ipairs(objs) do
    if v.slave_id then
      v.slave_type = by_id[v.slave_id]
      v.slave_type.master_type = v
    end
    local ok, err = pcall(env.Object.processTypeDefinition, v)
    if ok then processed.ok = processed.ok + 1
    else processed.failed[#processed.failed + 1] = {id = v.id, error = tostring(err)} end
  end
  local out = {}
  for _, v in ipairs(objs) do
    -- slave_type/master_type son referencias cruzadas: se dejan como id
    local c = {}
    for k, x in pairs(v) do
      if k == "slave_type" or k == "master_type" then c[k] = x.id else c[k] = x end
    end
    out[v.id] = c
  end
  write("objects_procesados.json", out)
end
summary.objects_procesados = processed

write("markers.json", report.markers)
local by_file = {}
for _, m in ipairs(report.markers) do by_file[m.file] = (by_file[m.file] or 0) + 1 end
report.markers_by_file = by_file
report.markers = #report.markers
summary.stubs = report
write("resumen.json", summary)

for _, kind in ipairs({"objects", "rooms", "diseases", "diagnosis"}) do
  local s = summary[kind]
  print(string.format("%-10s ok=%d fallidos=%d campos=%d", kind, s.ok, #s.failed, #s.fields))
  for _, f in ipairs(s.failed) do print("   FALLO " .. f.file .. ": " .. f.error) end
end
print(string.format("objects procesados con Object.processTypeDefinition: %d (fallos %d)",
  processed.ok, #processed.failed))
for k, v in pairs(report.base_files) do print("base " .. k .. ": " .. v) end
print("marcadores de animación registrados: " .. report.markers)
