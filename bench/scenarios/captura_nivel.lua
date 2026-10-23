-- Oráculo de la vista del mapa (hito 2 de la fase 5): carga un nivel de la
-- campaña, oculta las ventanas de la interfaz y hace capturas del mapa en
-- varias posiciones de la cámara. Para cada una apunta en capturas.txt el
-- fichero y el desplazamiento de pantalla con que CorsixTH dibujó el mapa
-- (GameUI.screen_offset_x/y), que es lo que necesita th64sim vista para
-- dibujar lo mismo.
-- Opciones: --th64-nivel=N (por defecto 1)
--           --th64-casillas="x,y;x,y;..." casillas (desde 0, como el motor)
--           en el centro de cada captura; «i» es la cámara inicial del nivel
--           (por defecto "i"). CorsixTH limita la cámara al rombo del mapa.
local th64 = th64
local nivel = tonumber(th64.opts.nivel or "1")
assert(TheApp:loadLevel(nivel, "full"), "no se pudo cargar el nivel " .. nivel)
th64.wait_ticks(2)
local ui = TheApp.ui
assert(ui.zoom_factor == 1, "zoom distinto de 1")
-- Sin ventanas (panel inferior, reloj, faxes): solo queda el mapa.
local ventanas, cursor = ui.windows, ui.simulated_cursor
ui.windows = {}
ui.simulated_cursor = nil -- el cursor que CorsixTH pinta en Xvfb
local x0, y0 = ui.screen_offset_x, ui.screen_offset_y
local lista = assert(io.open(th64.out("capturas.txt"), "w"))
local n = 0
for pos in (th64.opts.casillas or "i"):gmatch("[^;]+") do
  local cx, cy = pos:match("^(%d+),(%d+)$")
  if cx then
    -- Centro del rombo de la casilla (x, y) en el centro de la pantalla.
    cx, cy = tonumber(cx), tonumber(cy)
    ui.screen_offset_x = 32 * (cx - cy) - TheApp.config.width / 2
    ui.screen_offset_y = 16 * (cx + cy) + 16 - TheApp.config.height / 2
  else
    ui.screen_offset_x, ui.screen_offset_y = x0, y0
  end
  ui:scrollMap(0, 0)
  th64.wait_ticks(3)
  n = n + 1
  local png = th64.out(("nivel%d_%d.png"):format(nivel, n))
  local ok, err = TheApp.video:takeScreenshot(png)
  assert(ok, err)
  local w, h = TheApp.config.width, TheApp.config.height
  lista:write(("%s\t%d\t%d\t%d\t%d\n"):format(png, ui.screen_offset_x, ui.screen_offset_y, w, h))
  th64.log("captura %s: desplazamiento (%d, %d), %dx%d", png, ui.screen_offset_x, ui.screen_offset_y, w, h)
end
lista:close()
ui.windows, ui.simulated_cursor = ventanas, cursor
th64.quit(0)
