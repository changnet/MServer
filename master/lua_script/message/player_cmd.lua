-- 玩家相关协议处理

local CS = CS
local message_mgr = require "message/message_mgr"
local network_mgr = require "network/network_mgr"

local function player_login( clt_conn,pkt )
    vd( pkt )
end


-- 这里注册系统模块的协议处理
message_mgr:clt_register( CS.PLAYER_LOGIN,player_login,true )