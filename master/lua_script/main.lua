-- main.lua
-- 2018-04-04
-- xzc

-- 进程入口文件

require "global.oo"
require "global.require"

require "global.global" -- 这个要放require后面，它除了一个测试用的函数是可以热更的

local function main( command,srvname,srvindex,srvid,... )
    local App = require( string.format( "%s.app",srvname ) )

    g_app = App( command,srvname,srvindex,srvid,... )
    g_app:exec()
end

xpcall( main, __G__TRACKBACK__,... )