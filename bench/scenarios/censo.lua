-- Censo aproximado del heap de Lua de una partida cargada (--load=...) o del
-- menú principal (sin --load). Recorre el grafo de objetos desde varias raíces
-- en orden y asigna cada objeto a la primera raíz que lo alcanza:
--   1. mundo (TheApp.world: estado de la simulación)
--   2. interfaz (TheApp.ui)
--   3. textos (_S)
--   4. resto (_G y el registro: clases, configuración, datos de objetos...)
-- Los tamaños son estimaciones con el modelo de memoria de Lua 5.4 en 64 bits
-- (tabla 56 B + 16 B por hueco de array + 24 B por nodo hash; cadena 24 B +
-- longitud + 1; clausura Lua 32 B + 8 B por upvalue). Los prototipos de las
-- funciones (código) no se pueden recorrer desde Lua; se miden aparte con
-- bench/host/codigo_lua.lua. La diferencia con el heap vivo medido se reporta.
local th64 = th64
th64.wait_ticks(3)

local function live() collectgarbage("collect") collectgarbage("collect") return collectgarbage("count") end
local live_kb = live()

local seen = setmetatable({}, {__mode = "k"})
local seen_str = {}
local function pow2(n) local p = 1 while p < n do p = p * 2 end return p end

local function new_acc() return {tables = 0, table_bytes = 0, strings = 0, string_bytes = 0,
  closures = 0, closure_bytes = 0, userdata = 0, threads = 0, protos = {}} end

-- Objetos que no se recorren desde el mundo o la IU porque son infraestructura
-- compartida (se asignan después a "resto").
local stop = {}
local function mark_stop(t) if t ~= nil then stop[t] = true end end

local function walk(root, acc)
  local stack = {root}
  while #stack > 0 do
    local v = table.remove(stack)
    local tv = type(v)
    if tv == "string" then
      if not seen_str[v] then
        seen_str[v] = true
        acc.strings = acc.strings + 1
        acc.string_bytes = acc.string_bytes + 24 + #v + 1
      end
    elseif (tv == "table" or tv == "function" or tv == "userdata" or tv == "thread") and not seen[v] and not stop[v] then
      seen[v] = true
      if tv == "table" then
        local narr = 0
        while rawget(v, narr + 1) ~= nil do narr = narr + 1 end
        local nhash = 0
        for k, x in next, v do
          if not (math.type(k) == "integer" and k >= 1 and k <= narr) then nhash = nhash + 1 end
          stack[#stack + 1] = k
          stack[#stack + 1] = x
        end
        acc.tables = acc.tables + 1
        acc.table_bytes = acc.table_bytes + 56 + 16 * (narr > 0 and pow2(narr) or 0) +
          24 * (nhash > 0 and pow2(nhash) or 0)
        local mt = getmetatable(v)
        if type(mt) == "table" then stack[#stack + 1] = mt end
      elseif tv == "function" then
        local info = debug.getinfo(v, "Su")
        if info.what ~= "C" then
          acc.closures = acc.closures + 1
          acc.closure_bytes = acc.closure_bytes + 32 + 8 * info.nups
          acc.protos[info.source .. ":" .. info.linedefined] = true
        end
        for i = 1, info.nups do
          local _, uv = debug.getupvalue(v, i)
          stack[#stack + 1] = uv
        end
      elseif tv == "userdata" then
        acc.userdata = acc.userdata + 1
        local mt = getmetatable(v)
        if type(mt) == "table" then stack[#stack + 1] = mt end
        local uv = debug.getuservalue and debug.getuservalue(v)
        if uv ~= nil then stack[#stack + 1] = uv end
      else
        acc.threads = acc.threads + 1
      end
    end
  end
end

local roots = {}
local app = TheApp
-- Infraestructura que no es estado de la simulación ni de la IU.
mark_stop(app) mark_stop(app.gfx) mark_stop(app.anims) mark_stop(app.audio)
mark_stop(app.objects) mark_stop(app.rooms) mark_stop(app.config) mark_stop(_S) mark_stop(_G)
mark_stop(app.video) mark_stop(app.strings) mark_stop(app.fs) mark_stop(app.map)
mark_stop(app.ui)
for _, t in pairs(app.objects or {}) do mark_stop(t) end
for _, t in pairs(app.rooms or {}) do mark_stop(t) end
for k, v in pairs(_G) do if type(v) == "table" and rawget(v, "__index") then mark_stop(v) end end
if app.world and app.world.map then mark_stop(app.world.map.level_config) end
-- _S es un proxy: los textos cuelgan de su metatabla.
local smt = getmetatable(_S)

local order = {}
if app.world then order[#order + 1] = {"mundo", app.world} end
order[#order + 1] = {"interfaz", app.ui}
if app.map then order[#order + 1] = {"mapa (Lua)", app.map} end
order[#order + 1] = {"textos (_S)", smt or _S}

for _, r in ipairs(order) do
  local acc = new_acc()
  stop[r[2]] = nil
  walk(r[2], acc)
  roots[#roots + 1] = {r[1], acc}
end
-- Resto: se quitan las barreras y se recorre todo lo demás.
stop = {}
local acc = new_acc()
walk(_G, acc)
walk(debug.getregistry(), acc)
roots[#roots + 1] = {"resto (clases, configuración, datos)", acc}

-- Desglose del mundo por campo de primer nivel (con las mismas barreras).
local por_campo = {}
if app.world then
  seen = setmetatable({}, {__mode = "k"})
  seen_str = {}
  stop = {}
  mark_stop(app) mark_stop(app.ui) mark_stop(app.map) mark_stop(_S) mark_stop(_G)
  for _, t in pairs(app.objects or {}) do mark_stop(t) end
  for _, t in pairs(app.rooms or {}) do mark_stop(t) end
  for _, v in pairs(_G) do if type(v) == "table" and rawget(v, "__index") then mark_stop(v) end end
  local keys = {}
  for k in pairs(app.world) do keys[#keys + 1] = k end
  table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)
  local base_stop = {}
  for t in pairs(stop) do base_stop[t] = true end
  for _, k in ipairs(keys) do
    -- Cada campo se mide sin atravesar el propio mundo ni los demás campos.
    seen = setmetatable({}, {__mode = "k"})
    seen_str = {}
    stop = {[app.world] = true}
    for t in pairs(base_stop) do stop[t] = true end
    for _, j in ipairs(keys) do
      local v = app.world[j]
      if j ~= k and (type(v) == "table" or type(v) == "userdata") then stop[v] = true end
    end
    local a = new_acc()
    walk(app.world[k], a)
    local kb = (a.table_bytes + a.string_bytes + a.closure_bytes) / 1024
    if kb >= 20 then por_campo[#por_campo + 1] = {campo = tostring(k), kb = kb, tablas = a.tables} end
  end
  table.sort(por_campo, function(a, b) return a.kb > b.kb end)
  for i = 1, math.min(12, #por_campo) do
    th64.log("  world.%-28s %7.0f KB (%d tablas)", por_campo[i].campo, por_campo[i].kb, por_campo[i].tablas)
  end
end

local out = {live_kb = live_kb, lua = _VERSION, save = th64.opts.save or "menu", raices = {},
  mundo_por_campo = por_campo}
local est_total = 0
for _, r in ipairs(roots) do
  local a = r[2]
  local nprotos = 0
  for _ in pairs(a.protos) do nprotos = nprotos + 1 end
  local kb = (a.table_bytes + a.string_bytes + a.closure_bytes) / 1024
  est_total = est_total + kb
  out.raices[#out.raices + 1] = {nombre = r[1], estimado_kb = kb, tablas = a.tables,
    tablas_kb = a.table_bytes / 1024, cadenas = a.strings, cadenas_kb = a.string_bytes / 1024,
    clausuras = a.closures, clausuras_kb = a.closure_bytes / 1024, prototipos_distintos = nprotos,
    userdata = a.userdata, hilos = a.threads}
  th64.log("%-40s %8.0f KB  (%d tablas, %d cadenas, %d clausuras)", r[1], kb, a.tables, a.strings, a.closures)
end
out.estimado_total_kb = est_total
out.sin_asignar_kb = live_kb - est_total
th64.log("heap vivo %.0f KB; estimado %.0f KB; sin asignar (código, upvalues, userdata) %.0f KB",
  live_kb, est_total, live_kb - est_total)
th64.write_json("censo.json", out)
th64.quit(0)
