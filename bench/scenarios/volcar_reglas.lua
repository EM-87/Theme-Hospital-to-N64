-- Oráculo de las reglas (fase 5, hito 0): vuelca, desde dentro de CorsixTH y
-- en el JSON canónico de tools/conv/json_canonico.lua, lo que el extractor
-- del conversor (tools/conv/reglas.lua) saca con stubs fuera del juego:
--   * definiciones.json: los objetos (ya procesados con
--     Object.processTypeDefinition), las salas, las enfermedades y los
--     diagnósticos, tal como los tiene TheApp;
--   * nivel_<dificultad><nn>.json: la configuración del nivel cargado
--     (world.map.level_config), para cada nivel pedido.
-- tools/conv/comparar_reglas.py compara los dos.
-- Opciones: --th64-niveles=1,2,... (por defecto 1..12), --th64-dificultades=easy,full,hard
local th64 = th64
local o = th64.opts
local canonico = dofile(th64.bench_dir .. "/../tools/conv/json_canonico.lua")

local function escribir(nombre, texto)
  local f = assert(io.open(th64.out(nombre), "w"))
  f:write(texto, "\n")
  f:close()
end

-- Las referencias entre objetos (slave_type, master_type) van como id, igual
-- que en el extractor.
local function cambiar(k, v)
  if (k == "slave_type" or k == "master_type") and type(v) == "table" then return v.id end
  return v
end

local por_id = function(lista)
  local t = {}
  for _, v in ipairs(lista) do t[v.id] = v end
  return t
end
local enfermedades, diagnosticos = {}, {}
for _, d in ipairs(TheApp.diseases) do
  if d.pseudo then diagnosticos[d.id] = d else enfermedades[d.id] = d end
end
escribir("definiciones.json", canonico.escribir({
  objetos = por_id(TheApp.objects),
  salas = por_id(TheApp.rooms),
  enfermedades = enfermedades,
  diagnosticos = diagnosticos,
}, {cambiar = cambiar}))

local niveles = {}
for n in (o.niveles or "1,2,3,4,5,6,7,8,9,10,11,12"):gmatch("%d+") do niveles[#niveles + 1] = tonumber(n) end
local hechos = 0
for dif in (o.dificultades or "easy,full,hard"):gmatch("[a-z]+") do
  for _, n in ipairs(niveles) do
    assert(TheApp:loadLevel(n, dif), ("no se pudo cargar el nivel %d (%s)"):format(n, dif))
    th64.wait_ticks(1)
    escribir(("nivel_%s%02d.json"):format(dif, n), canonico.escribir(TheApp.world.map.level_config))
    hechos = hechos + 1
  end
end
th64.log("reglas volcadas: definiciones y %d niveles", hechos)
th64.quit(0)
