-- Oráculo exacto: duración (en fotogramas) de cada animación según CorsixTH
-- (AnimationManager:getAnimLength, con los ajustes que hace la Lua al cargar).
local th64 = th64
local am = TheApp.animation_manager
local n = tonumber(th64.opts.count or "5186")
local out = assert(io.open(th64.out("anim_len.txt"), "w"))
for i = 0, n - 1 do out:write(i, " ", am:getAnimLength(i), "\n") end
out:close()
th64.log("anim_len: %d animaciones", n)
th64.quit(0)
