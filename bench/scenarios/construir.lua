-- La batería de construir (hito 6.3, pasos 2 y 3): CorsixTH aplica una lista
-- de órdenes del jugador como su interfaz (bench/lib/ordenes.lua) y escribe,
-- orden a orden, lo que pasó. Después th64sim construir repite la misma lista
-- con el núcleo desde el mismo punto de partida y la compara
-- (tools/hito6/construir.sh).
--
-- Opciones:
--   --th64-ordenes=<lista.tsv>  la lista (ruta absoluta; formato en ordenes.lua)
--   --th64-nivel=N              el nivel, en dificultad full (por defecto 1)
--
-- El nivel se prepara así: sin objetivos, blocking_off_areas = 2 (el bloqueo de
-- zonas del original, la excepción 5 de docs/04_construccion.md), sin contenido
-- medio de las salas (enable_avg_contents), sin las ventanas de información de
-- las salas, en pausa con allow_user_actions_while_paused (nadie se mueve ni
-- llega), sin construcción libre y sin paredes transparentes (las capas no
-- llevan banderas de dibujo).
--
-- Escribe en el directorio de salida (coordenadas desde 0):
--   inicio/celdas.tsv, inicio/escenario.json  el punto de partida, en el formato
--       de exportar_escenario.lua (bench/lib/escenario.lua);
--   ordenes.tsv     n y la orden aplicada (el nombre y sus argumentos, con tabuladores);
--   resultados.tsv  n, orden, resultado (aceptada o rechazada), motivo (- si se
--       aceptó), saldo, valor (del hospital), money_in y money_out tras la orden,
--       y detalle: la fase en la que queda la obra (walls, door, windows,
--       objects), o - si no hay obra;
--   dinero.tsv      un movimiento por fila: n, importe (negativo lo que se paga),
--       motivo (buy_object:<id>, sell_object:<id>, build_room:<tipo de sala> o
--       sell_room:<tipo>) y valor (su cambio del valor del hospital);
--   cambios.tsv     tras cada orden, las casillas que cambian, con n delante y las
--       columnas de celdas.tsv (la capa 4, la de la interfaz, no va);
--   objetos.tsv     tras cada orden que cambia algo de los objetos, todos los de
--       world.entities: n, id, x, y, direccion, sala, recogido (picked_up) y orden
--       (su sitio en world.objects de su casilla, desde 0);
--   salas.tsv       tras cada orden que cambia algo de las salas, todas las de
--       world.rooms por id: n, id, tipo (room_info.id), x, y, ancho, alto,
--       puerta_x, puerta_y (la casilla del objeto puerta, la maestra en la
--       doble; - si la obra la ha quitado), puerta_lado (su direction, north o
--       west, el wall de blueprint_door), construida (built), activa
--       (is_active) y fase (la de la obra si es la suya, o -);
--   celdas.tsv, escenario.json  el estado final, como inicio/.
local th64 = th64
local escenario = dofile(th64.bench_dir .. "/lib/escenario.lua")
local ordenes = dofile(th64.bench_dir .. "/lib/ordenes.lua")

local ruta_ordenes = assert(th64.opts.ordenes, "falta --th64-ordenes=<lista.tsv>")
-- CorsixTH corre desde $CORSIXTH_DIR/CorsixTH (tools/bench_run.sh).
assert(ruta_ordenes:sub(1, 1) == "/", "--th64-ordenes tiene que ser una ruta absoluta")
local nivel = math.tointeger(tonumber(th64.opts.nivel or "1"))
assert(nivel, "--th64-nivel=N")

-- El nivel ------------------------------------------------------------------------
-- Semilla fija antes de cargar: el mundo sortea al crearse los días del mes en
-- que llegan pacientes (World:updateSpawnDates, world.lua:156 y 1140-1172), que
-- van en escenario.json. Con la de CorsixTH (la hora, app.lua:215), dos
-- ejecuciones no darían el mismo inicio/escenario.json.
math.randomseed(1)
assert(TheApp:loadLevel(nivel, "full"), "no se pudo cargar el nivel " .. nivel)
th64.wait_ticks(3)
local world, ui = TheApp.world, TheApp.ui
local h = world.hospitals[1]
local th = world.map.th
world.endconditions.win_goals = {}
world.endconditions.lose_goals = {}
TheApp.config.allow_user_actions_while_paused = true
TheApp.config.blocking_off_areas = 2
TheApp.config.enable_avg_contents = false
-- El mundo copia la opción al crearse (world.lua:75): no vale cambiar TheApp.config.
world.room_information_dialogs = false
world:setSpeed("Pause")
world:updateUserActionsAllowed()
assert(world.user_actions_allowed, "acciones del jugador no permitidas")
assert(not world.free_build_mode, "construcción libre")
assert(not ui.transparent_walls, "paredes transparentes")
local W, H = th:size()

local function sin_gente()
  for _, e in ipairs(world.entities) do
    if class.is(e, Humanoid) then error("hay personas en el mapa: la batería es sin gente", 0) end
  end
end
sin_gente()

-- Los objetos obligatorios de cada sala (room_info.objects_needed) se recorren
-- con pairs al cobrarlos (UIEditRoom:enterObjectsPhase, edit_room.lua:993-997),
-- y el orden de pairs sobre claves de texto cambia de una ejecución a otra
-- (Lua 5.4 siembra el hash al azar): el orden de los apuntes y de la lista de
-- la obra no sería repetible. Con __pairs por orden alfabético (utility.lua
-- lo respeta) el conjunto es el mismo y el orden, fijo.
local function pairs_ordenado(t)
  local claves = {}
  for k in next, t do claves[#claves + 1] = k end
  table.sort(claves)
  local i = 0
  return function()
    i = i + 1
    local k = claves[i]
    if k ~= nil then return k, t[k] end
  end
end
for _, info in ipairs(TheApp.rooms) do
  if info.objects_needed and not getmetatable(info.objects_needed) then
    setmetatable(info.objects_needed, {__pairs = pairs_ordenado})
  end
end

local lista = ordenes.leer(ruta_ordenes, W, H)

-- Números: enteros. Un importe con decimales sería un fallo de la batería (en
-- CorsixTH solo divide vender una sala, edit_room.lua:145-156).
local function entero(v)
  if math.type(v) == "integer" then return tostring(v) end
  assert(v == math.floor(v), "importe con decimales: " .. tostring(v))
  return ("%d"):format(v)
end

-- El punto de partida ------------------------------------------------------------
os.execute('mkdir -p "' .. th64.out("inicio") .. '"')
th64.log("inicio: %s", escenario.exportar(world, th64.out("inicio/celdas.tsv"), th64.out("inicio/escenario.json")))

-- El dinero: Hospital:spendMoney y receiveMoney (hospital.lua:1207-1234). El
-- motivo que dan es el texto traducido; el id del objeto sale del último
-- getObjectBuildCost, que se llama justo antes en los tres sitios que mueven
-- dinero por objetos (furnish_corridor.lua:182-186, place_objects.lua:186-188,
-- 222-224 y 256-258), y se comprueba con el texto. La sala se cobra con su
-- nombre traducido (UIEditRoom:confirm, edit_room.lua:243): se busca el tipo
-- de sala con ese nombre. Vender una sala pagada (abortRoom, 141-157) apunta
-- sell_object a secas: la sala es la de la obra que aborta.
local movimientos = {}
local ultimo_id, sala_abortada
local function envolver(nombre, fn)
  local orig = assert(Hospital[nombre], nombre)
  Hospital[nombre] = function(self, ...)
    local r = table.pack(orig(self, ...))
    fn(self, ...)
    return table.unpack(r, 1, r.n)
  end
end
envolver("getObjectBuildCost", function(_, id) ultimo_id = id end)
local abortRoom = UIEditRoom.abortRoom
UIEditRoom.abortRoom = function(self, ...)
  sala_abortada = self.room and self.room.room_info
  return abortRoom(self, ...)
end
local function sala_con_nombre(prefijo, razon)
  local tipo
  for _, info in ipairs(TheApp.rooms) do
    if tostring(prefijo .. ": " .. info.name) == razon then
      assert(not tipo, "dos tipos de sala con el mismo nombre: " .. razon)
      tipo = info.id
    end
  end
  return tipo
end
local function motivo(tipo, razon)
  razon = tostring(razon)
  local obj = ultimo_id and TheApp.objects[ultimo_id]
  if obj and tostring(_S.transactions[tipo] .. ": " .. obj.name) == razon then return tipo .. ":" .. ultimo_id end
  if tipo == "buy_object" then
    local sala = sala_con_nombre(_S.transactions.build_room, razon)
    if sala then return "build_room:" .. sala end
  elseif razon == tostring(_S.transactions.sell_object) and sala_abortada then
    return "sell_room:" .. sala_abortada.id
  end
  error(("movimiento de dinero sin clasificar: %s (último objeto: %s)"):format(razon, tostring(ultimo_id)), 0)
end
envolver("spendMoney", function(self, cantidad, razon, valor)
  if self ~= h then return end
  movimientos[#movimientos + 1] = {-cantidad, motivo("buy_object", razon), valor or 0}
end)
envolver("receiveMoney", function(self, cantidad, razon, valor)
  if self ~= h then return end
  movimientos[#movimientos + 1] = {cantidad, motivo("sell_object", razon), -(valor or 0)}
end)

-- Fotos del mapa, de los objetos y de las salas -----------------------------------
local flags = {}
local function foto_celdas()
  local s, i = {}, 0
  for y = 1, H do
    for x = 1, W do
      i = i + 1
      s[i] = escenario.fila_celda(th, x, y, flags)
    end
  end
  return s
end

local function foto_objetos()
  local filas = {}
  for _, e in ipairs(world.entities) do
    if class.is(e, Object) then
      assert(e.tile_x, "objeto sin casilla")
      local orden = escenario.orden_en_casilla(world, e)
      filas[#filas + 1] = table.concat({e.object_type.id, e.tile_x - 1, e.tile_y - 1, e.direction,
        escenario.sala_de(e), e.picked_up and 1 or 0, orden or ""}, "\t")
    end
  end
  return filas, table.concat(filas, "\n")
end

local function foto_salas()
  local ids = {}
  for id, r in pairs(world.rooms) do
    if r then ids[#ids + 1] = id end
  end
  table.sort(ids)
  local win = ui:getWindow(UIEditRoom)
  local filas = {}
  for _, id in ipairs(ids) do
    local r = world.rooms[id]
    local d = r.door
    filas[#filas + 1] = table.concat({id, r.room_info.id, r.x - 1, r.y - 1, r.width, r.height,
      d and d.tile_x and d.tile_x - 1 or "-", d and d.tile_y and d.tile_y - 1 or "-", d and d.direction or "-",
      r.built and 1 or 0, r.is_active and 1 or 0, win and win.room == r and win.phase or "-"}, "\t")
  end
  return filas, table.concat(filas, "\n")
end

local function abrir(nombre, cabecera)
  local f = assert(io.open(th64.out(nombre), "w"))
  f:write(cabecera, "\n")
  return f
end
local f_ordenes = abrir("ordenes.tsv", "n\torden")
local f_resultados = abrir("resultados.tsv", "n\torden\tresultado\tmotivo\tsaldo\tvalor\tmoney_in\tmoney_out\tdetalle")
local f_dinero = abrir("dinero.tsv", "n\timporte\tmotivo\tvalor")
local f_cambios = abrir("cambios.tsv", "n\t" .. escenario.cabecera_celdas)
local f_objetos = abrir("objetos.tsv", "n\tid\tx\ty\tdireccion\tsala\trecogido\torden")
local f_salas = abrir("salas.tsv", "n\tid\ttipo\tx\ty\tancho\talto\tpuerta_x\tpuerta_y\tpuerta_lado\tconstruida\tactiva\tfase")

-- Las órdenes -----------------------------------------------------------------------
local interprete = ordenes.nuevo(TheApp)
local celdas = foto_celdas()
local _, objetos_antes = foto_objetos()
local _, salas_antes = foto_salas()
local cuenta = {aceptada = 0, rechazada = 0}
local por_motivo = {}
for n, orden in ipairs(lista) do
  movimientos = {}
  local ok, por_que = interprete.aplicar(orden)
  sin_gente()
  local resultado = ok and "aceptada" or "rechazada"
  local mot = ok and "-" or assert(por_que, "rechazo sin motivo")
  cuenta[resultado] = cuenta[resultado] + 1
  local clase = ok and "aceptada" or mot:gsub(":.*", "")
  por_motivo[clase] = (por_motivo[clase] or 0) + 1
  local obra = interprete.obra()
  local detalle = obra and obra.phase or "-"

  f_ordenes:write(n, "\t", ordenes.campos(orden), "\n")
  f_resultados:write(table.concat({n, ordenes.texto(orden), resultado, mot, entero(h.balance), entero(h.value),
    entero(h.money_in), entero(h.money_out), detalle}, "\t"), "\n")
  local suma = 0
  for _, m in ipairs(movimientos) do
    f_dinero:write(table.concat({n, entero(m[1]), m[2], entero(m[3])}, "\t"), "\n")
    suma = suma + m[1]
  end

  local ahora = foto_celdas()
  local cambiadas = 0
  for i = 1, W * H do
    if ahora[i] ~= celdas[i] then
      cambiadas = cambiadas + 1
      f_cambios:write(n, "\t", ahora[i], "\n")
    end
  end
  celdas = ahora

  local filas, objetos = foto_objetos()
  local objetos_cambian = objetos ~= objetos_antes
  if objetos_cambian then
    for _, fila in ipairs(filas) do f_objetos:write(n, "\t", fila, "\n") end
    objetos_antes = objetos
  end
  local filas_salas, salas = foto_salas()
  local salas_cambian = salas ~= salas_antes
  if salas_cambian then
    for _, fila in ipairs(filas_salas) do f_salas:write(n, "\t", fila, "\n") end
    salas_antes = salas
  end

  th64.log("%d %s: %s %s; saldo %s (%s%s en %d movimientos); %d casillas%s%s; obra %s", n, ordenes.texto(orden),
    resultado, ok and "" or mot, entero(h.balance), suma >= 0 and "+" or "", entero(suma), #movimientos, cambiadas,
    objetos_cambian and "; objetos" or "", salas_cambian and "; salas" or "", detalle)
end
for _, f in ipairs({f_ordenes, f_resultados, f_dinero, f_cambios, f_objetos, f_salas}) do f:close() end

-- El estado final ------------------------------------------------------------------
assert(not interprete.obra(), "la lista termina con una obra abierta")
th64.log("final: %s", escenario.exportar(world, th64.out("celdas.tsv"), th64.out("escenario.json")))
local motivos = {}
for k, v in pairs(por_motivo) do motivos[#motivos + 1] = ("%s %d"):format(k, v) end
table.sort(motivos)
th64.log("%d órdenes de %s: %d aceptadas y %d rechazadas (%s); saldo %s, valor %s, money_in %s, money_out %s",
  #lista, ruta_ordenes, cuenta.aceptada, cuenta.rechazada, table.concat(motivos, ", "), entero(h.balance),
  entero(h.value), entero(h.money_in), entero(h.money_out))
th64.quit(0)
