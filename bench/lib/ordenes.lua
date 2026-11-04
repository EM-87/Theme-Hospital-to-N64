-- Intérprete de órdenes de construir (hito 6.3, paso 2: amueblar el pasillo;
-- paso 3: las salas rectangulares).
--
-- Cada orden es un paso de la interfaz de CorsixTH v0.70.1, y se aplica
-- llamando a lo mismo que llaman sus manejadores de eventos, con las mismas
-- condiciones previas: no hace falta mover el puntero por la pantalla, que
-- depende de la cámara. Es el oráculo de las órdenes del núcleo
-- (motor/core/include/th/build.h), que tiene que dar lo mismo orden a orden:
-- aceptada o rechazada, el motivo y lo que cambia (dinero, casillas, objetos
-- y salas).
--
-- Vocabulario (coordenadas desde 0, como el motor; orientaciones y lados
-- north, east, south y west):
--   comprar <objeto> <cantidad>        amueblar el pasillo: elegir y confirmar
--   colocar <objeto> <x> <y> <orient.> soltar un objeto de la ventana de colocar
--                                      (la del pasillo o la de una obra)
--   cerrar                             cerrar la ventana de colocar; en una obra,
--                                      lo que hace abrir otra ventana desde el
--                                      panel: termina la sala si se puede y si
--                                      no la abandona (verifyOrAbortRoom)
--   recoger <x> <y>                    clic derecho en el objeto con ese origen;
--                                      en una obra, el botón de coger y clic
--   vender                             tecla de vender el objeto cogido
--   deshacer                           Escape: el objeto cogido vuelve a su sitio
--   sala <tipo> <x> <y> <ancho> <alto> elegir la sala en la ventana de construir,
--                                      arrastrar el plano y confirmarlo
--   puerta <x> <y> <lado>              la puerta en la casilla de dentro (x, y)
--                                      del borde, en ese lado; y confirmar
--   ventana <x> <y> <lado>             una ventana, igual
--   quitar_ventana <x> <y> <lado>      clic derecho sobre una ventana del plano
--   fin_ventanas                       el botón ✓ en la fase de ventanas:
--                                      despejar el área y levantar la sala
--   comprar_sala <objeto> <cantidad>   el botón de comprar de la obra: cantidad
--                                      pulsaciones de «+» y confirmar (cantidad ≥ 1)
--   fin_sala                           el botón ✓ en la fase de objetos: se cobra
--   atras                              el botón ✗ de la obra (UIEditRoom:cancel):
--                                      un paso atrás; desde el plano, abandonar
--
-- Motivos de rechazo:
--   pausa             world.user_actions_allowed es falso: comprar, colocar,
--                     recoger, sala, puerta, ventana y quitar_ventana (los
--                     botones y las teclas de las ventanas no lo miran: cerrar,
--                     vender, deshacer, fin_ventanas, comprar_sala, fin_sala y
--                     atras se pueden en pausa)
--   no_es_de_pasillo  el objeto no está en la ventana de amueblar el pasillo
--   sin_dinero        el saldo no llega al total (comprar, comprar_sala) o al
--                     precio de la sala (sala)
--   maximo            ya hay 99 en la ventana de amueblar
--   sin_ventana       no hay ventana de colocar (ni obra): cerrar, vender,
--                     deshacer, colocar; no hay obra: atras; y quitar_ventana
--                     donde no hay ventana
--   sin_objeto        no está ese objeto en la ventana; o no hay objeto con ese origen
--   orientacion       el objeto no tiene esa orientación
--   casillas:<n>      n casillas malas: de la huella (las de fuera del mapa
--                     también; las invisibles, como el uso del banco, no) o
--                     del plano de la sala (no construibles, de otra sala o
--                     de otro dueño)
--   bloqueo           el plano del objeto no vale y no hay ninguna casilla
--                     mala que se vea
--   persona           alguien tapa la casilla de origen (hace falta gente)
--   no_se_puede       el objeto no se puede coger (las puertas; en una obra,
--                     lo que no es de su sala); vender y deshacer con una obra
--   otra_ventana      recoger con la ventana de colocar del pasillo abierta: la
--                     interfaz no deja
--   no_disponible     la sala no está en la ventana de construir (no descubierta)
--   pequena           el plano no llega a minimum_size en ancho o en alto
--   zonas_inaccesibles al confirmar el plano, el anillo de alrededor queda
--                     partido (checkReachability)
--   fase              la obra no está en la fase de la orden (o no hay obra):
--                     puerta, ventana, quitar_ventana, fin_ventanas,
--                     comprar_sala, fin_sala; y recoger con una obra que no
--                     está en la fase de objetos (el botón de coger está apagado)
--   posicion          la puerta o la ventana no está donde el puntero puede
--                     ponerla (screenToWall): ni en las esquinas ni, con la
--                     puerta doble, en las casillas que se salta
--   puerta_invalida   la puerta no vale (blueprint_door.valid)
--   ventana_invalida  la ventana no vale (blueprint_window.valid)
--   no_es_de_sala     el objeto no está en la ventana de comprar de la obra
--   faltan_objetos    fin_sala sin todos los objetos obligatorios colocados
--
-- Las ventanas que cierran a otras. UIFurnishCorridor, UIPlaceObjects,
-- UIEditRoom (que hereda de UIPlaceObjects) y UIBuildRoom son ventanas «main»,
-- y UI:addWindow cierra la «main» que hubiera con su close() sin argumentos
-- (ui.lua:1048-1054). Para UIPlaceObjects eso es close(nil), lo mismo que el
-- botón de cancelar (place_objects.lua:344-356): removeAllObjects(nil), y cada
-- unidad que queda pasa por removeObject (245-312):
--   * comprada y sin colocar: se devuelve su precio («purchase_cancellation»,
--     252-259, receiveMoney con el valor del hospital a la baja);
--   * cogida para moverla: SE VENDE («sale_previously_placed»: como no es un
--     movimiento cancelado, se cobra su precio y destroyEntity, 269-273). Solo
--     deshacer (close(true), 365-367) la devuelve a su sitio.
-- Así, comprar con la ventana de colocar abierta primero la cierra (devuelve
-- lo no colocado y vende lo cogido) y después mira el dinero, ya con lo
-- devuelto; y lo hace también si después se rechaza (no_es_de_pasillo,
-- sin_dinero o maximo), porque la ventana de amueblar ya se abrió. Recoger no
-- puede pasar con la ventana de colocar abierta: esa ventana apaga la prueba
-- del mundo (place_objects.lua:89; game_ui.lua:437, sin objeto bajo el
-- puntero) y se queda el clic derecho para girar el plano (507-509, y
-- window.lua:1715-1721 no lo deja pasar a UI:onMouseUp, ui.lua:911-917).
-- El núcleo tiene que copiar todo esto.
--
-- Con una obra abierta (UIEditRoom), los botones del panel que abren otra
-- ventana (construir, bottom_panel.lua:721-743; amueblar, 745-767) llaman
-- antes a GameUI:setEditRoom(false) (game_ui.lua:1259-1284), que es
-- UIEditRoom:verifyOrAbortRoom (edit_room.lua:131-139): si la obra está en la
-- fase de objetos con todos los obligatorios colocados, close() la termina
-- (confirm(true): se cobra la sala, markRoomAsBuilt); si no, abortRoom
-- (141-164): close() cancela hasta el plano (los objetos vuelven a la lista
-- y se devuelven; la sala, si ya estaba levantada, sale del mapa) y la sala
-- se borra de world.rooms, sin dinero porque no está pagada. Es lo que hacen
-- aquí sala y comprar con una obra abierta, y también cerrar, que llama a
-- verifyOrAbortRoom: un close() a secas de la obra dejaría en world.rooms una
-- sala sin levantar (el caso raro de UIPlaceObjects:undo, que en la interfaz
-- no se da porque Escape dispara antes setEditRoom(false), game_ui.lua:155).
-- Las ventanas de construir (UIBuildRoom) y de comprar de la obra
-- (UIFurnishCorridor con edit_dialog, modal «furnish») no quedan abiertas
-- entre dos órdenes: la orden las cierra si se rechaza.
local O = {}

O.orientaciones = {north = true, east = true, south = true, west = true}

-- Nombre de la orden → argumentos: "id" (objeto o tipo de sala), "n" (entero
-- ≥ 0), "n1" (entero ≥ 1), "x"/"y" (casilla del mapa) y "o" (orientación o
-- lado).
O.vocabulario = {
  comprar = {"id", "n"},
  colocar = {"id", "x", "y", "o"},
  cerrar = {},
  recoger = {"x", "y"},
  vender = {},
  deshacer = {},
  sala = {"id", "x", "y", "n1", "n1"},
  puerta = {"x", "y", "o"},
  ventana = {"x", "y", "o"},
  quitar_ventana = {"x", "y", "o"},
  fin_ventanas = {},
  comprar_sala = {"id", "n1"},
  fin_sala = {},
  atras = {},
}

local function entero(s)
  return s and s:match("^%-?%d+$") and math.tointeger(tonumber(s)) or nil
end

--! Lee una lista de órdenes: una por línea, el nombre y sus argumentos
--! separados por tabuladores; las líneas vacías y las que empiezan por # no
--! cuentan. Devuelve una lista de {nombre, args, linea}; los números ya
--! convertidos. Una línea mal escrita es un error (con su número).
function O.leer(ruta, ancho, alto)
  local f, err = io.open(ruta, "r")
  if not f then error(("no se puede leer la lista de órdenes %s: %s"):format(ruta, tostring(err)), 0) end
  local lista, k = {}, 0
  for linea in f:lines() do
    k = k + 1
    linea = linea:gsub("\r$", "")
    if not (linea:match("^%s*$") or linea:match("^#")) then
      local campos = {}
      for c in (linea .. "\t"):gmatch("([^\t]*)\t") do campos[#campos + 1] = c end
      local nombre = table.remove(campos, 1)
      local tipos = O.vocabulario[nombre]
      local function mal(msg) error(("%s:%d: %s: «%s»"):format(ruta, k, msg, linea), 0) end
      if not tipos then mal("orden desconocida") end
      if #campos ~= #tipos then mal(("%s lleva %d argumentos"):format(nombre, #tipos)) end
      local args = {}
      for i, t in ipairs(tipos) do
        local c = campos[i]
        if t == "id" then
          if not c:match("^[%w_]+$") then mal("objeto o sala mal escrito") end
          args[i] = c
        elseif t == "n" then
          args[i] = entero(c)
          if not args[i] or args[i] < 0 then mal("la cantidad tiene que ser un entero ≥ 0") end
        elseif t == "n1" then
          args[i] = entero(c)
          if not args[i] or args[i] < 1 then mal("la cantidad o el tamaño tiene que ser un entero ≥ 1") end
        elseif t == "x" or t == "y" then
          args[i] = entero(c)
          local lim = t == "x" and ancho or alto
          if not args[i] or args[i] < 0 or args[i] >= lim then
            mal(("la casilla tiene que estar en el mapa (0..%d)"):format(lim - 1))
          end
        else
          if not O.orientaciones[c] then mal("orientación o lado: north, east, south o west") end
          args[i] = c
        end
      end
      lista[#lista + 1] = {nombre = nombre, args = args, linea = k}
    end
  end
  f:close()
  return lista
end

--! El texto de una orden, con espacios (para resultados.tsv y el registro).
function O.texto(orden)
  local partes = {orden.nombre}
  for _, a in ipairs(orden.args) do partes[#partes + 1] = tostring(a) end
  return table.concat(partes, " ")
end

--! La fila de ordenes.tsv, sin n: el nombre y sus argumentos con tabuladores.
function O.campos(orden)
  local partes = {orden.nombre}
  for _, a in ipairs(orden.args) do partes[#partes + 1] = tostring(a) end
  return table.concat(partes, "\t")
end

-- Capa 4 del mapa (la de la interfaz) de una casilla mala de la huella:
-- bad_tile = 67 + flag_alpha75 (place_objects.lua:689, 797). Las casillas de
-- fuera del mapa quedan en object_footprint con x = 0 (792-793).
local HUELLA_MALA = 67 + 256 * 8
-- Y de una casilla mala del plano de una sala: 67 + (thdf_alpha_50 << 8)
-- (th_lua_map.cpp:244, 290-293); con el plano pequeño son malas todas
-- (entire_invalid, 232-236).
local PLANO_MALO = 67 + 256 * 4
-- Las marcas de las ventanas en la capa 4 de la casilla de dentro
-- (window_floor_blueprint_markers, edit_room.lua:1069-1074).
local MARCA_VENTANA = {north = 33, east = 34, south = 35, west = 36}

--! Un intérprete sobre la partida cargada (TheApp).
function O.nuevo(app)
  local ui, world = app.ui, app.world
  local th = world.map.th
  local I = {}

  -- La ventana de colocar: la del pasillo o una obra (UIEditRoom hereda de
  -- UIPlaceObjects y class.is la encuentra, window.lua:565-573).
  local function ventana()
    return ui:getWindow(UIPlaceObjects)
  end
  I.ventana = ventana
  local function obra()
    return ui:getWindow(UIEditRoom)
  end
  I.obra = obra

  -- Casillas de la huella del último plano que están mal (motivo casillas:n).
  local function huella_mala(win)
    local m = 0
    for _, xy in ipairs(win.object_footprint) do
      if xy[1] == 0 or th:getCell(xy[1], xy[2], 4) == HUELLA_MALA then m = m + 1 end
    end
    return m
  end

  -- Las posiciones de la puerta y de las ventanas que puede dar el puntero
  -- (UIEditRoom:screenToWall, edit_room.lua:753-869): (x, y) es la casilla de
  -- dentro, en el borde, y nunca una esquina. Puerta normal y ventanas: al
  -- norte y al sur, x en rx+1..rx+w-2; al oeste y al este, y en ry+1..ry+h-2.
  -- Con la puerta doble (swing_doors, solo en la fase de la puerta, 771-776),
  -- (x, y) es la hoja maestra, la del centro: al norte desde rx+3 («Swing
  -- doors in top corner to the east, actually skip another tile», 836-839),
  -- al sur desde rx+2 (860-861) y al oeste y al este desde ry+2 (830-832,
  -- 849-850). setDoorBlueprint y setWindowBlueprint no lo comprueban: con una
  -- posición de fuera de este conjunto pueden aceptar lo que la interfaz
  -- nunca daría (o fallar), así que se filtra antes (motivo posicion).
  local function posicion_valida(win, x, y, lado, doble)
    local r = win.blueprint_rect
    if lado == "north" or lado == "south" then
      local x0 = r.x + 1
      if doble then x0 = lado == "north" and r.x + 3 or r.x + 2 end
      local fila = lado == "north" and r.y or r.y + r.h - 1
      return y == fila and x0 <= x and x <= r.x + r.w - 2
    end
    local y0 = doble and r.y + 2 or r.y + 1
    local columna = lado == "west" and r.x or r.x + r.w - 1
    return x == columna and y0 <= y and y <= r.y + r.h - 2
  end

  local A = {}

  -- UIBottomPanel:dialogFurnishCorridor (bottom_panel.lua:745-767) abre la
  -- ventana de amueblar (antes, setEditRoom(false): la obra que hubiera se
  -- termina o se abandona, ver arriba); cada unidad es una pulsación de «+»
  -- en su fila, purchaseItem(i, 1) (furnish_corridor.lua:95-113, 136-174),
  -- que no cobra: falla con 99 ya en la ventana (maximo) o si el saldo no
  -- llega al total (sin_dinero); entonces la orden se cancela con el botón de
  -- cerrar (UIFurnishCorridor:close) y no se compra nada. Si no, confirm
  -- (176-205) cobra precio × cantidad (spendMoney, con el valor del hospital
  -- al alza) y abre UIPlaceObjects con lo comprado; con cantidad 0, confirm
  -- solo cierra. Abrir la ventana de amueblar cierra la de colocar: ver arriba.
  function A.comprar(id, cantidad)
    assert(not (app.key_modifiers.ctrl or app.key_modifiers.shift), "Ctrl o Mayúsculas multiplican la cantidad")
    -- Con la ventana de amueblar ya abierta, el botón la cerraría; entre dos
    -- órdenes nunca lo está.
    assert(not ui:getWindow(UIFurnishCorridor), "la ventana de amueblar ya estaba abierta")
    ui.bottom_panel:dialogFurnishCorridor()
    local dlg = assert(ui:getWindow(UIFurnishCorridor), "no se abrió la ventana de amueblar")
    local i
    for k, o in ipairs(dlg.objects) do
      if o.object.id == id then i = k end
    end
    if not i then
      dlg:close()
      return false, "no_es_de_pasillo"
    end
    for _ = 1, cantidad do
      local antes = dlg.objects[i].qty
      if not dlg:purchaseItem(i, 1) then
        dlg:close()
        return false, antes >= 99 and "maximo" or "sin_dinero"
      end
    end
    dlg:confirm()
    return true
  end

  -- UIPlaceObjects:onMouseUp con el botón izquierdo (place_objects.lua:498-529):
  -- elegir la fila del objeto como el clic en la lista (place_objects = true,
  -- setActiveIndex y, en una obra, dejar el modo de coger: 112-125), girarlo,
  -- llevar el puntero a la casilla (setBlueprintCell, que calcula
  -- object_blueprint_good, 696-731) y soltarlo si el plano vale y no hay
  -- nadie (anyHumanoidObscuringArea, world.lua:1520-1529). Antes, el puntero
  -- sale del mapa (setBlueprintCell(nil)) para que el plano solo se compruebe
  -- en la casilla de la orden: comprobar un objeto de borde deja travel* a
  -- true al acabar, no como estaba (_isSideObjectPlacementValid, 1003-1007).
  -- En una obra, placeObject es UIEditRoom:placeObject (edit_room.lua:
  -- 1635-1640): la huella tiene que quedar dentro de su sala (las casillas de
  -- fuera son malas, world.lua:1870-1890), el bloqueo de zonas es el «blando»
  -- del modo 2 mientras la sala no está activa (place_objects.lua:875-897,
  -- 972-988) y el objeto entra en room.objects (582-584).
  function A.colocar(id, x, y, orient)
    local win = ventana()
    if not win then return false, "sin_ventana" end
    local i
    for k, o in ipairs(win.objects) do
      if o.object.id == id and o.qty > 0 then
        i = k
        break
      end
    end
    if not i then return false, "sin_objeto" end
    if not app.objects[id].orientations[orient] then return false, "orientacion" end
    win:setBlueprintCell(nil, nil)
    win.place_objects = true
    win:setActiveIndex(i)
    local edicion = obra()
    if edicion and edicion.in_pickup_mode then edicion:stopPickupItems() end
    win:setOrientation(orient)
    win:setBlueprintCell(x + 1, y + 1)
    if not win.object_blueprint_good then
      local m = huella_mala(win)
      return false, m > 0 and ("casillas:" .. m) or "bloqueo"
    end
    if world:anyHumanoidObscuringArea(x + 1, y + 1) then return false, "persona" end
    win:placeObject()
    return true
  end

  -- El botón de cancelar de la ventana de colocar del pasillo
  -- (UIPlaceObjects:cancel, close(false)): devuelve lo comprado sin colocar y
  -- vende lo cogido. En una obra, lo que hace abrir otra ventana desde el
  -- panel, verifyOrAbortRoom (ver arriba): la termina si está en la fase de
  -- objetos con todo lo obligatorio colocado (se cobra la sala) y si no la
  -- abandona; en los dos casos lo comprado sin colocar se devuelve y lo
  -- cogido dentro de la obra, que ya está destruido, se «vende» a precio
  -- entero (sale_previously_placed, place_objects.lua:250, 269-273).
  function A.cerrar()
    local win = ventana()
    if not win then return false, "sin_ventana" end
    if class.is(win, UIEditRoom) then
      win:verifyOrAbortRoom()
    else
      win:cancel()
    end
    return true
  end

  -- El objeto con ese origen: el último de world.entities, si hay varios.
  local function objeto_en(x, y)
    local obj
    for _, e in ipairs(world.entities) do
      if class.is(e, Object) and e.tile_x == x and e.tile_y == y then obj = e end
    end
    return obj
  end

  -- Clic derecho en el objeto: UI:onMouseUp → Object:onClick(ui, "right")
  -- (entities/object.lua:760-825). En el pasillo lo deja en el mapa, invisible
  -- y con picked_up, y abre UIPlaceObjects sin cobrar. No hace nada con las
  -- puertas de la entrada (no son de pasillo, 772-775) ni con los objetos de
  -- una sala que no se edita (777-780).
  -- En una obra en la fase de objetos: el botón de coger (pickupItems,
  -- edit_room.lua:595-607, que es un conmutador: solo se pulsa si no está ya
  -- en ese modo) y el clic izquierdo sobre el objeto. La prueba del mundo
  -- solo da los objetos de esa sala que no son su puerta (setWorldHitTest(
  -- room) en enterObjectsPhase, 973; game_ui.lua:437-444): con otro objeto no
  -- pasa nada (no_se_puede). onClick (800-818) destruye el objeto (sale de
  -- room.objects y del mapa) y lo mete en la lista de la obra sin cobrar
  -- (existing_object), deja el modo de coger y vuelve a mirar
  -- checkEnableConfirm. Si la obra no está en la fase de objetos, el botón de
  -- coger está apagado (736-737, 983) y el mundo no recibe clics (fase).
  function A.recoger(x, y)
    local win = ventana()
    if win and not class.is(win, UIEditRoom) then return false, "otra_ventana" end
    if win and win.phase ~= "objects" then return false, "fase" end
    local obj = objeto_en(x + 1, y + 1)
    if not obj then return false, "sin_objeto" end
    if win then
      if obj:getRoom() ~= win.room or obj == win.room.door then return false, "no_se_puede" end
      if not win.in_pickup_mode then win:pickupItems() end
      obj:onClick(ui, "left")
      if win.in_pickup_mode then
        -- No se cogió: el botón de coger se vuelve a pulsar y queda apagado.
        win:stopPickupItems()
        return false, "no_se_puede"
      end
      return true
    end
    obj:onClick(ui, "right")
    if not ventana() then return false, "no_se_puede" end
    return true
  end

  -- La tecla de vender (UIPlaceObjects:sell, place_objects.lua:359-363): fuera
  -- de una sala en edición es close(false), lo mismo que cerrar; dentro no
  -- hace nada, y aquí se rechaza (no_se_puede): es una restricción del
  -- vocabulario, no de CorsixTH.
  function A.vender()
    local win = ventana()
    if not win then return false, "sin_ventana" end
    if class.is(win, UIEditRoom) then return false, "no_se_puede" end
    win:sell()
    return true
  end

  -- Escape (UIPlaceObjects:undo, close(true)): el objeto cogido vuelve a su
  -- casilla y orientación sin comprobar nada y sin dinero (261-267); lo
  -- comprado sin colocar se devuelve. Con una obra se rechaza (no_se_puede):
  -- close(true) desde la obra dejaría la sala en world.rooms sin levantar, y
  -- en la interfaz Escape dispara antes setEditRoom(false) (game_ui.lua:155).
  function A.deshacer()
    local win = ventana()
    if not win then return false, "sin_ventana" end
    if class.is(win, UIEditRoom) then return false, "no_se_puede" end
    win:undo()
    return true
  end

  -- UIBottomPanel:dialogBuildRoom (bottom_panel.lua:721-743): setEditRoom(
  -- false) (la obra que hubiera se termina o se abandona) y la ventana de
  -- construir, que cierra la «main» que hubiera (la de colocar del pasillo,
  -- con sus devoluciones). La sala se busca en las cuatro categorías
  -- (UIBuildRoom:setCategory, build_room.lua:153-173: solo las descubiertas,
  -- updateBuildableRooms, 109-125); si no está, se cierra la ventana
  -- (no_disponible). UIBuildRoom:buildRoom (175-196) exige saldo ≥ build_cost
  -- (sin dinero, la ventana de construir se queda abierta y se cierra:
  -- sin_dinero) y abre UIEditRoom (cierra la de construir). setBlueprintRect
  -- (edit_room.lua:1167-1208): el plano se recorta al mapa (1170-1173) y vale
  -- si y solo si el botón ✓ queda activo; si no: pequena si el ancho o el
  -- alto recortados no llegan a minimum_size (1180; entonces el C pinta malas
  -- todas las casillas) y si no casillas:n, las del plano a PLANO_MALO (de
  -- otra sala, no construibles o de otro dueño, th_lua_map.cpp:232-236). Con
  -- el plano rechazado la obra se cierra (close(): sin sala ni dinero).
  -- confirm (214-220) → enterDoorPhase (915-962): el plano deja de ser
  -- transitable y, si el anillo de alrededor queda partido
  -- (checkReachability, 873-913), vuelve a la fase del plano
  -- (zonas_inaccesibles) y la obra se cierra igual.
  function A.sala(tipo, x, y, w, h)
    assert(not ui:getWindow(UIBuildRoom), "la ventana de construir ya estaba abierta")
    ui.bottom_panel:dialogBuildRoom()
    local dlg = assert(ui:getWindow(UIBuildRoom), "no se abrió la ventana de construir")
    local categoria, indice
    for c = 1, 4 do
      dlg:setCategory(c)
      for k, info in ipairs(dlg.list) do
        if info.id == tipo then categoria, indice = c, k end
      end
    end
    if not indice then
      dlg:close()
      return false, "no_disponible"
    end
    dlg:setCategory(categoria)
    dlg:buildRoom(indice)
    local win = obra()
    if not win then
      assert(ui:getWindow(UIBuildRoom) == dlg, "sin obra y sin la ventana de construir")
      dlg:close()
      return false, "sin_dinero"
    end
    assert(dlg.closed and win.phase == "walls", "la obra no empieza en la fase del plano")
    win:setBlueprintRect(x + 1, y + 1, w, h)
    if not win.confirm_button.enabled then
      local r, minimo = win.blueprint_rect, win.room_type.minimum_size
      local motivo
      if r.w < minimo or r.h < minimo then
        motivo = "pequena"
      else
        local m = 0
        for yy = r.y, r.y + r.h - 1 do
          for xx = r.x, r.x + r.w - 1 do
            if th:getCell(xx, yy, 4) == PLANO_MALO then m = m + 1 end
          end
        end
        assert(m > 0, "plano rechazado sin casillas malas")
        motivo = "casillas:" .. m
      end
      win:close()
      return false, motivo
    end
    win:confirm()
    if win.phase ~= "door" then
      assert(win.phase == "walls", "fase rara tras confirmar el plano: " .. tostring(win.phase))
      win:close()
      return false, "zonas_inaccesibles"
    end
    return true
  end

  -- En la fase de la puerta, el puntero sobre el borde es setDoorBlueprint
  -- (edit_room.lua:1296-1423: pared que ya existe, casillas de dentro y de
  -- fuera del jugador, sin objeto (salvo basura) y sin uso exclusivo de otro
  -- objeto; la doble mira también las dos hojas de al lado) y el clic
  -- izquierdo, con blueprint_door.valid, es confirm(true) (1091-1095), que
  -- pasa a las ventanas. Si no vale, el puntero sale del borde
  -- (setDoorBlueprint(nil), como al ir al botón) y la puerta no queda marcada.
  function A.puerta(x, y, lado)
    local win = obra()
    if not win or win.phase ~= "door" then return false, "fase" end
    if not posicion_valida(win, x + 1, y + 1, lado, win.room_type.swing_doors) then return false, "posicion" end
    win:setDoorBlueprint(x + 1, y + 1, lado)
    if not win.blueprint_door.valid then
      win:setDoorBlueprint(nil, nil, nil)
      return false, "puerta_invalida"
    end
    win:confirm(true)
    assert(win.phase == "windows", "la puerta no llevó a las ventanas")
    return true
  end

  -- En la fase de ventanas, el puntero sobre el borde es setWindowBlueprint
  -- (edit_room.lua:1434-1500): no vale sobre un tramo con etiqueta (la
  -- puerta, sus hojas o una ventana ya puesta, 1458-1462) ni donde el mapa ya
  -- tiene una pared conocida (de otra sala o de fachada, 1481-1494). El clic
  -- izquierdo es placeWindowBlueprint (1425-1432), que la deja puesta (la
  -- etiqueta window del tramo). Si no vale, el puntero sale del borde.
  function A.ventana(x, y, lado)
    local win = obra()
    if not win or win.phase ~= "windows" then return false, "fase" end
    if not posicion_valida(win, x + 1, y + 1, lado, false) then return false, "posicion" end
    win:setWindowBlueprint(x + 1, y + 1, lado)
    if not win.blueprint_window.valid then
      win:setWindowBlueprint(nil, nil, nil)
      return false, "ventana_invalida"
    end
    win:placeWindowBlueprint()
    return true
  end

  -- El clic derecho sobre una ventana del plano: el cuerpo de
  -- tryRemoveWindowFromWall (edit_room.lua:1106-1132) con la casilla y el
  -- lado en vez del puntero: si la casilla de dentro lleva la marca de
  -- ventana de ese lado en la capa 4, el tramo vuelve a ser pared (animación
  -- 120) sin etiqueta.
  function A.quitar_ventana(x, y, lado)
    local win = obra()
    if not win or win.phase ~= "windows" then return false, "fase" end
    if not posicion_valida(win, x + 1, y + 1, lado, false) then return false, "posicion" end
    local cx, cy = x + 1, y + 1
    if th:getCell(cx, cy, 4) ~= MARCA_VENTANA[lado] then return false, "sin_ventana" end
    local wx, wy = cx, cy
    if lado == "south" then wy = cy + 1 elseif lado == "east" then wx = cx + 1 end
    local anim = assert(win.blueprint_wall_anims[wx][wy], "ventana marcada sin animación de pared")
    th:setCell(cx, cy, 4, 24)
    anim:setAnimation(win.anims, 120, (lado == "south" or lado == "north") and 0 or 1)
    anim:setTag(nil)
    return true
  end

  -- El botón ✓ en la fase de ventanas (confirm, edit_room.lua:224-226):
  -- clearArea (259-324) y, sin nadie en el rectángulo ampliado, confirm(true)
  -- al momento: finishRoom (359-552: las paredes, las puertas como objetos y
  -- la sala, newRoom + markRoom), resetSideObjects y enterObjectsPhase
  -- (970-1000), que cobra los objetos obligatorios (addObjects con pay_for,
  -- place_objects.lua:221-225). Con gente se quedaría en clear_area
  -- esperando ticks de la interfaz: la batería no tiene gente.
  function A.fin_ventanas()
    local win = obra()
    if not win or win.phase ~= "windows" then return false, "fase" end
    win:confirm()
    assert(win.phase == "objects", "la obra no llegó a la fase de objetos (" .. tostring(win.phase) .. "): ¿hay gente?")
    return true
  end

  -- El botón de comprar de la obra (UIEditRoom:purchaseItems, edit_room.lua:
  -- 554-592): la ventana de comprar con los opcionales de la sala
  -- (objects_additional, los disponibles en el nivel y descubiertos), cada
  -- unidad una pulsación de «+» como en comprar (sin_dinero, maximo), y
  -- confirm (furnish_corridor.lua:176-205), que cobra y añade a la lista de
  -- la obra sin volver a cobrar. Solo cantidades ≥ 1: bajar una cantidad es
  -- el posible doble reembolso de CorsixTH (187-196 y place_objects.lua:
  -- 252-259), fuera del paso 3.
  function A.comprar_sala(id, cantidad)
    local win = obra()
    if not win or win.phase ~= "objects" then return false, "fase" end
    assert(cantidad > 0, "comprar_sala solo con cantidad ≥ 1")
    assert(not (app.key_modifiers.ctrl or app.key_modifiers.shift), "Ctrl o Mayúsculas multiplican la cantidad")
    assert(not ui:getWindow(UIFurnishCorridor), "la ventana de comprar ya estaba abierta")
    win:purchaseItems()
    local dlg = assert(ui:getWindow(UIFurnishCorridor), "no se abrió la ventana de comprar")
    assert(dlg.edit_dialog == win, "la ventana de comprar no es la de la obra")
    local i
    for k, o in ipairs(dlg.objects) do
      if o.object.id == id then i = k end
    end
    if not i then
      dlg:close()
      return false, "no_es_de_sala"
    end
    for _ = 1, cantidad do
      local antes = dlg.objects[i].qty
      if not dlg:purchaseItem(i, 1) then
        dlg:close()
        return false, antes >= 99 and "maximo" or "sin_dinero"
      end
    end
    dlg:confirm()
    return true
  end

  -- El botón ✓ en la fase de objetos: solo con todos los obligatorios
  -- colocados (checkEnableConfirm, edit_room.lua:1605-1633); confirm
  -- (233-256) cobra build_cost menos los obligatorios ya pagados
  -- (build_room), markRoomAsBuilt (world.lua:637-649: built, is_active, el
  -- libro de casos y notifyNewRoom) y close(), que devuelve lo comprado sin
  -- colocar.
  function A.fin_sala()
    local win = obra()
    if not win or win.phase ~= "objects" then return false, "fase" end
    if not win:checkEnableConfirm() then return false, "faltan_objetos" end
    win:confirm()
    assert(win.closed, "la obra no se cerró al confirmar")
    return true
  end

  -- El botón ✗ de la obra (UIEditRoom:cancel, edit_room.lua:166-205): en la
  -- fase de objetos, returnToDoorPhase (728-751: la sala sale del mapa con
  -- removeRoom, los objetos colocados y los de la lista se devuelven, la
  -- sala sigue en world.rooms sin levantar y el plano vuelve a dejar de ser
  -- transitable); en la de la puerta o las ventanas, returnToWallPhase
  -- (617-638: se pierden la puerta y las ventanas y el plano vuelve a ser
  -- transitable); en la del plano, abortRoom (141-164): la obra se cierra y
  -- la sala, si existía, se borra de world.rooms, sin dinero porque no está
  -- pagada (con una sala pagada, la de una edición, preguntaría antes).
  function A.atras()
    local win = obra()
    if not win then return false, "sin_ventana" end
    assert(not win.confirm_dialog_open, "la obra está preguntando si vender la sala")
    win:cancel()
    return true
  end

  -- Las órdenes que exigen world.user_actions_allowed (world.lua:813-817): los
  -- botones de amueblar y de construir del panel (bottom_panel.lua:722, 746),
  -- el clic en el mapa con la ventana de colocar (place_objects.lua:503), el
  -- clic en un objeto (ui.lua:936) y el clic en el mapa con una obra
  -- (UIEditRoom:onMouseDown, edit_room.lua:1135: la puerta y las ventanas).
  -- Los botones de una ventana (window.lua:1724-1736) y las teclas
  -- (UI:onKeyDown, ui.lua:768-779) no lo miran.
  local CON_PAUSA = {comprar = true, colocar = true, recoger = true, sala = true, puerta = true, ventana = true,
    quitar_ventana = true}

  --! Aplica la orden. Devuelve aceptada (booleano) y el motivo del rechazo.
  function I.aplicar(orden)
    if CON_PAUSA[orden.nombre] and not world.user_actions_allowed then return false, "pausa" end
    return A[orden.nombre](table.unpack(orden.args))
  end

  return I
end

return O
