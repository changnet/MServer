local SsSocket = require "network.ss_socket"

-- 把远程集群节点伪装成本地worker，统一调用接口
local ClusterWorker = oo.class(SsSocket)


-- 接受新的连接
function ClusterWorker:on_accepted()
end

-- 连接成功
function ClusterWorker:on_connected()
end

-- 连接断开
function ClusterWorker:on_disconnected()
end

-- 服务器之间消息回调
function ClusterWorker:on_message(src, dst, mtype, usize, udata)
    -- return Cmd.dispatch_srv(self, cmd, pkt)
end

return ClusterWorker
