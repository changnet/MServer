-- 网关服务器入口

Main = {}
Main.command,Main.srvname,Main.srvindex,Main.srvid = ...

-- 这个服务器需要等待其他服务器才能初始化完成
Main.wait = 
{
    world = 1, -- 等待一个world服OK
}

require "modules.module_pre_header"

Main.session = g_unique_id:srv_session(
    Main.srvname,tonumber(Main.srvindex),tonumber(Main.srvid) )

require "modules.module_header"

-- 信号处理
function sig_handler( signum )
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
    network_mgr:set_curr_session( Main.session )

    g_command_mgr:load_schema()

    if not g_network_mgr:srv_listen( g_setting.sip,g_setting.sport ) then
        ELOG( "gateway server listen fail,exit" )
        os.exit( 1 )
    end

    if not g_httpd:http_listen( g_setting.hip,g_setting.hport ) then
        ELOG( "gateway http listen fail,exit" )
        os.exit( 1 )
    end
end

-- 最终初始化
function Main.final_init()
    if not g_network_mgr:clt_listen( g_setting.cip,g_setting.cport ) then
        ELOG( "gateway client listen fail,exit" )
        os.exit( 1 )
    end

    Main.ok = true
    PLOG( "gateway server(0x%.8X) start OK",Main.session )
end

local function main()
    ev:signal( 2  )
    ev:signal( 15 )

    Main.init()

    ev:backend()
end

xpcall( main, __G__TRACKBACK__ )
