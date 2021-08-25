-- 玩家相关协议处理

if WORLD == APP_TYPE then
    local Player = require "modules.player.player"

    Cmd.reg(PLAYER.ENTERDUNGEON, Player.enter_dungeon)
    Cmd.reg(PLAYER.ENTER, PlayerMgr.on_enter_world, true)

    Cmd.reg_srv(SYS.PLAYER_OFFLINE, PlayerMgr.on_player_offline)
    Cmd.reg_srv(SYS.PLAYER_OTHERWHERE, PlayerMgr.on_login_otherwhere)

--     PE.reg(PE_ENTER, player_base_cb(Base.send_info))
--     g_res:reg_player_res(RES.GOLD, Player.get_gold, Player.add_gold,
--                          Player.dec_gold)
end

