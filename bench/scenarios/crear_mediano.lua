-- Partida de referencia "mediano": nivel 5 con unos 50 pacientes a la vez.
-- Un hospital de mitad de campaña (con el terreno del nivel comprado):
-- diagnóstico básico, tratamientos
-- disponibles en el nivel, sala de personal, aseos y bancos.
local P = dofile(th64.bench_dir .. "/lib/plan.lua")
P.crear{
  level = 5, name = "mediano", buy_all_plots = true,
  rooms = {
    {"gp", 5, 5, n = 2},
    {"general_diag", 5, 5},
    {"pharmacy", 5, 5},
    {"psych", 5, 5},
    {"ward", 6, 6, extras = {"bed", "radiator", "plant"}},
    {"inflation", 5, 5},
    {"staff_room", 5, 5, extras = {"sofa", "tv", "radiator"}},
    {"toilets", 4, 5, extras = {"loo", "sink", "radiator"}},
  },
  corridor = {bench = 8, plant = 3, bin = 4, drinks_machine = 1, extinguisher = 2},
  receptionists = 1, handymen = 4, extra_doctors = 1,
  max_reputation = true,
  target_patients = 50, max_hours = 50 * 30 * 36,
}
th64.quit(0)
