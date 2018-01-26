-- hot_fix.lua 代理热更新http接口

local page200 = 
{
    'HTTP/1.1 200 OK\r\n',
    'Content-Length: %d\r\n',
    'Content-Type: text/html\r\n',
    'Server: Mini-Game-Distribute-Server/1.0\r\n',
    'Connection: close\r\n\r\n%s'
}
page200 = table.concat( page200 )

local util = require "util"
local json = require "lua_parson"

-- 这个模块自己指定路径，因为http请求是/来区分："http/hot_fix"
-- 我们一般用点
local Hot_fix = oo.singleton( nil,... )

-- 热更单个文件
function Hot_fix:fix_one( path )
    require_ex( path )
    PLOG( "hot fix %s",path )
end

-- 热更协议
function Hot_fix:fix_proto()
    if g_command_mgr.modify or g_rpc.modify then
        local _pkt = g_command_mgr:command_pkt()
        g_command_mgr:srv_broadcast( SS.SYS_CMD_SYNC,_pkt )
    end
end

-- 热更schema文件
function Hot_fix:fix_schema()
    g_command_mgr:load_schema()
end

-- 全局热更
function Hot_fix:global_fix()
    oo.hot_fix( PLOG )

    self:fix_one( "modules/module_pre_header" )
    self:fix_one( "modules/module_header" )
end

function Hot_fix:fix( list,schema )
    if not list then return end

    g_rpc.modify = false
    g_command_mgr.modify = false -- 自动检测协议注册是否变动

    local sec, usec = util.timeofday()

    -- 没有指定文件则全部更新
    if table.empty( list ) then
        self:global_fix()
        self:fix_proto()
        self:fix_schema()
    else
        for _,module in pairs( list ) do
            self:fix_one( module )
        end

        self:fix_proto()
        if schema then self:fix_schema() end
    end

    local nsec, nusec = util.timeofday()
    local msec = (nsec - sec)*1000000 + nusec - usec
    PLOG( "hot fix finish,time elapsed %d microsecond",msec ) -- 微秒
end

--[[
curl -l -H "Content-type: application/json" -X POST -d '{"gateway":[],"world":[]}' 127.0.0.1:10003/hot_fix
curl -l -H "Content-type: application/json" -X POST -d '{"gateway":["network.network_mgr"],"world":["network.network_mgr"],"schema":1}' 127.0.0.1:10003/hot_fix
]]
function Hot_fix:exec( conn,fields,body )
    local tbl = json.decode( body )

    -- 这个http请求总是在gateway收到的
    local local_name = Main.srvname
    self:fix( tbl[local_name],tbl.schema )

    -- 热更其他服务器
    for srvname in pairs( SRV_NAME ) do
        local module_list = tbl[srvname]
        if local_name ~= srvname and module_list then
            local pkt = { module = module_list,schema = tbl.schema }

            g_network_mgr:srv_name_send( srvname,SS.SYS_HOT_FIX,pkt )
        end
    end

    local tips = "hot fix success!\n"
    local ctx = string.format( page200,string.len(tips),tips )

    conn:send_pkt( ctx )
end

local hf = Hot_fix()

return hf
