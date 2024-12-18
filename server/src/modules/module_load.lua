-- 模块加载入口
-- 所有业务模块必须都放这里，热更会重载此文件
-- 放这里的模块都是可热更的，不能热更的放Bootstrap.preload

local __require = require

local local_type = LOCAL_TYPE -- 当前worker的类型

local function require_by_type(path, wtype, ...)
    if not wtype then return end

    if local_type == wtype then __require(path) end

    return require_by_type(path, ...)
end

-- 重写require函数，根据类型判断是否在当前worker加载
local function require(path, wtype, ...)
    if not wtype then return __require(path) end

    return require_by_type(path, wtype, ...)
end

-- app也能热更,在这里重新require一次才会热更
require "application.app"

-- 引用一起基础文件。其他逻辑初始化时可能会用到这些库
require "global.global"
require "global.table"
require "global.math"
require "global.string"
require "global.time"
require "global.name"

require_define "modules.system.define"

require "modules.system.hot_fix"
E = require "modules.system.error"
