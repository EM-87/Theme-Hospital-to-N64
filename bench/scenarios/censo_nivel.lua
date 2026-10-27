-- Censo de un nivel al empezar (fase 6): las salas que se pueden construir,
-- los objetos del nivel, los candidatos de cada tipo de personal, las
-- enfermedades y los objetivos para ganar y perder. Escribe nivel<N>.json.
-- Opciones: --th64-nivel=N (por defecto 1)
local th64 = th64
local nivel = tonumber(th64.opts.nivel or "1")
assert(TheApp:loadLevel(nivel, "full"), "no se pudo cargar el nivel " .. nivel)
th64.wait_ticks(3)
local world, h = TheApp.world, TheApp.world.hospitals[1]
local salas = {}
for _, r in ipairs(world:getAvailableRooms()) do
  salas[#salas + 1] = {id = r.room.id, descubierta = r.is_discovered, coste = r.build_cost,
    categorias = r.room.categories, objetos = r.room.objects_needed}
end
local objetos = {}
local cfg = world.map.level_config.objects
for _, o in ipairs(TheApp.objects) do
  if o.thob and cfg[o.thob] and cfg[o.thob].AvailableForLevel == 1 then
    objetos[#objetos + 1] = {id = o.id, de_pasillo = o.corridor_object and true or false,
      al_empezar = cfg[o.thob].StartAvail == 1, coste = cfg[o.thob].StartCost, mes = cfg[o.thob].WhenAvail}
  end
end
local personal = {}
for class, group in pairs(world.available_staff) do personal[class] = #group end
local enf = {}
for _, d in ipairs(world.available_diseases) do
  enf[#enf + 1] = {id = d.id, visual = d.visuals_id and true or false, descubierta = h.disease_casebook[d.id] and h.disease_casebook[d.id].discovered or false, tratamiento = d.treatment_rooms}
end
th64.write_json("nivel" .. nivel .. ".json", {saldo = h.balance, fecha = world.game_date:tostring(), salas = salas,
  objetos = objetos, personal = personal, enfermedades = enf,
  ganar = world.endconditions.win_goals, perder = world.endconditions.lose_goals})
th64.quit(0)
