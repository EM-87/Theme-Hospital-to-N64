-- Escenario del hito 4 (fase 5): nivel 1 con un mostrador de recepción y su
-- recepcionista, unos bancos y una consulta de GP con su médico. No hay más
-- salas: tras la consulta, los pacientes esperan una sala que no existe o se
-- van. Se construye con el constructor del arnés (bench/lib/builder.lua, que
-- usa las mismas ventanas y validaciones que la interfaz) y se guarda en el
-- directorio de partidas (CORSIXTH_SAVES).
--
-- Dos ajustes sobre el nivel 1:
--   * el ritmo de llegadas se fija: world.spawn_rate = --th64-ritmo, sin
--     aumento mensual. Llegan ritmo × 0,25 pacientes al mes (0,25 es la
--     población de CorsixTH con reputación 500). Una consulta atiende unos 6
--     al mes (cada visita dura unas 220 horas), así que por defecto llegan 6;
--   * las enfermedades del nivel se dan por descubiertas, como si el GP ya
--     las conociera. Si no, el fax de «no hay más salas de diagnóstico» no
--     deja arriesgar un diagnóstico y nadie llega a diagnosticarse.
-- Opciones: --th64-seed=N (por defecto 1), --th64-ritmo=R (por defecto 24),
--           --th64-bancos=B (por defecto 6), --th64-descubiertas=0|1 (1),
--           --th64-nombre=fichero (hito4)
local th64 = th64
local B = dofile(th64.bench_dir .. "/lib/builder.lua")
local o = th64.opts
local seed = tonumber(o.seed or "1")
local ritmo = tonumber(o.ritmo or "24")
assert(TheApp:loadLevel(1, "full"), "no se pudo cargar el nivel 1")
math.randomseed(seed)
th64.wait_ticks(3)
local world, h = TheApp.world, TheApp.world.hospitals[1]
world.endconditions.win_goals = {}
world.endconditions.lose_goals = {}
-- Sin emergencias, VIP, terremotos ni epidemias: no son del hito 4.
world.next_emergency_month, world.next_emergency_date, world.next_emergency = 0, nil, nil
world.next_vip_date = Date(99, 1, 1)
world.earthquake.disabled = true
h.epidemics_disabled = true
B.reset()
local ex, ey = B.entrance()
assert(B.place_corridor_objects({reception_desk = 1, bench = tonumber(o.bancos or "6")}, ex, ey, 10),
  "sin sitio para la recepción")
local gp, why
for _ = 1, 6 do
  gp, why = B.build_room("gp", 4, 4, {})
  if gp or why == "sin sitio" then break end
end
assert(gp, "no se pudo construir la consulta: " .. tostring(why))
B.hire("Receptionist", 1)
B.hire("Doctor", 1)
h:open()
if o.descubiertas ~= "0" then
  for id, c in pairs(h.disease_casebook) do
    if not c.pseudo and not c.discovered then
      c.discovered = true
      h.discovered_diseases[#h.discovered_diseases + 1] = id
    end
  end
end
world.map.level_config.popn = {[0] = {Change = 0, Month = 0}}
world.spawn_rate = ritmo
world.monthly_spawn_increase = 0
world:updateSpawnDates()
for _, w in ipairs({table.unpack(TheApp.ui.windows)}) do
  if class.is(w, UIWatch) or class.is(w, UIInformation) then w:close() end
end
th64.wait_ticks(2)
local nombre = (o.nombre or "hito4") .. ".sav"
TheApp:save(TheApp.savegame_dir .. nombre)
th64.write_json("resumen_hito4.json", {fecha = world.game_date:tostring(), saldo = h.balance, seed = seed,
  ritmo = ritmo, entidades = th64.count_entities(world), consulta = {x = gp.x, y = gp.y, w = gp.width,
  h = gp.height}})
th64.log("guardada %s: consulta en %d,%d; saldo %d", nombre, gp.x, gp.y, h.balance)
th64.quit(0)
