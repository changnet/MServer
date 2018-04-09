-- main.lua
-- 2018-04-04
-- xzc

-- 进程入口文件

-- 设置lua文件搜索路径
package.path = "./lua_script/?.lua;" .. package.path
-- 设置c库搜索路径
package.cpath = "./c_module/?.so;" .. package.cpath

require "global.oo"
require "global.require"

require "global.global" -- 这个要放require后面，它除了一个测试用的函数是可以热更的

math.randomseed( ev:time() )

local function main( command,srvname,srvindex,srvid,... )
    local App = require( string.format( "%s.app",srvname ) )

    g_app = App( command,srvname,srvindex,srvid,... )
    g_app:exec()
end

xpcall( main, __G__TRACKBACK__,... )
