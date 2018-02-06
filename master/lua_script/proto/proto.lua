-- // AUTO GENERATE,DO NOT MODIFY

local PLAYER = (0x01 << 8)
local BAG = (0x02 << 8)
local MISC = (0x03 << 8)


local SC =
{
    PLAYER_LOGIN = { PLAYER + 0x01,"player.pb","player.SLogin" },
    PLAYER_PING = { PLAYER + 0x02,"player.pb","player.SPing" },
    PLAYER_CREATE = { PLAYER + 0x03,"player.pb","player.SCreateRole" },
    PLAYER_ENTER = { PLAYER + 0x04,"player.pb","player.SEnterWorld" },
    PLAYER_OTHER = { PLAYER + 0x05,"player.pb","player.SLoginOtherWhere" },

}

local CS =
{
    PLAYER_LOGIN = { PLAYER + 0x01,"player.pb","player.CLogin" },
    PLAYER_PING = { PLAYER + 0x02,"player.pb","player.CPing" },
    PLAYER_CREATE = { PLAYER + 0x03,"player.pb","player.CCreateRole" },
    PLAYER_ENTER = { PLAYER + 0x04,"player.pb","player.CEnterWorld" },

}

return { SC,CS }
