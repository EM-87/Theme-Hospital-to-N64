-- Extractor de los textos de CorsixTH para el conversor (fase 5, hito 0).
--
-- Ejecuta con lua5.4 de serie el strings.lua real de CorsixTH (Strings:init y
-- Strings:load) para cada idioma pedido, y escribe la tabla _S resultante en
-- el JSON canónico de json_canonico.lua. Los ficheros de idioma de CorsixTH
-- heredan unos de otros y de las cadenas originales del juego
-- (languages/original_strings.lua lee LANG-n.DAT).
--
-- Uso: lua5.4 textos.lua <dir Lua de CorsixTH> <dir DATA del juego> <salida.json> <idioma>...
--
-- Lo que en el juego es C++ se hace aquí en Lua:
--   * TH.LoadStrings (th_string_list en th.cpp): secciones de cadenas
--     terminadas en 0, pasadas de CP437 a UTF-8 con la tabla de CorsixTH, que
--     se lee de Src/cp437_table.h (no es CP437 puro: 0x92 pasa a «?»);
--   * lfs.dir, App:getFullPath y App:readDataFile.
-- Solo se admite la codificación CP437 (la de las versiones inglesa y
-- española); las demás se rechazan.

local base, data_dir, out_path = arg[1], arg[2], arg[3]
local idiomas = {table.unpack(arg, 4)}
assert(base and data_dir and out_path and #idiomas > 0,
  "uso: lua5.4 textos.lua <dir Lua> <dir DATA> <salida.json> <idioma>...")

local function leer(path)
  local f = assert(io.open(path, "rb"))
  local s = f:read("a")
  f:close()
  return s
end

local function listar(dir)
  local p = io.popen('ls "' .. dir .. '"')
  local out = {}
  for f in p:lines() do out[#out + 1] = f end
  p:close()
  return out
end

-- Tabla de CorsixTH de 0x80–0xFF a Unicode.
local tabla = {}
do
  local src = leer(base .. "/../Src/cp437_table.h")
  local cuerpo = assert(src:match("cp437_to_unicode_table%s*{(.-)}"), "no encuentro cp437_to_unicode_table")
  cuerpo = cuerpo:gsub("/%*.-%*/", ""):gsub("//[^\n]*", "")
  for hex in cuerpo:gmatch("0x(%x+)") do tabla[#tabla + 1] = tonumber(hex, 16) end
  assert(#tabla == 128, "cp437_to_unicode_table tiene " .. #tabla .. " entradas")
end

--! th_string_list (th.cpp): u16 secciones, u16 cadenas por sección, y las
--! cadenas terminadas en 0, de CP437 a UTF-8.
local function load_strings(datos)
  assert(#datos >= 2, "fichero de cadenas demasiado corto")
  local secciones = string.unpack("<I2", datos, 1)
  local pos = 3 + 2 * secciones
  assert(#datos >= pos - 1, "fichero de cadenas sin cabecera completa")
  -- detect_encoding: el 10 % de bytes 0xB0–0xDF delata GB2312 (chino).
  local cajas = 0
  for i = pos, #datos do
    local c = datos:byte(i)
    if c >= 0xB0 and c <= 0xDF then cajas = cajas + 1 end
  end
  assert(cajas * 10 < #datos - pos + 1, "codificación distinta de CP437")
  assert(datos:sub(pos, pos + 4) ~= "\x8C\xA5\xA4\xB1\xA5", "codificación MIK (ruso)")
  local out = {}
  for s = 1, secciones do
    local n = string.unpack("<I2", datos, 1 + 2 * s)
    local sec = {}
    for i = 1, n do
      local partes = {}
      if pos <= #datos then
        while true do
          local c = datos:byte(pos)
          pos = pos + 1
          if c == nil or c == 0 then break end
          partes[#partes + 1] = c < 0x80 and string.char(c) or utf8.char(tabla[c - 0x7F])
        end
      end
      sec[i] = table.concat(partes)
    end
    out[s] = sec
  end
  return out
end

local function th_file(dir, name)
  for _, f in ipairs(listar(dir)) do
    if f:lower() == name:lower() then return dir .. "/" .. f end
  end
  error("no encuentro " .. name .. " en " .. dir)
end

-- Entorno como el del juego: _G con lo estándar, class.lua y utility.lua.
local env = setmetatable({}, {__index = _G})
env._G = env
env.strict_declare_global = function() end
env.destrict = function(f) return f end
env.TheApp = {good_install_folder = true}
local TH = {LoadStrings = function(datos) return load_strings(datos) end}
local lfs = {dir = function(path)
  local i, lista = 0, listar(path)
  return function() i = i + 1 return lista[i] end
end}
env.require = function(name)
  if name == "lfs" then return lfs end
  if name == "TH" then return TH end
  error("require inesperado: " .. name)
end
local function run(path)
  return assert(loadfile(path, "t", env))()
end
run(base .. "/class.lua")
run(base .. "/utility.lua")
run(base .. "/strings.lua")

local app = {}
function app:getFullPath(folders, trailing)
  if type(folders) ~= "table" then folders = {folders} end
  return base .. "/../" .. table.concat(folders, "/") .. (trailing and "/" or "")
end
function app:readDataFile(dir, filename)
  if filename == nil then dir, filename = "Data", dir end
  assert(dir == "Data", dir)
  local datos = leer(th_file(data_dir, filename))
  assert(datos:sub(1, 3) ~= "RNC", "fichero comprimido con RNC: " .. filename)
  return datos
end

local strings = env.Strings(app)
strings:init()
local doc = {}
for _, idioma in ipairs(idiomas) do
  local t = strings:load(idioma)
  doc[idioma] = t
end

local canonico = dofile((arg[0]:match("^(.*)[/\\]") or ".") .. "/json_canonico.lua")
local f = assert(io.open(out_path, "w"))
f:write(canonico.escribir(doc), "\n")
f:close()
print(("textos: %s"):format(table.concat(idiomas, ", ")))
