-- Memoria que ocupa el código Lua de CorsixTH una vez cargado (prototipos con
-- su bytecode, constantes e información de depuración), sin ejecutarlo.
-- Uso: lua codigo_lua.lua <CorsixTH/Lua> <idioma.lua>
local dir, lang = arg[1], arg[2]
local function files()
  local list = {}
  local p = io.popen('find "' .. dir .. '" -name "*.lua" -not -path "*/languages/*" | sort')
  for f in p:lines() do list[#list + 1] = f end
  p:close()
  return list
end
local function live() collectgarbage("collect") collectgarbage("collect") return collectgarbage("count") end

local keep, src_bytes, dump_full, dump_strip = {}, 0, 0, 0
local base = live()
for _, f in ipairs(files()) do
  local fh = assert(io.open(f, "rb")) local s = fh:read("a") fh:close()
  src_bytes = src_bytes + #s
  local chunk = assert(loadfile(f)) -- loadfile se salta una primera línea con "#"
  keep[#keep + 1] = chunk
  dump_full = dump_full + #string.dump(chunk)
  dump_strip = dump_strip + #string.dump(chunk, true)
end
local code_kb = live() - base
local base2 = live()
local fh = assert(io.open(dir .. "/languages/" .. lang, "rb")) local s = fh:read("a") fh:close()
keep[#keep + 1] = assert(load(s, "@" .. lang))
local lang_kb = live() - base2
print(string.format('{"lua": "%s", "ficheros": %d, "fuente_kb": %.0f, "codigo_en_memoria_kb": %.0f, ' ..
  '"bytecode_con_depuracion_kb": %.0f, "bytecode_sin_depuracion_kb": %.0f, "idioma_%s_codigo_kb": %.0f}',
  _VERSION, #keep - 1, src_bytes / 1024, code_kb, dump_full / 1024, dump_strip / 1024, lang:gsub("%.lua", ""), lang_kb))
