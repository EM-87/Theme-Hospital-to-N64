-- Sonda: tras N horas, ¿qué hacen los pacientes y el personal?
-- Opciones: hours (por defecto 1000), batch (100), seed.
local th64 = th64
local o = th64.opts
th64.skip_frames = o.frames == "0"
if o.seed then math.randomseed(tonumber(o.seed)) end
th64.autorun_speed = "Normal"
th64.run_hours(tonumber(o.hours or "1000"), tonumber(o.batch or "100"))
local world = TheApp.world
local h = world.hospitals[1]
local acts, staff = {}, {}
for _, e in ipairs(world.entities) do
  if class.is(e, Patient) then
    local a = e.action_queue and e.action_queue[1]
    local k = (a and a.name or "-") .. (e:getRoom() and (" @" .. e:getRoom().room_info.id) or "")
      .. (e.diagnosed and " diag" or "") .. (e.has_passed_reception and " recep" or " sin_recep")
    acts[k] = (acts[k] or 0) + 1
  elseif class.is(e, Staff) then
    local a = e.action_queue and e.action_queue[1]
    staff[#staff + 1] = string.format("%s %s @%s (%s,%s) fatiga=%.2f", e.humanoid_class,
      a and a.name or "-", e:getRoom() and e:getRoom().room_info.id or "pasillo",
      tostring(e.tile_x), tostring(e.tile_y), e.attributes and e.attributes.fatigue or -1)
  end
end
local shown = 0
for _, e in ipairs(world.entities) do
  if class.is(e, Patient) and not e.has_passed_reception and shown < 6 then
    shown = shown + 1
    local a = e.action_queue[1]
    th64.log("  sin_recep en (%s,%s) -> (%s,%s) cola=%d acciones; siguiente=%s", tostring(e.tile_x), tostring(e.tile_y),
      tostring(a and a.x), tostring(a and a.y), #e.action_queue, tostring(e.action_queue[2] and e.action_queue[2].name))
  elseif class.is(e, ReceptionDesk) then
    th64.log("  mostrador en (%s,%s), cola %d, recepcionista %s", tostring(e.tile_x), tostring(e.tile_y),
      e.queue and e.queue:size() or -1, tostring(e.receptionist ~= nil))
  end
end
for k, v in pairs(acts) do th64.log("paciente %-40s %d", k, v) end
for _, s in ipairs(staff) do th64.log("personal %s", s) end
th64.log("curados %d, visitas %d, diag_cost gp=%s", h.num_cured, h.num_visitors,
  tostring(h.disease_casebook and h.disease_casebook.diag_gp and h.disease_casebook.diag_gp.price))
th64.quit(0)
