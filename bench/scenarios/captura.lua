-- Captura de pantalla de una partida cargada con --load=<fichero>, centrada en
-- la entrada del hospital. Opción --th64-png=<ruta>.
local th64 = th64
local B = dofile(th64.bench_dir .. "/lib/builder.lua")
th64.autorun_speed = "Normal"
th64.wait_ticks(5)
local ex, ey = B.entrance()
local ox, oy = tonumber(th64.opts.dx or "0"), tonumber(th64.opts.dy or "0")
local x, y = TheApp.map:WorldToScreen(ex + ox, ey + oy)
TheApp.ui:scrollMapTo(x, y)
th64.wait_ticks(60)
local ok, err = TheApp.video:takeScreenshot(th64.opts.png)
th64.log("captura %s: %s", th64.opts.png, tostring(ok or err))
th64.quit(0)
