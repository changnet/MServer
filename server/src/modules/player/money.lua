-- 玩家虚拟货币
Money = {}

-- 玩家虚拟货币定义
MoneyType ={
    [RES_GOLD] = RES_GOLD,
    [RES_COPPER] = RES_COPPER,
}

local info_pkt = {}
local update_pkt = {type = 0, num = 0}

function Money.init(player)
    local money = player.money
    for mtype in pairs(MoneyType) do
        if not money[mtype] then
            money[mtype] = 0
        end
    end
end

-- 登录时，下发所有货币信息
function Money.send_info(player)
    local money = player.money

    for mtype in pairs(MoneyType) do
        info_pkt[mtype].num = money[mtype]
    end

    return NetMsg.send(player, M.MoneyInfo, info_pkt)
end

-- 获取玩家当前货币数量
function Money.get_num(player, mtype)
    return player.money[mtype]
end

local function res_num_factory(mtype)
    return function(player, res)
        return player.money[mtype]
    end
end

local function log_money(player, mtype, num, op, log_str)
    Log.info("Money log, pid = %d, mtype = %d, num = %d, op = %d, log_str = %s",
        player.pid, mtype, num, op, log_str)
end

-- 扣除虚拟货币
function Money.add(player, res, op, log_str, ext)
    local mtype = res.id
    local num = res.num

    local old_num = player.money[mtype]
    local new_num = old_num + num
    if new_num < 0 then
        eprintf("Money add error, pid = %d, mtype = %d, num = %d",
            player.pid, mtype, num)
        eprint(debug.traceback())
    end

    player.money[mtype] = new_num

    log_money(player, mtype, num, op, log_str)

    update_pkt.type = mtype
    update_pkt.num = new_num
    return NetMsg.send(player, M.MoneyUpdate, update_pkt)
end

function Money.dec(player, res, op, log_str, ext)
    local mtype = res.id
    local num = res.num

    local old_num = player.money[mtype]
    local new_num = old_num - num
    if new_num < 0 then
        eprintf("Money dec error, pid = %d, mtype = %d, num = %d",
            player.pid, mtype, num)
        eprint(debug.traceback())
    end

    player.money[mtype] = new_num

    log_money(player, mtype, num, op, log_str)

    update_pkt.type = mtype
    update_pkt.num = new_num
    return NetMsg.send(player, M.MoneyUpdate, update_pkt)
end

for mtype in pairs(MoneyType) do
    info_pkt[mtype] = {type = mtype, num = 0}
    Res.reg(mtype, res_num_factory(mtype), Money.add, Money.dec)
end
