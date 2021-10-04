-- base.lua
-- 2018-03-23
-- xzc
-- 玩家存库基础数据模块
local level_conf = require_conf("player.levelup_conf")

local Module = require "modules.player.module"

local Base = oo.class(..., Module)

function Base:__init(pid, player)
    self.root = nil
    Module.__init(self, pid, player)
end

-- 数据存库接口，自动调用
-- 必须返回操作结果
function Base:db_save()
    g_mongodb:update("base", string.format('{"_id":%d}', self.pid), self.root,
                     true)
    return true
end

-- 数据加载接口，自动调用
-- 必须返回操作结果
function Base:db_load(sync_db)
    local ecode, res = sync_db:find("base",
                                    string.format('{"_id":%d}', self.pid))
    if 0 ~= ecode then return false end -- 出错

    -- 新号，空数据
    if not res then
        self.root = {}
    else
        self.root = res[1] -- find函数返回的是一个数组
    end

    -- 开发中，会新增一些字段，如果这些字段需要初始化，则在这里初始化
    local root = self.root
    if not root.money then root.money = {} end

    -- 强制以object形式保存货币，这样查看数据时比较直观
    -- 另一个原因是，开发过程中由于不断增删资源以及预留，这id也不是连续的
    table.set_array(root.money, false)

    return true
end

-- 数据加载完成自动调用，用于初始化从数据库加载的数据
-- 必须返回操作结果
function Base:on_init()
    -- 新玩家初始化
    if 1 == self.root.new then
        self.root.level = 1 -- 初始1级
        self.root.money = {}
    end

    -- 计算基础属性
    self:calc_abt()

    return true
end

-- 计算基础属性
function Base:calc_abt()
    local conf = level_conf[self.root.level]
    self.player:set_sys_abt(ABTSYS.BASE, conf.attr)
end

-- 玩家数据已加载完成,必须返回操作结果
local base_info = {}
function Base:on_login()
    self.root.login = ev:time()

    return true
end

function Base:update()
    -- 更新基础数据到场景服
    base_info.sex = self.root.sex
    base_info.level = self.root.level
    base_info.name = self.root.name

    Rpc.call(self.player.session,
        EntityCmd.player_update_base, self.pid, base_info, true)
end

-- 玩家退出游戏
-- 必须返回操作结果，但这个结果不影响玩家数据存库
function Base:on_logout()
    self.root.logout = ev:time()
    return true
end

-- 发送基础数据
function Base:send_info()
    local pkt = {}
    pkt.money = self.root.money

    self.player:send_pkt(PLAYER.BASE, pkt)
end

-- 更新虚拟资源(铜钱、元宝等)
local res_pkt = {res_type = 0, val = 0}
function Base:update_res(res_type, val)
    res_pkt.val = val
    res_pkt.res_type = res_type
    self.player:send_pkt(PLAYER.UPDATE_RES, res_pkt)
end

-- 检测货币是否足够
function Base:check_dec_money(id, count)
    return count <= (self.root.money[id] or 0)
end

-- 增加货币
function Base:add_money(id, count, op, msg)
    local val = self.root.money[id] or 0
    local new_val = val + count
    g_log_mgr:money_log(self.pid, count, new_val, op, msg)

    self.root.money[id] = new_val
    self:update_res(id, new_val)
end

-- 扣除货币
function Base:dec_money(id, count, op, msg)
    local new_val = (self.root.money[id] or 0) - count
    g_log_mgr:money_log(self.pid, -count, new_val, op, msg)

    self.root.money[id] = new_val
    self:update_res(id, new_val)

    if new_val < 0 then
        elogf("money < 0, pid = %d, id = %d, new_val = %d, count = %d, op = %d",
            self.pid, id, count, new_val, op)
    end
end

PE.reg(PE_ENTER, Base.send_info)

return Base
