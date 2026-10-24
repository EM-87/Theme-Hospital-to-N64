-- Oráculo de la caminata (hito 3 de la fase 5): carga un nivel, pone un
-- humanoide («Standard Male Patient», con capas fijas) en una casilla, le
-- manda caminar a otra (WalkAction y luego IdleAction) y vuelca su estado
-- tras cada tick del mundo: casilla, desplazamiento en píxeles, animación,
-- fotograma y banderas. Es lo que el motor tiene que repetir tick a tick.
-- Opciones: --th64-nivel=N (por defecto 1)
--           --th64-ruta="x1,y1;x2,y2" (desde 0, como el motor)
--           --th64-ticks=T (por defecto 200), --th64-nombre=fichero
-- Si se da --th64-png=ruta, hace además una captura del mapa (sin ventanas
-- ni cursor) en el tick --th64-tick-png, centrada en el humanoide.
local th64 = th64
local nivel = tonumber(th64.opts.nivel or "1")
local x1, y1, x2, y2 = (th64.opts.ruta or "62,75;70,65"):match("(%d+),(%d+);(%d+),(%d+)")
x1, y1, x2, y2 = tonumber(x1), tonumber(y1), tonumber(x2), tonumber(y2)
local ticks = tonumber(th64.opts.ticks or "200")
local capas = {[0] = 4, [1] = 2, [2] = 2}
assert(TheApp:loadLevel(nivel, "full"), "no se pudo cargar el nivel " .. nivel)
th64.autorun_speed = "Normal"
th64.wait_ticks(2)
local world = TheApp.world
local h = world:newEntity("Humanoid", 2, 1)
h:setType("Standard Male Patient")
h.going_home = true -- sin efectos diarios (temperatura, iconos de humor)
for capa, id in pairs(capas) do h:setLayer(capa, id) end
h:setTile(x1 + 1, y1 + 1)
local f = assert(io.open(th64.out(th64.opts.nombre or ("caminata_nivel%d.tsv"):format(nivel)), "w"))
f:write(("# nivel %d ruta %d,%d;%d,%d capas 0=%d 1=%d 2=%d\n"):format(nivel, x1, y1, x2, y2, capas[0], capas[1],
  capas[2]))
f:write("tick\tx\ty\tpx\tpy\tanimacion\tfotograma\tbanderas\n")
local k = 0
local function volcar()
  local px, py = h.th:getPosition()
  f:write(("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n"):format(k, h.tile_x - 1, h.tile_y - 1, px, py, h.th:getAnimation(),
    h.th:getFrame(), h.th:getFlag()))
end
local tick_png = tonumber(th64.opts["tick-png"] or "-1")
local ui = TheApp.ui
local function captura()
  local ventanas, cursor = ui.windows, ui.simulated_cursor
  ui.windows, ui.simulated_cursor = {}, nil
  local sx, sy = 32 * (h.tile_x - h.tile_y) - TheApp.config.width / 2, 16 * (h.tile_x + h.tile_y - 2) - TheApp.config.height / 2
  ui.screen_offset_x, ui.screen_offset_y = sx, sy
  ui:scrollMap(0, 0)
  return ventanas, cursor
end
h:setNextAction(WalkAction(x2 + 1, y2 + 1))
h:queueAction(IdleAction())
volcar()
local pendiente_png = false
th64.set_sampler(function()
  k = k + 1
  volcar()
  if k == tick_png then pendiente_png = true end
end)
if tick_png >= 0 then
  th64.run_hours(tick_png, 1)
  -- Pausa para que el dibujo muestre el estado de ese tick.
  world:setSpeed("Pause")
  th64.autorun_speed = nil
  TheApp.video:setBlueFilterActive(false) -- la pausa tiñe la pantalla de azul
  local ventanas, cursor = captura()
  th64.wait_ticks(3)
  local ok, err = TheApp.video:takeScreenshot(th64.opts.png)
  assert(ok, err)
  th64.log("captura %s en el tick %d: desplazamiento (%d, %d)", th64.opts.png, k, ui.screen_offset_x, ui.screen_offset_y)
  local cf = assert(io.open(th64.opts.png .. ".txt", "w"))
  cf:write(("%d\t%d\t%d\n"):format(k, ui.screen_offset_x, ui.screen_offset_y))
  cf:close()
  ui.windows, ui.simulated_cursor = ventanas, cursor
else
  th64.run_hours(ticks, 1)
end
th64.set_sampler(nil)
f:close()
th64.log("caminata del nivel %d de (%d,%d) a (%d,%d): %d ticks", nivel, x1, y1, x2, y2, k)
th64.quit(0)
