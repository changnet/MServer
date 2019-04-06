-- web_stat.lua http 查询服务器状态接口

local Web_stat = oo.singleton( nil,... )

local json = require "lua_parson"
local stat = require "modules.system.statistic"

--[[
    curl 127.0.0.1:10003/web_stat
]]
function Web_stat:exec( fields,body )
    local total_stat = stat.collect()

    local ctx = json.encode(total_stat)

    return HTTPE.OK,ctx
end

local wst = Web_stat()

return wst
