-- hot_swap.lua 代理热更新http接口

local page200 = 
[[
HTTP/1.1 200 OK
Content-Length: %d
Content-Type: text/html
Server: Mini-Game-Distribute-Server/1.0
Connection: close

%s
]]

local json = require "lua_parson"

-- 这个模块自己指定路径，因为http请求是/来区分："http/hot_swap"
-- 我们一般用点
local Hot_swap = oo.singleton( nil,"http.hot_swap" )

function Hot_swap:swap( list )
    if not list then return end

    -- 没有指定文件则全部更新
    if table.empty( list ) then
        return oo.hot_swap( PLOG )
    end

    for _,module in pairs( list ) do
        require_ex( module )
        PLOG( "hot swap %s",module )
    end
end

--[[
curl -l -H "Content-type: application/json" -X POST -d '{"gateway":[],"world":[]}' 127.0.0.1:10003/hot_swap
curl -l -H "Content-type: application/json" -X POST -d '{"gateway":["network.network_mgr"],"world":["network.network_mgr"]}' 127.0.0.1:10003/hot_swap
]]
function Hot_swap:exec( conn_id,fields,body )
    local tbl = json.decode( body )

    -- 这个http请求总是在gateway收到的
    local local_name = Main.srvname
    self:swap( tbl[local_name] )

    -- 热更其他服务器
    for srvname,module_list in pairs( tbl ) do
        if srvname ~= local_name then
            local pkt = { module = module_list }

            g_network_mgr:srv_name_send( srvname,SS.SYS_HOT_SWAP,pkt )
        end
    end

    local tips = "hot swap success!\n"
    local ctx = string.format( page200,string.len(tips),tips )

    network_mgr:send_http_packet( conn_id,ctx )

    g_rpc:invoke( "rpc_test",1,2,3,"abc",nil,{a = 5,b = 9},99.75863 )
end

local hs = Hot_swap()

return hs