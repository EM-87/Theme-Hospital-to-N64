-- Oráculo de los textos (fase 5, hito 0): carga cada idioma con el
-- Strings:load de CorsixTH, dentro del juego (con su TH.LoadStrings en C++),
-- y lo vuelca en el JSON canónico de tools/conv/json_canonico.lua, para
-- compararlo con el extractor del conversor (tools/conv/textos.lua):
-- python3 -I tools/conv/comparar_reglas.py --textos <textos.json> <dir del oráculo>
-- Opciones: --th64-idiomas=english,spanish (por defecto)
local th64 = th64
local canonico = dofile(th64.bench_dir .. "/../tools/conv/json_canonico.lua")
local doc = {}
for idioma in (th64.opts.idiomas or "english,spanish"):gmatch("[^,]+") do
  doc[idioma] = TheApp.strings:load(idioma)
end
local f = assert(io.open(th64.out("textos.json"), "w"))
f:write(canonico.escribir(doc), "\n")
f:close()
th64.log("textos volcados")
th64.quit(0)
