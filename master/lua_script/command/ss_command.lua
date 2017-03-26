-- server与server交互协议定义

--[[
1.schema文件名就是模块名。如登录模块MODULE_LOGIN中所有协议内容都定义在login.fbs
2.schema中的object名与具体功能协议号相同，并标记服务器或客户端使用。如登录功能LOGIN，
    s2c则为slogin，c2s则为clogin,s2s则为sslogin，当然，也可以使用通用的结构。如empty
    表示空协议。
3.注意协议号都为大写。这样在代码与容易与其他函数调用区分，并表明它为一个宏定义，不可修改。
4.协议号命名为Module_Function，如SYS_SYN表示 系统模块_Socket同步
5.协议号是一个16bit数字，前8bits表示模块，后8位表示功能。

]]

local MODULE_SYSTEM = (0x00 << 8) -- 0

local SS =
{
    CLT_CMD   = { MODULE_SYSTEM + 0x01 },     -- 收到另外一个srv转发的客户端包
    RPC_REQ   = { MODULE_SYSTEM + 0x02 },     -- rcp通信request包
    RPC_RES   = { MODULE_SYSTEM + 0x03 },     -- rcp通信response包

    SYS_SYN = { MODULE_SYSTEM + 0x04,"system.bfbs","ssregister" },
    SYS_ACK = { MODULE_SYSTEM + 0x05,"system.bfbs","ssregister" }

    -- 其他功能模块协议
    -- LOGIN = { MODULE_LOGIN + 0x01,"login,bfbs","clogin" },
}

-- 使用oo的define功能让local后仍能热更
return oo.define( SS,... )
