local SsSocket = require "network.ss_socket"

-- 把远程集群节点伪装成本地worker，统一调用接口
local ClusterWorker = oo.class(SsSocket)

function ClusterWorker:__init(addr)
    SsSocket.__init(self)
    self.addr = addr -- 对端的地址
end

-- 是否准备就绪（连接上并且握手完成）
function ClusterWorker:ready()
    return self.ready
end

-- 接受新的连接
function ClusterWorker:on_accepted()
end

-- 连接成功
function ClusterWorker:on_connected()
    -- 客户端主动发起握手
    if not self:is_server() then
        local tm = Engine.time()
        local sign = Engine.make_srv_signature(tm)
        Send.Cluster.authenticate(self.addr, LOCAL_ADDR, tm, sign)
    end
end

-- 连接断开
function ClusterWorker:on_disconnected()
    self.ready = nil
    Cluster.unauthenticate(self.addr)
end

-- 服务器之间消息回调
function ClusterWorker:on_message(src, dst, mtype, usize, udata)
    -- return Cmd.dispatch_srv(self, cmd, pkt)
end

return ClusterWorker
