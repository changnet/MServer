-- 玩家相关协议处理

if WORLD == APP_TYPE then
    local Player = require "modules.player.player"

    Cmd.reg(PLAYER.ENTERDUNGEON, Player.enter_dungeon)
    Cmd.reg(PLAYER.ENTER, PlayerMgr.on_enter_world, true)

    Cmd.reg_srv(SYS.PLAYER_OFFLINE, PlayerMgr.on_player_offline)
    Cmd.reg_srv(SYS.PLAYER_OTHERWHERE, PlayerMgr.on_login_otherwhere)

    -- 自动注册货币
    local RES = Res.get_def()
    local Base = require "modules.player.base"

    for _, id in pairs(RES) do
        if Res.is_money(id) then
            Res.reg(id, Base.check_dec_money, nil, Base.add_money, Base.dec_money)
        end
    end
end
