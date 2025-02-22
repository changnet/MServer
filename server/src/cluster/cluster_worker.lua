local SsSocket = require "network.ss_socket"

-- 把远程集群节点伪装成本地worker，统一调用接口
local ClusterWorker = oo.class(SsSocket)

function ClusterWorker:__init(addr)
    SsSocket.__init(self)
    self.addr = addr -- 对端的地址
    self.cluster_worker = true
end

-- 是否准备就绪（连接上并且握手完成）
function ClusterWorker:ready()
    return self.ready
end

-- 接受新的连接
function ClusterWorker:on_accepted()
end

-- 发送节点之间的签名数字
-- @param mask 客户端为“0”，服务器为“1”
function ClusterWorker:send_signature(tm, mask)
    tm = tm or Engine.time()

    -- mask保证发送和返回的签名是不一样的
    -- 避免另一端直接返回收到的签名也能校验通过
    local sign = Engine.make_srv_signature(tm, mask)

    local ptr, size
    if LOCAL_ADDR ~= PROCESS_ADDR then
        ptr, size = g_lcodec:encode_to_buffer(tm, sign)
    else
        -- 在主线程发起的连接是公用的，相互通知各自所有worker地址
        local addr_list = Worker.local_addr_list()
        ptr, size = g_lcodec:encode_to_buffer(tm, sign, addr_list)
    end

    -- 这时候还没认证，不能走rpc调用的
    self:send_pkt(LOCAL_ADDR, 0, 0, size, ptr)
end

-- 连接成功
function ClusterWorker:on_connected()
    -- 客户端才主动发起握手
    if self:is_server() then return end

    return self:send_signature("0")
end

-- 连接断开
function ClusterWorker:on_disconnected()
    self.ready = nil
    Cluster.unauthenticate(self.addr)
end

function ClusterWorker:do_authenticate(src, tm, sign, addr_list)
    local ok = true
    if self:is_server() then
        local expect_sign = Engine.make_srv_signature(tm, "0")
        if expect_sign ~= sign then
            print("cluster auth signature fail", self.addr, tm, sign, expect_sign)

            ok = false
            self:send_signature("1")
        else
            self:send_signature("1")
        end
    else
        if 0 == (tm or 0) then
            ok = false
            print("remote auth fail", self.addr)
        end
        local expect_sign = Engine.make_srv_signature(tm, "1")
        if expect_sign ~= sign then
            print("cluster remote signature fail", self.addr, tm, sign, expect_sign)
        end
    end

    Cluster.on_authenticate(self.addr, ok, addr_list)
    if ok then self.ready = true end
end

-- 服务器之间消息回调
function ClusterWorker:on_message(src, dst, mtype, usize, udata)
    -- 未经过认证的连接，禁止直接将消息派发到其他模块
    if not self.ready then
        assert(0 == mtype)
        return self:do_authenticate(
            src, g_lcodec:decode_from_buffer(udata, usize))
    end

    if LOCAL_ADDR == PROCESS_ADDR then
        main_message_dispatch(src, dst, mtype, usize, udata)
    else
        on_worker_message(src, dst, mtype, usize, udata)
    end
end

return ClusterWorker
