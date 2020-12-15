-- android.lua
-- 2015-10-05
-- xzc

-- 机器人对象

-- websocket opcodes
-- local WS_OP_CONTINUE = 0x0
-- local WS_OP_TEXT     = 0x1
-- local WS_OP_BINARY   = 0x2
-- local WS_OP_CLOSE    = 0x8
-- local WS_OP_PING     = 0x9
-- local WS_OP_PONG     = 0xA

-- -- websocket marks
-- local WS_FINAL_FRAME = 0x10
-- local WS_HAS_MASK    = 0x20

local network_mgr = network_mgr
local Android = oo.class( ... )

-- 构造函数
function Android:__init( index )
    self.index   = index

    self.ai = g_ai_mgr:new( self,1,1 ) -- 加载AI
end

-- 设置连接Id
function Android:set_conn( conn_id )
    if conn_id then
        self.conn_id = conn_id
        return g_android_mgr:set_android_conn(conn_id,self)
    else
        local old_conn = self.conn_id
        self.conn_id = nil
        g_android_mgr:set_android_conn(old_conn,nil)
    end
end

-- 发送数据包
function Android:send_pkt( cmd,pkt )
    -- 使用tcp二进制流
    return network_mgr:send_srv_packet( self.conn_id,cmd.i,pkt )

    -- 使用websocket二进制流
    -- return network_mgr:send_srv_packet(
    --     self.conn_id,cfg[1],WS_OP_BINARY | WS_FINAL_FRAME,pkt )
end

function Android:routine( ms_now )
    self.ai:routine( ms_now )
end

return Android
