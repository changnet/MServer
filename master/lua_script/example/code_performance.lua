-- code_performance.lua
-- xzc
-- 2016-03-06

require "global.global"
local json = require "lua_parson"

f_tm_start()

vd( json.decode("[1,2,3,4,5,6,7,0]"))

f_tm_stop( "json decode cost")
