-- network_mgr 网络连接管理

local Stream_socket = require "Stream_socket"

-- 服务器名字转索引，不经常改。运维也不需要知道，暂时不做成配置
local name_type =
{
    gateway = 1,
    world   = 2,
    test    = 3,
    example = 4
}

local Network_mgr = oo.singleton( nil,... )

function Network_mgr:__init()
    self.srv = {}
    self.clt = {}

    self.srv_listen = nil
end

-- 生成服务器session id
-- @name  服务器名称，如gateway、world...
-- @index 服务器索引，如可能有多个gateway，分别为1,2...
-- @srvid 服务器id，与运维相关。开了第N个服
function Network_mgr:generate_srv_session( name,index,srvid )
    local ty = name_type[name]

    assert( ty,"server name type not define" )
    assert( index < (1 << 24),"server index out of boundry" )
    assert( srvid < (1 << 16),   "server id out of boundry" )

    -- int32 ,8bits is ty,8bits is index,16bits is srvid
    return (ty << 24) + (index <<16) + srvid
end

--  监听服务器连接
function Network_mgr:srv_listen( ip,port )
    local conn = Stream_socket()
    conn:set_self_ref( self )
    conn:set_on_acception( Network_mgr.on_acception )

    local fd = conn:listen( ip,port )
    if not fd then return false end

    self.srv_listen = conn
end

local network_mgr = Network_mgr()

return network_mgr
