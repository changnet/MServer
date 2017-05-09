-- 客户端网络连接

local network_mgr = network_mgr
local Clt_conn = oo.class( nil,... )

function Clt_conn:__init( conn_id )
    self.auth = false
    self.beat = 0
    self.fchk = 0 -- fail check

    self.conn_id = conn_id
end

-- 发送数据包
function Clt_conn:send_pkt( cmd,pkt,errno )
    return network_mgr:send_s2c_packet( self.conn_id,cmd,errno or 0,pkt )
end

-- 认证成功
function Clt_conn:authorized()
    self.auth = true
end

-- 将该链接绑定一个角色
function Clt_conn:bind_role( pid )
    self.pid = pid
end
