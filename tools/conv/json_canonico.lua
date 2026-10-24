-- JSON canónico de tablas de Lua, el que leen tools/conv/reglas.py y
-- th64sim reglas. Lo usan el extractor de reglas (tools/conv/reglas.lua) y
-- su oráculo dentro de CorsixTH (bench/scenarios/volcar_reglas.lua).
--   * Las claves enteras van como "#n" (en Lua, 1 y "1" son claves
--     distintas); una tabla cuyas claves son exactamente 1..n es una lista.
--   * Los reales llevan siempre punto o exponente (1.0 no es 1).
--   * Las funciones van como {"$fn": "ruta/desde/Lua.lua:primera-última"}.
--   * opts.ref(v): si devuelve una cadena, la tabla v se escribe como
--     {"$ref": cadena} (las cadenas de idioma del extractor).
--   * opts.cambiar(k, v): si existe, el valor de la clave k se escribe como
--     lo que devuelva (por ejemplo, una referencia a otra definición por su id).
local M = {}

local function esc(s)
  return '"' .. s:gsub('[%c"\\]', function(c)
    if c == '"' then return '\\"' elseif c == "\\" then return "\\\\"
    elseif c == "\n" then return "\\n" elseif c == "\t" then return "\\t" end
    return string.format("\\u%04x", c:byte())
  end) .. '"'
end

local function is_array(t)
  local n = 0
  for k in pairs(t) do
    if math.type(k) ~= "integer" or k < 1 then return false end
    n = n + 1
  end
  if n == 0 then return false end
  for i = 1, n do
    if t[i] == nil then return false end
  end
  return true
end

local function fuente(f)
  local info = debug.getinfo(f, "S")
  local ruta = info.source:match("[/\\]Lua[/\\](.*)$") or info.source
  return string.format('{"$fn": "%s:%d-%d"}', ruta:gsub("\\", "/"), info.linedefined, info.lastlinedefined)
end

local function json(v, opts, stack)
  local t = type(v)
  if t == "nil" then return "null"
  elseif t == "boolean" then return tostring(v)
  elseif t == "number" then
    if math.type(v) == "integer" then return string.format("%d", v) end
    assert(v == v and v ~= math.huge and v ~= -math.huge, "real no representable")
    local s = string.format("%.17g", v)
    if not s:find("[.eE]") then s = s .. ".0" end
    return s
  elseif t == "string" then return esc(v)
  elseif t == "function" then return fuente(v)
  elseif t ~= "table" then return string.format('{"$%s": true}', t)
  end
  local ref = opts.ref and opts.ref(v)
  if ref then return string.format('{"$ref": %s}', esc(ref)) end
  assert(not stack[v], "ciclo")
  stack[v] = true
  local parts = {}
  if is_array(v) then
    for _, x in ipairs(v) do parts[#parts + 1] = json(x, opts, stack) end
    stack[v] = nil
    return "[" .. table.concat(parts, ",") .. "]"
  end
  for k, x in pairs(v) do
    local key
    if math.type(k) == "integer" then key = "#" .. k
    else
      assert(type(k) == "string", "clave de tipo " .. type(k))
      assert(not k:find("^[#$]"), "clave reservada " .. k)
      key = k
    end
    if opts.cambiar then x = opts.cambiar(k, x) end
    parts[#parts + 1] = esc(key) .. ":" .. json(x, opts, stack)
  end
  stack[v] = nil
  table.sort(parts)
  return "{" .. table.concat(parts, ",") .. "}"
end

function M.escribir(v, opts)
  return json(v, opts or {}, {})
end

return M
