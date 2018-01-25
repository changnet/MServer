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

local json = require "lua_parson"

-- 这个模块自己指定路径，因为http请求是/来区分："http/hot_fix"
-- 我们一般用点
local Hot_fix = oo.singleton( nil,... )

function Hot_fix:fix( list )
    if not list then return end

    -- 没有指定文件则全部更新
    if table.empty( list ) then
        return oo.hot_fix( PLOG )
    end

    for _,module in pairs( list ) do
        require_ex( module )
        PLOG( "hot fix %s",module )
    end
end

--[[
curl -l -H "Content-type: application/json" -X POST -d '{"gateway":[],"world":[]}' 127.0.0.1:10003/hot_fix
curl -l -H "Content-type: application/json" -X POST -d '{"gateway":["network.network_mgr"],"world":["network.network_mgr"]}' 127.0.0.1:10003/hot_fix
]]
function Hot_fix:exec( conn,fields,body )
    local tbl = json.decode( body )

    -- 这个http请求总是在gateway收到的
    local local_name = Main.srvname
    self:fix( tbl[local_name] )

    -- 热更其他服务器
    for srvname,module_list in pairs( tbl ) do
        if srvname ~= local_name then
            local pkt = { module = module_list }

            g_network_mgr:srv_name_send( srvname,SS.SYS_HOT_FIX,pkt )
        end
    end

    local tips = "hot fix success!\n"
    local ctx = string.format( page200,string.len(tips),tips )

    conn:send_pkt( ctx )
end

local hf = Hot_fix()

return hf
