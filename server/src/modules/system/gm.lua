-- gm.lua
-- 2018-04-11
-- xzc

-- gm处理

local gm_map = {}
local forward_map = {}
local GM = oo.singleton( ... )

-- 检测聊天中是否带gm
function GM:chat_gm( player,context )
    if not g_setting.gm then return false end

    local gm_ctx = string.sub( context,2 ) -- 去掉@

    self:exec( "chat",player,gm_ctx )

    return true
end

-- 转发或者运行gm
function GM:exec( where,player,context )
    if not string.start_with( context,"@" ) then
        return false,"gm need to start with@:" .. context
    end

    local raw_ctx = string.sub( context,2 ) -- 去掉@

    -- 分解gm指令(@level 999分解为{@level,999})
    local args = string.split(raw_ctx," ")
    if self:auto_forward( where,player,args[1],args ) then
        return true
    end

    return self:raw_exec( where,player,table.unpack( args ) )
end

-- 自动转发gm到对应的服务器
-- TODO:暂时不考虑存在多个同名服务器的情况
function GM:auto_forward( where,player,cmd,args )
    local srvname = forward_map[cmd]
    if not srvname or g_app.srvname == srvname then return false end

    local srv_conn = g_network_mgr:get_conn_by_name( srvname )
    if not srv_conn then
        ERROR("gm auto forward no conn found:%s",cmd)
        return true
    end

    g_rpc:proxy(srv_conn):rpc_gm( g_app.srvname,cmd,table.unpack( args ) )
    return true
end

-- gm指令运行入口（注意Player对象可能为nil，因为有可能从http接口调用gm）
function GM:raw_exec( where,player,cmd,... )
    -- 防止日志函数本身报错，连热更的指令都没法刷了
    xpcall( PRINT,__G__TRACKBACK,"exec gm:",where,cmd,... )

    -- 优先查找注册过来的gm指令
    local gm_func = gm_map[cmd]
    if gm_func then
        local ok,msg = gm_func( player,... )
        if ok == nil then
            return true -- 很多不重要的指令默认不写返回值，认为是成功的
        else
            return ok,msg
        end
    end

    -- 写在gm模块本身的指令
    gm_func = self[cmd]
    if gm_func then
        local ok,msg = gm_func( self,player,... )
        if ok == nil then
            return true -- 很多不重要的指令默认不写返回值，认为是成功的
        else
            return ok,msg
        end
    end

    PRINTF( "try to call gm:%s,no such gm",cmd )
    return false,"no such gm:" .. cmd
end

-- 广播gm
function GM:broadcast( cmd,... )
    -- 仅允许网关广播
    ASSERT( cmd and g_app.srvname == "gateway" )

    -- 目前服务器没有同一个name多开，直接通过名字对比
    for srvname in pairs( SRV_NAME ) do
        if g_app.srvname ~= srvname then
            local srv_conn = g_network_mgr:get_conn_by_name( srvname )
            if srv_conn then
                g_rpc:proxy( srv_conn):rpc_gm( g_app.srvname,cmd,... )
            end
        end
    end
end

-- 注册gm指令
-- @srvname:真正运行该gm的服务器名
function GM:reg_cmd( cmd,gm_func,srvname )
    gm_map[cmd] = gm_func
    forward_map[cmd] = srvname
end

--------------------------------------------------------------------------------

-- 热更新本服，包括脚本、协议、rpc
function GM:hf()
    hot_fix()
end

-- 只热更脚本
function GM:hfs()
    hot_fix_script()
end

-- 全局热更，会更新其他进程
function GM:ghf()
    hot_fix()
    if g_app.srvname == "gateway" then self:broadcast( "hf" ) end
end

-- ping一下服务器间的延迟，看卡不卡
function GM:ping()
    return g_ping:start(1)
end

-- 立刻进一次rpc耗时统计,不带参数表示不重置
-- @rpc_perf 1
function GM:rpc_perf( reset )
    if not g_rpc:serialize_statistic( reset ) then
        return false,"rpc_perf not set,set it with:@set_rpc_perf log/rpc_perf 1"
    end

    if g_app.srvname == "gateway" then self:broadcast( "rpc_perf",reset ) end

    return true
end

-- 设置rpc耗时统计,取消统计不带任何参数
-- @set_rpc_perf "log/rpc_perf" 1
function GM:set_rpc_perf( perf,reset )
    g_rpc:set_statistic( perf,reset )

    if g_app.srvname == "gateway" then
        self:broadcast( "set_rpc_perf",perf,reset )
    end

    return true
end

-- 立刻进一次cmd耗时统计,不带参数表示不重置
-- @cmd_perf 1
function GM:cmd_perf( reset )
    if not g_command_mgr:serialize_statistic( reset ) then
        return false,"rpc_perf not set,set it with:@set_cmd_perf log/cmd_perf 1"
    end

    if g_app.srvname == "gateway" then self:broadcast( "cmd_perf",reset ) end

    return true
end

-- 设置cmd耗时统计,取消统计不带任何参数
-- @set_cmd_perf "log/cmd_perf" 1
function GM:set_cmd_perf( perf,reset )
    g_command_mgr:set_statistic( perf,reset )

    if g_app.srvname == "gateway" then
        self:broadcast( "set_cmd_perf",perf,reset )
    end

    return true
end

-- 设置是否统计gc时间,取消统计不带任何参数
-- @set_gc_stat 1
function GM:set_gc_stat( set,reset )
    ev:set_gc_stat( set and true or false,reset and true or false )
    if g_app.srvname == "gateway" then
        self:broadcast( "set_gc_stat",set,reset )
    end

    return true
end

-- 添加元宝
function GM:add_gold( player,count )
    player:add_gold( tonumber(count),LOG.GM )
end

-- 添加道具
function GM:add_item( player,id,count )
    local bag = player:get_module( "bag" )
    bag:add( tonumber(id),tonumber(count),LOG.GM )
end

-- 邮件测试
function GM:sys_mail( player,title,ctx )
    g_mail_mgr:send_sys_mail( title,ctx )
end

-- 发送邮件测试
function GM:send_mail( player,pid,title,ctx )
    g_mail_mgr:send_mail( tonumber(pid),title,ctx )
end

local gm = GM()

return gm
