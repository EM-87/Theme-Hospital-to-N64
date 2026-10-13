-- Partida mínima para el oráculo del hito 4: nivel 1, un mostrador con
-- recepcionista, una consulta de GP con su médico y una farmacia con su
-- enfermera. Se fija la semilla justo después de cargar el nivel, se construye
-- con el constructor del arnés (bench/lib/builder.lua, validado por la UI) y se
-- guarda como hito4.sav en el directorio de partidas (CORSIXTH_SAVES).
-- Opción: --th64-seed=<n> (por defecto 1).
local th64 = th64
local B = dofile(th64.bench_dir .. "/lib/builder.lua")
local seed = tonumber(th64.opts.seed or "1")
assert(TheApp:loadLevel(1, "full"), "no se pudo cargar el nivel 1")
math.randomseed(seed)
th64.wait_ticks(3)
local world, h = TheApp.world, TheApp.world.hospitals[1]
world.endconditions.win_goals = {}
world.endconditions.lose_goals = {}
B.reset()
local ex, ey = B.entrance()
assert(B.place_corridor_objects({reception_desk = 1, bench = tonumber(th64.opts.benches or "4")}, ex, ey, 10), "sin sitio para recepción")
local built = {}
for _, spec in ipairs({{"gp", 4, 4}, {"pharmacy", 4, 4}}) do
  local room, why
  for _ = 1, 6 do
    room, why = B.build_room(spec[1], spec[2], spec[3], {})
    if room or why == "sin sitio" then break end
  end
  th64.log("%s: %s", spec[1], room and "construida" or tostring(why))
  if room then built[#built + 1] = spec[1] end
end
h.policies.grant_wage_increase = true
B.hire("Receptionist", 1)
B.hire("Doctor", 1)
B.hire("Nurse", 1)
h:open()
-- --th64-roujin=1: truco "Roujin's Challenge" (+40 pacientes al mes), para que
-- el hospital mínimo tenga tráfico suficiente para comparar.
if th64.opts.roujin == "1" then h.hosp_cheats:roujinOn() end
for _, w in ipairs({table.unpack(TheApp.ui.windows)}) do
  if class.is(w, UIWatch) or class.is(w, UIInformation) then w:close() end
end
th64.wait_ticks(2)
local file = TheApp.savegame_dir .. (th64.opts.name or "hito4") .. ".sav"
TheApp:save(file)
th64.write_json("resumen_hito4.json", {built = built, date = world.game_date:tostring(),
  entities = th64.count_entities(world), balance = h.balance, seed = seed})
th64.log("guardada %s; %s", file, table.concat(built, ", "))
th64.quit(0)
