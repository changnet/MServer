-- 需要通信的两个节点，一般需要通过Cluster.connect()建立连接
-- 但如果需要数据访问，又不想建立连接就需要使用代理转发
ClusterProxy = {}

local this = memory("ClusterProxy", {
    proxy = {}, -- [from_addr] = ｛[to_addr] = from_addr}
    beproxy = {}, -- 被代理的节点
})

-- 把节点from作为代理，与节点to建立连接
-- @param from 代理节点名字，如gateway1
-- @param to 被代理节点名字，如gateway2
function ClusterProxy.create(from, to)
    local from_addr = Worker.name_addr(from)
    local to_addr = Worker.name_addr(to)

    local addr_proxy = this.proxy[from_addr]
    if not addr_proxy then
        addr_proxy = {}
        this.proxy[from_addr] = addr_proxy
    end
    assert(not addr_proxy[to_addr])
    addr_proxy[to_addr] = from_addr

    local w = WorkerHash[from_addr]
    if w then
        assert(w.cluster_worker)
        Send.ClusterProxy.on_request(from_addr, LOCAL_ADDR, to_addr)
    end
end

-- 等待其他节点启动完成
-- @param proxy_list 代理列表，格式为{{from_name, to_name}}
function ClusterProxy.create_later(proxy_list)
    for _, name in pairs(proxy_list) do
        local to_name = name[2]
        local addr = Worker.name_addr(to_name)
        Startup.reg({
            name = string.format("cluster proxy wait %s", to_name),
            boot = function()
                ClusterProxy.create(name[1], to_name)
            end,
            ready = function()
                local w = WorkerHash[addr]
                if w and w:is_ready() then return true end

                return false, string.format("cluster proxy waitting %s", to_name)
            end
        }, 0xFFFF)
    end
end

-- 对from_addr发起针对to_addr的代理请求
-- @param from_addr 作为发转数据代理的节点
function ClusterProxy.request(from_addr)
    local addr_proxy = this.proxy[from_addr]
    if not addr_proxy then return end

    local w = WorkerHash[from_addr]

    assert(w.cluster_worker)
    for to_addr in pairs(addr_proxy) do
        Send.ClusterProxy.on_request(from_addr, LOCAL_ADDR, to_addr)
    end
end

-- 代理节点收到请求，尝试寻找被代理节点，然后建立连接
function ClusterProxy.on_request(request_addr, to_addr)
    local addr_proxy = this.beproxy[to_addr]
    if not addr_proxy then
        addr_proxy = {}
        this.beproxy[to_addr] = addr_proxy
    end
    local old = addr_proxy[request_addr] -- 另一端进程重启，可能会导致重复
    assert(not old or old == to_addr)

    addr_proxy[request_addr] = to_addr
    printf("accept cluster proxy request %d to %d", request_addr, to_addr)

    local w = WorkerHash[to_addr]
    if w then
        assert(w.cluster_worker)
        ClusterProxy.response(to_addr)
    end
end

-- 把被代理节点的数据发送给请求代理的节点
function ClusterProxy.response(addr)
    local addr_proxy = this.beproxy[addr]
    if not addr_proxy then
        return
    end

    local status_list
    local w = WorkerHash[addr]
    -- 如果w不存在，则表示断开连接
    if w then
        assert(w.cluster_worker)

        -- 把被代理节点的worker状态数据发送给请求代理的节点
        status_list = {}
        for other_addr, other_w in pairs(WorkerHash) do
            if w == other_w then
                local data = WorkerData[other_addr]
                table.insert(status_list, {
                    addr = other_addr,
                    status = data.status
                })
            end
        end
    end

    for request_addr, to_addr in pairs(addr_proxy) do
        local request_w = WorkerHash[request_addr]
        if request_w then
            assert(request_w.cluster_worker)
            Send.ClusterProxy.on_response(request_addr,
                LOCAL_ADDR, to_addr, status_list)
        end
    end
end

-- 发起代理请求的节点收到目标节点的数据
-- @param status_list 如果为nil则表示断开连接
function ClusterProxy.on_response(from_addr, to_addr, status_list)
    local addr_proxy = this.proxy[from_addr]
    if not addr_proxy then
        eprint("cluster proxy on response no from addr", from_addr, to_addr)
        return
    end
    if not addr_proxy[to_addr] then
        eprint("cluster proxy on response no to addr", from_addr, to_addr)
        return
    end

    local w = WorkerHash[from_addr]
    if not w then
        eprint("cluster proxy on response no worker", from_addr, to_addr)
        return
    end
    assert(w.cluster_worker)
    Cluster.set_proxy_worker(w, status_list, to_addr)
end

-- 同步单个worker的数据发送给请求代理的节点，通常用于单个worker启动、关闭
-- @param addr 同步的worker地址
-- @param status 同步的worker状态
function ClusterProxy.response_one(src_addr, addr, status)
    local addr_proxy = this.beproxy[src_addr]
    if not addr_proxy then
        return
    end

    for request_addr, to_addr in pairs(addr_proxy) do
        local request_w = WorkerHash[request_addr]
        if request_w then
            assert(request_w.cluster_worker)
            Send.ClusterProxy.on_response(request_addr,
                LOCAL_ADDR, to_addr, {{addr = addr, status = status}})
        end
    end
end

function ClusterProxy.on_response_one(from_addr, to_addr, addr, status)
    local addr_proxy = this.proxy[from_addr]
    if not addr_proxy then
        eprint("cluster proxy on response no from addr", from_addr, to_addr)
        return
    end
    if not addr_proxy[to_addr] then
        eprint("cluster proxy on response no to addr", from_addr, to_addr)
        return
    end

    local w = WorkerHash[from_addr]
    if not w then
        eprint("cluster proxy on response no worker", from_addr, to_addr)
        return
    end
    assert(w.cluster_worker)
    Cluster.set_one_proxy_worker(w, to_addr, addr, status)
end
