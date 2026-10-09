-- Punto de entrada alternativo de CorsixTH para generar partidas y medir sin
-- intervención humana. Se usa en lugar de CorsixTH.lua:
--
--   corsix-th --interpreter=bench/th64bench.lua \
--     --th64-corsixth=<CorsixTH>/CorsixTH/CorsixTH.lua \
--     --th64-script=bench/scenarios/<guion>.lua --th64-out=<dir> [args de CorsixTH]
--
-- Carga el CorsixTH.lua original sin modificarlo y después ejecuta el guion
-- como una corrutina dentro del bucle de eventos del juego (ver lib/th64.lua).

local args = {...}
local opts = {}
for _, a in ipairs(args) do
  local k, v = a:match("^%-%-th64%-([%w%-]+)=(.*)$")
  if k then opts[k] = v end
end

-- Con Tracy, CorsixTH v0.70.1 crea una zona por cada llamada a función Lua
-- (main.cpp, l_tracy_hook). Eso multiplica el coste de cada tick, así que se
-- desactiva salvo que se pida explícitamente con --th64-lua-hook=1.
if opts["lua-hook"] ~= "1" then
  debug.sethook()
end

local bench_dir = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$")
th64 = dofile(bench_dir .. "/lib/th64.lua") -- luacheck: ignore 111
th64.init(opts, bench_dir)

assert(opts.corsixth, "falta --th64-corsixth=<ruta a CorsixTH.lua>")
assert(loadfile(opts.corsixth))(table.unpack(args))

th64.start()
