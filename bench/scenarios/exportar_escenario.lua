-- Exporta una partida guardada de CorsixTH para que el motor la cargue (fase
-- 5, hito 4): lo que el jugador ha construido y el estado de la simulación
-- que el motor necesita para seguir desde ahí. Escribe celdas.tsv y
-- escenario.json; el formato está en bench/lib/escenario.lua, que también usa
-- la batería de construir (bench/scenarios/construir.lua).
-- Coordenadas desde 0, como en el motor.
-- Opción: --load=partida.sav (obligatoria)
local th64 = th64
local escenario = dofile(th64.bench_dir .. "/lib/escenario.lua")
local world = assert(TheApp.world, "falta --load=")
local resumen = escenario.exportar(world, th64.out("celdas.tsv"), th64.out("escenario.json"))
th64.log("escenario exportado: %s", resumen)
th64.quit(0)
