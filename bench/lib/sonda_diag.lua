-- La sonda del diagnóstico de la demo (--th64-sonda de crear_demo.lua): lo
-- mismo que TH64_SONDA_DIAG en th64sim partida (motor/platform/pc/hito4.c),
-- para comparar paso a paso las entradas de la fórmula del diagnóstico
-- (Patient:completeDiagnosticStep, patient.lua:197-226) y el camino del fax
-- de «sin sala de diagnóstico». Escribe en el directorio de la traza:
--   diagnostico.tsv  un paso de diagnóstico por línea: hora, paciente (el id
--                    de pacientes.tsv), enfermedad, MaxDiagDiff, si hay médico
--                    en la sala, su habilidad, atención y cansancio, los dos
--                    números aleatorios (r5, el factor 1..5 del consultor, 0 si
--                    no se sacó; div, el divisor 1..3) y el progreso antes y
--                    después;
--   faxes.tsv        cada fax de un paciente (makeNoDiagnosisRoomFax y
--                    makeNoTreatmentRoomFax) y su respuesta (UIFax:choice), con
--                    el progreso y si la enfermedad está descubierta;
--   descubre.tsv     las enfermedades descubiertas al empezar (hora −1) y las
--                    que descubre ResearchDepartment:discoverDisease;
--   medico_hora.tsv  el médico al acabar cada hora (habilidad, atención,
--                    cansancio, felicidad, sala y acción).
-- No gasta números aleatorios ni cambia nada: envuelve las funciones y, solo
-- mientras corre completeDiagnosticStep, math.random con una función que
-- llama a la original con los mismos argumentos y apunta lo que devuelve.
local th64 = th64
local S = {}

--! Instala la sonda. id_de(p): el id del paciente en la traza; hora(): la
--! hora de juego.
function S.nueva(world, h, hora, id_de)
  local sd = {}
  local function abrir(nombre, cabecera)
    local f = assert(io.open(th64.out(nombre), "w"))
    f:write(cabecera, "\n")
    return f
  end
  local f_diag = abrir("diagnostico.tsv",
    "hora\tpaciente\tenfermedad\tmaxdiag\tmedico\thabilidad\tatencion\tcansancio\tr5\tdiv\tantes\tdespues")
  local f_fax = abrir("faxes.tsv", "hora\tpaciente\tenfermedad\tsuceso\tprogreso\tdescubierta\tdetalle")
  local f_desc = abrir("descubre.tsv", "hora\tenfermedad")
  local f_med = abrir("medico_hora.tsv", "hora\thabilidad\tatencion\tcansancio\tfelicidad\tsala\taccion")
  local function descubierta(p)
    local c = h.disease_casebook[p.disease.id]
    return c and c.discovered and 1 or 0
  end

  -- El paso de diagnóstico, con los números aleatorios que saca.
  local orig_paso = Patient.completeDiagnosticStep
  Patient.completeDiagnosticStep = function(self, room)
    local antes = self.diagnosis_progress
    local medico = room.staff_member
    local hab, aten, cans = 0, 0, 0
    if medico then
      hab, aten, cans = medico.profile.skill, medico.profile.attention_to_detail, medico:getAttribute("fatigue")
    end
    local maxdiag = self.world.map.level_config.expertise[self.disease.expertise_id].MaxDiagDiff
    local random = math.random
    local r5, div, otros = 0, 0, 0
    math.random = function(...)
      local r = random(...)
      local a, b = ...
      if a == 1 and b == 5 then r5 = r elseif a == 1 and b == 3 then div = r else otros = otros + 1 end
      return r
    end
    local ok, err = pcall(orig_paso, self, room)
    math.random = random
    if not ok then error(err, 0) end
    assert(otros == 0, "completeDiagnosticStep sacó otros números aleatorios")
    f_diag:write(("%d\t%d\t%s\t%d\t%d\t%.6f\t%.6f\t%.6f\t%d\t%d\t%.6f\t%.6f\n"):format(hora(), id_de(self),
      self.disease.id, maxdiag, medico and 1 or 0, hab, aten, cans, r5, div, antes, self.diagnosis_progress))
  end

  -- Los faxes de los pacientes y sus respuestas.
  local function fax(p, suceso, detalle)
    f_fax:write(("%d\t%d\t%s\t%s\t%.6f\t%d\t%s\n"):format(hora(), id_de(p), p.disease.id, suceso,
      p.diagnosis_progress or 0, descubierta(p), detalle))
  end
  for nombre, detalle in pairs({makeNoDiagnosisRoomFax = "sin_diagnostico", makeNoTreatmentRoomFax = "sin_tratamiento"}) do
    local orig = PlayerHospital[nombre]
    PlayerHospital[nombre] = function(self, p, ...)
      fax(p, "fax", detalle)
      return orig(self, p, ...)
    end
  end
  local orig_choice = UIFax.choice
  UIFax.choice = function(self, n, ...)
    local p = self.owner
    if p and class.is(p, Patient) then
      local c = self.message.choices and self.message.choices[n]
      fax(p, "respuesta", c and c.choice or "disabled")
    end
    return orig_choice(self, n, ...)
  end

  -- Las enfermedades descubiertas.
  local function desc(t, id) f_desc:write(("%d\t%s\n"):format(t, id)) end
  local ids = {}
  for id, c in pairs(h.disease_casebook) do
    if c.discovered and not c.pseudo then ids[#ids + 1] = id end
  end
  table.sort(ids)
  for _, id in ipairs(ids) do desc(-1, id) end
  local orig_desc = ResearchDepartment.discoverDisease
  ResearchDepartment.discoverDisease = function(self, disease, ...)
    desc(hora(), disease.id)
    return orig_desc(self, disease, ...)
  end

  --! Tras cada hora: el médico (el primero que haya).
  function sd.muestra()
    for _, e in ipairs(world.entities) do
      if class.is(e, Doctor) then
        local a = e.action_queue[1]
        local room = e:getRoom()
        f_med:write(("%d\t%.6f\t%.6f\t%.6f\t%.6f\t%s\t%s\n"):format(hora(), e.profile.skill,
          e.profile.attention_to_detail, e:getAttribute("fatigue") or 0, e:getAttribute("happiness") or 0,
          room and room.room_info.id or "-", a and a.name or "-"))
        break
      end
    end
  end

  function sd.terminar()
    f_diag:close()
    f_fax:close()
    f_desc:close()
    f_med:close()
  end
  return sd
end

return S
