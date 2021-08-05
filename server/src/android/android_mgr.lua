-- 机器人管理
local AndroidMgr = oo.singleton(...)

require "network.cmd"
local Android = require "android.android"

local sc_cmd = {}

function AndroidMgr:__init()
    self.conn = {}
    self.android = {}
end

-- 注册指令处理
function AndroidMgr:reg(cmd, handler)
    local cfg = Cmd.CS[cmd.i]
    if not cfg or not cfg.s then
        printf("AndroidMgr:cmd reg no such cmd:%d", cmd.i)
        return
    end

    cfg.handler = handler
end

function on_cmd(conn, cmd, errno, ...)
    local cfg = sc_cmd[cmd]
    if not cfg then
        elog("android on cmd no such cmd:%d", cmd)
        return
    end

    if not cfg.handler then
        -- android_cmd:dump_pkt( ... )
        elog("android on cmd  no handler found:%d", cmd)
        return
    end

    cfg.handler(conn.entity, errno, ...)
end

-- 开始执行机器人测试
function AndroidMgr:start()
    -- 加载协议文件
    Cmd.USE_CS_CMD = true
    Cmd.load_protobuf()

    local srv_index = tonumber(g_app.index) -- 平台
    local id = tonumber(g_app.id) -- 服务器
    for index = 1, id do
        local idx = (srv_index << 16) | index
        local android = Android(idx)
        self.android[idx] = android

        android.conn.on_cmd = on_cmd
    end

    -- 剩下的由routine定时执行
end

function AndroidMgr:get_android_by_conn(conn_id)
    return self.conn[conn_id]
end

function AndroidMgr:routine(ms_now)
    for _, android in pairs(self.android) do android:routine(ms_now) end
end

local android_mgr = AndroidMgr()

return android_mgr

