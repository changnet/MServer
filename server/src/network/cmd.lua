-- 消息管理

local util = require "util"
local network_mgr = network_mgr -- 这个是C++底层的网络管理对象

local stat = g_stat_mgr:get("cmd")

local CS = require_define("proto.auto_cs", true)
local SS = require_define("proto.auto_ss", true)

local app_reg = {} -- 记录哪些服务器已注册过协议

local SESSION = g_app.session
local auth_pid = g_authorize:get_player_data()

-- 指令注册及分发模块
Cmd = {}

local cs_handler = {} -- 注册的客户端回调函数
local ss_handler = {} -- 注册的服务器之间通信回调

-- 加载协议描述文件，如protobuf、flatbuffers
-- @param schema_type 类型，如 CDC_PROTOBUF
-- @param path 协议描述文件路径，采用linux的路径，如/home/test
-- @param priority 优先加载的文件数组，如果没有顺序依赖可以不传
-- @param suffix 文件名后缀
local function load_schema(schema_type, path, priority, suffix)
    local tm = ev:real_ms_time()
    if g_app.ok then network_mgr:reset_schema(schema_type) end

    local count = 0

    -- 注意：pbc中如果一个pb文件引用了另一个pb文件中的message，则另一个文件必须优先加载
    local loaded = {}
    if priority then
        for _, file in pairs(priority) do
            loaded[file] = true
            count = count + 1
            if 0 ~= network_mgr:load_one_schema_file(schema_type, file) then
                PRINTF("fail to load %s", file)
                return false
            end
        end
    end

    local files = util.ls(path)
    for _, file in pairs(files or {}) do
        if not loaded[file] and string.end_with(file, suffix) then
            count = count + 1
            if 0 ~= network_mgr:load_one_schema_file(schema_type, file) then
                PRINTF("fail to load %s", file)
                return false
            end
        end
    end

    -- 把服务器发包打包协议数据时所使用的schama和协议号绑定
    -- 对于CS、SS数据包，因为要实现现自动转发，在注册回调时设置,因为要记录sesseion
    -- SC数据包则需要在各个进程设置到C++，这样就能在所有进程发协议给客户端
    for _, m in pairs(CS) do
        for _, mm in pairs(m) do
            if mm.s then
                -- 对于protobuf，使用"player.SLogin"这种结构
                -- 但对于flatbuffers，是文件名 ＋ 结构名，所以要拆分
                local package = string.match(mm.s, "^(%w+)%.%w+$")
                network_mgr:set_sc_cmd(mm.i, package, mm.s, 0, 0)
            end
        end
    end

    PRINTF("load %d scehma files, time %dms", count, ev:real_ms_time() - tm)

    return count > 0
end

function Cmd.load_protobuf()
    return load_schema(network_mgr.CDC_PROTOBUF, "../pb", nil, "pb")
end

function Cmd.load_flatbuffers()
end

-- 注册客户端协议回调
-- @param noauth 处理此协议时，不要求该链接可信
function Cmd.reg(cmd, handler, noauth)
    local i = cmd.i

    cs_handler[i] = {
        noauth = noauth,
        handler = assert(handler)
    }

    network_mgr:set_cs_cmd(i, string.match(cmd.c, "^(%w+)%.%w+$"), cmd.c, 0,
                           SESSION)
end

-- 注册客户端协议回调，回调时第一个参数为player对象，仅在world进程有效
function Cmd.reg_player(cmd, handler, noauth)
    Cmd.reg(cmd, handler, noauth)

    cs_handler[cmd.i].p = true
end

-- 注册服务器协议处理
-- @param noauth 处理此协议时，不要求该链接可信
function Cmd.reg_srv(cmd, handler, noauth)
    local i = cmd.i

    ss_handler[i] = {
        noauth = noauth,
        handler = assert(handler)
    }

    network_mgr:set_ss_cmd(i, string.match(cmd.s, "^(%w+)%.%w+$"), cmd.s, 0,
                           SESSION)
end

-- 向其他进程同步本进程需要注册的客户端指令
function Cmd:sync_cmd(conn)
    -- 如果本身就是网关，那么不用同步任何数据
    -- 其他进程需要同步该进程所需要的客户端指令，以实现指令自动分发到对应的进程

    if not conn then
        conn = g_network_mgr:get_conn_by_name("gateway")
    end

    local cmds = {}
    for cmd, cfg in pairs(self.cs) do
        if cfg.handler then table.insert(cmds, cmd) end
    end
    conn:send_pkt(SYS.CMD_SYNC, {
        clt_cmd = cmds
    })
end


-- 注册其他服务器指令,以实现协议自动转发
function Cmd.on_sync_cmd(srv_conn, pkt)
    local base_name = srv_conn:base_name()

    -- 同一类服务，他们的协议是一样的(例如多个场景服务器)，无需再注册一次
    -- 其实重复注册也不会有问题，但是起服的时候可能会触发协议重复注册的检测
    if app_reg[base_name] then
        PRINTF("cmd from %s,already register", srv_conn:conn_name())
        return true
    end

    app_reg[base_name] = true

    local mask = 0
    local app_setting = g_setting[base_name]
    if app_setting.process and app_setting.process > 1 then
        mask = 2 -- 标记为需要动态转发，见C++ MK_DYNAMIC定义
    end

    local session = srv_conn.session

    -- 记录该服务器所处理的cs指令
    for _, cmd in pairs(pkt.clt_cmd or {}) do
        local cfg = cs_handler[cmd]
        ASSERT(cfg, "other_cmd_register no such clt cmd", cmd)
        -- 仅在启动的时候检查是否重复，热更则直接覆盖
        if not g_app.ok then
            ASSERT(cfg, "other_cmd_register clt cmd register conflict", cmd)
        end

        -- 这个协议最终需要转发的，并不需要当前进程解析，所以不需要绑定schema文件
        network_mgr:set_cs_cmd(cmd, "", "", mask, session)
    end

    PRINTF("register cmd from %s", srv_conn:conn_name())

    return true
end


-- 发分服务器协议
function Cmd.dispatch_srv(srv_conn, cmd, ...)
    local cfg = ss_handler[cmd]

    if not srv_conn.auth and not cfg.noauth then
        return ERROR("dispatch_srv:try to call auth cmd %d", cmd)
    end

    local handler = cfg.handler
    if stat then
        --local beg = ev:real_ms_time()
        handler(srv_conn, ...)
        -- return stat:update_statistic(self.cs_stat, cmd, ev:real_ms_time() - beg)
    else
        return handler(srv_conn, ...)
    end
end

-- 分发协议
function Cmd.dispatch_clt(clt_conn, cmd, ...)
    local cfg = cs_handler[cmd]
    if not cfg then
        return ERROR("dispatch_clt:cmd %d no handle function found", cmd)
    end

    if not clt_conn.auth and not cfg.noauth then
        return ERROR("dispatch_clt:try to call auth cmd %d", cmd)
    end

    local handler = cfg.handler
    if stat then
        --local beg = ev:real_ms_time()
        handler(clt_conn, ...)
        -- return stat:update_statistic(self.cs_stat, cmd, ev:real_ms_time() - beg)
    else
        return handler(clt_conn, ...)
    end
end

-- 分发网关转发的客户端协议
function Cmd.dispatch_css(srv_conn, pid, cmd, ...)
    local cfg = cs_handler[cmd]

    if not cfg then
        return ERROR("dispatch_css:cmd %d no handle function found", cmd)
    end

    -- 判断这个服务器连接是已认证的
    if not srv_conn.auth then
        return ERROR("dispatch_css:srv conn not auth,cmd %d", cmd)
    end

    -- 判断这个玩家是已认证的
    if not cfg.noauth and not auth_pid[pid] then
        return
            ERROR("dispatch_css:player not auth,pid [%d],cmd %d", pid, cmd)
    end

    local handler = cfg.handler
    if stat then
        -- local beg = ev:real_ms_time()
        handler(srv_conn, pid, ...)
        -- return
        --     self:update_statistic(self.css_stat, cmd, ev:real_ms_time() - beg)
    else
        return handler(srv_conn, pid, ...)
    end
end

return Cmd
