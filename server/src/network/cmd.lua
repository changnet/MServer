-- 指令注册及分发模块
Cmd = {}

local util = require "util"
local network_mgr = network_mgr -- 这个是C++底层的网络管理对象

local stat = g_stat_mgr:get("cmd")

Cmd.CS = require_define("proto.auto_cs")
Cmd.SS = require_define("proto.auto_ss")

local app_reg = {} -- 记录哪些服务器已注册过协议

local last_cmd = nil -- 上一次执行的cmd
local last_connection = nil -- 上一次回调的连接，通常用于快速回包

local SESSION = g_app.session

-- 以玩家id为key，是否已认证该玩家
local auth = global_storage("Auth")

local cs_handler = {} -- 注册的客户端回调函数
local ss_handler = {} -- 注册的服务器之间通信回调

-- 当前使用的描述文件类型
Cmd.SCHEMA_TYPE = network_mgr.CDT_PROTOBUF

-- 拆分描述文件
local function split_schema(name, schema_type)
    schema_type = schema_type or Cmd.SCHEMA_TYPE

    -- 描述文件分为包名package，具体的功能object
    -- 对于protobuf，包名和功能是拼在一起的，使用点号分隔
    -- 然后package名是不需要的，这里只是方便调试，如"player.SLogin"
    if Cmd.SCHEMA_TYPE == network_mgr.CDT_PROTOBUF then
        local package = string.match(name, "^(%w+)%.%w+$")
        return package, name
    end

    -- 但对于flatbuffers，是文件名 ＋ 结构名
    -- 如 "player.SLogin" 表示 player.bfbs 文件中的 SLogin
    -- 如果在文件中使用了namespace(Test，则需要拆分为 player.bfbs 和 Test.SLogin
    -- 现在暂时不支持namespace，有文件名应该足够了
    if Cmd.SCHEMA_TYPE == network_mgr.CDT_FLATBUF then
        local package, object = string.match(name, "^(%w+)%.(%w+)$")
        return package .. ".bfbs", object
    end

    assert(false)
end

-- 加载协议描述文件，如protobuf、flatbuffers
-- @param schema_type 类型，如 CDT_PROTOBUF
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
                printf("fail to load %s", file)
                return false
            end
        end
    end

    local files = util.ls(path)
    for _, file in pairs(files or {}) do
        if not loaded[file] and string.end_with(file, suffix) then
            count = count + 1
            if 0 ~= network_mgr:load_one_schema_file(schema_type, file) then
                printf("fail to load %s", file)
                return false
            end
        end
    end

    -- 把服务器之间通信打包协议数据时所使用的schama和协议号绑定
    for _, m in pairs(Cmd.SS) do
        for _, mm in pairs(m) do
            -- 目前服务器之间的连接默认使用protobuf
            local package, object = split_schema(mm.s, network_mgr.CDT_PROTOBUF)
            network_mgr:set_ss_cmd(mm.i, package, object, 0, SESSION)
        end
    end

    -- 把服务器发往客户打包协议数据时所使用的schama和协议号绑定
    -- 对于CS，因为要实现现自动转发，在注册回调时设置,因为要记录sesseion
    -- SC数据包则需要在各个进程设置到C++，这样就能在所有进程发协议给客户端
    for _, m in pairs(Cmd.CS) do
        for _, mm in pairs(m) do
            if mm.s then
                local package, object = split_schema(mm.s)
                network_mgr:set_sc_cmd(mm.i, package, object, 0, 0)
            end

            -- 注册客户端发往服务器的指令配置（机器人会用到）
            -- 服务端用的话是在注册回调时根据服务器session自动分发
            if mm.c and Cmd.USE_CS_CMD then
                local package, object = split_schema(mm.c)
                network_mgr:set_cs_cmd(mm.i, package, object, 0, 0)
            end
        end
    end

    printf("load %d scehma files, time %d ms", count, ev:real_ms_time() - tm)

    return count > 0
end

-- 加载protobuf的pb描述文件
function Cmd.load_protobuf()
    return load_schema(network_mgr.CDT_PROTOBUF, "../pb", nil, "pb")
end

-- 加载flatbuffers的描述文件
function Cmd.load_flatbuffers()
    return load_schema(network_mgr.CDT_FLATBUF, "../fbs", nil, "bfbs")
end

-- 获取上一次回调的网络连接id
function Cmd.last_conn()
    return last_connection
end

-- 注册客户端协议回调
-- @param noauth 处理此协议时，不要求该链接可信
function Cmd.reg(cmd, handler, noauth)
    local i = cmd.i

    cs_handler[i] = {
        noauth = noauth,
        handler = assert(handler)
    }

    local package, object = split_schema(cmd.c)
    network_mgr:set_cs_cmd(i, package, object, 0, SESSION)
end

-- 注册客户端协议回调，回调时第一个参数为player对象，仅在world进程有效
function Cmd.reg_player(cmd, handler, noauth)
    Cmd.reg(cmd, handler, noauth)

    cs_handler[cmd.i].t = 1
end

-- 注册客户端协议回调，回调时第一个参数为玩家在场景的实体对象，仅在场景进程有效
function Cmd.reg_entity(cmd, handler, noauth)
    Cmd.reg(cmd, handler, noauth)

    cs_handler[cmd.i].t = 2
end

-- 认证该玩家
function Cmd.auth(pid, b)
    -- 认证并不是说通过密码认证，而是进入了游戏
    -- 主要是防止前端乱发协议，比如没有进入游戏就发移动包，就通过这个认证机制来隔离的
    auth[pid] = b
end

-- 注册服务器协议处理
-- @param obj 回调模块，仅支持全局对象
-- @param noauth 处理此协议时，不要求该链接可信
function Cmd.reg_srv(cmd, handler, obj, noauth)
    local i = cmd.i

    ss_handler[i] = {
        obj = obj,
        noauth = noauth,
        handler = assert(handler)
    }

    -- 目前服务器之间的协议取消自动转发功能了，不需要设置session
    -- network_mgr:set_ss_cmd(i, package, object, 0, SESSION)
end

-- 向其他进程同步本进程需要注册的客户端指令
function Cmd.sync_cmd(conn)
    -- 如果本身就是网关，那么不用同步任何数据
    -- 其他进程需要同步该进程所需要的客户端指令，以实现指令自动分发到对应的进程

    if not conn then
        conn = SrvMgr.get_conn_by_session(GSE)
    end

    local cmds = {}
    for cmd, cfg in pairs(cs_handler) do
        if cfg.handler then table.insert(cmds, cmd) end
    end
    conn:send_pkt(SYS.CMD_SYNC, {
        clt_cmd = cmds
    })
end


-- 注册其他服务器指令,以实现协议自动转发
function Cmd.on_sync_cmd(srv_conn, pkt)
    local base_name = string.lower(srv_conn:base_name())

    -- 同一类服务，他们的协议是一样的(例如多个场景服务器)，无需再注册一次
    -- 其实重复注册也不会有问题，但是起服的时候可能会触发协议重复注册的检测
    if app_reg[base_name] then
        printf("cmd from %s,already register", srv_conn:conn_name())
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
        -- 仅在启动的时候检查是否重复，热更则直接覆盖
        if not g_app.ok then
            assert(not cs_handler[cmd], "on sync cmd conflict", cmd)
        end

        cs_handler[cmd] = {
            session = session
        }
        -- 这个协议最终需要转发的，并不需要当前进程解析，所以不需要绑定schema文件
        network_mgr:set_cs_cmd(cmd, "", "", mask, session)
    end

    printf("register cmd from %s", srv_conn:conn_name())

    return true
end

local function do_handler(handler, ...)
    if stat then
        --local beg = ev:real_ms_time()
        handler( ...)
        -- return stat:update_statistic(self.cs_stat, cmd, ev:real_ms_time() - beg)
    else
        return handler(...)
    end
end

-- 发分服务器协议
function Cmd.dispatch_srv(srv_conn, cmd, ...)
    local cfg = ss_handler[cmd]
    if not cfg then
        elog("dispatch_srv:cmd not found", cmd)
        return
    end

    if not srv_conn.auth and not cfg.noauth then
        return elog("dispatch_srv:try to call auth cmd", cmd)
    end

    last_cmd = cmd
    last_connection = srv_conn
    return do_handler(cfg.handler, srv_conn, ...)
end

-- 分发协议
function Cmd.dispatch_clt(clt_conn, cmd, ...)
    local cfg = cs_handler[cmd]
    if not cfg then
        return elog("dispatch_clt:cmd %d no handle function found", cmd)
    end

    if not clt_conn.auth and not cfg.noauth then
        return elog("dispatch_clt:try to call auth cmd %d", cmd)
    end

    last_cmd = cmd
    last_connection = clt_conn
    return do_handler(cfg.handler, ...)
end

-- 分发网关转发的客户端协议
function Cmd.dispatch_css(srv_conn, pid, cmd, ...)
    local cfg = cs_handler[cmd]

    if not cfg then
        return elogf("dispatch_css:cmd %d no handle function found", cmd)
    end

    -- 判断这个服务器连接是已认证的
    if not srv_conn.auth then
        return elogf("dispatch_css:srv conn not auth,cmd %d", cmd)
    end

    -- 判断这个玩家是已认证的
    if not cfg.noauth and not auth[pid] then
        return
            elogf("dispatch_css:player not auth,pid [%d],cmd %d", pid, cmd)
    end

    last_cmd = cmd
    last_connection = srv_conn
    return do_handler(cfg.handler, pid, ...)
end

-- 生成模块、实体回调函数
function Cmd.make_this_cb()
    local ThisCall = require "modules.system.this_call"

    for cmd, cfg in pairs(cs_handler) do
        local this_cb = ThisCall.make(cfg.handler, cfg.t, "cmd", cmd)
        if this_cb then cfg.handler = this_cb end
    end
end

return Cmd
