-- Intérprete de órdenes de construir (hito 6.3, paso 2: amueblar el pasillo).
--
-- Cada orden es un paso de la interfaz de CorsixTH v0.70.1, y se aplica
-- llamando a lo mismo que llaman sus manejadores de eventos, con las mismas
-- condiciones previas: no hace falta mover el puntero por la pantalla, que
-- depende de la cámara. Es el oráculo de las órdenes del núcleo
-- (motor/core/include/th/build.h), que tiene que dar lo mismo orden a orden:
-- aceptada o rechazada, el motivo y lo que cambia (dinero, casillas, objetos).
--
-- Vocabulario (coordenadas desde 0, como el motor; orientaciones north, east,
-- south y west):
--   comprar <objeto> <cantidad>        amueblar el pasillo: elegir y confirmar
--   colocar <objeto> <x> <y> <orient.> soltar un objeto de la ventana de colocar
--   cerrar                             botón de cancelar de esa ventana
--   recoger <x> <y>                    clic derecho en el objeto con ese origen
--   vender                             tecla de vender el objeto cogido
--   deshacer                           Escape: el objeto cogido vuelve a su sitio
--
-- Motivos de rechazo:
--   pausa             world.user_actions_allowed es falso: comprar, colocar y
--                     recoger (los botones y las teclas de la ventana de colocar no
--                     lo miran: cerrar, vender y deshacer se pueden en pausa)
--   no_es_de_pasillo  el objeto no está en la ventana de amueblar el pasillo
--   sin_dinero        el saldo no llega al total
--   maximo            ya hay 99 en la ventana de amueblar
--   sin_ventana       no hay ventana de colocar
--   sin_objeto        no está ese objeto en la ventana; o no hay objeto con ese origen
--   orientacion       el objeto no tiene esa orientación
--   casillas:<n>      n casillas de la huella malas en el plano (las de fuera del
--                     mapa también; las invisibles, como el uso del banco, no)
--   bloqueo           el plano no vale y no hay ninguna casilla mala que se vea
--   persona           alguien tapa la casilla de origen (hace falta gente)
--   no_se_puede       el objeto no se puede coger (las puertas de la entrada)
--   otra_ventana      recoger con la ventana de colocar abierta: la interfaz no deja
--
-- Las ventanas que cierran a otras. UIFurnishCorridor y
-- UIPlaceObjects son ventanas «main», y UI:addWindow cierra la «main» que
-- hubiera con su close() sin argumentos (ui.lua:1048-1054). Para
-- UIPlaceObjects eso es close(nil), lo mismo que el botón de cancelar
-- (place_objects.lua:344-356): removeAllObjects(nil), y cada unidad que queda
-- pasa por removeObject (245-312):
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
local O = {}

O.orientaciones = {north = true, east = true, south = true, west = true}

-- Nombre de la orden → argumentos: "id" (objeto), "n" (entero ≥ 0),
-- "x"/"y" (casilla del mapa) y "o" (orientación).
O.vocabulario = {
  comprar = {"id", "n"},
  colocar = {"id", "x", "y", "o"},
  cerrar = {},
  recoger = {"x", "y"},
  vender = {},
  deshacer = {},
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
          if not c:match("^[%w_]+$") then mal("objeto mal escrito") end
          args[i] = c
        elseif t == "n" then
          args[i] = entero(c)
          if not args[i] or args[i] < 0 then mal("la cantidad tiene que ser un entero ≥ 0") end
        elseif t == "x" or t == "y" then
          args[i] = entero(c)
          local lim = t == "x" and ancho or alto
          if not args[i] or args[i] < 0 or args[i] >= lim then
            mal(("la casilla tiene que estar en el mapa (0..%d)"):format(lim - 1))
          end
        else
          if not O.orientaciones[c] then mal("orientación: north, east, south o west") end
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

--! Un intérprete sobre la partida cargada (TheApp).
function O.nuevo(app)
  local ui, world = app.ui, app.world
  local th = world.map.th
  local I = {}

  local function ventana()
    return ui:getWindow(UIPlaceObjects)
  end
  I.ventana = ventana

  -- Casillas de la huella del último plano que están mal (motivo casillas:n).
  local function huella_mala(win)
    local m = 0
    for _, xy in ipairs(win.object_footprint) do
      if xy[1] == 0 or th:getCell(xy[1], xy[2], 4) == HUELLA_MALA then m = m + 1 end
    end
    return m
  end

  local A = {}

  -- UIBottomPanel:dialogFurnishCorridor (bottom_panel.lua:745-767) abre la
  -- ventana de amueblar; cada unidad es una pulsación de «+» en su fila,
  -- purchaseItem(i, 1) (furnish_corridor.lua:95-113, 136-174), que no cobra:
  -- falla con 99 ya en la ventana (maximo) o si el saldo no llega al total
  -- (sin_dinero); entonces la orden se cancela con el botón de cerrar
  -- (UIFurnishCorridor:close) y no se compra nada. Si no, confirm (176-205)
  -- cobra precio × cantidad (spendMoney, con el valor del hospital al alza) y
  -- abre UIPlaceObjects con lo comprado; con cantidad 0, confirm solo cierra.
  -- Abrir la ventana de amueblar cierra la de colocar: ver arriba.
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
  -- elegir la fila del objeto (setActiveIndex, como el clic en la lista),
  -- girarlo, llevar el puntero a la casilla (setBlueprintCell, que calcula
  -- object_blueprint_good, 696-731) y soltarlo si el plano vale y no hay
  -- nadie (anyHumanoidObscuringArea, world.lua:1520-1529). Antes, el puntero
  -- sale del mapa (setBlueprintCell(nil)) para que el plano solo se compruebe
  -- en la casilla de la orden: comprobar un objeto de borde deja travel* a
  -- true al acabar, no como estaba (_isSideObjectPlacementValid, 1003-1007).
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
    assert(win.place_objects, "la ventana de colocar no está colocando")
    win:setBlueprintCell(nil, nil)
    win:setActiveIndex(i)
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

  -- El botón de cancelar de la ventana de colocar (UIPlaceObjects:cancel,
  -- close(false)): devuelve lo comprado sin colocar y vende lo cogido.
  function A.cerrar()
    local win = ventana()
    if not win then return false, "sin_ventana" end
    win:cancel()
    return true
  end

  -- Clic derecho en el objeto: UI:onMouseUp → Object:onClick(ui, "right")
  -- (entities/object.lua:760-825). En el pasillo lo deja en el mapa, invisible
  -- y con picked_up, y abre UIPlaceObjects sin cobrar. No hace nada con las
  -- puertas de la entrada (no son de pasillo, 772-775) ni con los objetos de
  -- una sala que no se edita (777-780). Si hay varios objetos con ese origen,
  -- el último de world.entities.
  function A.recoger(x, y)
    if ventana() then return false, "otra_ventana" end
    local obj
    for _, e in ipairs(world.entities) do
      if class.is(e, Object) and e.tile_x == x + 1 and e.tile_y == y + 1 then obj = e end
    end
    if not obj then return false, "sin_objeto" end
    obj:onClick(ui, "right")
    if not ventana() then return false, "no_se_puede" end
    return true
  end

  -- La tecla de vender (UIPlaceObjects:sell, place_objects.lua:359-363): fuera
  -- de una sala en edición es close(false), lo mismo que cerrar.
  function A.vender()
    local win = ventana()
    if not win then return false, "sin_ventana" end
    win:sell()
    return true
  end

  -- Escape (UIPlaceObjects:undo, close(true)): el objeto cogido vuelve a su
  -- casilla y orientación sin comprobar nada y sin dinero (261-267); lo
  -- comprado sin colocar se devuelve.
  function A.deshacer()
    local win = ventana()
    if not win then return false, "sin_ventana" end
    win:undo()
    return true
  end

  -- Las órdenes que exigen world.user_actions_allowed (world.lua:813-817): el
  -- botón de amueblar (bottom_panel.lua:746), el clic en el mapa con la ventana
  -- de colocar (place_objects.lua:503) y el clic en un objeto (ui.lua:936). Los
  -- botones de una ventana (window.lua:1724-1736) y las teclas
  -- (UI:onKeyDown, ui.lua:768-779) no lo miran.
  local CON_PAUSA = {comprar = true, colocar = true, recoger = true}

  --! Aplica la orden. Devuelve aceptada (booleano) y el motivo del rechazo.
  function I.aplicar(orden)
    if CON_PAUSA[orden.nombre] and not world.user_actions_allowed then return false, "pausa" end
    return A[orden.nombre](table.unpack(orden.args))
  end

  return I
end

return O
