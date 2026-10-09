-- Construcción automática de hospitales usando las mismas ventanas y
-- validaciones que la interfaz de CorsixTH (UIEditRoom, UIPlaceObjects,
-- UIPlaceStaff). No se toca el estado del juego por detrás: si la interfaz
-- rechazaría una posición, el constructor prueba la siguiente.
local th64 = th64
local B = {}

local function world() return TheApp.world end
local function hosp() return TheApp.world.hospitals[1] end
local function map() return TheApp.map end

B.reserved = {}   -- casillas ocupadas o reservadas como pasillo, por "x,y"
local function key(x, y) return x .. "," .. y end

function B.reset()
  B.reserved = {}
end

-- Dinero ---------------------------------------------------------------------

function B.ensure_money(amount)
  local h = hosp()
  if h.balance < amount then
    h:receiveMoney(amount - h.balance, "th64")
  end
end

-- Mapa -----------------------------------------------------------------------

-- Se reutiliza una sola tabla: getCellFlags crearía una por llamada y la
-- búsqueda de sitio hace cientos de miles. El resultado vale hasta la siguiente.
local flag_tmp = {}
function B.flags(x, y)
  local m = map()
  if x < 1 or y < 1 or x > m.width or y > m.height then return nil end
  return m.th:getCellFlags(x, y, flag_tmp)
end

--! Casilla de pasillo transitable (para soltar personal), aunque esté reservada.
function B.is_corridor(x, y)
  local f = B.flags(x, y)
  return f and f.hospital and f.passable and f.roomId == 0 and f.thob == 0 or false
end

-- Rectángulo que contiene las casillas propias del hospital.
local function hospital_bbox()
  local m = map()
  local x1, y1, x2, y2 = m.width, m.height, 1, 1
  for y = 1, m.height do
    for x = 1, m.width do
      local f = B.flags(x, y)
      if f.hospital and f.owner == 1 then
        if x < x1 then x1 = x end
        if y < y1 then y1 = y end
        if x > x2 then x2 = x end
        if y > y2 then y2 = y end
      end
    end
  end
  return x1, y1, x2, y2
end

--! Casilla de pasillo libre: dentro del hospital, propia, construible, sin
--! habitación ni objeto y sin reservar.
function B.is_free(x, y)
  local f = B.flags(x, y)
  return f and f.hospital and f.buildable and f.owner == 1 and f.roomId == 0
    and f.thob == 0 and f.passable and not B.reserved[key(x, y)] or false
end

function B.entrance()
  for _, e in ipairs(world().entities) do
    if class.is(e, Object) and e.object_type.id == "entrance_left_door" then
      return e.tile_x, e.tile_y
    end
  end
  error("no encuentro la puerta de entrada del hospital")
end

function B.buy_all_plots()
  local h, m = hosp(), map()
  local bought = 0
  local progress = true
  while progress do
    progress = false
    for plot = 1, m.th:getPlotCount() do
      if m.th:getPlotOwner(plot) ~= 1 and m.th:isParcelPurchasable(plot, 1) then
        B.ensure_money(m:getParcelPrice(plot) + 100000)
        if h:purchasePlot(plot) then
          bought = bought + 1
          progress = true
        end
      end
    end
  end
  return bought
end

-- Habitaciones ---------------------------------------------------------------

function B.room_info(id)
  for _, r in ipairs(TheApp.rooms) do
    if r.id == id then return r end
  end
  error("habitación desconocida: " .. id)
end

-- Busca un rectángulo w x h cuyo anillo exterior (1 casilla) sea pasillo libre
-- o fachada; las casillas del anillo quedan reservadas como pasillo, así que
-- entre dos habitaciones siempre quedan al menos 2 casillas de pasillo.
local function ring_ok(x, y)
  local f = B.flags(x, y)
  if not f then return false end
  if not f.hospital then return true end -- fachada exterior
  return B.is_free(x, y)
end

function B.find_spot(w, h, ox, oy)
  local bx1, by1, bx2, by2 = hospital_bbox()
  local best, best_d
  for y = by1, by2 - h + 1 do
    for x = bx1, bx2 - w + 1 do
      local d = math.abs(x + w / 2 - ox) + math.abs(y + h / 2 - oy)
      if not best_d or d < best_d then
        local ok = true
        for yy = y, y + h - 1 do
          for xx = x, x + w - 1 do
            if not B.is_free(xx, yy) then ok = false break end
          end
          if not ok then break end
        end
        if ok then
          for xx = x - 1, x + w do
            if not (ring_ok(xx, y - 1) and ring_ok(xx, y + h)) then ok = false break end
          end
        end
        if ok then
          for yy = y, y + h - 1 do
            if not (ring_ok(x - 1, yy) and ring_ok(x + w, yy)) then ok = false break end
          end
        end
        if ok then best, best_d = {x = x, y = y, w = w, h = h}, d end
      end
    end
  end
  return best
end

local function reserve_rect(r, margin)
  for y = r.y - margin, r.y + r.h - 1 + margin do
    for x = r.x - margin, r.x + r.w - 1 + margin do
      B.reserved[key(x, y)] = true
    end
  end
end

-- Puertas candidatas: casillas interiores del borde, empezando por el centro
-- de cada lado y por el lado que mira hacia (ox, oy).
local function door_candidates(r, ox, oy)
  local sides = {
    {wall = "north", out = {0, -1}, cells = {}},
    {wall = "south", out = {0, 1}, cells = {}},
    {wall = "west", out = {-1, 0}, cells = {}},
    {wall = "east", out = {1, 0}, cells = {}},
  }
  local cx, cy = r.x + (r.w - 1) / 2, r.y + (r.h - 1) / 2
  for _, s in ipairs(sides) do
    if s.wall == "north" or s.wall == "south" then
      local y = s.wall == "north" and r.y or r.y + r.h - 1
      for x = r.x + 1, r.x + r.w - 2 do s.cells[#s.cells + 1] = {x, y} end
    else
      local x = s.wall == "west" and r.x or r.x + r.w - 1
      for y = r.y + 1, r.y + r.h - 2 do s.cells[#s.cells + 1] = {x, y} end
    end
    table.sort(s.cells, function(a, b)
      return math.abs(a[1] - cx) + math.abs(a[2] - cy) < math.abs(b[1] - cx) + math.abs(b[2] - cy)
    end)
    s.dist = math.abs(cx + s.out[1] * r.w - ox) + math.abs(cy + s.out[2] * r.h - oy)
  end
  table.sort(sides, function(a, b) return a.dist < b.dist end)
  local out = {}
  for _, s in ipairs(sides) do
    for _, c in ipairs(s.cells) do
      out[#out + 1] = {x = c[1], y = c[2], wall = s.wall, ox = c[1] + s.out[1], oy = c[2] + s.out[2]}
    end
  end
  return out
end

-- Coloca todos los objetos pendientes de una ventana UIPlaceObjects probando
-- orientaciones y casillas. tiles: lista de {x, y}.
B.debug = false
local function dbg(...) if B.debug then th64.log(...) end end

-- Al colocar el último objeto, UIPlaceObjects cierra la ventana sin quitarlo
-- de la lista (solo deja qty = 0), así que se mira la cantidad pendiente.
local function pending(win)
  for _, o in ipairs(win.objects) do
    if o.qty > 0 then return true end
  end
  return false
end

local function place_pending(win, tiles)
  dbg("place_pending: %d objetos, %d casillas", #win.objects, #tiles)
  while pending(win) do
    win:setActiveIndex(1)
    local obj = win.objects[1].object
    local placed, tries = false, 0
    if B.debug then
      local ks = {}
      for k in pairs(obj.orientations) do ks[#ks + 1] = tostring(k) end
      dbg("  claves de orientations: %s", table.concat(ks, ","))
    end
    for orient in pairs(obj.orientations) do
      win:setOrientation(orient)
      dbg("  %s orientación %s (activa %s), intentos %d", obj.id, orient, tostring(win.object_orientation), tries)
      for _, t in ipairs(tiles) do
        tries = tries + 1
        win:setBlueprintCell(t[1], t[2])
        if win.object_blueprint_good then
          win:placeObject()
          placed = true
          break
        end
      end
      if placed then break end
    end
    if not placed then
      return false, string.format("%s (%d intentos en %d casillas)", obj.id, tries, #tiles)
    end
  end
  return true
end

local function rect_tiles(r)
  local edge, inner = {}, {}
  for y = r.y, r.y + r.h - 1 do
    for x = r.x, r.x + r.w - 1 do
      local on_edge = x == r.x or y == r.y or x == r.x + r.w - 1 or y == r.y + r.h - 1
      local t = {x, y}
      if on_edge then edge[#edge + 1] = t else inner[#inner + 1] = t end
    end
  end
  for _, t in ipairs(inner) do edge[#edge + 1] = t end
  return edge
end

--! Construye una habitación. extras: lista de ids de objetos opcionales.
--! Devuelve la habitación o nil y el motivo.
function B.build_room(id, w, h, extras)
  local info = B.room_info(id)
  local ex, ey = B.entrance()
  local r = B.find_spot(w, h, ex, ey)
  if not r then return nil, "sin sitio" end
  B.ensure_money(200000)

  local ui = TheApp.ui
  local win = UIEditRoom(ui, info)
  ui:addWindow(win)
  win:setBlueprintRect(r.x, r.y, r.w, r.h)
  if not win.confirm_button.enabled then
    win:close()
    reserve_rect(r, 0)
    return nil, "rectángulo rechazado"
  end
  win:confirm()
  if win.phase ~= "door" then
    win:close()
    reserve_rect(r, 0)
    return nil, "dejaría zonas inaccesibles"
  end
  local door_ok = false
  for _, d in ipairs(door_candidates(r, ex, ey)) do
    if B.is_free(d.ox, d.oy) then
      win:setDoorBlueprint(d.x, d.y, d.wall)
      if win.blueprint_door.valid then
        door_ok = true
        break
      end
    end
  end
  if not door_ok then
    win:close()
    reserve_rect(r, 0)
    return nil, "sin puerta válida"
  end
  win:confirm(true) -- puerta -> ventanas
  win:confirm(true) -- ventanas -> despejar área (-> objetos si está vacía)
  local waited = 0
  while win.phase == "clear_area" and waited < 2000 do
    th64.wait_ticks(1)
    waited = waited + 1
  end
  if win.phase ~= "objects" then
    return nil, "no se despejó el área (" .. tostring(win.phase) .. ")"
  end
  local room = win.room
  if not win.closed_cleanly then
    local ok, failed = place_pending(win, rect_tiles(r))
    if not ok then
      win:close()
      reserve_rect(r, 0)
      return nil, "no cabe el objeto obligatorio " .. failed
    end
    if extras and #extras > 0 then
      local list = {}
      for _, oid in ipairs(extras) do
        list[#list + 1] = {object = assert(TheApp.objects[oid], "objeto desconocido: " .. oid), qty = 1}
      end
      win:addObjects(list, true)
      ok, failed = place_pending(win, rect_tiles(r))
      if not ok then
        -- Un opcional que no cabe no invalida la habitación.
        th64.log("  %s: no cabe el opcional %s", id, failed)
        win:removeAllObjects(true)
      end
    end
    if not win:checkEnableConfirm() then
      win:close()
      return nil, "faltan objetos obligatorios"
    end
    win:confirm(true)
  end
  reserve_rect(r, 1)
  return room, r
end

-- Objetos de pasillo -----------------------------------------------------------

--! Coloca objetos de pasillo (bancos, recepción...) cerca de (cx, cy).
function B.place_corridor_objects(list, cx, cy, radius)
  local tiles = {}
  for y = cy - radius, cy + radius do
    for x = cx - radius, cx + radius do
      local f = B.flags(x, y)
      if f and f.hospital and f.roomId == 0 and f.owner == 1 then
        tiles[#tiles + 1] = {x, y}
      end
    end
  end
  table.sort(tiles, function(a, b)
    return math.abs(a[1] - cx) + math.abs(a[2] - cy) < math.abs(b[1] - cx) + math.abs(b[2] - cy)
  end)
  local objs = {}
  for oid, n in pairs(list) do
    objs[#objs + 1] = {object = assert(TheApp.objects[oid], "objeto desconocido: " .. oid), qty = n}
  end
  B.ensure_money(100000)
  local ui = TheApp.ui
  local win = UIPlaceObjects(ui, objs, true)
  ui:addWindow(win)
  local ok, failed = place_pending(win, tiles)
  if not ok then
    th64.log("  pasillo: no cabe %s cerca de %d,%d", failed, cx, cy)
    win:close()
  end
  return ok
end

-- Personal ---------------------------------------------------------------------

local function take_profile(class_name, want)
  local pool = world().available_staff[class_name]
  for i, p in ipairs(pool) do
    if not want or (p[want] or 0) > 0 then
      table.remove(pool, i)
      return p
    end
  end
  -- Sin candidatos: se crea un perfil como haría el mercado de personal del mes.
  local names = {Doctor = "doctor", Nurse = "nurse", Handyman = "handyman", Receptionist = "receptionist"}
  local p = StaffProfile(world(), class_name, _S.staff_class[names[class_name]])
  p:randomise(world().game_date:monthOfGame())
  if want then p[want] = 1.0 end
  return p
end

--! Contrata y coloca en el pasillo, como UIPlaceStaff:onMouseUp.
--! want: nil o "is_surgeon"/"is_psychiatrist"/"is_researcher".
function B.hire(class_name, n, want)
  local ex, ey = B.entrance()
  local h = hosp()
  for _ = 1, n do
    local profile = take_profile(class_name, want)
    local x, y = ex, ey + 2
    for r = 2, 12 do
      local found = false
      for dy = -r, r do
        for dx = -r, r do
          if B.is_corridor(ex + dx, ey + dy) then x, y, found = ex + dx, ey + dy, true break end
        end
        if found then break end
      end
      if found then break end
    end
    B.ensure_money(50000)
    local entity = world():newEntity(profile.humanoid_class, 2, 2)
    entity:setProfile(profile)
    entity:setTile(x, y)
    h:addStaff(entity)
    entity:setHospital(h)
    local room = entity:getRoom()
    if room then room:onHumanoidEnter(entity) else entity:onPlaceInCorridor() end
  end
end

return B
