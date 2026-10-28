-- Hospital de prueba de las vistas (fase 6, hito 6.2): el nivel 1 con sus 8
-- salas, para ver girados los objetos de todas ellas (los que solo se
-- dibujaron de cara a la cámara: el váter, el lavabo, el diván, el armario de
-- la farmacia, la máquina de hinchar…). Se abre y corre 50 horas, para que
-- haya gente usando los objetos. Guarda vistas.sav.
local th64 = th64
local P = dofile(th64.bench_dir .. "/lib/plan.lua")
P.crear{
  level = 1, name = "vistas",
  rooms = {
    {"gp", 5, 5},
    {"psych", 5, 5, extras = {"radiator", "plant", "bookcase"}},
    {"inflation", 5, 5},
    {"pharmacy", 5, 5},
    {"general_diag", 5, 5},
    {"ward", 6, 6, extras = {"bed", "radiator", "plant"}},
    {"toilets", 5, 5},
    {"staff_room", 5, 5, extras = {"radiator", "plant", "pool_table"}},
  },
  corridor = {bench = 4, drinks_machine = 1, extinguisher = 1, plant = 2, bin = 1},
  max_hours = 50,
}
th64.quit(0)
