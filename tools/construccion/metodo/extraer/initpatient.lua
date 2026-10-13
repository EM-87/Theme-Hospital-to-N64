-- Convierte disease.initPatient (función) en datos: ejecuta cada una N veces con
-- un paciente que registra setType/setLayer y cuenta las combinaciones.
-- Uso: lua5.4 initpatient.lua <dir Lua de CorsixTH> <N>
local base, N = arg[1], tonumber(arg[2] or "20000")
local Stub = {}
Stub.__index = function(t, k) return setmetatable({p = rawget(t, "p") .. "." .. k}, Stub) end
Stub.__concat = function(a, b) return tostring(a) .. tostring(b) end
Stub.__tostring = function(t) return rawget(t, "p") end
local env = setmetatable({_S = setmetatable({p = "_S"}, Stub),
  TheApp = {config = {}}, AnimationEffect = {Jelly = 2, Glowing = 1, None = 0}},
  {__index = _G})
math.randomseed(1)
local function lsdir(d)
  local p, r = io.popen('ls "' .. d .. '"'), {}
  for f in p:lines() do if f:match("%.lua$") then r[#r + 1] = f end end
  p:close()
  return r
end
local total_variants, total_layers, max_variants = 0, 0, 0
for _, f in ipairs(lsdir(base .. "/diseases")) do
  local d = assert(loadfile(base .. "/diseases/" .. f, "t", env))()
  local combos, ncombo = {}, 0
  local per_type = {}
  for _ = 1, N do
    local rec = {layers = {}}
    local patient = {}
    function patient:setType(t) rec.type = t end
    function patient:setLayer(l, v) rec.layers[l] = v end
    d.initPatient(patient)
    local key = {rec.type}
    for l = 0, 5 do key[#key + 1] = tostring(rec.layers[l]) end
    local k = table.concat(key, ",")
    if not combos[k] then combos[k] = true; ncombo = ncombo + 1 end
    local pt = per_type[rec.type] or {}
    per_type[rec.type] = pt
    for l, v in pairs(rec.layers) do
      pt[l] = pt[l] or {}
      pt[l][v] = true
    end
  end
  -- ¿Las capas son independientes dentro de cada tipo? Si el producto de
  -- opciones por capa coincide con las combinaciones vistas, una tabla
  -- "tipo -> opciones por capa" reproduce la función.
  local product = 0
  local desc = {}
  for t, layers in pairs(per_type) do
    local p = 1
    local parts = {}
    for l = 0, 5 do
      if layers[l] then
        local vals = {}
        for v in pairs(layers[l]) do vals[#vals + 1] = v end
        table.sort(vals)
        p = p * #vals
        parts[#parts + 1] = l .. ":{" .. table.concat(vals, ",") .. "}"
        total_layers = total_layers + 1
      end
    end
    product = product + p
    desc[#desc + 1] = t .. " " .. table.concat(parts, " ")
  end
  total_variants = total_variants + #desc
  if #desc > max_variants then max_variants = #desc end
  print(string.format("%-26s combinaciones=%4d producto=%4d %s | %s", d.id, ncombo, product,
    ncombo == product and "INDEP" or "DEPEND", table.concat(desc, " || ")))
end
print("variantes (tipo de paciente) en total:", total_variants, "máximo por enfermedad:", max_variants)
