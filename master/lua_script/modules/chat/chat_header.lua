-- chat_header.lua
-- 2018-02-06
-- xzc

-- 聊天宏定义

local CHAT = 
{
    -- channel定义
    CHL_WORLD   = 1, -- 世界聊天
    CHL_PRIVATE = 2 -- 私聊
}

return oo.define( CHAT,... )
