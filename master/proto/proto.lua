-- // AUTO GENERATE,DO NOT MODIFY

local PLAYER = (1 << 8) -- 256
local BAG = (2 << 8) -- 512
local MISC = (3 << 8) -- 768
local CHAT = (4 << 8) -- 1024
local MAIL = (5 << 8) -- 1280
local ENTITY = (6 << 8) -- 1536


local SC =
{
    PLAYER_LOGIN = { PLAYER + 1,"player.pb","player.SLogin" }, -- 257
    PLAYER_PING = { PLAYER + 2,"player.pb","player.SPing" }, -- 258
    PLAYER_CREATE = { PLAYER + 3,"player.pb","player.SCreateRole" }, -- 259
    PLAYER_ENTER = { PLAYER + 4,"player.pb","player.SEnterWorld" }, -- 260
    PLAYER_OTHER = { PLAYER + 5,"player.pb","player.SLoginOtherWhere" }, -- 261
    PLAYER_BASE = { PLAYER + 6,"player.pb","player.SBase" }, -- 262
    PLAYER_UPDATE_RES = { PLAYER + 7,"player.pb","player.SUpdateRes" }, -- 263
    BAG_INFO = { BAG + 1,"bag.pb","bag.SBagInfo" }, -- 513
    MISC_WELCOME = { MISC + 1,"misc.pb","misc.SWelcome" }, -- 769
    MISC_TIPS = { MISC + 3,"misc.pb","misc.STips" }, -- 771
    CHAT_CHATINFO = { CHAT + 1,"chat.pb","chat.SChatInfo" }, -- 1025
    CHAT_DOCHAT = { CHAT + 2,"chat.pb","chat.SDoChat" }, -- 1026
    MAIL_INFO = { MAIL + 1,"mail.pb","mail.SMailInfo" }, -- 1281
    MAIL_DEL = { MAIL + 2,"mail.pb","mail.SMailDel" }, -- 1282
    MAIL_NEW = { MAIL + 3,"mail.pb","mail.SNewMail" }, -- 1283
    ENTITY_APPEAR = { ENTITY + 1,"entity.pb","entity.SAppear" }, -- 1537
    ENTITY_DISAPPEAR = { ENTITY + 2,"entity.pb","entity.SDisappear" }, -- 1538
    ENTITY_MOVE = { ENTITY + 3,"entity.pb","entity.SMove" }, -- 1539
    ENTITY_POS = { ENTITY + 4,"entity.pb","entity.SPos" }, -- 1540
    ENTITY_ENTERSCENE = { ENTITY + 5,"entity.pb","entity.SEnterScene" }, -- 1541
    ENTITY_PROPERTY = { ENTITY + 6,"entity.pb","entity.SProperty" }, -- 1542

}

local CS =
{
    PLAYER_LOGIN = { PLAYER + 1,"player.pb","player.CLogin" }, -- 257
    PLAYER_PING = { PLAYER + 2,"player.pb","player.CPing" }, -- 258
    PLAYER_CREATE = { PLAYER + 3,"player.pb","player.CCreateRole" }, -- 259
    PLAYER_ENTER = { PLAYER + 4,"player.pb","player.CEnterWorld" }, -- 260
    MISC_WELCOME_GET = { MISC + 2,"misc.pb","misc.CWelcomeGet" }, -- 770
    CHAT_DOCHAT = { CHAT + 2,"chat.pb","chat.CDoChat" }, -- 1026
    MAIL_DEL = { MAIL + 2,"mail.pb","mail.CMailDel" }, -- 1282
    ENTITY_MOVE = { ENTITY + 3,"entity.pb","entity.CMove" }, -- 1539

}

return { SC,CS }
