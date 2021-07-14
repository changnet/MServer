-- web_stat.lua http 查询服务器状态接口
local WebStat = oo.singleton(...)

local json = require "lua_parson"

local function rpc_stat()
    return stat:collect()
end

--[[
    curl 127.0.0.1:10003/web_stat
    curl -l --data 'area 2' 127.0.0.1:10003/web_stat
]]
function WebStat:exec(conn, fields, body)
    -- 未指定进程名或者查询的是当前进程
    if "" == body or body == g_app.name then
        local total_stat = g_stat.collect()

        local ctx = json.encode(total_stat)

        return HTTP.OK_NIL, ctx
    end

    local app = string.split(body, " ", true)

    -- 通过rpc获取其他进程数据
    local app_type = APP[string.upper(app[1])]
    if not app_type then
        PRINTF("invalid app name: %s", app[1])
        return HTTP.INVALID, body
    end

    local session = g_app:encode_session(app_type, app[2] or 1, g_app.id)

    -- TODO:这个rpc调用有问题，不能引用conn为up value的，conn可能会被客户端断开
    g_rpc:proxy(function(ecode, ctx)
        return
            g_httpd:do_return(conn, 0 == ecode, HTTP.OK_NIL, json.encode(ctx))
    end):call(session, rpc_stat)

    return HTTP.PENDING -- 阻塞等待数据返回
end

reg_func("rpc_stat", rpc_stat)

local wst = WebStat()

return wst
