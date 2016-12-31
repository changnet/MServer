-- server与client交互协议定义

--[[
1.schema文件名就是模块名。如登录模块MODULE_PLAYER中所有协议内容都定义在player.fbs
2.schema中的object名与具体功能协议号相同，并标记服务器或客户端使用。
    如登录功能LOGIN，s2c则为slogin，c2s则为clogin,s2s则为sslogin，当然，也可以使用通用的结构。如empty表示空协议。
3.注意协议号都为大写。这样在代码与容易与其他函数调用区分，并表明它为一个宏定义，不可修改。
4.协议号是一个16bit数字，前8bits表示模块，后8位表示功能。

]]

local MODULE_PLAYER = (0x01 << 8)
local MODULE_BAG    = (0x02 << 8)


local SC =
{
    PLAYER_LOGIN = { MODULE_PLAYER + 0x01,"player.bfbs","slogin" },
}

local CS =
{
    PLAYER_LOGIN = { MODULE_PLAYER + 0x01,"player.bfbs","clogin" },
}

-- 使用oo的define功能让这两个表local后仍能热更
return {oo.define( SC,"message_sc" ),oo.define( CS,"message_cs" )}
