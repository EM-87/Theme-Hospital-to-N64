-- Orientaciones de los objetos (fase 6, las cuatro vistas). Para cada objeto:
-- en qué orientaciones se puede colocar y cuáles tienen dibujo propio. Para
-- los que solo se colocan de cara a la cámara (sin sur ni oeste), cuántas
-- animaciones y fotogramas tienen en reposo y en uso: es el arte que falta
-- para verlos de espaldas. Escribe orientaciones.json.
local th64 = th64
assert(TheApp:loadLevel(1, "full"), "no se pudo cargar el nivel 1")
th64.wait_ticks(1)
local am = TheApp.animation_manager
local cfg = TheApp.world.map.level_config.objects
local function anims_de(t, out)
  if type(t) == "number" then
    out[t] = true
  elseif type(t) == "table" then
    for _, v in pairs(t) do anims_de(v, out) end
  end
end
local res = {}
for _, o in ipairs(TheApp.objects) do
  local ors, idle = {}, {}
  for _, d in ipairs({"north", "east", "south", "west"}) do
    if o.orientations and o.orientations[d] then ors[#ors + 1] = d end
    if o.idle_animations and o.idle_animations[d] then idle[#idle + 1] = d end
  end
  local r = {id = o.id, colocar = table.concat(ors, " "), dibujo = table.concat(idle, " "),
    nivel1 = cfg[o.thob] and cfg[o.thob].AvailableForLevel == 1 or false}
  if o.orientations and o.orientations.north and not o.orientations.south and not o.orientations.west then
    local reposo, uso = {}, {}
    anims_de(o.idle_animations and o.idle_animations.north, reposo)
    anims_de(o.usage_animations and o.usage_animations.north, uso)
    anims_de(o.multi_usage_animations, uso)
    r.anim_reposo, r.fotogramas_reposo, r.anim_uso, r.fotogramas_uso = 0, 0, 0, 0
    for a in pairs(reposo) do
      r.anim_reposo, r.fotogramas_reposo = r.anim_reposo + 1, r.fotogramas_reposo + am:getAnimLength(a)
    end
    for a in pairs(uso) do
      if not reposo[a] then
        r.anim_uso, r.fotogramas_uso = r.anim_uso + 1, r.fotogramas_uso + am:getAnimLength(a)
      end
    end
  end
  res[#res + 1] = r
end
th64.write_json("orientaciones.json", res)
th64.quit(0)
