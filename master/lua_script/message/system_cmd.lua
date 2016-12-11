
local SS = SS
local message_mgr = require "message/message_mgr"
local network_mgr = require "network/network_mgr"

local function srv_authenticate( conn,pkt )
    return network_mgr:srv_authenticate( conn,pkt )
end

-- 这里注册系统模块的协议处理

message_mgr:srv_register( SS.REG,srv_authenticate )
