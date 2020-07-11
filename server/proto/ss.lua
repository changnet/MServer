-- server & server protocol define

--[[
定义服务器之间通信协议

1.schema文件名就是模块名。如登录模块MODULE_LOGIN中所有协议内容都定义在login.fbs
2.schema中的object名与具体功能协议号相同，并标记服务器或客户端使用。如登录功能LOGIN，
    s2c则为SLogin，c2s则为CLogin,s2s则为SSLogin.
3.注意协议号都为大写。这样在代码与容易与其他函数调用区分，并表明它为一个宏定义，不可修改。
]]

-- 系统通用协议
SYS = {
    REG = {
        s = "system.SSRegister"
    },
    CMD_SYNC  = {
        s = "system.SSCommandSync"
    },
    -- 心跳包
    BEAT = {
        s = "system.SSBeat"
    },
    -- 同步完成
    SYNC_DONE = {
        s = "system.SSSyncDone"
    },
    -- 玩家下线
    PLAYER_OFFLINE = {
        s = "player.SSOffline"
    },
    -- 玩家被顶号
    PLAYER_OTHERWHERE = {
        s = "player.SSOtherWhere"
    },
}

--[[
如果使用的是flatbuffer，没有package名,那定义就是 文件名.结构名
SYS = {
    REG = {
        s = "system.SSRegister", -- 协议结构定义在system.fbs中
    },
}
]]
