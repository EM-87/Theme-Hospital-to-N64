-- Partida de referencia "inicio": nivel 1 recién cargado, antes de construir
-- nada. Es el estado mínimo de una partida (mapa, hospital vacío, IU).
local th64 = th64
assert(TheApp:loadLevel(1, "full"), "no se pudo cargar el nivel 1")
th64.wait_ticks(3)
local world = TheApp.world
world.endconditions.win_goals = {}
world.endconditions.lose_goals = {}
local file = TheApp.savegame_dir .. "inicio.sav"
TheApp:save(file)
th64.log("guardada %s", file)
th64.write_json("resumen_inicio.json", {
  name = "inicio", level = 1, date = world.game_date:tostring(),
  entities = th64.count_entities(world), lua = _VERSION,
})
th64.quit(0)
