-- player_mgr.lua
-- 2017-04-03
-- xzc

-- 玩家对象管理
PlayerMgr = {}

-- local Player = require "modules.player.player"
local this = memory("PlayerMgr", {
    player = {}, -- [pid] = Player，已初始化的玩家对象
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

    -- 玩家被顶号或者重复登录时，在验证时就会通知玩家下线，并且等玩家下线完成才会enter
    -- 如果这时候还存在玩家对象，说明是逻辑出错了，这里只能直接丢掉
    if this.player[pid] then
        eprint("player already exist", pid)
        this.player[pid] = nil
    end
    if this.uninit_player[pid] then
        eprint("player already exist in uninit", pid)
        this.uninit_player[pid] = nil
    end

    this.uninit_player[pid] = info

    -- 开始登录流程
    local ok = Player.login(info)
    -- 可能被顶号，判断session
    local new_info = this.uninit_player[pid]
    if new_info and new_info.session_id ~= info.session_id then
        eprint("player enter, session not match",
                pid, new_info.session_id, info.session_id)
        return
    end
    if ok then
        this.player[pid] = info
        this.uninit_player[pid] = nil
    else
        eprint("player enter fail", pid, info.session_id)
        this.uninit_player[pid] = nil
        assert(not this.player[pid])
    end
end

-- 退出游戏，保存数据销毁玩家对象
function PlayerMgr.exit_game(pid, reason)
    print("player exit game", pid, reason)

    local player = this.player[pid] or this.uninit_player[pid]

    if not player then
        print("exit game no player", pid)
        return PlayerMgr.exit_completed(pid, 0)
    end

    -- 如果玩家登录成功，通知退出
    -- 如果玩家在登录中或者在退出当中，标记为等待退出状态，等登录或者退出完成后再真正退出
    -- 如果中途出错中断了流程，这里是不处理的，等gm手动处理或者enter_game超时直接顶掉
    local status = player.status
    if status == PlayerStatus.NORMAL then
        Player.logout(player, reason)
    else
        print("player exit game in status", pid, status)
        player.status = status | PlayerStatus.WLOGOUT
    end
end

-- 退出游戏完成，销毁玩家对象
function PlayerMgr.exit_completed(pid, session_id)
    print("player logout completed", pid)
    this.player[pid] = nil
    this.uninit_player[pid] = nil

    local addr = Router.find_worker_addr(W_ACCOUNT, pid)
    Send.Account.logout_completed(addr, pid, session_id)
end

return PlayerMgr
