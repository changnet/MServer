-- android.lua
-- 2015-10-05
-- xzc
-- 机器人对象

local Android = oo.class(...)

local CsConn = require "network.cs_conn"

-- 构造函数
function Android:__init(index)
    self.index = index

    self.ai = g_ai_mgr:new(self, 1, 1) -- 加载AI

    self.conn = CsConn()
    self.conn.entity = self
end

-- 发送数据包
function Android:send_pkt(cmd, pkt)
    self.conn:send_pkt(cmd, pkt)
end

function Android:routine(ms_now)
    self.ai:routine(ms_now)
end

return Android
