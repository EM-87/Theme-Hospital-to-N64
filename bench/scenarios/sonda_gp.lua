-- Sonda de la consulta de GP (fase 5, hito 4): tras cada hora de juego anota
-- qué hace el médico (acción, objeto que usa, animación, si va a medio ritmo,
-- su cansancio) y cuándo un paciente se sienta en la silla y cuándo el médico
-- lo despacha (GPRoom:dealtWithPatient). Sirve para comparar el ciclo del
-- médico del motor con el de CorsixTH sin gastar números aleatorios.
-- Escribe medico.tsv (una línea por hora) y visitas.tsv (sentarse y despachar).
-- Abre y contesta los faxes como traza_hito4.lua, para que la partida sea la misma.
-- Opciones: --load=partida.sav (obligatoria), --th64-horas=H (por defecto 9000),
--           --th64-faxes=a,b,... (por defecto guess_cure,send_home)
local th64 = th64
local world = assert(TheApp.world, "falta --load=")
th64.skip_frames = true
th64.fax_choices = {}
for c in (th64.opts.faxes or "guess_cure,send_home"):gmatch("[^,]+") do th64.fax_choices[#th64.fax_choices + 1] = c end
local hora = th64.hours_done
local medico
for _, e in ipairs(world.entities) do
  if class.is(e, Doctor) then medico = e end
end
assert(medico, "no hay médico")
local fm = assert(io.open(th64.out("medico.tsv"), "w"))
fm:write("hora\taccion\tobjeto\tanimacion\tlento\tcansancio\tsala\n")
local fv = assert(io.open(th64.out("visitas.tsv"), "w"))
fv:write("hora\tsuceso\n")
local orig = GPRoom.dealtWithPatient
GPRoom.dealtWithPatient = function(self, patient, ...)
  if (patient or self:getPatient()) ~= self.just_dealt_with then fv:write(("%d\tdespacha\n"):format(hora())) end
  return orig(self, patient, ...)
end
local sentado = {}
th64.set_sampler(function()
  local panel = TheApp.ui.bottom_panel
  if panel and #panel.message_windows > 0 then panel:openFirstMessage() end
  local a = medico.action_queue[1]
  local obj = a and a.object and a.object.object_type.id or "-"
  local room = medico:getRoom()
  fm:write(("%d\t%s\t%s\t%d\t%d\t%.4f\t%s\n"):format(hora(), a and a.name or "-", obj, medico.th:getAnimation(),
    medico.slow_animation and 1 or 0, medico.attributes.fatigue or 0, room and room.room_info.id or "-"))
  for _, e in ipairs(world.entities) do
    if class.is(e, Patient) then
      local pa = e.action_queue[1]
      local en_silla = pa and pa.name == "use_object" and pa.object and pa.object.object_type.id == "chair"
      if en_silla and not sentado[e] then fv:write(("%d\tsienta\n"):format(hora())) end
      sentado[e] = en_silla or nil
    end
  end
end)
th64.autorun_speed = "Normal"
local horas = tonumber(th64.opts.horas or "9000")
for _ = 1, horas // 50 do th64.run_hours(50, 50) end
fm:close()
fv:close()
local decisiones = {}
for k, v in pairs(th64.decisions) do decisiones[#decisiones + 1] = ("%s=%d"):format(k, v) end
table.sort(decisiones)
local function fnv(s)
  local hh = 2166136261
  for i = 1, #s do hh = ((hh ~ s:byte(i)) * 16777619) & 0xffffffff end
  return hh
end
th64.log("sonda_gp: %d horas; saldo %d; {%s}; rng %d", th64.hours_done(), world.hospitals[1].balance,
  table.concat(decisiones, ", "), fnv(math.randomdump()))
th64.quit(0)
