--[[
定义客户端与服务器之间的通信协议，其格式必须是固定的，用于自动解析并生成协议号
解析此文件的脚本能力有限，不要瞎写或者加特殊字符

-- 格式要求
-- 1. 名字和等号比如在同一行，如 PlayerLogin =
-- 2. c、s字段和结构名比如在同一行，如s = "chat.SChat"，但c、s可以分开在多行写
-- 3. 大括号必须分开在不同行
]]

-- net message
M = {
    -- 1 登录
    PlayerLogin = {
        s = "player.SLogin", c = "player.CLogin", i = 1, w = "gateway"

    },
    -- 2 ping后端延迟
    PlayerPing = {
        s = "player.SPing", c = "player.CPing", i = 2

    },
    -- 3 创角
    PlayerCreate = {
        s = "player.SCreateRole", c = "player.CCreateRole", i = 3, w = "gateway"

    },
    -- 4 进入游戏
    PlayerEnter = {
        s = "comm.Empty", c = "player.CEnterGame", i = 4, w = "gateway"

    },
    -- 24 登录游戏完成，所有数据已下发
    PlayerReady = {
        s = "comm.Empty", i = 24
    },
    -- 5 踢下线
    PlayerKick = {
        s = "player.SKick", i = 5
    },
    -- 6 登录时下发基础数据
    PlayerBase = {
        s = "player.SBase", i = 6
    },
    -- 8 进入副本
    PlayerEnterDungeon = {
        c = "player.CEnterDungeon", i = 8
    },
    -- 25 货币信息初始化
    PlayerMoneyInfo = {
        s = "player.MoneyInfo", i = 25
    },
    -- 26 
    PlayerMoneyUpdate = {
        s = "player.MoneyUpdate", i = 26
    },

    -- 9 聊天
    ChatChatInfo = {
        s = "chat.SChatInfo", i = 9
    },
    -- 10 
    ChatDoChat = {
        s = "chat.SDoChat", c = "chat.CDoChat", i = 10

    },

    -- 11 一些零散的功能，不必定义独立的协议，可以都放这里
    MiscWelcome = {
        s = "misc.SWelcome", i = 11
    },
    -- 12 
    MiscAlertMsg = {
        s = "misc.AlertMSG", i = 12
    },
    -- 13 
    MiscWelcomeGet = {
        c = "misc.CWelcomeGet", i = 13
    },

    -- 14 背包模块
    BagInfo = {
        s = "bag.SBagInfo", i = 14
    },

    -- 15 邮件模块
    MailInfo = {
        s = "mail.SMailInfo", i = 15
    },
    -- 16 
    MailDel = {
        s = "mail.SMailDel", c = "mail.CMailDel", i = 16

    },
    -- 17 
    MailNew = {
        s = "mail.SNewMail", i = 17
    },

    -- 18 场景实体相关协议
    EntityAppear = {
        s = "entity.SAppear", i = 18
    },
    -- 19 场景中实体消失
    EntityDisappear = {
        s = "entity.SDisappear", i = 19
    },
    -- 20 
    EntityMove = {
        s = "entity.SMove", c = "entity.CMove", i = 20

    },
    -- 21 
    EntityPos = {
        s = "entity.SPos", i = 21
    },
    -- 22 
    EntityEnterScene = {
        s = "entity.SEnterScene", i = 22
    },
    -- 23 
    EntityProperty = {
        s = "entity.SProperty", i = 23
    },
}
