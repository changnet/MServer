-- player_mgr.lua
-- 2017-04-03
-- xzc
-- 玩家对象管理
local Player = require "modules.player.player"
local PlayerMgr = oo.singleton(...)

function PlayerMgr:__init()
    self.player = {} -- pid为key，Player为对象
    self.raw_player = {} -- 未初始化的玩家对象

    g_app:reg_5s_timer(self, self.check_enter_fail)
end

-- 判断玩家是否存在
function PlayerMgr:is_pid_exist(pid)
    -- TODO:考虑起服加载玩家基础数据
    return true
end

-- 获取玩家对象
function PlayerMgr:get_raw_player(pid)
    return self.raw_player[pid]
end

-- 获取已初始化的玩家对象
function PlayerMgr:get_player(pid)
    return self.player[pid]
end

-- 获取所有已初始化玩家对象
function PlayerMgr:get_all_player()
    return self.player
end

-- 处理玩家初始化成功
function PlayerMgr:enter_success(player)
    local pid = player:get_pid()

    self.player[pid] = player
    self.raw_player[pid] = nil
    g_authorize:set_player(pid)

    printf("player enter,pid = %d", pid)
end

-- 玩家初始化失败
function PlayerMgr:do_enter_fail(player)
    local pid = player:get_pid()
    print("player enter timeout,kill connection", pid)

    -- 通知网关关闭连接
    g_rpc:kill_player_connect(pid)
end

-- 定时检测加载失败的玩家
function PlayerMgr:check_enter_fail()
    local wait_del = {}
    for pid, player in pairs(self.raw_player) do
        if not player:is_loading() then
            wait_del[pid] = true
            self:do_enter_fail(player)
        end
    end

    for k in pairs(wait_del) do self.raw_player[k] = nil end
end

-- 玩家进入游戏世界，创建对象
function PlayerMgr:on_enter_world(clt_conn, pid, pkt)
    local player = Player(pid)

    self.raw_player[pid] = player
    if not player:db_load() then
        elog("player db load fail,pid = %d", pid)
        return
    end

    printf("player login,pid = %d", pid)
end

-- 玩家离线
function PlayerMgr:on_player_offline(srv_conn, pkt)
    local pid = pkt.pid
    g_authorize:unset_player(pid)

    local player = self.player[pid]
    if not player then
        elog("player offline,object not found:%d", pid)
        return
    end

    player:on_logout()

    self.player[pid] = nil
    printf("player offline,pid = %d", pid)
end

-- 顶号
function PlayerMgr:on_login_otherwhere(srv_conn, pkt)
    printf("player login other where,pid = %d", pkt.pid)
end

local player_mgr = PlayerMgr()

return player_mgr
