-- Microbenchmarks de Lua para estimar el factor de CPU entre el host y la
-- N64 (VR4300 a 93,75 MHz). El mismo fichero se ejecuta en los dos lados:
--   N64:  ROM n64/luabench (th64_us y th64_log vienen de C)
--   host: tools/luabench_host.sh (los define a partir de os.clock)
-- Cada prueba imita un patrón de la lógica de CorsixTH y devuelve una suma de
-- control, para comprobar que los dos lados hacen exactamente el mismo trabajo.

local us, log = th64_us, th64_log

-- 1. Clases al estilo de CorsixTH (class.lua): metatablas, herencia y una
--    cola de acciones por entidad que se recorre en cada tick.
local function bench_clases()
  local Entity = {}
  Entity.__index = Entity
  function Entity.new(id) return setmetatable({id = id, x = id % 64, y = id // 64, queue = {}, mood = 0}, Entity) end
  function Entity:queueAction(a) self.queue[#self.queue + 1] = a end
  function Entity:tick()
    local a = self.queue[1]
    if a then
      a.left = a.left - 1
      if a.left <= 0 then table.remove(self.queue, 1) end
      self.x = (self.x + a.dx) % 64
      self.y = (self.y + a.dy) % 64
    else
      self:queueAction({left = 3 + self.id % 5, dx = 1, dy = self.id % 3 - 1})
    end
    self.mood = self.mood + (self.x > self.y and 1 or -1)
  end
  local Patient = setmetatable({}, {__index = Entity})
  Patient.__index = Patient
  function Patient.new(id) local p = Entity.new(id) p.health = 100 return setmetatable(p, Patient) end
  function Patient:tick() Entity.tick(self) self.health = self.health - (self.mood % 2) end
  local ents = {}
  for i = 1, 300 do ents[i] = (i % 3 == 0) and Entity.new(i) or Patient.new(i) end
  local sum = 0
  for _ = 1, 400 do
    for i = 1, #ents do ents[i]:tick() end
  end
  for i = 1, #ents do sum = sum + ents[i].x + ents[i].y + ents[i].mood end
  return sum
end

-- 2. Tablas: recorridos con ipairs/pairs, búsquedas por clave y filtrado,
--    como World:onTick y los dispatchers.
local function bench_tablas()
  local rooms = {}
  for i = 1, 200 do rooms["room" .. i] = {id = i, queue = {}, staff = i % 4} end
  local list = {}
  for i = 1, 2000 do list[i] = {room = "room" .. (i % 200 + 1), w = i % 7} end
  local sum = 0
  for _ = 1, 60 do
    for _, it in ipairs(list) do
      local r = rooms[it.room]
      if r.staff > 0 then sum = sum + it.w + r.id end
    end
    for _, r in pairs(rooms) do sum = sum + #r.queue + r.staff end
  end
  return sum
end

-- 3. Cadenas: concatenación, format y búsqueda (textos de la IU y nombres).
local function bench_cadenas()
  local sum = 0
  for i = 1, 20000 do
    local s = string.format("Paciente %d: %s (%.1f)", i, (i % 2 == 0) and "Cabezudismo" or "Lengua larga", i / 3)
    local a, b = s:find("%((%d+)")
    sum = sum + #s + (a or 0) + (b or 0)
    if i % 100 == 0 then
      local parts = {}
      for w in s:gmatch("%a+") do parts[#parts + 1] = w:upper() end
      sum = sum + #table.concat(parts, " ")
    end
  end
  return sum
end

-- 4. Camino: búsqueda en anchura en una rejilla de 64x64 con muros, en Lua puro.
local function bench_camino()
  local W, H = 64, 64
  local wall = {}
  for y = 0, H - 1 do for x = 0, W - 1 do
    wall[y * W + x] = (x % 8 == 4 and y % 16 ~= 0) or (y % 8 == 4 and x % 16 ~= 8)
  end end
  local sum = 0
  for run = 1, 12 do
    local start, goal = (run % 8), (H - 1 - run % 8) * W + (W - 1)
    local dist = {[start] = 0}
    local q, head = {start}, 1
    while head <= #q do
      local c = q[head]; head = head + 1
      if c == goal then break end
      local x, y = c % W, c // W
      local d = dist[c] + 1
      if x > 0 and not wall[c - 1] and not dist[c - 1] then dist[c - 1] = d q[#q + 1] = c - 1 end
      if x < W - 1 and not wall[c + 1] and not dist[c + 1] then dist[c + 1] = d q[#q + 1] = c + 1 end
      if y > 0 and not wall[c - W] and not dist[c - W] then dist[c - W] = d q[#q + 1] = c - W end
      if y < H - 1 and not wall[c + W] and not dist[c + W] then dist[c + W] = d q[#q + 1] = c + W end
    end
    sum = sum + (dist[goal] or -1) + #q
  end
  return sum
end

-- 5. GC: muchas tablas pequeñas de vida corta (acciones, posiciones, eventos).
local function bench_gc()
  local keep, sum = {}, 0
  for i = 1, 60000 do
    local t = {x = i, y = i * 2, tag = (i % 10 == 0) and "evento" or nil}
    if i % 50 == 0 then keep[#keep % 200 + 1] = t end
    sum = sum + t.x % 7
  end
  for _, t in ipairs(keep) do sum = sum + t.y % 11 end
  return sum
end

-- 6. Numérico: aritmética en coma flotante (doble precisión), como los
--    cálculos de temperatura, dinero y probabilidades.
local function bench_numerico()
  local s, v = 0.0, 1.0
  for i = 1, 200000 do
    v = v * 1.0000001 + (i % 7) * 0.25
    s = s + v / (i + 1.5) - math.floor(v * 0.001)
  end
  return math.floor(s * 1000)
end

local benches = {
  {"clases", bench_clases}, {"tablas", bench_tablas}, {"cadenas", bench_cadenas},
  {"camino", bench_camino}, {"gc", bench_gc}, {"numerico", bench_numerico},
}

log(string.format("TH64LUA version %s", _VERSION))
for _, b in ipairs(benches) do
  collectgarbage("collect")
  local t0 = us()
  local chk = b[2]()
  local t1 = us()
  log(string.format("TH64LUA %s us=%d chk=%s", b[1], t1 - t0, tostring(chk)))
end
log("TH64LUA fin")
