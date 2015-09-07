local name = require "lua.global.oo"
require "lua.global.global"
print("run ================================================================")
print(package.path)

local Player = require "lua.module.player.player"
local obj_player = Player(998756)
print( obj_player:get_pid() )
vd( package.loaded )
