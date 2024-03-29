-- res.lua
-- 2018-04-09
-- xzc

-- 资源管理模块
Res = {}

local res_func = {}

local RES = require_define("modules.res.res_header")

local RES_MONEY = 599
local RES_ITEM  = RES_ITEM

local item_conf = nil

local made = false -- 生成回调函数后，不允许再注册事件

-- 获取所有资源定义
function Res.get_def()
    return RES
end

-- 判断该类资源是否为货币
function Res.is_money(id)
    return id <= RES_MONEY -- [1,599]为货币，具体规则见res_header定义
end

-- 判断该类资源是否为道具
function Res.is_item(id)
    return id >= RES_ITEM
end

-- 检测两个资源能否合并
local function can_merge(a, b)
    -- id已在外部检测
    -- 其他属性如 绑定、随机属性、星级、过期时间等可根据需求增加
    if a.unbind ~= b.unbind then return false end
    if a.attr ~= b.attr then return false end

    return true
end

-- 把奖励数组list1合并到list，注意list会被改变，不要改到配置
function Res.merge(list, list1)
    -- 不要复制list，因为调用的时候通常需要合并多次，调用的需要自己创建一个table
    -- list = table.deep_copy(list)
    for _, res in pairs(list1) do
        local found = false
        for _, old_res in pairs(list) do
            if old_res[1] == res[1] and can_merge(old_res, res) then
                found = true
                old_res[2] = res[2]
            end
        end

        if not found then
            table.insert(list, table.deep_copy(res))
        end
    end

    return list
end

-- 奖励数量乘以系数N，向下取整，数量为<=0的将被删除，此波函数会修改res_list
function Res.multi(res_list, f)
    for i = #res_list, 1, -1 do
        local res = res_list[i]
        res[2] = math.floor(res[2] * f)
        if res[2] <= 0 then table.remove(res_list, i) end
    end

    return res_list
end

-- 检验资源配置是否有效，通常用于检验来自后台等动态输入的数据
function Res.valid(res_list)
    for _, res in pairs(res_list) do
        local id = res[1]
        if not id then
            return false, "no id set"
        end
        if not res_func[id] then
            if not Res.is_item(id) then
                return false, "invalid res id = " .. tostring(id)
            end

            -- 校验是否为有效道具id
            if not item_conf[id] then
                return false, "invalid item id = " .. tostring(id)
            end

            -- TODO 校验其他属性？？
        end

        -- 校验数量
        if not res[2] or res[2] <= 0 then
            return false, "invalid res count, id = " .. tostring(id)
        end
    end

    return true
end


-- 注册资源处理函数
-- @param id 资源类型
-- @param check_add 检测能否添加资源函数
-- @param check_dec 检测能否扣除资源函数
-- @param add 添加资源函数，必须返回添加的数量
--              < 0表示出错，> 0表示成功，如果部分成功，则会自动发邮件
-- @param dec 删除资源函数
function Res.reg(id, check_add, check_dec, add, dec)
    assert(add and dec and not made, id)
    res_func[id] = {
        check_add = check_add,
        check_dec = check_dec,
        add = add,
        dec = dec,
    }
end

--//////////////////////////////////////////////////////////////////////////////
--// 下面为玩家相关接口

-- 添加资源，如果背包放不下，放不下那部分将自动发放到邮件
-- @param res_list 资源数组{{1, 999},{100001, 9, bind = 1}}
-- @param op 操作代码，用于记录日志，见log_header定义
-- @param msg 额外日志信息字符串。比如op为邮件，那么sub_op可以表示哪个功能发的
-- @param ext 扩展参数，如邮件标题、绑定属性、星级等
function Res.add(player, res_list, op, msg, ext)
    -- ext 扩展参数
    -- 有些特殊的需求，例如根据玩家花的钱统一指定是否发放非绑定道具
    -- 或者在这里指定发放到邮件的邮件标题、内容
    -- 有些需求还希望获得道具对象用于发公告等，都是通过这个ext参数来传参

    local mail_items = nil
    for _, res in pairs(res_list) do
        local count = res[2]
        local cnt = Res.add_one(player, res[1], count, res, op, msg, ext)
        if cnt > 0 and cnt < count then
            assert(Res.is_item()) -- 除了道具有背包大小限制，还有什么东西放不下？
            if not mail_items then mail_items = {} end

            local item = table.deep_copy(res)
            item.count = count - cnt
            table.insert(mail_items, item)
        end
    end

    -- 放不下背包的，默认发邮件
    if mail_items then
        local title = (ext and ext.mail_title) or LANG.mail001
        local ctx =  (ext and ext.mail_ctx) or LANG.mail002
        g_mail_mgr:raw_send_mail(player.pid, title, ctx, mail_items, op)
    end
end

-- 扣除资源(这个函数并不检测资源是否足够)
-- @param res_list 资源数组{{1, 999},{100001, 9, bind = 1}}
-- @param op 操作代码，用于记录日志，见log_header定义
-- @param msg 额外日志信息字符串。比如op为邮件，那么sub_op可以表示哪个功能发的
function Res.dec(player, res_list, op, msg)
    for _, res in pairs(res_list) do
        Res.dec_one(player, res[1], res[2], res, op, msg)
    end
end

-- 增加一个资源
-- @param id 资源id
-- @param count 资源数量
-- @param res 资源结构，包含绑定、星级、强化等特殊属性时需要传，否则传nil
-- @param op 操作代码，用于记录日志，见log_header定义
-- @param msg 额外日志信息字符串。比如op为邮件，那么sub_op可以表示哪个功能发的
-- @param ext 扩展参数，如邮件标题、绑定属性、星级等
function Res.add_one(player, id, count, res, op, msg, ext)
    local res_m = res_func[id]
    if not res_m then
        if Res.is_item(id) then
            res_m = res_func[RES_ITEM]
        else
            eprintf("Res.add_one no function found:%s,op %d", tostring(id), op)
            return
        end
    end

    -- 把res也传过去，有时候某些资源会有特殊属性，如强化、升星...
    local add_cnt = res_m.add(player, id, count, op, msg, res, ext)
    if add_cnt < 0 then
        eprintf("Res add error, pid = %d, id = %d, count = %d",
            player.pid, id, count)
    end
    return add_cnt
end

-- 扣除一个资源
-- @param id 资源id
-- @param count 资源数量
-- @param res 资源结构，包含绑定、星级、强化等特殊属性时需要传，否则传nil
-- @param op 操作代码，用于记录日志，见log_header定义
-- @param msg 额外日志信息字符串。比如op为邮件，那么sub_op可以表示哪个功能发的
function Res.dec_one(player, id, count, res, op, msg)
    local res_m = res_func[id]
    if not res_m then
        if Res.is_item(id) then
            res_m = res_func[RES_ITEM]
        else
            eprintf("Res.dec_one no function found:%s,op %d", tostring(id), op)
            return
        end
    end

    -- 把res也传过去，有时候某些资源会有特殊属性，如强化、升星...
    return res_m.dec(player, id, count, op, msg, res)
end

-- 检查某个资源是否足够
-- @param id 资源id
-- @param count 资源数量
-- @param res 资源结构，包含绑定、星级、强化等特殊属性时需要传，否则传nil
function Res.check_dec_one(player, id, count, res)
    local res_m = res_func[id]
    if not res_m then
        if Res.is_item(id) then
            res_m = res_func[RES_ITEM]
        else
            eprintf("Res.check_dec_one no function found:%s, %s",
                    tostring(id), debug.traceback())
            return false
        end
    end

    -- 把res也传过去，有时候某些资源会有特殊属性，如强化、升星...
    return res_m.check_dec(player, res.id, res.count, res)
end

-- 检测资源是否足够
-- @param res_list 资源数组
function Res.check_dec(player, res_list)
    for _, res in pairs(res_list) do
        if not Res.check_dec_one(player, res[1], res[2], res) then
            return false, res
        end
    end

    return true
end

-- 检测单个资源能否放下
function Res.check_add_one(player, id, count, res, ext)
    local res_m = res_func[id]
    if not res_m then
        if Res.is_item(id) then
            res_m = res_func[RES_ITEM]
        else
            eprintf("Res.check_add_one no function found:%s, %s",
                    tostring(id), debug.traceback())
            return false
        end
    end

    -- 货币类一定是能放下的，没注册函数
    local check_add_func = res_m.check_add
    if not check_add_func then return true end

    return check_add_func(player, res.id, res.count, res, ext)
end

-- 检测能否放下资源
function Res.check_add(player, res_list, ext)
    for _, res in pairs(res_list) do
        if not Res.check_add_one(player, res[1], res[2], res) then
            return false, res
        end
    end

    return true
end

-- 添加资源，如果背包放不下，所有奖励发放到邮件
-- @param res_list 资源数组{{1, 999},{100001, 9, bind = 1}}
-- @param op 操作代码，用于记录日志，见log_header定义
-- @param msg 额外日志信息字符串。比如op为邮件，那么sub_op可以表示哪个功能发的
-- @param ext 扩展参数，如邮件标题、绑定属性、星级等
function Res.add_or_mail(player, res_list, op, msg, ext)
    if Res.check_add(player, res_list, ext) then
        Res.add(player, res_list, op, msg, ext)
    else
        local title = (ext and ext.mail_title) or LANG.mail001
        local ctx =  (ext and ext.mail_ctx) or LANG.mail002
        g_mail_mgr:raw_send_mail(player.pid, title, ctx, res_list, op)
    end
end

--//////////////////////////////////////////////////////////////////////////////
--// 下面为聚合接口，把多个步骤合并成一次操作
--// 通常用于rpc在其他进程通过pid调用，以解决异步问题
--// 如跨进程的购买，check_and_add_dec可以一次执行完成

-- 检测能否放入背包，如果可以则直接放背包，否则返回false
function Res.check_and_add(player, res_list, op, msg, ext)
    if not Res.check_add(player, res_list, ext) then
        return false, 1
    end

    Res.add(player, res_list, op, msg, ext)
    return true
end

-- 检测能否直接扣除，如果可以则直接扣除，否则返回false
function Res.check_and_dec(player, res_list, op, msg)
    if not Res.check_dec(player, res_list) then
        return false, 1
    end

    Res.dec(player, res_list, op, msg)
    return true
end

-- 检测能否添加、扣除，如果可以则一次性执行扣除、添加操作，否则返回false
function Res.check_and_add_dec(player, add_list, dec_list, op, msg, ext)
    if not Res.check_add(player, add_list, ext) then
        return false, 1
    end
    if not Res.check_dec(player, dec_list) then
        return false, 2
    end

    Res.dec(player, dec_list, op, msg)
    Res.add(player, add_list, op, msg, ext)

    return true
end

-- 生成具体的资源回调函数
local function make_cb()
    made = true
    local ThisCall = require "modules.system.this_call"

    local make = function (func, msg, id)
        if not func then return nil end
        return ThisCall.make_from_player(func, nil, msg, id) or func
    end

    local f_name = {"check_add", "check_dec", "add", "dec"}
    for id, res in pairs(res_func) do
        for _, name in pairs(f_name) do
            res[name] = make(res[name], name, id)
        end
    end

    item_conf = require "config.item_conf"
end
SE.reg(SE_SCRIPT_LOADED, make_cb, 10)

return Res
