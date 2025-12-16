-- player_mgr.lua
-- 2017-04-03
-- xzc

-- 玩家对象管理
PlayerMgr = {}

local Player = require "modules.player.player"
local this = memory("PlayerMgr", {
    player = {}, -- [pid] = Player，已已初始化的玩家对象
    uninit_player = {}, -- 未初始化的玩家对象
})

-- g_app:reg_5s_timer(self, this.check_enter_fail)

-- 判断玩家是否存在
function PlayerMgr.is_pid_exist(pid)
    -- TODO:考虑起服加载玩家基础数据
    return true
end

-- 获取玩家对象
function PlayerMgr.get_uninit_player(pid)
    return this.uninit_player[pid]
end

-- 获取已初始化的玩家对象
function PlayerMgr.get_player(pid)
    return this.player[pid]
end

-- 获取所有已初始化玩家对象
function PlayerMgr.get_all_player()
    return this.player
end

-- 进入游戏，创建玩家对象
function PlayerMgr.enter_game(info)
    local pid = info.pid

    print("player enter game", pid)
end

-- 退出游戏，保存数据销毁玩家对象
function PlayerMgr.exit_game(pid, reason)
    print("player exit game", pid, reason)

    local player = this.uninit_player[pid]

    local session_id
    if not player then
        print("exit game no player", pid)
    else
        session_id = player.session_id
    end
    local addr = Router.find_worker_addr(W_ACCOUNT, pid)
    Send.Account.logout_completed(addr, pid, session_id)
end

return PlayerMgr
