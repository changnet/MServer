-- client & server protocol define

--[[
定义客户端与服务器之间的通信协议，其格式必须是固定的，用于自动解析并生成协议号

-- 定义模块号，大写。之所以没定义成全局的CHAT_CHAT而是定义成一个table是方便模块里本地化
CHAT = {
    -- 定义功能，大写，表示常量不可修改
    -- s表示服务器发往客户端使用的protobuf或者flatbuffer结构名（没有可以忽略此字段）
    -- c表示客户端发往服务器使用的protobuf或者flatbuffer结构名（没有可以忽略此字段）
    -- i是工具自动生成的协议号，定义时无需定义该字段
    CHAT = { s = "chat.SChat", c = "chat.CChat", i = 1}
}

]]

-- 玩家模块
PLAYER = {
    -- 登录
    LOGIN = {s = "player.SLogin"},
    -- 心跳包
    PING = {s = "player.SPing"},
    -- 创角
    CREATE = {s = "player.SCreateRole"},
    -- 进入游戏
    ENTER = {s = "player.SEnterWorld"},
    -- 顶号
    OTHER = {s = "player.SLoginOtherWhere"},
    -- 登录时下发基础数据
    BASE = {s = "player.SBase"},
    -- 更新通用资源
    UPDATE_RES = {s = "player.SUpdateRes"},
}
