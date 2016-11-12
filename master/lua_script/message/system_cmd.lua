
local SS = SS
local msg = require "message/message_mgr"

-- 这里注册系统模块的协议处理

msg:srv_register( SS.CLT,msg.clt_dispatcher )
-- msg:srv_register( SS.SRV,msg. )
-- msg:srv_register( SS.RPC,msg. )
