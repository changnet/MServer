-- ping.lua
-- 2019-06-22
-- xzc
-- 测试各服务器之间延迟

Ping = {}

local this = global_storage("Ping", {
    seed = 1,
    pending = {},
})

-- 开始ping所有进程以测试延迟
-- @param how 来源，1 gm，2客户端
-- @param conn_id 返回数据的连接，可能是gm，也可能是机器人
function Ping.start(how, conn_id)
    local id = this.seed
    this.seed = this.seed + 1

    local wait = 0
    for _, srv_conn in pairs(SrvMgr.get_all_srv_conn()) do
        wait = wait + 1
        Rpc.proxy(Ping.on_ping).conn_call(srv_conn, Ping.do_ping, id)
    end

    -- 没有其他服务器连接?
    if 0 == wait then return print("ping no other app found") end

    this.pending[id] = {
        srvs = {},
        how = how,
        wait = wait,
        conn_id = conn_id,
        ms_time = ev:real_ms_time()
    }
end

-- 服务器之间ping
function Ping.do_ping(id)
    return id
end

-- 其他服务器进程收到ping返回
function Ping.on_ping(id)
    local info = this.pending[id]
    if not info then return print("ping no info found", id) end

    local srv_conn = Rpc.last_conn()

    local conn_id = srv_conn.conn_id
    if info.srvs[conn_id] then
        this.pending[id] = nil
        return print("ping info error", id)
    end
    info.srvs[conn_id] = {
        name = srv_conn:conn_name(),
        time = ev:real_ms_time() - info.ms_time
    }

    info.wait = info.wait - 1
    if info.wait <= 0 then Ping.done(id, info) end
end

-- 完成一个ping
function Ping.done(id, info)
    this.pending[id] = nil

    local how = info.how
    if 1 == how then -- gm请求，直接打印
        print("ping done /////////////////////////")
        print(table.dump(info))
        return
    elseif 2 == how then -- 来自客户端的
        -- 连接已断开
        local conn = CltMgr.get_conn(info.conn_id)
        if not conn then
            return print("ping done,no conn found", info.conn_id, info.index)
        end

        local pkt = {
            delay = {}
        }
        for _, delay in pairs(info.srvs) do
            table.insert(pkt.delay, delay)
        end

        return conn:send_pkt(PLAYER.PING, pkt)
    end
end

-- 来自机器人的ping请求，走的是客户端连接
local function player_ping(pkt)
    local clt_conn = Cmd.last_conn()
    return Ping.start(2, clt_conn.conn_id, pkt)
end

if GATEWAY == APP_TYPE then
    Cmd.reg(PLAYER.PING, player_ping)
end

return Ping
