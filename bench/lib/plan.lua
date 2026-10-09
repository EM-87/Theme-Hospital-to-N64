-- Creación de partidas de referencia: carga un nivel, construye el hospital
-- con el constructor, contrata personal, abre y avanza el tiempo hasta tener
-- los pacientes pedidos. Guarda la partida y un resumen en JSON.
local th64 = th64
local B = dofile(th64.bench_dir .. "/lib/builder.lua")
local P = {}

-- Opcionales por defecto en cada habitación, como haría un jugador.
local DEFAULT_EXTRAS = {"radiator", "plant"}

local function hosp() return TheApp.world.hospitals[1] end

local function close_windows(pred)
  for _, win in ipairs({table.unpack(TheApp.ui.windows)}) do
    if pred(win) then win:close() end
  end
end

--! cfg = {
--!   level, name, rooms = {{id, w, h, n=, extras=}, ...},
--!   buy_all_plots, research_all, receptionists, handymen, extra_doctors,
--!   extra_nurses, corridor = {objeto = n}, target_patients, max_hours,
--!   max_reputation (reputación al máximo cada mes, como un hospital bien llevado),
--!   roujin (truco "Roujin's Challenge": +40 pacientes al mes),
--!   after_target = function(h) end (por ejemplo, emergencia o epidemia) }
function P.crear(cfg)
  th64.log("creando '%s' en el nivel %d", cfg.name, cfg.level)
  assert(TheApp:loadLevel(cfg.level, "full"), "no se pudo cargar el nivel")
  th64.wait_ticks(3)
  local world, h = TheApp.world, hosp()

  -- Las condiciones de victoria y derrota cerrarían la partida al avanzar meses.
  world.endconditions.win_goals = {}
  world.endconditions.lose_goals = {}

  B.reset()
  if cfg.buy_all_plots then
    th64.log("parcelas compradas: %d", B.buy_all_plots())
  end
  if cfg.research_all then
    h.hosp_cheats:cheatResearch()
  end

  local ex, ey = B.entrance()
  -- Un mostrador por recepcionista, como haría un jugador.
  assert(B.place_corridor_objects({reception_desk = cfg.receptionists or 1}, ex, ey, 10),
    "no caben los mostradores de recepción")

  -- Solo habitaciones que el jugador podría construir en este nivel.
  local discovered = {}
  for _, disc in pairs(h.room_discoveries) do
    if disc.is_discovered then discovered[disc.room.id] = true end
  end

  local need = {Doctor = 0, Nurse = 0, Psychiatrist = 0, Researcher = 0, Surgeon = 0}
  local built, failed, unavailable = {}, {}, {}
  for _, spec in ipairs(cfg.rooms) do
    if not discovered[spec[1]] then
      unavailable[#unavailable + 1] = spec[1]
      th64.log("  %s no está disponible en este nivel", spec[1])
    end
    for _ = 1, discovered[spec[1]] and (spec.n or 1) or 0 do
      -- Si la interfaz rechaza el sitio (p. ej. dejaría zonas inaccesibles),
      -- el constructor lo descarta y se prueba el siguiente.
      local room, why
      for _ = 1, 6 do
        room, why = B.build_room(spec[1], spec[2], spec[3], spec.extras or DEFAULT_EXTRAS)
        if room or why == "sin sitio" then break end
      end
      if room then
        built[#built + 1] = spec[1]
        for cls, n in pairs(room.room_info.required_staff or {}) do
          need[cls] = (need[cls] or 0) + n
        end
      else
        failed[#failed + 1] = spec[1] .. ": " .. tostring(why)
        th64.log("  no se pudo construir %s: %s", spec[1], tostring(why))
      end
    end
  end
  th64.log("habitaciones construidas: %d (%d fallidas)", #built, #failed)

  if cfg.corridor then
    B.place_corridor_objects(cfg.corridor, ex, ey, 20)
  end

  -- Política del hospital (opción del juego): conceder las subidas de sueldo.
  -- Sin ella, las peticiones sin atender acaban en despido al cabo de ~25 días.
  h.policies.grant_wage_increase = true

  B.hire("Receptionist", cfg.receptionists or 1)
  B.hire("Doctor", need.Doctor + (cfg.extra_doctors or 0))
  B.hire("Doctor", need.Psychiatrist, "is_psychiatrist")
  B.hire("Doctor", need.Surgeon, "is_surgeon")
  B.hire("Doctor", need.Researcher, "is_researcher")
  B.hire("Nurse", need.Nurse + (cfg.extra_nurses or 0))
  B.hire("Handyman", cfg.handymen or 2)
  th64.log("personal contratado: %d", #h.staff)

  -- Abrir el hospital sin esperar a la cuenta atrás.
  h:open()
  close_windows(function(w) return class.is(w, UIWatch) end)

  if cfg.roujin then
    h.hosp_cheats:roujinOn()
  end
  if cfg.max_reputation then
    h.hosp_cheats:cheatMaxReputation()
  end

  th64.autorun_speed = "And then some more"
  local hours = 0
  local month = world.game_date:monthOfGame()
  local max_hours = cfg.max_hours or 50 * 30 * 24
  while hours < max_hours do
    th64.run_hours(50, 50)
    hours = hours + 50
    B.ensure_money(200000)
    if cfg.max_reputation and world.game_date:monthOfGame() ~= month then
      month = world.game_date:monthOfGame()
      h.hosp_cheats:cheatMaxReputation()
    end
    local c = th64.count_entities(world)
    if hours % 500 == 0 then
      th64.log("  %s: hora %d, %d pacientes, %d personal, curados %d",
        world.game_date:tostring(), hours, c.patients, c.staff, h.num_cured or 0)
    end
    if cfg.target_patients and c.patients >= cfg.target_patients then break end
  end

  if cfg.after_target then cfg.after_target(h) end

  th64.autorun_speed = "Normal"
  -- Las ventanas informativas que el juego abre al construir cada tipo de
  -- habitación se quedarían guardadas con la partida y se dibujarían siempre.
  close_windows(function(w) return class.is(w, UIInformation) end)
  th64.wait_ticks(2)
  local windows = {}
  for _, w in ipairs(TheApp.ui.windows) do windows[#windows + 1] = class.type(w) or "?" end
  local summary = {
    windows_open = windows,
    name = cfg.name, level = cfg.level, date = world.game_date:tostring(),
    hours_simulated = hours, rooms_built = built, rooms_failed = failed,
    rooms_unavailable = unavailable,
    entities = th64.count_entities(world), num_cured = h.num_cured,
    balance = h.balance, reputation = h.reputation, lua = _VERSION,
    decisions = th64.decisions, cheats = {max_reputation = cfg.max_reputation or false,
      roujin = cfg.roujin or false},
  }
  local file = TheApp.savegame_dir .. cfg.name .. ".sav"
  TheApp:save(file)
  th64.log("guardada %s (%d pacientes)", file, summary.entities.patients)
  th64.write_json("resumen_" .. cfg.name .. ".json", summary)
  return summary
end

return P
