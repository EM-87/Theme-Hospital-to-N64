-- Captura de un escenario guardado (fase 5, hito 4), para comparar el dibujo
-- de los objetos del motor con el de CorsixTH: carga la partida, la pausa,
-- quita a las personas del mapa si se pide y hace una captura de 640 × 480
-- (sin ventanas ni cursor) centrada en una casilla.
-- Opciones: --load=partida.sav (obligatoria), --th64-png=ruta (obligatoria),
--           --th64-centro="x,y" (casilla, desde 0; por defecto 62,74),
--           --th64-personas=0|1 (por defecto 0)
-- Escribe también ruta.txt con «0 desplazamiento_x desplazamiento_y».
local th64 = th64
local world = assert(TheApp.world, "falta --load=")
local png = assert(th64.opts.png, "falta --th64-png=")
local cx, cy = (th64.opts.centro or "62,74"):match("(%d+),(%d+)")
cx, cy = tonumber(cx) + 1, tonumber(cy) + 1
world:setSpeed("Pause")
th64.autorun_speed = nil
TheApp.video:setBlueFilterActive(false) -- la pausa tiñe la pantalla de azul
if th64.opts.personas ~= "1" then
  for _, e in ipairs(world.entities) do
    if class.is(e, Humanoid) then e.th:setTile(nil) end
  end
end
local ui = TheApp.ui
ui.windows, ui.simulated_cursor = {}, nil
ui.screen_offset_x = 32 * (cx - cy) - TheApp.config.width / 2
ui.screen_offset_y = 16 * (cx + cy - 2) - TheApp.config.height / 2
ui:scrollMap(0, 0)
th64.wait_ticks(3)
local ok, err = TheApp.video:takeScreenshot(png)
assert(ok, err)
local f = assert(io.open(png .. ".txt", "w"))
f:write(("0\t%d\t%d\n"):format(ui.screen_offset_x, ui.screen_offset_y))
f:close()
th64.log("captura %s centrada en (%d,%d): desplazamiento (%d, %d)", png, cx - 1, cy - 1, ui.screen_offset_x,
  ui.screen_offset_y)
th64.quit(0)
