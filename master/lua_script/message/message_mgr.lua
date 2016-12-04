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
function Message_mgr:srv_register( msg,handler )
    if not self.ss[msg[1]] then
        return error( "srv_register:message not define" )
    end

    msg.handler = handler
end

-- 注册rpc处理
function Message_mgr:rpc_register( name,handler )
end

-- 分发服务器协议处理
function Message_mgr:srv_dispatcher( cmd,conn )
    local msg = self.ss[cmd]
    if not msg then
        return ELOG( "srv_dispatcher:message [%d] not define",cmd )
    end

    local handler = msg.handler
    if not handler then
        return ELOG( "srv_dispatcher:message [%d] define but no handler register",cmd )
    end
    local pkt = conn:ss_flatbuffers_decode( self.lfb,cmd,msg[2],msg[3] )
    vd( pkt )
    -- return handler( conn,msg )
end

-- 处理来着gateway转发的客户端包
function Message_mgr:clt_dispatcher( conn,msg )
    local cmd,tbl = conn:scs_flatbuffers_decode( msg[2],msg[3] )

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

--发送服务器消息
function Message_mgr:srv_send( conn,msg,pkt )
    assert( msg,"srv_send nil message" )

    conn.conn:ss_flatbuffers_send( self.lfb,msg[1],msg[2],msg[3],pkt )
end

local message_mgr = Message_mgr()

return message_mgr
