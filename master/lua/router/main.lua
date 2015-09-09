print("run ================================================================")
print(package.path)

local Player = require "lua.module.player.player"
local obj_player = Player(998756)
print( obj_player:get_pid() )
vd( oo.metatableof("lctest") )
vd( lobj )
vd( getmetatable(lobj) )
lobj:show()
print( lobj.S_OK )

package.loaded["lctest"] = oo.metatableof("lctest")
local lctest = require "lctest"
local _lobj = lctest()

print("run ================================================================2222")
local mt = getmetatable(lobj)
vd( mt )
lobj:show()
print( lobj.S_OK )

print("run ================================================================33333")
vd( getmetatable(_lobj) )
_lobj:show()
print( _lobj.S_OK )
