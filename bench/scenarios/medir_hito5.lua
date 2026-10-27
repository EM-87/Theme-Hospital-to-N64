-- Hito 5 (fase 5): medir.lua sobre el escenario del hito 5 (el del hito 4 con
-- 18 llegadas al mes), con el jugador de traza_hito4.lua: abre cada fax en
-- cuanto llega, arriesga el diagnóstico si puede y, si no, manda al paciente
-- a casa. Así la partida es la de la campaña, y el coste por hora de
-- CorsixTH se puede comparar con el del motor (tools/hito5/README.md).
-- Opciones: las de medir.lua (--load, --th64-warmup, --th64-hours).
local th64 = th64
th64.fax_choices = {"guess_cure", "send_home"}
th64.skip_frames = true
local function abrir_faxes()
  local panel = TheApp.ui.bottom_panel
  if panel and #panel.message_windows > 0 then panel:openFirstMessage() end
end
-- medir.lua pone su propio muestreador: se encadena con el de los faxes.
local poner = th64.set_sampler
th64.set_sampler = function(f)
  poner(function(...)
    abrir_faxes()
    if f then return f(...) end
  end)
end
th64.set_sampler(nil)
dofile(th64.bench_dir .. "/scenarios/medir.lua")
