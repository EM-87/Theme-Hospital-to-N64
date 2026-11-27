-- Seguir a unos pacientes hora a hora (--th64-seguir de crear_demo.lua): lo
-- mismo que TH64_SEGUIR en th64sim partida (motor/platform/pc/hito4.c), para
-- comparar el camino de un paciente tick a tick. Escribe seguir.tsv en el
-- directorio de la traza, una línea por paciente seguido y hora: la casilla,
-- la sala, si ya pasó la recepción, el temporizador de la entidad
-- (Entity:setTimer, entity.lua:253-260) y las acciones de su cola. Y
-- saltos.tsv, de todas las partidas que siguen a alguien: cada vez que una
-- entidad sale de world.entities (World:destroyEntity, world.lua:1792-1800,
-- con table.remove) mientras World:onTick recorre la lista con ipairs
-- (world.lua:923-928) y está en la casilla de la que hace tick o antes, la
-- que va detrás de esa se queda sin tick esa hora: lo que se quita, lo que
-- se salta y la hora. No gasta números aleatorios ni cambia nada: solo lee.
local th64 = th64
local S = {}

--! ids: los ids de pacientes.tsv separados por comas. id_de(p): el id del
--! paciente en la traza; hora(): la hora de juego.
function S.nueva(world, ids, hora, id_de)
  local quiero = {}
  for i in ids:gmatch("%d+") do quiero[math.tointeger(tonumber(i))] = true end
  local f = assert(io.open(th64.out("seguir.tsv"), "w"))
  f:write("hora\tpaciente\tx\ty\tsala\trecepcion\ttimer\tacciones\n")
  local sg = {}

  -- Cómo se llama una entidad en saltos.tsv.
  local function nombre(e)
    if not e then return "-" end
    if class.is(e, Patient) then return "paciente " .. id_de(e) end
    if class.is(e, Staff) then return "personal " .. (e.humanoid_class or "?") end
    if class.is(e, Humanoid) then return "humanoide " .. (e.humanoid_class or "?") end
    if class.is(e, Litter) then return "basura" end
    if class.is(e, Object) then return "objeto " .. e.object_type.id .. (e.ticks and " (tick)" or "") end
    return "otra"
  end
  local f_saltos = assert(io.open(th64.out("saltos.tsv"), "w"))
  f_saltos:write("hora\tquitada\tsaltada\n")
  local destroy = World.destroyEntity
  World.destroyEntity = function(self, entity, ...)
    local cur = self.current_tick_entity
    if cur then
      local i_cur, i_quita
      for i, e in ipairs(self.entities) do
        if e == cur then i_cur = i end
        if e == entity then i_quita = i end
      end
      if i_cur and i_quita and i_quita <= i_cur then
        f_saltos:write(("%d\t%s\t%s\n"):format(hora(), nombre(entity), nombre(self.entities[i_cur + 1])))
      elseif i_quita and not i_cur then
        f_saltos:write(("%d\t%s\t%s\n"):format(hora(), nombre(entity), "? (la entidad del tick ya no está)"))
      end
    end
    return destroy(self, entity, ...)
  end

  --! Tras cada hora: una línea por paciente seguido que esté en el mapa.
  function sg.muestra()
    for _, e in ipairs(world.entities) do
      if class.is(e, Patient) and quiero[id_de(e)] then
        local room = e:getRoom()
        local a = {}
        for _, q in ipairs(e.action_queue) do
          local extra = ""
          if q.name == "walk" then extra = ("(%d,%d)"):format(q.x or -1, q.y or -1)
          elseif q.name == "queue" then extra = ("(%s)"):format(tostring(q.queue and q.queue:size()))
          end
          a[#a + 1] = q.name .. extra .. (q.must_happen and "!" or "") .. (q.todo_interrupt and "*" or "")
        end
        f:write(("%d\t%d\t%d\t%d\t%s\t%d\t%s\t%s\n"):format(hora(), id_de(e), e.tile_x or -1, e.tile_y or -1,
          room and room.room_info.id or "-", e.has_passed_reception and 1 or 0, tostring(e.timer_time),
          table.concat(a, " ")))
      end
    end
  end

  function sg.terminar() f:close() f_saltos:close() end
  return sg
end

return S
