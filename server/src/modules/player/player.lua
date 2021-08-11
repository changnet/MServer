-- playe.lua
-- 2017-04-03
-- xzc
-- 玩家对象

local AutoId = require "modules.system.auto_id"

--[[
玩家子模块，以下功能会自动触发
1. 创建对象
2. 加载数据库:db_load
3. 数据初始化:on_init
4. 定时存库  :db_save
5. 定时器    :on_timer
]]

local Base = require "modules.player.base"
local Chat = require "modules.chat.chat"
local Misc = require "modules.misc.misc"
local Bag = require "modules.bag.bag"
local Mail = require "modules.mail.mail"

local SyncMongodb = require "mongodb.sync_mongodb"
local AttributeSys = require "modules.attribute.attribute_sys"

local RES = RES
local method_thunk = method_thunk

local AREA_SESSION = {}

-- 这些子模块是指需要存库的数据模块(在登录、读库、初始化、存库都用的同一套流程)
local sub_module = {
    {name = "base", new = Base},
    {name = "chat", new = Chat},
    {name = "misc", new = Misc},
    {name = "bag", new = Bag},
    {name = "Mail", new = Mail}
}

local Player = oo.class(...)

function Player:__init(pid)
    self.pid = pid

    self.auto_id = AutoId()

    self.timer_cnt = 0
    self.timer_1scb = {}
    self.timer_5scb = {}

    -- 创建各个子模块，这些模块包含统一的db存库、加载、初始化规则
    for _, module in pairs(sub_module) do
        self[module.name] = module.new(pid, self)
    end

    -- 不需要标准流程的子模块
    self.abt_sys = AttributeSys(pid)

    self.timer = g_timer_mgr:interval(5000, 5000, -1, self, self.do_timer)
end

-- 获取玩家id
function Player:get_pid()
    return self.pid
end

-- 发送数据包到客户端
function Player:send_pkt(cmd, pkt, ecode)
    return SrvMgr.send_clt_pkt(self.pid, cmd, pkt, ecode)
end

-- 定时器事件
function Player:do_timer()
    for _, cb in pairs(self.timer_5scb) do cb() end
end

-- 注册1s定时器
function Player:reg_5s_timer(this, method)
    local id = self.auto_id:next_id()

    self.timer_5scb[id] = method_thunk(this, method)
    return id
end

-- 取消1s定时器
function Player:remove_5s_timer(id)
    self.timer_5scb[id] = nil
end

-- 是否新创建的玩家(仅在登录过程有用，登录完成后肯定不是新玩家了)
function Player:is_new()
    return (1 == self.base.root.new)
end

-- 开始从db加载各模块数据
function Player:module_db_load()
    -- 根据顺序加载数据库数据
    for step, module in pairs(sub_module) do
        local ok = self[module.name]:db_load(self.sync_db)
        if not ok then
            elog("player db load error,pid %d,step %d", self.pid, step)
            return false
        end
    end

    -- 是否为新创建的玩家
    local is_new = self:is_new()

    -- db数据初始化，如果模块之间有数据依赖，请自己调整好顺序
    for _, module in pairs(sub_module) do self[module.name]:on_init(is_new) end
    self.base_root = self.base.root -- 增加一个引用，快速取基础数据

    self:on_login()

    -- 如果为新创建的玩家，初始化后及时存库
    if is_new then
        self.base.root.new = nil
        if not self.base:db_save() then
            elog("player is new,db save fail:%d", self.pid)
            return false
        end
    end

    self.sync_db = nil
    self.ok = true -- 标识初始化完成，未初始化完成的不要存库

    PlayerMgr.enter_success(self)
    return true
end

-- 开始加载数据
function Player:db_load()
    local sync_db = SyncMongodb(g_mongodb, self.module_db_load)

    self.sync_db = sync_db
    return sync_db:start(self)
end

-- 获取同步加载db(只在登录过程中有效)
function Player:get_sync_db()
    return self.sync_db
end

-- 是否正在加载中
function Player:is_loading()
    if not self.sync_db or not self.sync_db:valid() then return false end

    return true
end

-- 登录游戏
function Player:on_login()
    for _, module in pairs(sub_module) do self[module.name]:on_login() end

    PE.fire_event(PE_ENTER, self)

    -- 所有系统处理完后，计算一次总属性
    self.abt_sys:calc_final_abt()

    -- TODO 暂时默认进入第一个场景服
    self:set_session(ASE)

    local conn = SrvMgr.get_conn_by_session(ASE)
    -- 同步基础属性，名字、外显等
    self.base:update()
    -- 同步战斗属性到场景
    self.abt_sys:sync_battle_abt(ASE)
    -- 实体进入场景
    Rpc.proxy(conn):player_init_scene(self.pid, nil, 0)

    g_log_mgr:login_or_logout(self.pid, LOG.LOGIN)

    self:send_pkt(PLAYER.ENTER, {}) -- 通知前端玩家所有数据初始化完成已进入场景
    return true
end

-- 设置玩家所在的session
function Player:set_session(session)
    self.session = session

    -- 设置session到网关，实现自动转发协议
    Rpc.call(GSE, CltMgr.set_player_session, self.pid, session)
end

-- 获取玩家所在session
function Player:get_session()
    return self.session
end

-- 退出游戏
function Player:on_logout()
    g_timer_mgr:stop(self.timer)

    -- 未初始化完成不能存库，防止数据被覆盖
    if not self.ok then
        elog("player initialize not finish,runing logout:%d", self.pid)
        return
    end

    -- 退出事件
    PE.fire_event(PE_EXIT, self)

    -- 各个子模块退出处理
    for _, module in pairs(sub_module) do self[module.name]:on_logout() end

    -- 存库
    for _, module in pairs(sub_module) do self[module.name]:db_save() end

    -- 退出场景
    Rpc.proxy(self.pid):player_exit(self.pid)
    g_log_mgr:login_or_logout(self.pid, LOG.LOGOUT)

    printf("player logout,pid = %d", self.pid)
end

-- 获取对应的模块
-- @name:模块名，参考sub_module
function Player:get_module(name)
    return self[name]
end

-- 获取通用的存库数据
function Player:get_misc_var(key)
    return self.misc:get_var(key)
end

-- 获取通用数据
function Player:get_misc_root()
    return self.misc:get_root()
end

-- 获取元宝数量
function Player:get_gold()
    return self.base_root.gold
end

-- 增加元宝
function Player:add_gold(count, op, sub_op)
    local new_val = self.base_root.gold + count
    g_log_mgr:gold_log(self.pid, count, new_val, op, sub_op)

    self.base_root.gold = new_val
    self.base:update_res(RES.GOLD, self.base_root.gold)
end

-- 增加元宝
function Player:dec_gold(count, op, sub_op)
    local new_val = self.base_root.gold - count
    g_log_mgr:gold_log(self.pid, count, new_val, op, sub_op)

    self.base_root.gold = new_val
    self.base:update_res(RES.GOLD, self.base_root.gold)
end

-- 获取等级
function Player:get_level()
    return self.base_root.level
end

-- 设置某个系统属性
function Player:set_sys_abt(id, abt_list)
    self.abt_sys:set_sys_abt(id, abt_list)
end

-- 进入某个副本
function Player:enter_dungeon(pkt)
    local id = pkt.id
    -- 先到对应的进程检测是否能够进入对应的副本，这里只是测试不用检测

    -- 双数在场景进程1,单数在2，用来测试切换。真实情况是根据拥挤场景来规则
    local index = (0 == id % 2 and 1 or 2)

    local session = AREA_SESSION[index]
    if not session then
        session = g_app:encode_session("area", index, tonumber(g_app.id))
        AREA_SESSION[index] = session
    end

    local conn = SrvMgr.get_conn_by_session(session)

    -- 然后玩家从当前进程退出场景
    if session ~= self.session then
        Rpc.proxy(self.pid):player_exit(self.pid)

        self:set_session(session)

        -- 同步基础属性，名字、外显等
        self.base:update()
        -- 同步战斗属性到场景
        self.abt_sys:sync_battle_abt(session)
        -- 在另一个进程初始化玩家场景
        Rpc.proxy(conn):player_init_scene(self.pid, nil, id)
    else
        -- 在同一进程，直接进入场景
        Rpc.proxy(conn):player_enter_dungeon(self.pid, id)
    end

    print("player enter dungeon", self.pid, id)
end

return Player
