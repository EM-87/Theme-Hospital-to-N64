-- Extractor de las reglas de CorsixTH para el conversor (fase 5, hito 0).
--
-- Carga con lua5.4 de serie, y unos stubs mínimos, las definiciones de
-- CorsixTH (objetos, salas, enfermedades y diagnósticos), las procesa como
-- hace el juego al arrancar (Object.processTypeDefinition) y calcula la
-- configuración de cada nivel de la campaña como Map:_loadOriginalCampaignLevel:
-- base_config.lua, después DIFF00.SAM, DIFFnn.SAM y, en algunos niveles, el
-- originalNN.level de CorsixTH. Lo escribe todo en un JSON canónico que
-- reglas.py pasa al formato binario TRG1.
--
-- Uso: lua5.4 reglas.lua <dir Lua de CorsixTH> <dir LEVELS del juego> <salida.json>
--
-- JSON canónico (json_canonico.lua): las claves enteras de Lua van como "#n",
-- las tablas con claves 1..n son listas, los reales llevan siempre punto o
-- exponente, las cadenas de idioma (_S, _A) van como
-- {"$ref": "_S.object.bench"} y las funciones como
-- {"$fn": "fichero.lua:primera-última"}.
--
-- Stubs (todo lo demás es Lua estándar o código de CorsixTH sin cambios):
--   * strict_declare_global / destrict: los usa class.lua;
--   * _S, _A: cadenas de idioma (devuelven una referencia);
--   * TheApp: animation_manager (registra los marcadores y las duraciones
--     fijadas a mano), config (vacía) y el resto, stub;
--   * corsixth.require y require: no cargan nada;
--   * permanent"x"(t): devuelve t y lo guarda por nombre (así se recogen las
--     animaciones de caminar y de las puertas de humanoid.lua).
-- El aspecto de cada paciente (disease.initPatient, que usa math.random) se
-- convierte en una distribución exacta recorriendo todas las ramas de sus
-- llamadas a math.random.

local base, levels_dir, out_path = arg[1], arg[2], arg[3]
assert(base and levels_dir and out_path, "uso: lua5.4 reglas.lua <dir Lua> <dir LEVELS> <salida.json>")

local report = {markers = {}, anim_lengths = {}, permanent = {}, stub_calls = {}}

-- Stub automático: cualquier acceso devuelve otro stub con la ruta.
local Stub = {}
local function stub(path)
  return setmetatable({__stub = path}, Stub)
end
Stub.__index = function(t, k)
  if k == "__stub" then return rawget(t, "__stub") end
  return stub(rawget(t, "__stub") .. "." .. tostring(k))
end
Stub.__call = function(t)
  local p = rawget(t, "__stub")
  report.stub_calls[p] = (report.stub_calls[p] or 0) + 1
  return stub(p .. "()")
end
Stub.__tostring = function(t) return rawget(t, "__stub") end
Stub.__concat = function(a, b) return tostring(a) .. tostring(b) end

local SAFE = {"assert", "error", "ipairs", "next", "pairs", "pcall", "print", "rawequal", "rawget", "rawlen",
  "rawset", "select", "setmetatable", "getmetatable", "tonumber", "tostring", "type", "xpcall", "math", "string",
  "table", "utf8"}
local env = {}
for _, k in ipairs(SAFE) do env[k] = _G[k] end
env._G = env
env.io = {open = io.open, lines = io.lines}
env.unpack = table.unpack
env.strict_declare_global = function() end
env.destrict = function(f) return f end
env._S = stub("_S")
env._A = stub("_A")
env.permanent = function(name)
  return function(v)
    report.permanent[name] = v
    return v
  end
end
env.corsixth = {require = function() end}
env.require = function(name) return stub(name) end
local anim_mgr = {}
-- Como AnimationManager:_unfoldAnims (graphics.lua): sin animación no se
-- hace nada y una tabla se recorre entera.
local function unfold(tipo, anim, ...)
  if not anim then return end
  if type(anim) == "table" then
    for _, v in pairs(anim) do unfold(tipo, v, ...) end
  else
    report.markers[#report.markers + 1] = {tipo = tipo, animacion = anim, args = {...}}
  end
end
function anim_mgr:setStaffMarker(anim, ...) unfold("staff", anim, ...) end
function anim_mgr:setPatientMarker(anim, ...) unfold("patient", anim, ...) end
function anim_mgr:setAnimLength(anim, len)
  report.anim_lengths[#report.anim_lengths + 1] = {animacion = anim, duracion = len}
end
function anim_mgr:getAnimLength(anim)
  return stub("getAnimLength(" .. tostring(anim) .. ")")
end

-- TheApp: lo que usan las definiciones y Map:loadMapConfig.
local corsixth_dir = base .. "/.."
local function th_file(dir, name)
  -- El sistema de ficheros de TH no distingue mayúsculas.
  local p = io.popen('ls "' .. dir .. '"')
  local found
  for f in p:lines() do
    if f:lower() == name:lower() then found = dir .. "/" .. f end
  end
  p:close()
  return found
end
local fs = {}
function fs:readContents(dir, name)
  assert(dir == "Levels", dir)
  local path = th_file(levels_dir, name)
  if not path then return nil end
  local f = assert(io.open(path, "rb"))
  local s = f:read("a")
  f:close()
  return s
end
-- La configuración por defecto de CorsixTH: el cuerpo de new_config_defaults()
-- de config_finder.lua (el fichero entero no se puede cargar: busca y escribe
-- el config.txt del usuario). La leen, por ejemplo, el aspecto de los huesos
-- rotos (disable_fractured_bones_females) y las subidas de sueldo.
local function config_defaults()
  local f = assert(io.open(base .. "/config_finder.lua"))
  local src = f:read("a")
  f:close()
  local body = assert(src:match("\nlocal function new_config_defaults%(%)\n(.-\n)end\n"),
    "no encuentro new_config_defaults en config_finder.lua")
  return assert(load(body, "new_config_defaults", "t", {}))()
end
local app = {animation_manager = anim_mgr, config = config_defaults(), fs = fs}
function app:getFullPath(parts)
  return corsixth_dir .. "/" .. table.concat(parts, "/")
end
env.TheApp = setmetatable(app, {__index = function(_, k) return stub("TheApp." .. k) end})
local faltan = {}
setmetatable(env, {__index = function(_, k)
  faltan[k] = (faltan[k] or 0) + 1
  return nil
end})

local function run(path)
  local chunk, err = loadfile(path, "t", env)
  if not chunk then return false, err end
  return pcall(chunk)
end

assert(run(base .. "/class.lua"))
for _, f in ipairs({"utility.lua", "entity.lua", "entities/humanoid.lua", "entities/object.lua",
    "entities/machine.lua", "map.lua", "room.lua"}) do
  local ok, err = run(base .. "/" .. f)
  assert(ok, f .. ": " .. tostring(err))
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

local defs = {objects = {}, rooms = {}, diseases = {}, diagnosis = {}}
local order = {objects = {}}
local function load_folder(kind, dir)
  for _, f in ipairs(list_lua(base .. "/" .. dir)) do
    local ok, val = run(base .. "/" .. dir .. "/" .. f)
    assert(ok and type(val) == "table" and val.id, dir .. "/" .. f .. ": " .. tostring(val))
    defs[kind][val.id] = val
    if order[kind] then order[kind][#order[kind] + 1] = val end
  end
end
load_folder("objects", "objects")
load_folder("objects", "objects/machines")
load_folder("objects", "objects/doors")
load_folder("rooms", "rooms")
load_folder("diseases", "diseases")
load_folder("diagnosis", "diagnosis")

-- Posproceso de app.lua: slave_type y Object.processTypeDefinition, en el
-- orden en que el juego carga los objetos.
for _, v in ipairs(order.objects) do
  if v.slave_id then
    v.slave_type = defs.objects[v.slave_id]
    v.slave_type.master_type = v
  end
  env.Object.processTypeDefinition(v)
end

-- Aspecto de los pacientes ----------------------------------------------------

local function gcd(a, b)
  while b ~= 0 do a, b = b, a % b end
  return a
end

--! Recorre todas las ramas de las llamadas a math.random de initPatient y
--! devuelve las variantes (tipo y capas) con un peso entero cada una.
local function aspecto(init)
  local BRANCH = {}
  local variants, keys = {}, {}
  local function explore(prefix, den)
    local depth, nueva = 0, nil
    local rec = {layers = {}}
    local patient = {}
    function patient:setType(t) rec.type = t end
    function patient:setLayer(l, v) rec.layers[l] = v end
    env.math = setmetatable({random = function(a, b)
      assert(a, "math.random() sin argumentos: no se puede enumerar")
      if b == nil then a, b = 1, a end
      depth = depth + 1
      if depth <= #prefix then return prefix[depth] end
      nueva = {a, b}
      error(BRANCH)
    end}, {__index = math})
    local ok, err = pcall(init, patient)
    env.math = math
    if ok then
      local parts = {rec.type}
      for l = 0, 12 do parts[#parts + 1] = tostring(rec.layers[l]) end
      local k = table.concat(parts, ",")
      if not keys[k] then
        keys[k] = {tipo = rec.type, capas = rec.layers, den = {}}
        variants[#variants + 1] = keys[k]
      end
      local d = keys[k].den
      d[#d + 1] = den
      return
    end
    assert(err == BRANCH, tostring(err))
    for v = nueva[1], nueva[2] do
      local p = {table.unpack(prefix)}
      p[#p + 1] = v
      explore(p, den * (nueva[2] - nueva[1] + 1))
    end
  end
  explore({}, 1)
  -- Pesos enteros: todo sobre el mínimo común múltiplo de los denominadores.
  local lcm = 1
  for _, v in ipairs(variants) do
    for _, d in ipairs(v.den) do lcm = lcm // gcd(lcm, d) * d end
  end
  local out, total = {}, 0
  for _, v in ipairs(variants) do
    local w = 0
    for _, d in ipairs(v.den) do w = w + lcm // d end
    total = total + w
    out[#out + 1] = {tipo = v.tipo, capas = v.capas, peso = w}
  end
  assert(total == lcm)
  return {variantes = out, total = total}
end

local aspectos = {}
for id, d in pairs(defs.diseases) do
  if d.initPatient then aspectos[id] = aspecto(d.initPatient) end
end

-- Configuración de los niveles --------------------------------------------------

local base_config = assert(loadfile(base .. "/base_config.lua", "t", env))()
local function deepcopy(v, seen)
  if type(v) ~= "table" or getmetatable(v) == Stub then return v end
  seen = seen or {}
  if seen[v] then return seen[v] end
  local c = {}
  seen[v] = c
  for k, x in pairs(v) do c[deepcopy(k, seen)] = deepcopy(x, seen) end
  return c
end
local niveles = {}
for _, dif in ipairs({"easy", "full", "hard"}) do
  niveles[dif] = {}
  for n = 1, 12 do
    local map = setmetatable({app = app}, {__index = env.Map})
    local err, cfg = env.Map._loadOriginalCampaignLevel(map, dif, n, deepcopy(base_config))
    assert(not err, tostring(err))
    niveles[dif][n] = cfg
  end
end

-- JSON canónico (json_canonico.lua, en el mismo directorio que este fichero).
local canonico = dofile((arg[0]:match("^(.*)[/\\]") or ".") .. "/json_canonico.lua")
local function ref(v)
  return getmetatable(v) == Stub and rawget(v, "__stub") or nil
end

-- Las referencias entre objetos (slave_type, master_type) van como id.
local objetos = {}
for id, v in pairs(defs.objects) do
  local c = {}
  for k, x in pairs(v) do
    if k == "slave_type" or k == "master_type" then c[k] = x.id else c[k] = x end
  end
  objetos[id] = c
end
local enfermedades = {}
for id, d in pairs(defs.diseases) do
  local c = {}
  for k, x in pairs(d) do c[k] = x end
  c.aspecto = aspectos[id]
  enfermedades[id] = c
end

local walk = report.permanent.humanoid_walk_animations
assert(walk and next(walk), "no se han recogido las animaciones de caminar")
local puertas = report.permanent.humanoid_door_animations
assert(puertas and next(puertas), "no se han recogido las animaciones de las puertas")
local doc = {
  version = 1,
  objetos = objetos,
  salas = defs.rooms,
  enfermedades = enfermedades,
  diagnosticos = defs.diagnosis,
  marcadores = report.markers,
  duraciones = report.anim_lengths,
  caminar = walk,
  puertas = puertas,
  config = app.config,
  base_config = base_config,
  niveles = niveles,
}
local f = assert(io.open(out_path, "w"))
f:write(canonico.escribir(doc, {ref = ref}), "\n")
f:close()

local n = {}
for k, t in pairs(defs) do
  local c = 0
  for _ in pairs(t) do c = c + 1 end
  n[#n + 1] = k .. " " .. c
end
table.sort(n)
local nf = {}
for k, c in pairs(faltan) do nf[#nf + 1] = k .. "=" .. c end
table.sort(nf)
print(("reglas: %s; %d marcadores; %d duraciones fijadas; %d enfermedades con aspecto; 36 niveles; " ..
  "globales sin definir: %s"):format(table.concat(n, ", "), #report.markers, #report.anim_lengths,
  (function() local c = 0 for _ in pairs(aspectos) do c = c + 1 end return c end)(),
  #nf > 0 and table.concat(nf, " ") or "ninguna"))
