-- gm.lua
-- 2018-04-11
-- xzc

-- 玩家gm及后台gm
GM = {}

--[[
gm派发机制

一开始想用所有节点往中心节点注册的机制，但那样太麻烦，热更还要重新注册

现在采用懒查询模式。如果找不到匹配的gm信息，就依次向各个节点查询
]]

local gm_data = {}
local is_query = false -- 是否已向其他节点查询

-- 注册gm指令
-- @param wtype 执行该gm的worker类型，默认为player，-1表示广播指定主线程执行：W.GAME | W.MAIN
-- @param level 执行此gm需要的权限等级，默认为256，数值越大权限越高
function GM.reg(cmd, func, wtype, level)
    gm_data[cmd] = {
        func = func,
        wtype = wtype or W.PLAYER,
        level = level or 256,
    }
end


function GM.on_query_data()
    local list = {}
    for k, v in pairs(gm_data) do
        table.insert(list, {
            cmd = k,
            wtype = v.wtype,
            level = v.level,
        })
    end
    return list
end

local function query_data()
    -- 要处理gm时，所有节点应该已启动成功，因此不考虑查询过一次后，新增节点的问题
    -- 至少要热更一次，才会重新查询

    for other_addr in pairs(WorkerData) do
        local data = Call[other_addr].GM.on_query_data()
        for _, v in pairs(data or {}) do
            local k = v.cmd
            local wtype = v.wtype
            -- 有些gm在多个worker注册，如果是共用的，只取其中一个就行
            if not gm_data[k] or -1 ~= wtype then
                v.cmd = nil
                gm_data[k] = v
            end
        end
    end

    is_query = true
end

local function find_gm(str)
    if string.byte(str, 1) ~= 64 then
        return nil, "gm need to start with @: " .. str
    end

    local pos = string.find(str, " ")
    if not pos and string.len(str) < 1 then
        return nil, "gm invalid string: " .. str
    end
    local cmd = string.sub(str, 2, pos and (pos - 1) or -1) -- 去掉@

    local data = gm_data[cmd]
    if not data then
        if is_query then
            return nil, string.format("no such gm [%s]", cmd)
        end

        query_data()
        return find_gm(str)
    end

    return data
end

-- 分解gm字符串
local function split_str(str)
    local raw_str = string.sub(str, 2) -- 去掉@

    -- 分解gm指令 @level 999分解为{level,999}
    return string.split(raw_str, " ")
end

-- 执行玩家发的gm，通常在聊天中发送并以@开头
function GM.run_player_gm(player, str)
    -- string.byte("@") == 64
    if string.byte(str, 1) ~= 64 then return false end

    local gm_lv = player.gm_level or 0
    if gm_lv <= 0 then return false end

    local data, msg = find_gm(str)
    if not data then
        eprint("GM error", msg)
        return
    end

    if gm_lv < data.level then
        print("GM error", "no enough gm level")
        return
    end

    local args = split_str(str)
    local ok, msg1 = GM.dispatch("player", player.pid, data, args)
    if not ok then
        eprint("GM error", msg1)
        -- 仍然返回true表示已执行gm
    end

    return true
end

function GM.run_console_gm(str)
    local data, msg = find_gm(str)
    if not data then
        eprint("GM error", msg)
        return
    end

    local args = split_str(str)
    GM.dispatch("console", 0, data, args)
end

-- 运行gm
function GM.run(source, pid, args)
    print(string.format(
        "GM run source = %s, pid = %s: ", tostring(source), tostring(pid)),
        table.unpack(args))

    local cmd = args[1]
    local data = gm_data[cmd]
    if not data then
        eprint("no such gm", cmd)
        return
    end

    -- 如果是在player worker并且存在pid，则以玩家对象为第一个参数执行gm
    -- 这个player是给玩家gm用的，如果是后台gm需要针对某个玩家操作，一般pid在args里
    local player
    if W.PLAYER == LOCAL_TYPE and pid > 0 then
        player = PlayerMgr.get(pid)
        if not player then
            eprint("gm no such player", pid)
            return
        end
    end

    data.func(player or pid, table.unpack(args, 2))
end

-- 派发gm到指定节点执行
-- @param pid 玩家id，后台gm或者控制台可以发0
function GM.dispatch(source, pid, data, args)
    assert(LOCAL_ADDR == MASTER_ADDR)
    -- 查询gm所在节点
    -- 如果是-1，则广播到所有节点执行
    -- 如果是某个类型的节点
    --     如果有玩家路由，则根据路由表查询玩家所在节点并执行(例如场景gm，在玩家当前所在场景执行)
    --     如果没有路由，则在任意一个节点执行
    local wtype = data.wtype
    if -1 == wtype then
        GM.run(source, pid, args)
        for addr in pairs(WorkerData) do
            Send[addr].GM.run(source, pid, args)
        end
        return
    end

    if 0 ~= pid then
        local addr = Worker.get_player_route(pid, wtype)
        if addr then
            Send[addr].GM.run(source, pid, args)
            return
        end
    end

    local is_main = 0 ~= (wtype & W.MAIN)
    wtype = wtype & ~W.MAIN

    for addr in pairs(WorkerData) do
        local wt, _, main = Engine.unmake_address(addr)
        if wt == wtype and (is_main == main) then
            Send[addr].GM.run(source, pid, args)
        end
    end

    return false, "no gm node addr found" .. tostring(args[1])
end

return GM
