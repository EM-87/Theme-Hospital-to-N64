-- Carga base_config.lua de CorsixTH (datos puros) y cuenta sus entradas.
local cfg = assert(loadfile(arg[1], "t", {}))()
local function count(t, seen)
  seen = seen or {}
  if seen[t] then return 0 end
  seen[t] = true
  local n = 0
  for _, v in pairs(t) do n = n + 1; if type(v) == "table" then n = n + count(v, seen) end end
  return n
end
local top = {}
for k, v in pairs(cfg) do top[#top + 1] = k .. "(" .. (type(v) == "table" and count(v) or 1) .. ")" end
table.sort(top)
print("primer nivel:", #top, table.concat(top, " "))
print("entradas totales:", count(cfg))
