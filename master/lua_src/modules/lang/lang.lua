-- lang.lau
-- 2018-05-13
-- xzc

-- 语言包

local misc_list = require "config.lang_misc"
local tips_list = require "config.lang_tips"

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

function Lang:get_misc( id )
    return misc_lang[id]
end

function Lang:get_tips( id )
    return tips_lang[id]
end

local lang = Lang()

return lang
