-- La campaña de la demo jugable (docs/06_plan.md, «La demo jugable: el orden
-- cambia», pieza 7): CorsixTH juega la misma partida que th64sim partida
-- sobre motor/assets/escenarios/demo.trg con una lista de órdenes con tick
-- (tools/hito6/ordenes/demo.tsv), y la traza (bench/lib/traza.lua) escribe
-- los ficheros del oráculo del hito 4 para compararla con el motor
-- (tools/hito6/campana_demo.sh).
--
-- La partida: una partida nueva del nivel 1 en dificultad full, con lo que
-- se aparta de él la demo (decisiones del plan) y lo que ya quitaba la
-- campaña del hito 4 (crear_hito4.lua):
--   * la semilla: math.randomseed(mezclar(--th64-semilla)) antes de cargar el nivel,
--     así que también World:new (los días de llegada del primer mes,
--     updateSpawnDates, world.lua:151-157, y el mercado de makeAvailableStaff,
--     que se cambia después) sale de ella;
--   * abierta desde el principio (decisión 1): sin el reloj de apertura.
--     UIWatch «initial_opening» (dialogs/watch.lua:114-125) al acabar llama
--     a Hospital:open (hospital.lua:2319-2321), que solo pone opened = true
--     (la fanfarria es sonido). Aquí se llama a h:open() antes del primer
--     tick, y el reloj que World:onTick añade en la fecha inicial
--     (world.lua:870-875) se cierra sin acabar tras cada hora. La ventana
--     de información del nivel que añade a la vez (GameUI:showBriefing,
--     game_ui.lua:1331-1340, un UIInformation) se queda abierta: no pausa
--     (Window:mustPause, window.lua:64-66, que UIInformation no cambia), así
--     que ni este guion ni keep_running de bench/lib/th64.lua la cierran, y
--     no cambia la partida;
--   * sin objetivos (endconditions), emergencias, VIP, terremotos ni
--     epidemias, como crear_hito4.lua;
--   * las opciones de la batería de construir, con las que se escribió y
--     comprobó la lista (construir.lua): blocking_off_areas = 2, sin
--     enable_avg_contents y sin las ventanas de información de las salas;
--   * el mercado de candidatos del núcleo (--th64-mercado): las órdenes
--     «contratar <clase> <índice>» contratan a la misma persona que el
--     núcleo con la misma semilla.
-- No se tocan las llegadas (el ritmo del nivel: popn) ni las enfermedades
-- descubiertas (solo las de Known 1), como demo.trg (tools/hito6/demo.py).
--
-- Las órdenes: cada línea de la lista lleva el tick (la hora de juego desde
-- el principio, 50 por día; th_sim.hours) y una orden del vocabulario de
-- bench/lib/ordenes.lua, que las aplica como la interfaz de CorsixTH. El
-- núcleo aplica las órdenes del tick t al empezar el tick (th_sim_tick:
-- th_build_tick y th_sim_apply_orders, antes del fin de día, las llegadas y
-- los humanoides; la hora pasa de t a t+1 al acabarlo). Aquí, las del tick 0
-- se aplican antes del primer World:onTick, y las del tick t > 0 tras el
-- World:onTick que deja la hora t (el muestreador de th64.lua, que corre
-- entre dos eventos del temporizador, fuera de World:onTick): es el mismo
-- momento, el estado entre el tick t−1 y el t. La traza mira la hora antes
-- de aplicar las órdenes, como traza_hora en th64sim partida (que va tras
-- th_sim_tick y antes de meter las órdenes del tick siguiente).
--
-- El mercado: --th64-mercado=<m.json> es el de th64sim mercado <assets> 1
-- <semilla> 0, el de la partida nueva. El núcleo lo regenera al acabar cada
-- mes con la semilla y el mes que acaba (end_month, como World:onEndMonth →
-- makeAvailableStaff(monthOfGame), world.lua:1127), y CorsixTH también, con
-- su azar: si una orden contrata en un mes k > 0 de la partida (desde 0), la
-- ruta tiene que llevar «%d», que se cambia por el mes (th64sim mercado
-- <assets> 1 <semilla> k), y el mercado de ese mes se carga antes de la
-- primera orden de contratar del mes. Con una ruta sin «%d», contratar
-- después del mes 0 es un error. ordenes/demo.tsv solo contrata en el mes 0
-- (ticks 1 y 100).
--
-- Opciones:
--   --th64-ordenes=<lista.tsv>  la lista con tick (ruta absoluta)
--   --th64-semilla=N            la semilla (por defecto 1)
--   --th64-horas=H              horas de juego (por defecto 9050: 181 días)
--   --th64-mercado=<ruta>       el mercado del núcleo (ruta absoluta; «%d», el mes)
--   --th64-faxes=a,b,...        preferencias de los faxes (por defecto
--                               guess_cure,send_home)
--   --th64-sonda=1              además, la sonda del diagnóstico (bench/lib/sonda_diag.lua:
--                               diagnostico.tsv, faxes.tsv, descubre.tsv y medico_hora.tsv,
--                               como TH64_SONDA_DIAG de th64sim partida); no cambia la partida
-- Escribe la traza (pacientes.tsv, dinero.tsv, dias.tsv, acciones.tsv y
-- final.tsv, bench/lib/traza.lua) y ordenes_resultado.tsv (tick, orden,
-- aceptada o rechazada, motivo), como th64sim partida, y acaba con la línea
-- «demo: N órdenes, A aceptadas; ...».
local th64 = th64
local ordenes = dofile(th64.bench_dir .. "/lib/ordenes.lua")
local builder = dofile(th64.bench_dir .. "/lib/builder.lua")
local json = dofile(th64.bench_dir .. "/lib/json.lua")
local traza = dofile(th64.bench_dir .. "/lib/traza.lua")
local o = th64.opts

local ruta_ordenes = assert(o.ordenes, "falta --th64-ordenes=<lista.tsv>")
assert(ruta_ordenes:sub(1, 1) == "/", "--th64-ordenes tiene que ser una ruta absoluta")
local semilla = math.tointeger(tonumber(o.semilla or "1"))
local horas = math.tointeger(tonumber(o.horas or "9050"))
assert(semilla and horas and horas > 0, "--th64-semilla=N --th64-horas=H")
local ruta_mercado = o.mercado
assert(not ruta_mercado or ruta_mercado:sub(1, 1) == "/", "--th64-mercado tiene que ser una ruta absoluta")

-- La partida nueva ---------------------------------------------------------------
-- Sin ceder el control hasta aplicar las órdenes del tick 0: el primer
-- World:onTick llega con el primer evento del temporizador.
-- La semilla, mezclada (splitmix64) antes de math.randomseed: Lua 5.4 siembra
-- xoshiro256** con (n, 0xff, 0, 0) y, como el generador es lineal, las
-- semillas pequeñas y seguidas dan estados emparejados cuando la partida gasta
-- casi los mismos números en todas. Con math.randomseed(1..40) la campaña
-- salía sesgada: el 31 de enero, el día de llegada de febrero
-- (updateSpawnDates, world.lua:1150-1171, con intervalo 31) caía en los días
-- 29 a 31 (que febrero no tiene: se pierden) en 22 de 40 semillas, cuando
-- updateSpawnDates da el 25 % (lo mismo medido dentro del juego con 20.000
-- repeticiones: 25,9 %); con la semilla mezclada, en 8 de 40. Ver
-- tools/hito6/README.md, «la campaña de la demo».
local function mezclar(x)
  x = x + 0x9E3779B97F4A7C15
  x = (x ~ (x >> 30)) * 0xBF58476D1CE4E5B9
  x = (x ~ (x >> 27)) * 0x94D049BB133111EB
  return x ~ (x >> 31)
end
math.randomseed(mezclar(semilla))
assert(TheApp:loadLevel(1, "full"), "no se pudo cargar el nivel 1")
local world, ui = TheApp.world, TheApp.ui
local h = world.hospitals[1]
assert(world.game_date:tostring() == "1-01-01T00", "la partida no empieza en la hora 0: " .. world.game_date:tostring())
local W, H = world.map.th:size()
world.endconditions.win_goals = {}
world.endconditions.lose_goals = {}
world.next_emergency_month, world.next_emergency_date, world.next_emergency = 0, nil, nil
world.next_vip_date = Date(99, 1, 1)
world.earthquake.disabled = true
h.epidemics_disabled = true
TheApp.config.blocking_off_areas = 2
TheApp.config.enable_avg_contents = false
world.room_information_dialogs = false
h:open()

-- El mercado del núcleo ----------------------------------------------------------
local mes_cargado
local function cargar_mercado(mes)
  assert(ruta_mercado, "contratar sin --th64-mercado: el candidato no sería el del núcleo")
  local ruta = ruta_mercado
  if ruta:find("%%d") then
    ruta = ruta:gsub("%%d", tostring(mes))
  elseif mes ~= 0 then
    error(("contratar en el mes %d con un solo mercado (--th64-mercado sin %%d)"):format(mes), 0)
  end
  local m = json.leer(ruta)
  assert(m.nivel == 1 and m.semilla == semilla and m.mes == mes,
    ("el mercado %s es del nivel %s, semilla %s y mes %s; la partida, del 1, %d y %d"):format(ruta,
      tostring(m.nivel), tostring(m.semilla), tostring(m.mes), semilla, mes))
  local c = builder.mercado_del_nucleo(m)
  mes_cargado = mes
  th64.log("mercado del núcleo (semilla %d, mes %d): %d médicos, %d enfermeras, %d bedeles, %d recepcionistas",
    semilla, mes, c.doctor, c.nurse, c.handyman, c.receptionist)
end
if ruta_mercado then cargar_mercado(0) end

-- La lista: tick y orden. ordenes.leer lee la orden sin el tick (se le pasa
-- la lista sin la primera columna, con las mismas líneas).
local ticks = {}
do
  local f = assert(io.open(ruta_ordenes, "r"))
  local sin_tick = assert(io.open(th64.out("ordenes_sin_tick.tsv"), "w"))
  local k = 0
  for linea in f:lines() do
    k = k + 1
    linea = linea:gsub("\r$", "")
    local tick, resto = linea:match("^(%d+)\t(.*)$")
    if tick then
      ticks[#ticks + 1] = math.tointeger(tonumber(tick))
      sin_tick:write(resto, "\n")
    elseif linea:match("^%s*$") or linea:match("^#") or linea:match("^tick\t") then
      sin_tick:write("\n")
    else
      error(("%s:%d: la línea no empieza por el tick: «%s»"):format(ruta_ordenes, k, linea), 0)
    end
  end
  f:close()
  sin_tick:close()
end
local lista = ordenes.leer(th64.out("ordenes_sin_tick.tsv"), W, H)
assert(#lista == #ticks, "la lista sin tick no tiene las mismas órdenes")
for i = 2, #ticks do
  assert(ticks[i] >= ticks[i - 1], ("los ticks tienen que ir de menor a mayor (línea %d)"):format(lista[i].linea))
end
local interprete = ordenes.nuevo(TheApp)

-- La traza y las órdenes ----------------------------------------------------------
local hora_actual = 0
local tr = traza.nueva(world, h, function() return hora_actual end, o.faxes)
local sonda = o.sonda and dofile(th64.bench_dir .. "/lib/sonda_diag.lua").nueva(world, h,
  function() return hora_actual end, tr.id)
local f_res = assert(io.open(th64.out("ordenes_resultado.tsv"), "w"))
f_res:write("tick\torden\tresultado\tmotivo\n")
local siguiente, aceptadas = 1, 0
local function aplicar_ordenes()
  while siguiente <= #lista and ticks[siguiente] <= hora_actual do
    local orden = lista[siguiente]
    assert(ticks[siguiente] == hora_actual, "una orden se ha quedado atrás")
    if orden.nombre == "contratar" then
      local mes = world.game_date:monthOfGame() - 1
      if mes ~= mes_cargado then cargar_mercado(mes) end
    end
    local ok, por_que = interprete.aplicar(orden)
    local mot = ok and "-" or assert(por_que, "rechazo sin motivo")
    aceptadas = aceptadas + (ok and 1 or 0)
    f_res:write(("%d\t%s\t%s\t%s\n"):format(hora_actual, ordenes.texto(orden), ok and "aceptada" or "rechazada", mot))
    th64.log("%d %s: %s %s; saldo %d", hora_actual, ordenes.texto(orden), ok and "aceptada" or "rechazada",
      ok and "" or mot, h.balance)
    siguiente = siguiente + 1
  end
end
-- El reloj de apertura que añade el primer World:onTick (world.lua:870-875):
-- fuera, sin acabar. Solo el reloj: la ventana de información del nivel
-- (showBriefing) se queda abierta, sin pausar (arriba).
local function cerrar_reloj()
  for _, w in ipairs({table.unpack(ui.windows)}) do
    if class.is(w, UIWatch) and w.count_type == "initial_opening" then w:close() end
  end
end

local base = th64.hours_done()
tr.volcar_dia()
aplicar_ordenes()
th64.set_sampler(function()
  hora_actual = th64.hours_done() - base
  cerrar_reloj()
  tr.muestra()
  if sonda then sonda.muestra() end
  if hora_actual % 50 == 0 then tr.volcar_dia() end
  aplicar_ordenes()
end)
th64.skip_frames = true
th64.autorun_speed = "Normal"
th64.run_hours(horas, 50)
th64.set_sampler(nil)
f_res:close()
assert(hora_actual == horas, ("la partida corrió %d horas, no %d"):format(hora_actual, horas))
assert(h.opened, "el hospital no está abierto")
tr.terminar()
if sonda then sonda.terminar() end
if siguiente <= #lista then
  th64.log("%d órdenes sin aplicar: su tick no llega en %d horas", #lista - siguiente + 1, horas)
end
th64.log("demo: %d órdenes, %d aceptadas; %d horas; saldo %d; semilla %d", siguiente - 1, aceptadas, hora_actual,
  h.balance, semilla)
th64.quit(0)
