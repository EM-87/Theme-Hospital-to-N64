-- La traza de una partida en marcha, como bench/scenarios/traza_hito4.lua,
-- pero como biblioteca: para un guion que, además de trazar, hace cosas
-- mientras la partida corre (la campaña de la demo, crear_demo.lua, aplica
-- las órdenes del jugador en su tick). Los ficheros tienen el formato de
-- traza_hito4.lua y de th64sim hito4 / partida (motor/platform/pc/hito4.c),
-- para que tools/oraculo_hito4/resumen.py y comparar.py sirvan:
--   pacientes.tsv  una línea por paciente: horas de cada paso (-1 = no llegó)
--   dinero.tsv     cada movimiento de dinero: hora, importe (+/-), motivo
--   dias.tsv       cada 50 horas: saldo y cuántos pacientes hay en cada fase
--   acciones.tsv   horas-paciente pasadas en cada acción, por fase
-- y uno más, que traza_hito4.lua no tiene:
--   final.tsv      clave y valor: saldo, valor (Hospital.value), reputacion,
--                  vomitos y orinadas (las veces que Patient:vomit y
--                  Patient:pee encolan su acción, patient.lua:447-475) y
--                  basura (los objetos litter que quedan en el mapa); las
--                  mismas claves que escribe el motor (hito4.c,
--                  traza_terminar).
--
-- Lo que cambia frente a traza_hito4.lua:
--   * la hora es la del guion (hora(), las horas de juego desde el principio
--     de la partida), no th64.hours_done;
--   * el mostrador se busca en cada muestra (la demo lo compra en marcha);
--     sin mostrador, nadie está en su cola;
--   * se cuentan los vómitos, las orinadas y la basura.
-- No gasta números aleatorios: envuelve World:spawnPatient, Patient:setDiagnosed,
-- Patient:goHome, Patient:despawn, Patient:vomit, Patient:pee,
-- Hospital:humanoidDeath, Hospital:receiveMoney, Hospital:spendMoney,
-- Hospital:addInsuranceMoney y Hospital:receiveMoneyForTreatment, y mira el
-- estado de cada paciente tras cada hora.
--
-- Los faxes: el jugador abre cada uno en cuanto llega
-- (UIBottomPanel:openFirstMessage) y th64.lua lo contesta (answer_window,
-- UIFax:choice) con las preferencias de th64.fax_choices; por defecto,
-- arriesgar un diagnóstico si se puede y, si no, mandar a casa, como el
-- núcleo (TH_ANSWER_GUESS_THEN_SEND_HOME); y siempre, al final, «close»
-- (el fax de la enfermedad descubierta, abajo).
local th64 = th64
local T = {}

--! Instala los ganchos y devuelve la traza. hora: función sin argumentos que
--! da la hora de juego. faxes: la lista de preferencias separada por comas.
function T.nueva(world, h, hora, faxes)
  local tr = {}
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
  th64.fax_choices = {}
  local con_close = false
  for c in (faxes or "guess_cure,send_home"):gmatch("[^,]+") do
    th64.fax_choices[#th64.fax_choices + 1] = c
    con_close = con_close or c == "close"
  end
  -- El fax de la enfermedad descubierta (PlayerHospital:adviseDiscoverDisease,
  -- player_hospital.lua:434-446; lo manda ResearchDepartment:discoverDisease,
  -- research_department.lua:603-607, desde GPRoom:dealtWithPatient al
  -- diagnosticar una enfermedad sin descubrir, gp.lua:150-153) solo tiene la
  -- elección «close». Si el jugador lo cierra sin elegir (UIFax:close), el
  -- mensaje sigue el primero del panel y tapa a los demás hasta que caduca
  -- (25 × 24 horas): el fax del paciente sin sala de tratamiento, que va
  -- detrás, no se contesta y el paciente espera. Se pulsa «close», al final de
  -- las preferencias. traza_hito4.lua no lo necesita: el hito 4 da todas las
  -- enfermedades por descubiertas. El núcleo no tiene este fax (descubre la
  -- enfermedad sin avisar, dealt_with_patient).
  if not con_close then th64.fax_choices[#th64.fax_choices + 1] = "close" end
  tr.faxes = 0
  tr.vomitos, tr.orinadas = 0, 0
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
  -- Patient:vomit solo vomita si has_vomitted era 0 y la casilla está
  -- vacía (patient.lua:447-458): ha vomitado si has_vomitted cambia.
  -- Patient:pee no deja marca (460-475): ha orinado si la acción de arriba
  -- de la cola es ahora un PeeAction (queueAction(PeeAction(), 1) e
  -- interruptAndRequeueAction la deja delante).
  do
    local vomit = Patient.vomit
    Patient.vomit = function(self, ...)
      local antes = self.has_vomitted
      local r = table.pack(vomit(self, ...))
      if self.has_vomitted ~= antes then tr.vomitos = tr.vomitos + 1 end
      return table.unpack(r, 1, r.n)
    end
    local pee = Patient.pee
    Patient.pee = function(self, ...)
      local antes = #self.action_queue
      local r = table.pack(pee(self, ...))
      if #self.action_queue > antes then tr.orinadas = tr.orinadas + 1 end
      return table.unpack(r, 1, r.n)
    end
  end
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
  local cerrado = false
  wrap(Hospital, "receiveMoney", function(_, _, amount, reason)
    if cerrado then return end
    dinero:write(("%d\t%d\t%s\t%d\n"):format(hora(), amount, tostring(reason), pagando and nuevo(pagando).id or 0))
  end)
  wrap(Hospital, "spendMoney", function(_, _, amount, reason)
    if cerrado then return end
    dinero:write(("%d\t%d\t%s\t0\n"):format(hora(), -amount, tostring(reason)))
  end)
  wrap(Hospital, "addInsuranceMoney", function(_, _, company, amount)
    if cerrado then return end
    dinero:write(("%d\t%d\tseguro %d (pendiente)\t%d\n"):format(hora(), amount, company,
      pagando and nuevo(pagando).id or 0))
  end)

  local function mostrador()
    for _, e in ipairs(world.entities) do
      if class.is(e, Object) and e.object_type.id == "reception_desk" then return e end
    end
  end

  local function fase(r)
    if r.a_casa >= 0 then return "a_casa" end
    if r.diagnostico >= 0 then return "diagnosticado" end
    if r.entra_gp >= 0 then return "consulta" end
    if r.recepcion >= 0 then return "tras_recepcion" end
    if r.cola_recepcion >= 0 then return "cola_recepcion" end
    return "llegando"
  end
  local acciones = {}

  --! Tras cada hora: abre el fax que haya y mira a cada paciente.
  function tr.muestra()
    local t = hora()
    local panel = TheApp.ui.bottom_panel
    if panel and #panel.message_windows > 0 then
      tr.faxes = tr.faxes + 1
      panel:openFirstMessage()
    end
    local desk = mostrador()
    for _, e in ipairs(world.entities) do
      if class.is(e, Patient) then
        local r = nuevo(e)
        local a = e.action_queue[1]
        if r.cola_recepcion < 0 and desk then
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
        local k = fase(r) .. "\t" .. (a and a.name or "-")
        acciones[k] = (acciones[k] or 0) + 1
      end
    end
  end

  local dias = assert(io.open(th64.out("dias.tsv"), "w"))
  dias:write("hora\tfecha\tsaldo\tpacientes\tllegando\tcola_recepcion\ttras_recepcion\tconsulta\tdiagnosticado\ta_casa\n")
  --! Una línea de dias.tsv (cada 50 horas, y una al empezar).
  function tr.volcar_dia()
    local n = {llegando = 0, cola_recepcion = 0, tras_recepcion = 0, consulta = 0, diagnosticado = 0, a_casa = 0}
    local total = 0
    for _, e in ipairs(world.entities) do
      if class.is(e, Patient) then
        total = total + 1
        local f = fase(nuevo(e))
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

  --! Cierra la traza: pacientes.tsv, acciones.tsv, final.tsv y la línea del
  --! registro con la huella del generador de números aleatorios.
  function tr.terminar()
    cerrado = true
    dias:close()
    dinero:close()
    local f = assert(io.open(th64.out("pacientes.tsv"), "w"))
    f:write("id\tenfermedad\tseguro\tllegada\tcola_recepcion\trecepcion\tentra_gp\tsale_gp\tvisitas_gp\tprogreso\t" ..
      "diagnostico\tpagos\ta_casa\tmotivo\tmuerte\tdesaparece\n")
    for _, r in ipairs(orden) do
      f:write(("%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%.3f\t%d\t%d\t%d\t%s\t%d\t%d\n"):format(r.id, r.enfermedad,
        r.seguro, r.llegada, r.cola_recepcion, r.recepcion, r.entra_gp, r.sale_gp, r.visitas_gp, r.progreso,
        r.diagnostico, r.pagado, r.a_casa, r.motivo, r.muerte, r.desaparece))
    end
    f:close()
    f = assert(io.open(th64.out("acciones.tsv"), "w"))
    f:write("fase\taccion\thoras_paciente\n")
    local ks = {}
    for k in pairs(acciones) do ks[#ks + 1] = k end
    table.sort(ks)
    for _, k in ipairs(ks) do f:write(("%s\t%d\n"):format(k, acciones[k])) end
    f:close()
    local basura = 0
    for _, e in ipairs(world.entities) do
      if class.is(e, Litter) then basura = basura + 1 end
    end
    f = assert(io.open(th64.out("final.tsv"), "w"))
    f:write(("clave\tvalor\nsaldo\t%d\nvalor\t%d\nreputacion\t%d\nvomitos\t%d\norinadas\t%d\nbasura\t%d\n"):format(
      h.balance, h.value, h.reputation, tr.vomitos, tr.orinadas, basura))
    f:close()
    local decisiones = {}
    for k, v in pairs(th64.decisions) do decisiones[#decisiones + 1] = ("%s=%d"):format(k, v) end
    table.sort(decisiones)
    th64.log("traza: %d pacientes en %d horas; saldo %d; reputación %d; %d vómitos, %d orinadas, %d basuras; " ..
      "faxes abiertos %d {%s}; rng %d", #orden, hora(), h.balance, h.reputation, tr.vomitos, tr.orinadas, basura,
      tr.faxes, table.concat(decisiones, ", "), fnv(math.randomdump()))
  end
  return tr
end

return T
