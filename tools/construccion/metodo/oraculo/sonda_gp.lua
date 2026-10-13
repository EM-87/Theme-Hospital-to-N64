-- Sonda: estado de las consultas (GP) cada 'step' horas: paciente dentro, su
-- acción, personal y su acción, cola. Para ver si una consulta está bloqueada.
local th64 = th64
local o = th64.opts
th64.skip_frames = o.frames ~= "1"
local world = assert(TheApp.world, "falta --load=")
local ids, n = setmetatable({}, {__mode = "k"}), 0
local function id(e)
  if not e then return "-" end
  if not ids[e] then n = n + 1; ids[e] = n end
  return tostring(ids[e])
end
local function act(e)
  local a = e and e.action_queue and e.action_queue[1]
  return a and a.name or "-"
end
local function dump()
  for _, r in pairs(world.rooms) do
    if r and r.room_info.id == (o.room or "gp") then
      local pats, staff = {}, {}
      for h in pairs(r.humanoids) do
        if class.is(h, Patient) then
          pats[#pats + 1] = "p" .. id(h) .. ":" .. act(h) .. (h.diagnosed and "(diag)" or "") ..
            string.format("[cert %.2f]", h.diagnosis_progress or -1)
        elseif class.is(h, Staff) then
          staff[#staff + 1] = "s" .. id(h) .. ":" .. act(h) .. string.format("[fat %.2f]", h.attributes.fatigue or -1)
        end
      end
      table.sort(pats); table.sort(staff)
      th64.log("h%d %s(%d,%d) activa=%s cola=%d pac={%s} pers={%s}", th64.hours_done(), r.room_info.id, r.x, r.y,
        tostring(r.is_active), r.door.queue and r.door.queue:size() or -1, table.concat(pats, " "), table.concat(staff, " "))
    end
  end
end
th64.autorun_speed = "Normal"
for _ = 1, tonumber(o.steps or "6") do
  dump()
  th64.run_hours(tonumber(o.step or "50"), 30)
end
dump()
th64.quit(0)
