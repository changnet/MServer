-- // AUTO GENERATE,DO NOT MODIFY

local PLAYER = (0x01 << 8)
local BAG = (0x02 << 8)
local MISC = (0x03 << 8)
local CHAT = (0x04 << 8)
local MAIL = (0x05 << 8)
local ENTITY = (0x06 << 8)


local SC =
{
    PLAYER_LOGIN = { PLAYER + 0x01,"player.pb","player.SLogin" },
    PLAYER_PING = { PLAYER + 0x02,"player.pb","player.SPing" },
    PLAYER_CREATE = { PLAYER + 0x03,"player.pb","player.SCreateRole" },
    PLAYER_ENTER = { PLAYER + 0x04,"player.pb","player.SEnterWorld" },
    PLAYER_OTHER = { PLAYER + 0x05,"player.pb","player.SLoginOtherWhere" },
    PLAYER_BASE = { PLAYER + 0x06,"player.pb","player.SBase" },
    PLAYER_UPDATE_RES = { PLAYER + 0x07,"player.pb","player.SUpdateRes" },
    BAG_INFO = { BAG + 0x01,"bag.pb","bag.SBagInfo" },
    MISC_WELCOME = { MISC + 0x01,"misc.pb","misc.SWelcome" },
    MISC_TIPS = { MISC + 0x03,"misc.pb","misc.STips" },
    CHAT_CHATINFO = { CHAT + 0x01,"chat.pb","chat.SChatInfo" },
    CHAT_DOCHAT = { CHAT + 0x02,"chat.pb","chat.SDoChat" },
    MAIL_INFO = { MAIL + 0x01,"mail.pb","mail.SMailInfo" },
    MAIL_DEL = { MAIL + 0x02,"mail.pb","mail.SMailDel" },
    MAIL_NEW = { MAIL + 0x03,"mail.pb","mail.SNewMail" },
    ENTITY_APPEAR = { ENTITY + 0x01,"entity.pb","entity.SAppear" },
    ENTITY_DISAPPEAR = { ENTITY + 0x02,"entity.pb","entity.SDisappear" },
    ENTITY_MOVE = { ENTITY + 0x03,"entity.pb","entity.SMove" },
    ENTITY_POS = { ENTITY + 0x04,"entity.pb","entity.SPos" },
    ENTITY_ENTERSCENE = { ENTITY + 0x05,"entity.pb","entity.SEnterScene" },

}

local CS =
{
    PLAYER_LOGIN = { PLAYER + 0x01,"player.pb","player.CLogin" },
    PLAYER_PING = { PLAYER + 0x02,"player.pb","player.CPing" },
    PLAYER_CREATE = { PLAYER + 0x03,"player.pb","player.CCreateRole" },
    PLAYER_ENTER = { PLAYER + 0x04,"player.pb","player.CEnterWorld" },
    MISC_WELCOME_GET = { MISC + 0x02,"misc.pb","misc.CWelcomeGet" },
    CHAT_DOCHAT = { CHAT + 0x02,"chat.pb","chat.CDoChat" },
    MAIL_DEL = { MAIL + 0x02,"mail.pb","mail.CMailDel" },
    ENTITY_MOVE = { ENTITY + 0x03,"entity.pb","entity.CMove" },

}

return { SC,CS }
