-- Partida de referencia "lleno": nivel 12 (el último de la campaña) con todo
-- el terreno comprado, todo investigado, todas las habitaciones disponibles,
-- reputación máxima y el truco "Roujin's Challenge" (+40 pacientes al mes)
-- para llegar al máximo realista. Al final se provocan una emergencia y una
-- epidemia y se deja correr un día para que estén activas al guardar.
local th64 = th64
local P = dofile(th64.bench_dir .. "/lib/plan.lua")
P.crear{
  level = 12, name = "lleno", buy_all_plots = true, research_all = true,
  rooms = {
    {"gp", 5, 5, n = 6},
    {"general_diag", 5, 5, n = 2},
    {"cardiogram", 5, 5},
    {"scanner", 6, 6},
    {"ultrascan", 5, 5},
    {"blood_machine", 5, 5},
    {"x_ray", 7, 7},
    {"psych", 5, 5, n = 2},
    {"ward", 7, 6, n = 2, extras = {"bed", "bed", "bed", "radiator", "plant"}},
    {"pharmacy", 5, 5, n = 3},
    {"operating_theatre", 7, 7, extras = {"op_sink1", "radiator"}},
    {"inflation", 5, 5},
    {"fracture_clinic", 5, 5},
    {"hair_restoration", 5, 5},
    {"slack_tongue", 5, 5},
    {"electrolysis", 6, 6},
    {"jelly_vat", 5, 5},
    {"dna_fixer", 6, 6},
    {"decontamination", 6, 6},
    {"research", 6, 6},
    {"training", 6, 6, extras = {"lecture_chair", "lecture_chair", "radiator"}},
    {"staff_room", 6, 6, n = 2, extras = {"sofa", "tv", "pool_table", "radiator"}},
    {"toilets", 5, 5, n = 3, extras = {"loo", "sink", "radiator"}},
  },
  corridor = {bench = 30, plant = 12, bin = 12, drinks_machine = 4, extinguisher = 8, radiator = 10},
  receptionists = 4, handymen = 10, extra_doctors = 8, extra_nurses = 4,
  max_reputation = true, roujin = true,
  -- En la prueba previa el número de pacientes se estabilizó en 310-328 a
  -- partir del mes 9; se guarda al llegar a la meseta.
  target_patients = 300, max_hours = 50 * 30 * 24,
  after_target = function(h)
    -- El truco elige una enfermedad al azar; se repite hasta que sea una que
    -- el hospital puede tratar.
    local ok, msg
    for i = 1, 50 do
      ok, msg = h.hosp_cheats:cheatEmergency()
      if ok ~= false then th64.log("emergencia creada (intento %d)", i) break end
    end
    if ok == false then th64.log("emergencia NO creada: %s", tostring(msg)) end
    h.hosp_cheats:cheatEpidemic()
    -- Se deja correr hasta que la epidemia esté en marcha (máx. 15 días).
    for _ = 1, 15 do
      th64.run_hours(50, 10)
      if h.epidemic or #h.future_epidemics_pool > 0 then break end
    end
    th64.log("epidemia activa: %s; emergencia activa: %s",
      tostring(h.epidemic ~= nil or #h.future_epidemics_pool > 0), tostring(h.emergency ~= nil))
  end,
}
th64.quit(0)
