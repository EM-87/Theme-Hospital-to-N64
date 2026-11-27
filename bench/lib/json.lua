-- Un lector de JSON mínimo para el arnés: objetos, listas, cadenas (con los
-- escapes de JSON, \uXXXX solo en el plano básico), números, true, false y
-- null (nil). Lo usa la batería de construir para leer el mercado de
-- candidatos del núcleo (th64sim mercado, --th64-mercado en
-- bench/scenarios/construir.lua). Un documento mal formado es un error con
-- la posición.
local J = {}

local escapes = {['"'] = '"', ["\\"] = "\\", ["/"] = "/", b = "\b", f = "\f", n = "\n", r = "\r", t = "\t"}

function J.decode(texto)
  local pos = 1
  local function mal(msg)
    error(("JSON: %s en la posición %d"):format(msg, pos), 0)
  end
  local function blancos()
    pos = texto:find("[^ \t\r\n]", pos) or #texto + 1
  end
  local valor
  local function cadena()
    pos = pos + 1
    local partes = {}
    while true do
      local c = texto:sub(pos, pos)
      if c == "" then mal("cadena sin cerrar") end
      if c == '"' then
        pos = pos + 1
        return table.concat(partes)
      elseif c == "\\" then
        local e = texto:sub(pos + 1, pos + 1)
        if e == "u" then
          local h = texto:sub(pos + 2, pos + 5)
          if not h:match("^%x%x%x%x$") then mal("\\u mal escrito") end
          partes[#partes + 1] = utf8.char(tonumber(h, 16))
          pos = pos + 6
        else
          partes[#partes + 1] = escapes[e] or mal("escape desconocido")
          pos = pos + 2
        end
      else
        local fin = texto:find('["\\]', pos) or #texto + 1
        partes[#partes + 1] = texto:sub(pos, fin - 1)
        pos = fin
      end
    end
  end
  function valor()
    blancos()
    local c = texto:sub(pos, pos)
    if c == "{" then
      pos = pos + 1
      local t = {}
      blancos()
      if texto:sub(pos, pos) == "}" then
        pos = pos + 1
        return t
      end
      while true do
        blancos()
        if texto:sub(pos, pos) ~= '"' then mal("se esperaba una clave") end
        local k = cadena()
        blancos()
        if texto:sub(pos, pos) ~= ":" then mal("se esperaba «:»") end
        pos = pos + 1
        t[k] = valor()
        blancos()
        c = texto:sub(pos, pos)
        pos = pos + 1
        if c == "}" then return t end
        if c ~= "," then mal("se esperaba «,» o «}»") end
      end
    elseif c == "[" then
      pos = pos + 1
      local t = {}
      blancos()
      if texto:sub(pos, pos) == "]" then
        pos = pos + 1
        return t
      end
      while true do
        t[#t + 1] = valor()
        blancos()
        c = texto:sub(pos, pos)
        pos = pos + 1
        if c == "]" then return t end
        if c ~= "," then mal("se esperaba «,» o «]»") end
      end
    elseif c == '"' then
      return cadena()
    elseif texto:sub(pos, pos + 3) == "true" then
      pos = pos + 4
      return true
    elseif texto:sub(pos, pos + 4) == "false" then
      pos = pos + 5
      return false
    elseif texto:sub(pos, pos + 3) == "null" then
      pos = pos + 4
      return nil
    end
    local num = texto:match("^-?%d+%.?%d*[eE]?[-+]?%d*", pos)
    if not num or num == "" or num == "-" then mal("valor desconocido") end
    pos = pos + #num
    return math.tointeger(tonumber(num)) or tonumber(num)
  end
  local v = valor()
  blancos()
  if pos <= #texto then mal("sobra texto") end
  return v
end

--! Lee y descodifica un fichero JSON.
function J.leer(ruta)
  local f, err = io.open(ruta, "r")
  if not f then error(("no se puede leer %s: %s"):format(ruta, tostring(err)), 0) end
  local texto = f:read("a")
  f:close()
  return J.decode(texto)
end

return J
