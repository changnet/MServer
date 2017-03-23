require "global.global"
require "global.oo"
require "global.table"
require "global.string"

Main = {}       -- store dynamic runtime info to global
Main.command,Main.srvname,Main.srvindex,Main.srvid = ...

-- 这个服务器需要等待其他服务器才能初始化完成
Main.wait = 
{
    world = 1, -- 等待一个world服OK
}

local setting     = require "gateway/setting"
local network_mgr = require "network/network_mgr"
local command_mgr = require "command/command_mgr"
local Srv_conn    = require "network/srv_conn"
local Clt_conn    = require "network/clt_conn"

-- 信号处理
function Main.sig_handler( signum )
    if g_store_mongo then g_store_mongo:stop() end
    if   g_store_sql then   g_store_sql:stop() end
    if     g_log_mgr then     g_log_mgr:stop() end

    ev:exit()
end

-- 检查需要等待的服务器是否初始化完成
function Main.one_wait_finish( name,val )
    if not Main.wait[name] then return end

    Main.wait[name] = Main.wait[name] - val
    if Main.wait[name] <= 0 then Main.wait[name] = nil end

    if table.empty( Main.wait ) then Main.final_init() end
end

-- 初始化
function Main.init()
    Main.starttime = ev:time()
    Main.session = network_mgr:generate_srv_session(
        Main.srvname,tonumber(Main.srvindex),tonumber(Main.srvid) )

    command_mgr:init_command()
    local fs = command_mgr:load_schema()
    PLOG( "gateway load flatbuffers schema:%d",fs )

    if not network_mgr:srv_listen( setting.sip,setting.sport ) then
        ELOG( "gateway server listen fail,exit" )
        os.exit( 1 )
    end
end

-- 最终初始化
function Main.final_init()
    if not network_mgr:clt_listen( setting.cip,setting.cport ) then
        ELOG( "server listen fail,exit" )
        os.exit( 1 )
    end

    Main.ok = true
    PLOG( "gateway server start OK" )
end

local function main()
    ev:set_signal_ref( Main.sig_handler )
    ev:signal( 2  )
    ev:signal( 15 )

    Main.init()

    ev:backend()
end

xpcall( main, __G__TRACKBACK__ )
