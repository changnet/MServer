-- 机器人管理
AndroidMgr = {}

local Android = require "android.android"
local this = global_storage("AndroidMgr", {
    android = {}
})

local sc_cmd = {}

-- 注册指令处理
function AndroidMgr.reg(cmd, handler)
    -- local cfg = Cmd.CS[cmd.i]
    -- if not cfg or not cfg.s then
    --     printf("AndroidMgr.cmd reg no such cmd:%d", cmd.i)
    --     return
    -- end

    sc_cmd[cmd.i] = handler
end

function on_cmd(conn, cmd, errno, ...)
    local handler = sc_cmd[cmd]

    if not handler then
        -- android_cmd:dump_pkt( ... )
        -- elog("android on cmd no handler found:%d", cmd)
        return
    end

    handler(conn.entity, errno, ...)
end

-- 开始执行机器人测试
function AndroidMgr.start()
    local srv_index = tonumber(g_app.index) -- 平台
    local id = tonumber(g_app.id) -- 服务器
    for index = 1, id do
        local idx = (srv_index << 16) | index
        local android = Android(idx)
        this.android[idx] = android

        android.conn.on_cmd = on_cmd
    end

    -- 剩下的由routine定时执行
end

function AndroidMgr.routine(ms_now)
    for _, android in pairs(this.android) do android:routine(ms_now) end
end

return AndroidMgr
