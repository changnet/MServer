-- 处理一些系统消息

-- 收到另一个服务器主动同步
local function srv_reg(srv_conn, pkt)
    if not g_srv_mgr:srv_register(srv_conn, pkt) then return false end
    srv_conn:authorized(pkt)

    Cmd.sync_cmd(srv_conn)
    srv_conn:send_pkt(SYS.SYNC_DONE, {})

    PRINTF("%s register succes:session %d", srv_conn:conn_name(),
           srv_conn.session)
end

-- 同步对方指令数据
local function srv_cmd_sync(srv_conn, pkt)
    Cmd.on_sync_cmd(srv_conn, pkt)
end

-- 对方服务器数据同步完成
local function srv_sync_done(srv_conn, pkt)
    g_app:one_initialized(string.lower(srv_conn:base_name()), 1)
end

-- 服务器之间心跳包
local beat_pkt = {response = false}
local function srv_beat(srv_conn, pkt)
    if pkt.response then srv_conn:send_pkt(SYS.BEAT, beat_pkt) end

    -- 在这里不用更新自己的心跳，因为在on_command里已自动更新
end

-- 其他服务器通过rpc调用gm
local function rpc_gm(where, cmd, ...)
    g_gm:raw_exec(where, nil, cmd, ...)
end

reg_func("rpc_gm", rpc_gm)

-- 这里注册系统模块的协议处理
Cmd.reg_srv(SYS.BEAT, srv_beat)
Cmd.reg_srv(SYS.REG, srv_reg, true)
Cmd.reg_srv(SYS.CMD_SYNC, srv_cmd_sync)
Cmd.reg_srv(SYS.SYNC_DONE, srv_sync_done)
