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
    MoneyInfo = {
        s = "player.MoneyInfo", i = 25
    },
    -- 26 货币更新
    MoneyUpdate = {
        s = "player.MoneyUpdate", i = 26
    },

    -- 9 聊天
    ChatInfo = {
        s = "chat.SChatInfo", i = 9
    },
    -- 10
    ChatMsg = {
        s = "chat.SChatMsg", c = "chat.CChatMsg", i = 10

    },

    -- 11 一些零散的功能，不必定义独立的协议，可以都放这里
    MiscWelcome = {
        s = "misc.SWelcome", i = 11
    },
    -- 12 提示消息
    AlertMsg = {
        s = "misc.AlertMSG", i = 12
    },
    -- 13 创角欢迎邮件
    WelcomeGet = {
        c = "misc.CWelcomeGet", i = 13
    },

    -- 14 背包数据
    BagInfo = {
        s = "item.SBagInfo", i = 14
    },
    -- 31 更新单个道具信息
    ItemUpdate = {
        s = "item.SItemUpdate", i = 31
    },
    -- 32 更新单个道具属性
    ItemAttrUpdate = {
        s = "item.SItemAttrUpdate", i = 32
    },
    -- 33 使用道具
    ItemUse = {
        s = "item.ItemUse", c = "item.ItemUse", i = 33
    },
    -- 34 穿戴装备
    EquipOn = {
        c = "item.CTakeOn", i = 34
    },
    -- 35 卸下装备
    EquipOff = {
        c = "item.CTakeOff", i = 35
    },

    -- 15 邮件数据
    MailInfo = {
        s = "mail.SMailInfo", i = 15
    },
    -- 16 删除邮件
    MailDel = {
        s = "mail.MailDel", c = "mail.MailDel", i = 16

    },
    -- 17 新增邮件
    MailNew = {
        s = "mail.SNewMail", i = 17
    },
    -- 27 阅读邮件
    MailRead = {
        s = "mail.SMailRead", c = "mail.CMailRead", i = 27
    },
    -- 28 领取附件
    MailClaim = {
        s = "mail.SMailClaim", c = "mail.CMailClaim", i = 28
    },
    -- 29 一键领取所有附件
    MailClaimAll = {
        s = "mail.SMailClaimAll", c = "mail.CMailClaimAll", i = 29
    },
    -- 30 一键删除已读邮件
    MailDelRead = {
        s = "mail.SMailDelRead", c = "mail.CMailDelRead", i = 30
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
    -- 22 实现进入场景
    EntityEnterScene = {
        s = "entity.SEnterScene", i = 22
    },
    -- 23 场景实体属性
    EntityProperty = {
        s = "entity.SProperty", i = 23
    },
}
