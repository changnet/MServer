-- server与server交互协议定义

--[[
1.schema文件名就是模块名。如登录模块MODULE_LOGIN中所有协议内容都定义在login.fbs
2.schema中的object名与具体功能协议号相同，并标记服务器或客户端使用。如登录功能LOGIN，
    s2c则为SLogin，c2s则为CLogin,s2s则为SSLogin.
3.注意协议号都为大写。这样在代码与容易与其他函数调用区分，并表明它为一个宏定义，不可修改。
4.协议号命名为Module_Function，如SYS_SYN表示 系统模块_Socket同步
5.协议号是一个16bit数字，前8bits表示模块，后8位表示功能。

]]

local MODULE_SYSTEM = (0x00 << 8) -- 0
local MODULE_PLAYER = (0x01 << 8) -- 256

local SS =
{
    SYS_REG       = { MODULE_SYSTEM + 0x04,"system.pb","system.SSRegister" },
    SYS_CMD_SYNC  = { MODULE_SYSTEM + 0x05,"system.pb","system.SSCommandSync" },
    SYS_BEAT      = { MODULE_SYSTEM + 0x06,"system.pb","system.SSBeat" },        -- 心跳包
    SYS_SYNC_DONE = { MODULE_SYSTEM + 0x08,"system.pb","system.SSSyncDone" },    -- 同步完成

    PLAYER_OFFLINE    = { MODULE_PLAYER + 0x01,"player.pb","player.SSOffline" }, -- 玩家下线
    PLAYER_OTHERWHERE = { MODULE_PLAYER + 0x02,"player.pb","player.SSOtherWhere" }, -- 玩家下线

    -- 其他功能模块协议
    -- LOGIN = { MODULE_LOGIN + 0x01,"login,bfbs","CLogin" },
}

-- flatbuffers
-- local SS =
-- {
--     SYS_SYN      = { MODULE_SYSTEM + 0x04,"system.bfbs","SSRegister" },
--     SYS_ACK      = { MODULE_SYSTEM + 0x05,"system.bfbs","SSRegister" },
--     SYS_BEAT     = { MODULE_SYSTEM + 0x06,"system.bfbs","SSBeat" },        -- 心跳包
--     SYS_HOT_Fix  = { MODULE_SYSTEM + 0x07,"system.bfbs","SSHotFix" },    -- 热更

--     PLAYER_OFFLINE    = { MODULE_PLAYER + 0x01,"player.bfbs","SSOffline" }, -- 玩家下线
--     PLAYER_OTHERWHERE = { MODULE_PLAYER + 0x02,"player.bfbs","SSOtherWhere" }, -- 玩家下线

--     -- 其他功能模块协议
--     -- LOGIN = { MODULE_LOGIN + 0x01,"login,bfbs","CLogin" },
-- }

return SS
