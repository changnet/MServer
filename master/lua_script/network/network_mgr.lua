-- network_mgr 网络连接管理

-- 服务器名字转索引，不经常改。运维也不需要知道，暂时不做成配置
local name_type =
{
    gateway = 1,
    world   = 2,
    test    = 3,
    example = 4
}

local NetWork_mgr = oo.singleton( nil,... )

function NetWork_mgr:__init()
    self.srv = {}
    self.clt = {}
end

function NetWork_mgr:generate_srv_id( name,index,srvid )
    local ty = name_type[name]

    assert( ty,"server name type not define" )
    assert( index < (1 << 24),"server index out of boundry" )
    assert( srvid < (1 << 16),   "server id out of boundry" )

    -- int32 ,8bits is ty,8bits is index,16bits is srvid
    return (ty << 24) + (index <<16) + srvid
end

local network_mgr = NetWork_mgr()

return network_mgr
