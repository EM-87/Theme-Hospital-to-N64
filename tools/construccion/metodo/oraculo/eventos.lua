-- Oráculo de eventos: registra cada llegada, diagnóstico, cura, muerte y
-- salida sin curar, con la hora de juego, la enfermedad y un número de paciente.
-- Envuelve funciones de CorsixTH desde el guion (no modifica CorsixTH) y no
-- consume aleatorios: al final escribe la huella del generador para comprobar
-- que la simulación no ha cambiado respecto a volcado.lua con la misma semilla.
-- Opciones: hours (1500), batch (30), frames (0), seed.
local th64 = th64
local o = th64.opts
th64.skip_frames = o.frames ~= "1"
local world = assert(TheApp.world, "falta --load=")
local h = world.hospitals[1]
if o.seed then math.randomseed(tonumber(o.seed)) end

local out = assert(io.open(th64.out("eventos.tsv"), "w"))
out:write("hora\tevento\tpaciente\tenfermedad\tdetalle\n")
local ids, next_id = setmetatable({}, {__mode = "k"}), 0
local function pid(p)
  if not ids[p] then next_id = next_id + 1; ids[p] = "e" .. next_id end -- ya estaba al cargar
  return ids[p]
end
local function ev(name, p, extra)
  out:write(string.format("%d\t%s\t%s\t%s\t%s\n", th64.hours_done(), name, pid(p),
    p.disease and p.disease.id or "-", extra or ""))
end
local function wrap(cls, fname, fn)
  local orig = assert(cls[fname], fname)
  cls[fname] = function(self, ...)
    local r = table.pack(orig(self, ...))
    fn(self, r, ...)
    return table.unpack(r, 1, r.n)
  end
end
local spawned = 0
wrap(World, "spawnPatient", function(_, r)
  local p = r[1]
  if p then spawned = spawned + 1; ids[p] = "n" .. spawned; ev("llegada", p) end
end)
wrap(Patient, "setDiagnosed", function(p) ev("diagnostico", p) end)
wrap(Hospital, "updateCuredCounts", function(_, _, p) ev("curado", p) end)
wrap(Hospital, "humanoidDeath", function(_, _, p) ev("muerte", p) end)
wrap(Hospital, "updateNotCuredCounts", function(_, _, p, reason) ev("no_curado", p, tostring(reason)) end)

local function fnv(s, hh)
  hh = hh or 2166136261
  for i = 1, #s do hh = ((hh ~ s:byte(i)) * 16777619) & 0xffffffff end
  return hh
end
th64.autorun_speed = "Normal"
th64.run_hours(tonumber(o.hours or "1500"), tonumber(o.batch or "30"))
out:close()
th64.log("eventos: llegadas %d; fin: hora %d rng %d curados %d muertes %d no_curados %d visitas %d",
  spawned, th64.hours_done(), fnv(math.randomdump()), h.num_cured, h.num_deaths, h.not_cured, h.num_visitors)
th64.quit(0)
