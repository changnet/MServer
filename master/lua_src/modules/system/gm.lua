-- gm.lua
-- 2018-04-11
-- xzc

-- gm处理

local gm_map = {}
local forward_map = {}
local GM = oo.singleton( nil,... )

-- 检测聊天中是否带gm
function GM:chat_gm( player,context )
    if not g_setting.gm or not string.start_with( context,"@" ) then
        return false
    end

    local gm_ctx = string.sub( context,2 ) -- 去掉@

    self:exec( "chat",player,gm_ctx )

    return true
end

-- 转发或者运行gm
function GM:exec( where,player,context )
    -- 分解gm指令(@level 999分解为{@level,999})
    local args = string.split(context," ")
    if self:auto_forward( where,player,cmd,context ) then
        return true
    end

    return self:raw_exec( where,player,table.unpack( args ) )
end

-- 自动转发gm到对应的服务器
-- TODO:暂时不考虑存在多个同名服务器的情况
function GM:auto_forward( where,player,cmd,context )
    local srvname = forward_map[cmd]
    if not srvname or g_app.srvname == srvname then return false end

    local srv_conn = g_network_mgr:get_conn_by_name( srvname )
    if not srv_conn then
        ELOG("gm auto forward no conn found:%s",cmd)
        return true
    end

    local pid = nil
    if player then pid = player:get_pid() end

    g_rpc:call( srv_conn,"rpc_gm",where,pid,context )
    return true
end

-- gm指令运行入口（注意Player对象可能为nil，因为有可能从http接口调用gm）
function GM:raw_exec( where,player,cmd,... )
    -- 防止PLOG函数本身报错，连热更的指令都没法刷了
    xpcall( PLOG,__G__TRACKBACK__,"exec gm:",where,cmd,... )

    -- 优先查找注册过来的gm指令
    local gm_func = gm_map[cmd]
    if gm_func then
        gm_func( player,... )
        return true
    end

    -- 写在gm模块本身的指令
    gm_func = self[cmd]
    if gm_func then
        gm_func( self,player,... )
        return true
    end

    PFLOG( "try to call gm:%s,no such gm",cmd )
    return false
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
    global_hot_fix()
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

local gm = GM()

return gm
