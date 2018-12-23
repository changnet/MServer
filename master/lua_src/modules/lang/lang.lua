-- lang.lau
-- 2018-05-13
-- xzc

-- 语言包

require "modules.lang.lang_header"
local misc_list = require_conf("lang_misc")
local tips_list = require_conf("lang_tips")

local lang_type = g_setting.lang

local misc_lang = {}
local tips_lang = {}

-- 初始化为kv结构
for _,one in pairs( misc_list ) do misc_lang[one.id] = one[lang_type] end
for _,one in pairs( tips_list ) do tips_lang[one.id] = one[lang_type] end

-- 原始配置用不到了
misc_list = nil
tips_list = nil

local Lang = oo.singleton( nil,... )

-- 获取misc语言包
function Lang:get_misc( id )
    return misc_lang[id]
end

-- 获取tips语言包
function Lang:get_tips( id )
    return tips_lang[id]
end

-- 发送tips
function Lang:send_tips( player,id )
    if not tips_lang[id] then
        ELOG( "Lang:send_tips no such id:%d",id )
        return
    end

    player:send_pkt( SC.MISC_TIPS,tips_lang[id] )
end

local lang = Lang()

return lang
