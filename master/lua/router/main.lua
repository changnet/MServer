print("run ================================================================")
print(package.path)

local Player = require "lua.module.player.player"
local obj_player = Player(998756)
print( obj_player:get_pid() )

vd( getmetatable(ev) )
print( ev,ev:now() )
print( util.md5() )

local ev_io = ev:listen("0.0.0.0",9997)
print( ev_io )
if not ev_io then
    print("listen fail")
end
ev:run()
