-- Sonda del hospital (fase 5, antes del hito 4): cada --th64-paso horas de
-- juego, escribe la fecha, el saldo, los pacientes (cuántos y qué acción hace
-- cada uno), el personal (dónde está y qué hace) y las colas de cada
-- habitación y de la recepción. Sirve para ver dónde se atasca un escenario.
-- Opciones: --load=partida.sav (obligatoria), --th64-pasos=N (por defecto 8),
--           --th64-paso=H (por defecto 50), --th64-detalle=1 (una línea por
--           paciente)
local th64 = th64
local o = th64.opts
th64.skip_frames = true
local world = assert(TheApp.world, "falta --load=")
local h = world.hospitals[1]
local ids, n = setmetatable({}, {__mode = "k"}), 0
local function id(e)
  if not ids[e] then n = n + 1; ids[e] = n end
  return ids[e]
end
local function act(e)
  local a = e.action_queue and e.action_queue[1]
  return a and a.name or "-"
end
local function sitio(e)
  local r = e:getRoom()
  return r and r.room_info.id or (e.tile_x and ("%d,%d"):format(e.tile_x, e.tile_y) or "fuera")
end
local function volcar()
  local pacientes, acciones = 0, {}
  local lineas = {}
  for _, e in ipairs(world.entities) do
    if class.is(e, Patient) then
      pacientes = pacientes + 1
      local k = act(e) .. "@" .. (e:getRoom() and e:getRoom().room_info.id or "pasillo")
      acciones[k] = (acciones[k] or 0) + 1
      if o.detalle == "1" then
        lineas[#lineas + 1] = ("  p%d %s en %s, enfermedad %s, diagnosticado %s, progreso %.2f, %s"):format(id(e),
          act(e), sitio(e), e.disease and e.disease.id or "-", tostring(e.diagnosed), e.diagnosis_progress or -1,
          e.going_home and "se va" or "")
      end
    end
  end
  local resumen = {}
  for k, v in pairs(acciones) do resumen[#resumen + 1] = ("%s=%d"):format(k, v) end
  table.sort(resumen)
  local personal = {}
  for _, s in ipairs(h.staff) do
    personal[#personal + 1] = ("%s#%d:%s@%s"):format(s.humanoid_class, id(s), act(s), sitio(s))
  end
  local colas = {}
  for _, r in pairs(world.rooms) do
    if r and r.door and r.door.queue then
      colas[#colas + 1] = ("%s(%s):%d"):format(r.room_info.id, r.is_active and "activa" or "inactiva",
        r.door.queue:size())
    end
  end
  for _, e in ipairs(world.entities) do
    if class.is(e, Object) and e.object_type.id == "reception_desk" and e.queue then
      colas[#colas + 1] = ("recepcion:%d%s"):format(e.queue:size(), e.receptionist and "" or "(sin recepcionista)")
    end
  end
  table.sort(colas)
  th64.log("h%d %s saldo %d, %d pacientes {%s}; personal {%s}; colas {%s}", th64.hours_done(),
    world.game_date:tostring(), h.balance, pacientes, table.concat(resumen, " "), table.concat(personal, " "),
    table.concat(colas, " "))
  for _, l in ipairs(lineas) do th64.log("%s", l) end
end
th64.autorun_speed = "Normal"
for _ = 1, tonumber(o.pasos or "8") do
  volcar()
  th64.run_hours(tonumber(o.paso or "50"), 30)
end
volcar()
th64.quit(0)
