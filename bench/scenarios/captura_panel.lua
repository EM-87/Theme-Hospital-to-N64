-- Oráculo del panel inferior (fase 6, hito 6.1): carga el nivel 1, deja solo
-- el panel inferior y hace capturas de 640 × 480 en varios estados: dinero,
-- fecha y reputación distintos, en pausa o no, con el puntero encima (se ven
-- los botones de gestión), con un botón pulsado y con una descripción.
-- Para cada una escribe una línea en capturas.tsv con lo que necesita
-- th64sim panel para dibujar lo mismo:
--   png  desp_x  desp_y  dinero  día  mes  reputación  pausa  puntero_x  puntero_y  pulsado  descripción
-- (puntero −1 = fuera de la pantalla; pulsado 1 = botón izquierdo abajo;
-- descripción 1 = con la descripción del puntero ya a la vista).
local th64 = th64
assert(TheApp:loadLevel(1, "full"), "no se pudo cargar el nivel 1")
th64.wait_ticks(2)
local ui, world = TheApp.ui, TheApp.world
local h = world.hospitals[1]
world:setSpeed("Pause")
TheApp.video:setBlueFilterActive(false) -- la pausa tiñe la pantalla de azul
local bp = assert(ui.bottom_panel, "no hay panel inferior")
ui.windows = {bp}
ui.simulated_cursor = nil
for _, w in ipairs(bp.windows or {}) do w.visible = false end -- el consejero
local en_pausa = world.isCurrentSpeed
local lista = assert(io.open(th64.out("capturas.txt"), "w"))
local n = 0

local function captura(c)
  h.balance = c.dinero
  h.reputation = c.rep
  world.game_date = Date(1, c.mes, c.dia, 1) -- la hora 0 del 1 de enero reabre la presentación
  -- Sin pausa: el juego sigue parado, pero el panel no lo sabe.
  world.isCurrentSpeed = c.pausa and en_pausa or function() return false end
  -- El puntero: los botones de gestión, el botón pulsado y la descripción.
  local px, py = c.px or -1, c.py or -1
  bp:onMouseMove(px - bp.x, py - bp.y, 0, 0)
  if c.pulsado then bp:onMouseDown("left", px - bp.x, py - bp.y) end
  ui.cursor_x, ui.cursor_y = px, py
  ui.tooltip, ui.tooltip_counter = nil, 30
  if c.descripcion then
    ui.tooltip = ui:getTooltipAt(px, py)
    ui.tooltip_counter = 0
  end
  th64.wait_ticks(2)
  if c.descripcion then ui.tooltip_counter = 0 end
  n = n + 1
  local png = th64.out(("panel_%d.png"):format(n))
  assert(TheApp.video:takeScreenshot(png))
  if c.pulsado then bp:onMouseUp("left", -100, -100) end -- soltar fuera: no hace nada
  lista:write(("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n"):format(png, ui.screen_offset_x,
    ui.screen_offset_y, c.dinero, c.dia, c.mes, c.rep, c.pausa and 1 or 0, px, py, c.pulsado and 1 or 0,
    c.descripcion and 1 or 0))
  th64.log("captura %d: %s", n, png)
end

captura({dinero = 40000, dia = 1, mes = 1, rep = 500, pausa = true})
captura({dinero = 40000, dia = 1, mes = 1, rep = 500})
captura({dinero = -1234, dia = 28, mes = 9, rep = 0})
captura({dinero = 1234567, dia = 31, mes = 12, rep = 1000})
captura({dinero = 987, dia = 15, mes = 4, rep = 333, px = 400, py = 450})
captura({dinero = 40000, dia = 2, mes = 2, rep = 500, px = 225, py = 455, pulsado = true})
captura({dinero = 40000, dia = 2, mes = 2, rep = 500, px = 470, py = 455, pulsado = true})
captura({dinero = 40000, dia = 3, mes = 3, rep = 500, px = 20, py = 450, descripcion = true})
captura({dinero = 40000, dia = 3, mes = 3, rep = 777, px = 90, py = 468, descripcion = true})
captura({dinero = 40000, dia = 3, mes = 3, rep = 500, px = 170, py = 460, descripcion = true})
captura({dinero = 40000, dia = 3, mes = 3, rep = 500, px = 620, py = 455, descripcion = true})
lista:close()
world.isCurrentSpeed = en_pausa
th64.quit(0)
