local SsSocket = require "network.ss_socket"

-- 把远程集群节点伪装成本地worker，统一调用接口
local ClusterWorker = oo.class(SsSocket)

local IS_MAIN = LOCAL_ADDR == MAIN_ADDR

function ClusterWorker:__init(name)
    SsSocket.__init(self)
    self.name = name -- 对端的名字
    self.cluster_worker = true
end

-- 是否准备就绪（连接上并且握手完成）
function ClusterWorker:is_ready()
    return self.ready == 0x2
end

-- 接受新的连接
function ClusterWorker:on_accepted()
    Cluster.accept(self)
end

-- 发送节点之间的签名数字
-- @param mask 客户端为“0”，服务器为“1”
function ClusterWorker:send_signature(mask)
    local tm = Engine.time()

    -- mask保证发送和返回的签名是不一样的
    -- 避免另一端直接返回收到的签名也能校验通过
    local sign = Engine.make_srv_signature(tm, mask)

    -- 在主线程发起的连接是公用的，相互通知各自所有worker地址
    local status_list = Worker.get_status_list()
    local ptr, size = g_lcodec:encode_to_buffer(LOCAL_NAME, {
        tm = tm,
        sign = sign,
        status_list = status_list,
    })

    -- 这时候还没认证，不能走rpc调用的
    self:send_pkt(LOCAL_ADDR, 0, -1, size, ptr)
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
    Cluster.unauthenticate(self)
end

function ClusterWorker:do_authenticate(src, name, signData)
    local ok = true
    local tm = signData.tm
    local sign = signData.sign

    if self:is_server() then
        local expect_sign = Engine.make_srv_signature(tm, "0")
        if expect_sign ~= sign then
            print("cluster auth signature fail", name, tm, sign, expect_sign)

            ok = false
            self:send_signature("2") -- 验证失败，故意发个错的过去
        else
            self:send_signature("1")
        end
    else
        local expect_sign = Engine.make_srv_signature(tm, "1")
        if expect_sign ~= sign then
            ok = false
            print("cluster remote signature fail", name, tm, sign, expect_sign)
        end
    end

    self.src = src -- 如果是worker直连，则是对方的worker addr，否则为对方MAIN_ADDR
    self.ready = 0x2
    self.name = name
    self.status_list = signData.status_list

    Cluster.authenticate(self, ok)
end

-- 服务器之间消息回调
function ClusterWorker:on_message(src, dst, mtype, usize, udata)
    -- 未经过认证的连接，禁止直接将消息派发到其他模块
    if 0x2 == self.ready then
        if IS_MAIN then
            return main_message_dispatch(src, dst, mtype, udata, usize)
        else
            return on_worker_message(src, dst, mtype, udata, usize)
        end
    else
        -- todo 0x1表示认证完成，但未握手, 0x2是握手完成，可以发送业务逻辑了
        assert(-1 == mtype)
        return self:do_authenticate(
            src, g_lcodec:decode_from_buffer(udata, usize))
    end
end

-- 对应ThreadWorker的push_message函数
function ClusterWorker:push_message(message)
    local src, dst, mtype, ptr, size = g_mthread:unpack_message(message)
    self:send_pkt(src, dst, mtype, size, ptr)

    -- TODO 如果上面的代码报错，这个message将会发生内存泄漏
    -- 用xpcall消耗太大，如果要处理，弄个last_message变量存一下
    -- 下次调用先检测上一次的message是否有正常销毁

    return g_mthread:destruct_message(message)
end

-- 对应ThreadWorker的emplace_message函数
function ClusterWorker:emplace_message(src, addr, mtype, ptr, size)
    return self:send_pkt(src, addr, mtype, size, ptr)
end

return ClusterWorker
