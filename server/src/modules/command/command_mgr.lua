-- 消息管理

local network_mgr = network_mgr -- 这个是C++底层的网络管理对象

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
            network_mgr:set_sc_cmd(mm.i, p, mm.s ,0, 0)
        end

        cs_map[mm.i] = mm
    end
end

for _, m in pairs(SS) do
    for _, mm in pairs(m) do
        ss_map[mm.i] = mm
    end
end

local SESSION = g_app.session

local g_rpc   = g_rpc

local CommandMgr = oo.singleton( ... )

function CommandMgr:__init()
    self.ss = {} -- 记录服务器之间回调函数
    for i in pairs( ss_map ) do
        self.ss[i] = {}
    end

    self.cs = {} -- 记录客户端-服务器之间的回调函数
    for i, mm in pairs( cs_map ) do
        if mm.c then self.cs[i] = {} end
    end

    self.app_reg = {} -- 哪些进程已注册过了

    -- 引用授权数据，防止频繁调用函数
    self.auth_pid = g_authorize:get_player_data()

    -- 状态统计数据
    self.cs_stat  = {}
    self.ss_stat  = {}
    self.css_stat = {}
    self.stat_tm  = ev:time()
    self.cmd_perf = g_setting.cmd_perf

    self:check_timer() -- 开启状态统计定时器
end

-- 检测是否需要开启定时器定时写统计信息到文件
function CommandMgr:check_timer()
    if not self.cmd_perf and self.timer then
        g_timer_mgr:stop( self.timer )

        self.timer = nil
        return
    end

    if self.cmd_perf and not self.timer then
        self.timer = g_timer_mgr:interval(1800000,1800000,-1,self,self.do_timer)
    end
end

function CommandMgr:do_timer()
    self:serialize_statistic( true )
end

-- 设置统计log文件
function CommandMgr:set_statistic( perf,reset )
    -- 如果之前正在统计，先写入旧的
    if self.cmd_perf and ( not perf or reset ) then
        self:serialize_statistic()
    end

    -- 如果之前没在统计，或者强制重置，则需要重设stat_tm
    if not self.cmd_perf or reset then
        self.stat = {}
        self.stat_tm = ev.time()
    end

    self.cmd_perf = perf

    self:check_timer()
end

-- 更新耗时统计
function CommandMgr:update_statistic( stat_list,cmd,ms )
    local stat = stat_list[cmd]
    if not stat then
        stat = { ms = 0, ts = 0, max = 0, min = 0}
        stat_list[cmd] = stat
    end

    stat.ms = stat.ms + ms
    stat.ts = stat.ts + 1
    if ms > stat.max then stat.max = ms end
    if 0 == stat.min or ms < stat.min then stat.min = ms end
end

-- 写入耗时统计到文件
function CommandMgr:raw_serialize_statistic( path,stat_name,stat_list )
    local stat_cmd = {}
    for k in pairs( stat_list ) do table.insert( stat_cmd,k ) end

    -- 按名字排序，方便对比查找
    table.sort( stat_cmd )

    g_log_mgr:raw_file_printf( path,"%s",stat_name )

    for _,cmd in pairs( stat_cmd ) do
        local stat = stat_list[cmd]
        g_log_mgr:raw_file_printf( path,
            "%-16s %-16d %-16d %-16d %-16d %-16d",
            string.format("%2d-%d",self:dismantle_cmd(cmd)),
            stat.ts,stat.ms,stat.max,stat.min,math.ceil(stat.ms/stat.ts))
    end
end

-- 写入耗时统计到文件
function CommandMgr:serialize_statistic( reset )
    if not self.cmd_perf then return false end

    local path = string.format( "%s_%s",self.cmd_perf,g_app.name )

    g_log_mgr:raw_file_printf( path,
        "%s ~ %s:",time.date(self.stat_tm),time.date(ev:time()))
    -- 指令 调用次数 总耗时(毫秒) 最大耗时 最小耗时 平均耗时
    g_log_mgr:raw_file_printf( path,
        "%-16s %-16s %-16s %-16s %-16s %-16s",
        "cmd","count","msec","max","min","avg" )

    self:raw_serialize_statistic( path,"cs_cmd:",self.cs_stat )
    self:raw_serialize_statistic( path,"ss_cmd:",self.ss_stat )
    self:raw_serialize_statistic( path,"css_cmd:",self.css_stat )

    g_log_mgr:raw_file_printf(
        path,"%s.%d end %s",g_app.name,g_app.index,"\n\n" )
    if reset then
        self.cs_stat  = {}
        self.ss_stat  = {}
        self.css_stat = {}
        self.stat_tm  = ev:time()
    end

    return true
end

-- 加载二进制flatbuffers schema文件
function CommandMgr:load_schema()
    -- 注意：pbc中如果一个pb文件引用了另一个pb文件中的message，则另一个文件必须优先加载
    local pfs = network_mgr:load_one_schema( network_mgr.CDC_PROTOBUF,"../pb" )
    PRINTF( "load protocol schema:%d",pfs )

    local ffs = network_mgr:load_one_schema( network_mgr.CDC_FLATBUF,"../fbs" )
    PRINTF( "load flatbuffers schema:%d",ffs )

    return (pfs >= 0 and ffs >= 0)
end

-- 拆分协议为模块 + 功能
function CommandMgr:dismantle_cmd( cmd )
    local f = cmd & 0x000F
    local m = cmd >> 0x0008

    return m,f
end

-- 注册客户端协议处理
function CommandMgr:clt_register( cmd,handler,noauth )
    local i = cmd.i
    local cfg = self.cs[i]
    if not cfg then
        return error( "clt_register:cmd not define: " .. i)
    end

    cfg.handler = handler
    cfg.noauth  = noauth  -- 处理此协议时，不要求该链接可信

    network_mgr:set_cs_cmd(
        i, string.match(cmd.c, "^(%w+)%.%w+$"), cmd.c,0,SESSION)
end

-- 注册服务器协议处理
-- @noauth    -- 处理此协议时，不要求该链接可信
-- @noreg     -- 此协议不需要注册到其他服务器
function CommandMgr:srv_register( cmd,handler,noreg,noauth )
    local i = cmd.i
    local cfg = self.ss[i]
    if not cfg then
        return error( "srv_register:cmd not define: " .. i)
    end

    cfg.handler  = handler
    cfg.noauth   = noauth
    cfg.noreg    = noreg

    network_mgr:set_ss_cmd(
        i, string.match(cmd.s, "^(%w+)%.%w+$"), cmd.s,0,SESSION )
end

-- 本进程需要注册的指令
function CommandMgr:command_pkt()
    local pkt = {}
    pkt.clt_cmd = self:clt_cmd()
    pkt.srv_cmd = self:srv_cmd()
    pkt.rpc_cmd = g_rpc:rpc_cmd()

    return pkt
end

-- 发分服务器协议
function CommandMgr:srv_dispatch( srv_conn,cmd,... )
    local cfg = self.ss[cmd]

    local handler = cfg.handler
    if not cfg.handler then
        return ERROR( "srv_dispatch:cmd [%d-%d] no handle function found",
            self:dismantle_cmd(cmd) )
    end

    if not srv_conn.auth and not cfg.noauth then
        return ERROR( "clt_dispatch:try to call auth cmd [%d-%d]",
            self:dismantle_cmd(cmd) )
    end

    if self.cmd_perf then
        local beg = ev:real_ms_time()
        handler( srv_conn,... )
        return self:update_statistic( self.ss_stat,cmd,ev:real_ms_time() - beg )
    else
        return handler( srv_conn,... )
    end
end

-- 分发协议
function CommandMgr:clt_dispatch( clt_conn,cmd,... )
    local cfg = self.cs[cmd]

    local handler = cfg.handler
    if not cfg.handler then
        return ERROR( "clt_dispatch:cmd [%d-%d] no handle function found",
            self:dismantle_cmd(cmd) )
    end

    if not clt_conn.auth and not cfg.noauth then
        return ERROR( "clt_dispatch:try to call auth cmd [%d-%d]",
            self:dismantle_cmd(cmd) )
    end

    if self.cmd_perf then
        local beg = ev:real_ms_time()
        handler( clt_conn,... )
        return self:update_statistic( self.cs_stat,cmd,ev:real_ms_time() - beg )
    else
        return handler( clt_conn,... )
    end
end

-- 分发网关转发的客户端协议
function CommandMgr:clt_dispatch_ex( srv_conn,pid,cmd,... )
    local cfg = self.cs[cmd]

    local handler = cfg.handler
    if not cfg.handler then
        return ERROR( "clt_dispatch_ex:cmd [%d-%d] no handle function found",
            self:dismantle_cmd(cmd) )
    end

    -- 判断这个服务器连接是已认证的
    if not srv_conn.auth then
        return ERROR( "clt_dispatch_ex:srv conn not auth,cmd [%d-%d]",
            self:dismantle_cmd(cmd) )
    end

    -- 判断这个玩家是已认证的
    if not cfg.noauth and not self.auth_pid[pid] then
        return ERROR(
            "clt_dispatch_ex:player not auth,pid [%d],cmd [%d-%d]",
            pid,self:dismantle_cmd(cmd) )
    end

    if self.cmd_perf then
        local beg = ev:real_ms_time()
        handler( srv_conn,pid,... )
        return self:update_statistic(self.css_stat,cmd,ev:real_ms_time() - beg)
    else
        return handler( srv_conn,pid,... )
    end
end

-- 获取当前进程处理的客户端指令
function CommandMgr:clt_cmd()
    local cmds = {}
    for cmd,cfg in pairs( self.cs ) do
        if cfg.handler then table.insert( cmds,cmd ) end
    end

    return cmds
end

-- 获取当前进程处理的服务端指令
function CommandMgr:srv_cmd()
    local cmds = {}
    for cmd,cfg in pairs( self.ss ) do
        if cfg.handler and not cfg.noreg then table.insert( cmds,cmd ) end
    end

    return cmds
end

-- 注册其他服务器指令,以实现协议自动转发
function CommandMgr:other_cmd_register( srv_conn,pkt )
    local base_name = srv_conn:base_name()
    -- 同一类服务，他们的协议是一样的，只不过需要做动态转发，无需再注册一次
    if self.app_reg[base_name] then
        PRINTF( "cmd from %s,already register",srv_conn:conn_name() )
        return true
    end

    self.app_reg[base_name] = true

    local mask = 0
    local multi_process = false
    local app_setting = g_setting[base_name]
    if app_setting.process and app_setting.process > 1 then
        mask = 2 -- 标记为需要动态转发，见C++ MK_DYNAMIC定义
        multi_process = true
    end

    local session = srv_conn.session

    -- 记录该服务器所处理的cs指令
    for _,cmd in pairs( pkt.clt_cmd or {} ) do
        local _cfg = self.cs[cmd]
        ASSERT( _cfg,"other_cmd_register no such clt cmd", cmd )
        if not g_app.ok then -- 启动的时候检查是否重复，热更操作则直接覆盖
            ASSERT( _cfg,"other_cmd_register clt cmd register conflict", cmd )
        end

        _cfg.session = session
        local raw_cfg = cs_map[cmd]
        network_mgr:set_cs_cmd(
            cmd, string.match(raw_cfg.c, "^(%w+)%.%w+$"),raw_cfg.c,mask,session)
    end

    -- 记录该服务器所处理的ss指令
    for _,cmd in pairs( pkt.srv_cmd or {} ) do
        local _cfg = self.ss[cmd]
        ASSERT( _cfg,"other_cmd_register no such srv cmd", cmd )
        ASSERT( _cfg,"other_cmd_register srv cmd register conflict", cmd )

        _cfg.session = session
        local raw_cfg = ss_map[cmd]
        network_mgr:set_ss_cmd(
            cmd, string.match(raw_cfg.s, "^(%w+)%.%w+$"),raw_cfg.s,0,session)
    end

    -- 记录该服务器所处理的rpc指令
    for _,cmd in pairs( pkt.rpc_cmd or {} ) do
        g_rpc:register( cmd,session,multi_process )
    end

    PRINTF( "register cmd from %s",srv_conn:conn_name() )

    return true
end

local command_mgr = CommandMgr()

return command_mgr
