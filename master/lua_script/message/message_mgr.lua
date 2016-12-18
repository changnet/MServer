-- 消息管理

local lua_flatbuffers = require "lua_flatbuffers"

-- 协议使用太频繁，放到全局变量
SC,CS = require "message/sc_message"
SS    = require "message/ss_message"

local SC = SC
local CS = CS
local SS = SS

local Message_mgr = oo.singleton( nil,... )

function Message_mgr:__init()
    self.ss = {}
    for _,v in pairs( SS ) do
        self.ss[ v[1] ] = v
    end

    self.cs = {}
    for _,v in pairs( CS or {} ) do
        self.cs[ v[1] ] = v
    end

    self.lfb = lua_flatbuffers()
end

-- 加载二进制flatbuffers schema文件
function Message_mgr:load_schema()
    return self.lfb:load_bfbs_path( "fbs","bfbs" )
end

-- 初始化协议定义
function Message_mgr:init_message()
    -- 这个不能放在文件头部或者__init函数中require
    -- 因为其他文件一般都需要引用message_mgr本身，造成循环require
    require "message/message_header"
end

-- 注册客户端协议处理
function Message_mgr:clt_register( msg,handler )
end

-- 注册服务器协议处理
function Message_mgr:srv_register( cfg,handler )
    if not self.ss[cfg[1]] then
        return error( "srv_register:message not define" )
    end

    cfg.handler = handler
end

-- 注册rpc处理
function Message_mgr:rpc_register( name,handler )
end

-- 分发服务器协议
function Message_mgr:srv_dispatcher( cmd,conn )
    if cmd == SS.CLT then
        return clt_dispatcher( cmd,conn )
    elseif cmd == SS.RPC then
        return -- rpc_dispatcher( cmd,conn )
    end

    -- server to server message handle here
    local cfg = self.ss[cmd]
    if not cfg then
        return ELOG( "srv_dispatcher:message [%d] not define",cmd )
    end

    local handler = cfg.handler
    if not handler then
        return ELOG( "srv_dispatcher:message [%d] define but no handler register",cmd )
    end
    local pkt = conn:ss_flatbuffers_decode( self.lfb,cmd,cfg[2],cfg[3] )
    return handler( conn,pkt )
end

-- 处理来着gateway转发的客户端包
function Message_mgr:clt_dispatcher( conn,cfg )
    local cmd,tbl = conn:scs_flatbuffers_decode( cfg[2],cfg[3] )

    local clt = self.cs[cmd]
    if not clt then
        return ELOG( "clt_dispatcher:message [%d] not define",cmd )
    end

    local handler = clt.handler
    if not handler then
        return ELOG( "clt_dispatcher:message [%d] define but no handler register",cmd )
    end

    return handler( conn,tbl )
end

-- 发送服务器消息
function Message_mgr:srv_send( conn,cfg,pkt )
    assert( cfg,"srv_send nil message" )

    conn.conn:ss_flatbuffers_send( self.lfb,cfg[1],cfg[2],cfg[3],pkt )
end

-- 获取当前进程处理的客户端协议
function Message_mgr:clt_cmd()
    local cmds = {}
    for cmd,cfg in pairs( self.cs ) do
        if cfg.handler then table.insert( cmds,cmd ) end
    end

    return cmds
end

-- 获取当前进程处理的服务端协议
function Message_mgr:srv_cmd()
    local cmds = {}
    for cmd,cfg in pairs( self.ss ) do
        if cfg.handler then table.insert( cmds,cmd ) end
    end

    return cmds
end

-- 服务器注册
function Message_mgr:do_srv_register( conn,network_mgr )
    local cfg = SS.REG
    local pkt = conn.conn:ss_flatbuffers_decode( self.lfb,cfg[1],cfg[2],cfg[3] )
    
    if not network_mgr:srv_register( conn,pkt ) then return false end

    for _,cmd in pairs( pkt.clt_cmd or {} ) do
        local _cfg = self.cs[cmd]
        assert( _cfg,"do_srv_register no such clt cmd" )
        assert( _cfg,"do_srv_register clt cmd register conflict" )

        _cfg.session = pkt.session
    end

    for _,cmd in pairs( pkt.srv_cmd or {} ) do
        local _cfg = self.ss[cmd]
        assert( _cfg,"do_srv_register no such srv cmd" )
        assert( _cfg,"do_srv_register srv cmd register conflict" )

        _cfg.session = pkt.session
    end

    PLOG( "server(%d) register succes",pkt.session )

    return true
end

local message_mgr = Message_mgr()

return message_mgr
