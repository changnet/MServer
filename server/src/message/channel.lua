-- 频道及广播
Channel = {}

--[[
注意一致性陷阱

1. 玩家在player线程登录调用Channel.join加入频道
2. game线程登录成功下发历史信息
3. game有人往该频道发送一条消息

join是要通过rpc调用的，到达时间不可控。

假如join没到达，game线程发送的新消息却到达网关了这时候玩家就会收不到消息
反过来，假如先有人发了一条数据，然后game线程登录成功下发历史信息，这时候那条数据到达网关
时，join已经在网关生效，则玩家会多收到一条数据

解决这个问题：必须保证Channel.join和Channel.broadcast在同一线程使用。比如：聊天在game
线程，那join和broadcast都必须在game线程

]]

local Buffer = require "engine.Buffer"

local buffer_read_int = Buffer.read_int
local buffer_write_int = Buffer.write_int
local buffer_write_buffer = Buffer.write_buffer

local g_mthread = g_mthread
local construct_message = g_mthread.construct_message

local pbc_encode = Pbc.encode

local IS_GATEWAY = LOCAL_TYPE == W.GATEWAY
local CLT_CAST = ThreadMessage.CLT_CAST
local CAST_CHANNEL = 2 -- 频道广播子类型

-- 网关侧频道数据
local this
if IS_GATEWAY then
    this = memory("Channel", {
        -- [chl_type][chl_id] = { [pid] = true }
        channels = {},
        -- [pid] = { {chl_type, chl_id}, ... } 反向索引
        pid_channels = {},
    })
end

-- 加入频道
-- @param pid integer 玩家id
-- @param chl_type integer 频道类型
-- @param chl_id integer 频道id
function Channel.join(pid, chl_type, chl_id)
    if IS_GATEWAY then
        return Channel.on_join(pid, chl_type, chl_id)
    end

    local gaddr = Router.find_player_addr(pid, W.GATEWAY)
    if not gaddr then
        eprint("Channel.join no gateway", pid)
        return
    end
    Send[gaddr].Channel.on_join(pid, chl_type, chl_id)
end

-- 离开频道
-- @param pid integer 玩家id
-- @param chl_type integer 频道类型
-- @param chl_id integer 频道id
function Channel.leave(pid, chl_type, chl_id)
    if IS_GATEWAY then
        return Channel.on_leave(pid, chl_type, chl_id)
    end

    local gaddr = Router.find_player_addr(pid, W.GATEWAY)
    if not gaddr then
        eprint("Channel.leave no gateway", pid)
        return
    end
    Send[gaddr].Channel.on_leave(pid, chl_type, chl_id)
end

-- 广播到指定频道的所有玩家
-- @param chl_type integer 频道类型
-- @param chl_id integer 频道id
-- @param cmd table 协议定义
-- @param pkt table 协议数据
function Channel.broadcast(chl_type, chl_id, cmd, pkt)
    local cmd_id = cmd.i
    local buffer, size = pbc_encode(cmd.s, pkt)
    -- [2:type][2:chl_type][4:chl_id][2:cmd_id][N:pb]
    local total = 10 + size

    local gw_list = Router.get_worker_list(W.GATEWAY)
    if not gw_list then return end

    for _, gw in ipairs(gw_list) do
        local worker = WorkerHash[gw.addr]
        if worker then
            local m, mbuffer = construct_message(
                g_mthread, LOCAL_ADDR, gw.addr,
                CLT_CAST, total)
            buffer_write_int(mbuffer, 2, CAST_CHANNEL)
            buffer_write_int(mbuffer, 2, chl_type, 2)
            buffer_write_int(mbuffer, 4, chl_id, 4)
            buffer_write_int(mbuffer, 2, cmd_id, 8)
            buffer_write_buffer(
                mbuffer, buffer, size, 10)
            worker:push_message(m)
        end
    end
end

-- ///////////////////////////////////////////////////////////
-- 以下为网关侧内部函数
-- ///////////////////////////////////////////////////////////

-- 网关RPC回调：加入频道
-- @param pid integer 玩家id
-- @param chl_type integer 频道类型
-- @param chl_id integer 频道id
function Channel.on_join(pid, chl_type, chl_id)
    local channels = this.channels
    local type_map = channels[chl_type]
    if not type_map then
        type_map = {}
        channels[chl_type] = type_map
    end
    local members = type_map[chl_id]
    if not members then
        members = {}
        type_map[chl_id] = members
    end

    if members[pid] then return end -- 已经在频道中
    members[pid] = true

    -- 反向索引
    local pc = this.pid_channels[pid]
    if not pc then
        pc = {}
        this.pid_channels[pid] = pc
    end
    pc[#pc + 1] = {chl_type, chl_id}
end

-- 网关RPC回调：离开频道
-- @param pid integer 玩家id
-- @param chl_type integer 频道类型
-- @param chl_id integer 频道id
function Channel.on_leave(pid, chl_type, chl_id)
    local channels = this.channels
    local type_map = channels[chl_type]
    if not type_map then return end
    local members = type_map[chl_id]
    if not members then return end

    members[pid] = nil
    if not next(members) then
        type_map[chl_id] = nil
        if not next(type_map) then
            channels[chl_type] = nil
        end
    end

    -- 清理反向索引
    local pc = this.pid_channels[pid]
    if pc then
        for i, info in ipairs(pc) do
            if info[1] == chl_type
                and info[2] == chl_id then
                table.remove(pc, i)
                break
            end
        end
        if #pc == 0 then
            this.pid_channels[pid] = nil
        end
    end
end

-- 玩家断线时清理所有频道
-- @param pid integer 玩家id
function Channel.on_disconnect(pid)
    local pc = this.pid_channels[pid]
    if not pc then return end

    local channels = this.channels
    for _, info in ipairs(pc) do
        local chl_type, chl_id = info[1], info[2]
        local type_map = channels[chl_type]
        if type_map then
            local members = type_map[chl_id]
            if members then
                members[pid] = nil
                if not next(members) then
                    type_map[chl_id] = nil
                end
            end
            if not next(type_map) then
                channels[chl_type] = nil
            end
        end
    end
    this.pid_channels[pid] = nil
end

-- 网关收到频道广播消息（由net_msg dispatch_cast调用）
-- @param udata lightuserdata 二进制数据
-- @param size integer 数据大小
function Channel._dispatch_broadcast(udata, size)
    local chl_type = buffer_read_int(udata, 2, 2)
    local chl_id = buffer_read_int(udata, 4, 4)
    local cmd_id, pb = buffer_read_int(udata, 2, 8)
    local pb_size = size - 10

    local channels = this.channels
    local type_map = channels[chl_type]
    if not type_map then return end
    local members = type_map[chl_id]
    if not members then return end

    for pid, _ in pairs(members) do
        local socket = CltMgr.get_by_pid(pid)
        if socket and socket.auth then
            socket:send_pkt(cmd_id, pb, pb_size)
        end
    end
end

return Channel
