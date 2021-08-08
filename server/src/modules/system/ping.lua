-- ping.lua
-- 2019-06-22
-- xzc
-- 测试各服务器之间延迟
local g_rpc = g_rpc

local Ping = oo.singleton(...)

function Ping:__init()
    self.seed = 1
    self.pending = {}
end

-- 开始一个所有进程ping
-- @how:1gm，2客户端
-- @conn_id:返回数据的连接，可能是gm，也可能是机器人
-- @other:机器人发过来的数据，原样返回以校验完整性。如果是gm，则为nil
function Ping:start(how, conn_id, other)
    local id = self.seed
    self.seed = self.seed + 1

    local wait = 0
    for _, srv_conn in pairs(SrvMgr.get_all_srv_conn()) do
        wait = wait + 1
        g_rpc:proxy(srv_conn, self.on_ping, self):rpc_ping(id)
    end

    -- 没有其他服务器连接?
    if 0 == wait then return print("ping no other app found") end

    self.pending[id] = {
        srvs = {},
        how = how,
        wait = wait,
        other = other,
        conn_id = conn_id,
        ms_time = ev:real_ms_time()
    }
end

-- 其他服务器进程收到ping返回
function Ping:on_ping(ecode, id)
    local info = self.pending[id]
    if not info then return print("ping no info found", id) end

    local srv_conn = g_rpc:last_conn()

    local conn_id = srv_conn.conn_id
    if info.srvs[conn_id] then
        self.pending[id] = nil
        return print("ping info error", id)
    end
    info.srvs[conn_id] = {
        name = srv_conn:conn_name(),
        time = ev:real_ms_time() - info.ms_time
    }

    info.wait = info.wait - 1
    if info.wait <= 0 then self:done(id, info) end
end

-- 完成一个ping
function Ping:done(id, info)
    self.pending[id] = nil

    local how = info.how
    if 1 == how then -- gm请求，直接打印
        print(table.dump(info))
        return
    end

    -- 连接已断开
    local conn = SrvMgr.get_conn(info.conn_id)
    if not conn then
        return print("ping done,no conn found", info.conn_id, info.index)
    end

    if 2 == how then -- 来自客户端的
        local pkt = info.other
        pkt.srvtime = {}
        for _, srvtime in pairs(info.srvs) do
            table.insert(pkt.srvtime, srvtime)
        end

        return conn:send_pkt(PLAYER.PING, pkt)
    end
end

local ping = Ping()

-- 来自机器人的ping请求，走的是客户端连接
local function player_ping(clt_conn, pkt)
    return ping:start(2, clt_conn.conn_id, pkt)
end

-- 服务器之间ping
local function rpc_ping(id)
    return id
end

reg_func("rpc_ping", rpc_ping)

if GATEWAY == APP_TYPE then
    Cmd.reg(PLAYER.PING, player_ping)
end

return ping
