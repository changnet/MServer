-- 处理一些系统消息

-- 服务器之间心跳包
local beat_pkt = {response = false}
local function srv_beat(srv_conn, pkt)
    if pkt.response then srv_conn:send_pkt(SYS.BEAT, beat_pkt) end

    -- 在这里不用更新自己的心跳，因为在on_command里已自动更新
end

-- 这里注册系统模块的协议处理
Cmd.reg_srv(SYS.BEAT, srv_beat)
