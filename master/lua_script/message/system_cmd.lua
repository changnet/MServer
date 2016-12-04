
local SS = SS
local message_mgr = require "message/message_mgr"
local network_mgr = require "network/network_mgr"

-- 这里注册系统模块的协议处理

message_mgr:srv_register( SS.CLT,message_mgr.clt_dispatcher )
message_mgr:srv_register( SS.REG,network_mgr.srv_authenticate )
-- msg:srv_register( SS.RPC,msg. )
