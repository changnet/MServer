-- 消息管理

local util = require "util"
local network_mgr = network_mgr -- 这个是C++底层的网络管理对象

local stat = g_stat_mgr:get("cmd")

local CS = load_global_define("proto.auto_cs", true)
local SS = load_global_define("proto.auto_ss", true)

local cs_map = {}
local ss_map = {}

-- 对于CS、SS数据包，因为要实现现自动转发，在注册回调时设置,因为要记录sesseion
-- SC数据包则需要在各个进程设置到C++，这样就能在所有进程发协议给客户端
for _, m in pairs(CS) do
    for _, mm in pairs(m) do
        if mm.s then
            -- 对于protobuf，使用"player.SLogin"这种结构
            -- 但对于flatbuffers，是文件名 ＋ 结构名，所以要拆分
            local p = string.match(mm.s, "^(%w+)%.%w+$")
            network_mgr:set_sc_cmd(mm.i, p, mm.s, 0, 0)
        end

        cs_map[mm.i] = mm
    end
end

for _, m in pairs(SS) do for _, mm in pairs(m) do ss_map[mm.i] = mm end end

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
function Cmd.load_schema(schema_type, path, priority, suffix)
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

    PRINTF("load %d scehma files, time %dms", count, ev:real_ms_time() - tm)

    return count > 0
end

-- 注册客户端协议处理
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

-- 注册服务器协议处理
-- @param noauth 处理此协议时，不要求该链接可信
function Cmd.reg_srv(cmd, handler, noreg, noauth)
    local i = cmd.i

    ss_handler[i] = {
        noauth = noauth,
        handler = assert(handler)
    }

    network_mgr:set_ss_cmd(i, string.match(cmd.s, "^(%w+)%.%w+$"), cmd.s, 0,
                           SESSION)
end

-- 本进程需要注册的指令
function Cmd:command_pkt()
    local pkt = {}
    pkt.clt_cmd = self:clt_cmd()

    return pkt
end

-- 发分服务器协议
function Cmd:srv_dispatch(srv_conn, cmd, ...)
    local cfg = self.ss[cmd]

    local handler = cfg.handler
    if not cfg.handler then
        return ERROR("srv_dispatch:cmd %d no handle function found", cmd)
    end

    if not srv_conn.auth and not cfg.noauth then
        return ERROR("srv_dispatch:try to call auth cmd %d", cmd)
    end

    if self.cmd_perf then
        local beg = ev:real_ms_time()
        handler(srv_conn, ...)
        return self:update_statistic(self.ss_stat, cmd, ev:real_ms_time() - beg)
    else
        return handler(srv_conn, ...)
    end
end

-- 分发协议
function Cmd.clt_dispatch(clt_conn, cmd, ...)
    local cfg = cs_handler[cmd]
    if not cfg then
        return ERROR("clt_dispatch:cmd %d no handle function found", cmd)
    end

    if not clt_conn.auth and not cfg.noauth then
        return ERROR("clt_dispatch:try to call auth cmd %d", cmd)
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
function Cmd:clt_dispatch_ex(srv_conn, pid, cmd, ...)
    local cfg = self.cs[cmd]

    local handler = cfg.handler
    if not cfg.handler then
        return ERROR("clt_dispatch_ex:cmd %d no handle function found", cmd)
    end

    -- 判断这个服务器连接是已认证的
    if not srv_conn.auth then
        return ERROR("clt_dispatch_ex:srv conn not auth,cmd %d", cmd)
    end

    -- 判断这个玩家是已认证的
    if not cfg.noauth and not self.auth_pid[pid] then
        return
            ERROR("clt_dispatch_ex:player not auth,pid [%d],cmd %d", pid, cmd)
    end

    if self.cmd_perf then
        local beg = ev:real_ms_time()
        handler(srv_conn, pid, ...)
        return
            self:update_statistic(self.css_stat, cmd, ev:real_ms_time() - beg)
    else
        return handler(srv_conn, pid, ...)
    end
end

-- 获取当前进程处理的客户端指令
function Cmd:clt_cmd()
    local cmds = {}
    for cmd, cfg in pairs(self.cs) do
        if cfg.handler then table.insert(cmds, cmd) end
    end

    return cmds
end

-- 注册其他服务器指令,以实现协议自动转发
function Cmd:other_cmd_register(srv_conn, pkt)
    local base_name = srv_conn:base_name()
    -- 同一类服务，他们的协议是一样的，只不过需要做动态转发，无需再注册一次
    if self.app_reg[base_name] then
        PRINTF("cmd from %s,already register", srv_conn:conn_name())
        return true
    end

    self.app_reg[base_name] = true

    local mask = 0
    local app_setting = g_setting[base_name]
    if app_setting.process and app_setting.process > 1 then
        mask = 2 -- 标记为需要动态转发，见C++ MK_DYNAMIC定义
    end

    local session = srv_conn.session

    -- 记录该服务器所处理的cs指令
    for _, cmd in pairs(pkt.clt_cmd or {}) do
        local _cfg = self.cs[cmd]
        ASSERT(_cfg, "other_cmd_register no such clt cmd", cmd)
        if not g_app.ok then -- 启动的时候检查是否重复，热更操作则直接覆盖
            ASSERT(_cfg, "other_cmd_register clt cmd register conflict", cmd)
        end

        _cfg.session = session
        local raw_cfg = cs_map[cmd]
        network_mgr:set_cs_cmd(cmd, string.match(raw_cfg.c, "^(%w+)%.%w+$"),
                               raw_cfg.c, mask, session)
    end

    PRINTF("register cmd from %s", srv_conn:conn_name())

    return true
end

return Cmd
