-- 玩家相关协议处理

if WORLD == APP_TYPE then
--     player_clt_cb(PLAYER.ENTERDUNGEON, Player.enter_dungeon)
    Cmd.reg(PLAYER.ENTER, PlayerMgr.on_enter_world, true)

--     player_mgr_srv_cb(SYS.PLAYER_OFFLINE, g_player_mgr.on_player_offline)
--     player_mgr_srv_cb(SYS.PLAYER_OTHERWHERE, g_player_mgr.on_login_otherwhere)

--     PE.reg(PE_ENTER, player_base_cb(Base.send_info))
--     g_res:reg_player_res(RES.GOLD, Player.get_gold, Player.add_gold,
--                          Player.dec_gold)
end

