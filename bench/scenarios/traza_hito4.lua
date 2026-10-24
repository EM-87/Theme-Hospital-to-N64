-- Traza del escenario del hito 4 (fase 5): sigue a cada paciente desde que
-- aparece hasta que se va y anota cada movimiento de dinero, para ver qué
-- mecanismos de CorsixTH intervienen y, después, comparar el motor con
-- estadística. No modifica CorsixTH ni gasta números aleatorios:
--   * envuelve World:spawnPatient, Patient:setDiagnosed, Patient:goHome,
--     Patient:despawn, Hospital:humanoidDeath, Hospital:receiveMoney,
--     Hospital:spendMoney y Hospital:addInsuranceMoney;
--   * tras cada hora de juego mira el estado de cada paciente (cola de la
--     recepción, consulta, acción en curso).
-- Escribe en el directorio de salida:
--   pacientes.tsv  una línea por paciente: horas de cada paso (-1 = no llegó)
--   dinero.tsv     cada movimiento de dinero: hora, importe (+/-), motivo
--   dias.tsv       por día: saldo y cuántos pacientes hay en cada fase
--   acciones.tsv   horas-paciente pasadas en cada acción, por fase
-- y al final una línea con la huella del generador de números aleatorios.
--
-- El jugador abre cada fax en cuanto llega (UIBottomPanel:openFirstMessage) y
-- lo responde con la ventana del juego (UIFax:choice) según --th64-faxes, una
-- lista de preferencias. Por defecto, arriesgar un diagnóstico si se puede y,
-- si no, mandar a casa; nunca esperar.
-- Opciones: --load=hito4.sav (obligatoria), --th64-horas=H (por defecto 3000),
--           --th64-faxes=a,b,... (por defecto guess_cure,send_home)
local th64 = th64
local o = th64.opts
th64.skip_frames = true
local world = assert(TheApp.world, "falta --load=")
local h = world.hospitals[1]

local pac, orden = {}, {}
local function nuevo(p)
  local r = pac[p]
  if not r then
    r = {id = #orden + 1, enfermedad = p.disease and p.disease.id or "-", llegada = -1, cola_recepcion = -1,
      recepcion = -1, entra_gp = -1, sale_gp = -1, visitas_gp = 0, diagnostico = -1, a_casa = -1,
      motivo = "-", desaparece = -1, muerte = -1, progreso = 0, pagado = 0, seguro = p.insurance_company and 1 or 0}
    pac[p] = r
    orden[#orden + 1] = r
  end
  return r
end
local function wrap(cls, fname, fn)
  local orig = assert(cls[fname], fname)
  cls[fname] = function(self, ...)
    local r = table.pack(orig(self, ...))
    fn(self, r, ...)
    return table.unpack(r, 1, r.n)
  end
end
local hora = th64.hours_done
th64.fax_choices = {}
for c in (o.faxes or "guess_cure,send_home"):gmatch("[^,]+") do th64.fax_choices[#th64.fax_choices + 1] = c end
local faxes = 0
wrap(World, "spawnPatient", function(_, r)
  if r[1] then nuevo(r[1]).llegada = hora() end
end)
wrap(Patient, "setDiagnosed", function(p) nuevo(p).diagnostico = hora() end)
wrap(Patient, "goHome", function(p, _, reason)
  local r = nuevo(p)
  if r.a_casa < 0 then r.a_casa, r.motivo = hora(), tostring(reason) end
end)
wrap(Patient, "despawn", function(p) nuevo(p).desaparece = hora() end)
wrap(Hospital, "humanoidDeath", function(_, _, p) nuevo(p).muerte = hora() end)
local dinero = assert(io.open(th64.out("dinero.tsv"), "w"))
dinero:write("hora\timporte\tmotivo\tpaciente\n")
local pagando -- paciente cuyo pago se está procesando
do
  local orig = Hospital.receiveMoneyForTreatment
  Hospital.receiveMoneyForTreatment = function(self, p, ...)
    pagando = p
    local r = table.pack(orig(self, p, ...))
    nuevo(p).pagado = nuevo(p).pagado + 1
    pagando = nil
    return table.unpack(r, 1, r.n)
  end
end
wrap(Hospital, "receiveMoney", function(_, _, amount, reason)
  dinero:write(("%d\t%d\t%s\t%d\n"):format(hora(), amount, tostring(reason), pagando and nuevo(pagando).id or 0))
end)
wrap(Hospital, "spendMoney", function(_, _, amount, reason)
  dinero:write(("%d\t%d\t%s\t0\n"):format(hora(), -amount, tostring(reason)))
end)
wrap(Hospital, "addInsuranceMoney", function(_, _, company, amount)
  dinero:write(("%d\t%d\tseguro %d (pendiente)\t%d\n"):format(hora(), amount, company,
    pagando and nuevo(pagando).id or 0))
end)

local desk
for _, e in ipairs(world.entities) do
  if class.is(e, Object) and e.object_type.id == "reception_desk" then desk = e end
end
assert(desk, "no hay mostrador de recepción")

local function fase(p, r)
  if r.a_casa >= 0 then return "a_casa" end
  if r.diagnostico >= 0 then return "diagnosticado" end
  if r.entra_gp >= 0 then return "consulta" end
  if r.recepcion >= 0 then return "tras_recepcion" end
  if r.cola_recepcion >= 0 then return "cola_recepcion" end
  return "llegando"
end
local acciones = {}
th64.set_sampler(function()
  local t = hora()
  local panel = TheApp.ui.bottom_panel
  if panel and #panel.message_windows > 0 then
    faxes = faxes + 1
    panel:openFirstMessage()
  end
  for _, e in ipairs(world.entities) do
    if class.is(e, Patient) then
      local r = nuevo(e)
      local a = e.action_queue[1]
      if r.cola_recepcion < 0 then
        -- El primero de la cola espera con una acción idle delante de la de cola.
        for _, q in ipairs(e.action_queue) do
          if q.name == "queue" and q.queue == desk.queue then r.cola_recepcion = t end
        end
      end
      if r.recepcion < 0 and e.has_passed_reception then r.recepcion = t end
      local room = e:getRoom()
      local en_gp = room and room.room_info.id == "gp"
      if en_gp and not r.dentro then
        r.dentro = true
        r.visitas_gp = r.visitas_gp + 1
        if r.entra_gp < 0 then r.entra_gp = t end
      elseif not en_gp and r.dentro then
        r.dentro = false
        r.sale_gp = t
      end
      r.progreso = e.diagnosis_progress or 0
      local k = fase(e, r) .. "\t" .. (a and a.name or "-")
      acciones[k] = (acciones[k] or 0) + 1
    end
  end
end)

local dias = assert(io.open(th64.out("dias.tsv"), "w"))
dias:write("hora\tfecha\tsaldo\tpacientes\tllegando\tcola_recepcion\ttras_recepcion\tconsulta\tdiagnosticado\ta_casa\n")
local function volcar_dia()
  local n = {llegando = 0, cola_recepcion = 0, tras_recepcion = 0, consulta = 0, diagnosticado = 0, a_casa = 0}
  local total = 0
  for _, e in ipairs(world.entities) do
    if class.is(e, Patient) then
      total = total + 1
      local f = fase(e, nuevo(e))
      n[f] = n[f] + 1
    end
  end
  dias:write(("%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n"):format(hora(), world.game_date:tostring(), h.balance, total,
    n.llegando, n.cola_recepcion, n.tras_recepcion, n.consulta, n.diagnosticado, n.a_casa))
end

local function fnv(s, hh)
  hh = hh or 2166136261
  for i = 1, #s do hh = ((hh ~ s:byte(i)) * 16777619) & 0xffffffff end
  return hh
end
th64.autorun_speed = "Normal"
local horas = tonumber(o.horas or "3000")
volcar_dia()
for _ = 1, horas // 50 do
  th64.run_hours(50, 50)
  volcar_dia()
end
dias:close()
dinero:close()

local f = assert(io.open(th64.out("pacientes.tsv"), "w"))
f:write("id\tenfermedad\tseguro\tllegada\tcola_recepcion\trecepcion\tentra_gp\tsale_gp\tvisitas_gp\tprogreso\t" ..
  "diagnostico\tpagos\ta_casa\tmotivo\tmuerte\tdesaparece\n")
for _, r in ipairs(orden) do
  f:write(("%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%.3f\t%d\t%d\t%d\t%s\t%d\t%d\n"):format(r.id, r.enfermedad, r.seguro,
    r.llegada, r.cola_recepcion, r.recepcion, r.entra_gp, r.sale_gp, r.visitas_gp, r.progreso, r.diagnostico, r.pagado,
    r.a_casa, r.motivo, r.muerte, r.desaparece))
end
f:close()
f = assert(io.open(th64.out("acciones.tsv"), "w"))
f:write("fase\taccion\thoras_paciente\n")
local ks = {}
for k in pairs(acciones) do ks[#ks + 1] = k end
table.sort(ks)
for _, k in ipairs(ks) do f:write(("%s\t%d\n"):format(k, acciones[k])) end
f:close()
local decisiones = {}
for k, v in pairs(th64.decisions) do decisiones[#decisiones + 1] = ("%s=%d"):format(k, v) end
table.sort(decisiones)
th64.log("traza: %d pacientes en %d horas; saldo %d; faxes abiertos %d {%s}; rng %d", #orden, th64.hours_done(),
  h.balance, faxes, table.concat(decisiones, ", "), fnv(math.randomdump()))
th64.quit(0)
