-- Sonda de un paciente (fase 5, hito 4): tras cada hora entre --th64-desde y
-- --th64-hasta escribe la cola de acciones del paciente --th64-id (por orden
-- de llegada, como traza_hito4.lua) y los mensajes del panel. Abre y contesta
-- los faxes como traza_hito4.lua, para que la partida sea la misma.
-- Opciones: --load=partida.sav (obligatoria), --th64-id=N, --th64-desde=H, --th64-hasta=H,
--           --th64-faxes=a,b,... (por defecto guess_cure,send_home)
local th64 = th64
local world = assert(TheApp.world, "falta --load=")
th64.skip_frames = true
th64.fax_choices = {}
for c in (th64.opts.faxes or "guess_cure,send_home"):gmatch("[^,]+") do th64.fax_choices[#th64.fax_choices + 1] = c end
local id, desde, hasta = tonumber(th64.opts.id), tonumber(th64.opts.desde), tonumber(th64.opts.hasta)
local hora = th64.hours_done
local n, paciente = 0, nil
local orig = World.spawnPatient
World.spawnPatient = function(...)
  local p = orig(...)
  if p then
    n = n + 1
    if n == id then paciente = p end
  end
  return p
end
local f = assert(io.open(th64.out("paciente.tsv"), "w"))
local function cola(p)
  local t = {}
  for _, a in ipairs(p.action_queue) do
    t[#t + 1] = a.name .. (a.must_happen and "!" or "") .. (a.todo_interrupt and "*" or "") ..
      (a.count and ("#" .. a.count) or "")
  end
  return table.concat(t, " ")
end
th64.set_sampler(function()
  local panel = TheApp.ui.bottom_panel
  local antes = panel and #panel.message_windows or 0
  if panel and #panel.message_windows > 0 then panel:openFirstMessage() end
  local h = hora()
  if paciente and h >= desde and h <= hasta then
    f:write(("%d\t(%s,%s)\t%s\tmensajes %d\tpuerta %s\tdiag %s\tcasa %s\tespera %s\n"):format(h, tostring(paciente.tile_x),
      tostring(paciente.tile_y), cola(paciente), antes, tostring(panel and panel.factory_counter),
      tostring(paciente.diagnosed), tostring(paciente.going_home), tostring(paciente.waiting)))
  end
end)
th64.autorun_speed = "Normal"
for _ = 1, hasta // 50 + 1 do th64.run_hours(50, 50) end
f:close()
th64.quit(0)
