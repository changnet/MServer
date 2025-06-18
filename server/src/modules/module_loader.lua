-- 通用模块加载入口
--[[
不同进程、worker的逻辑不一样，需要加载的模块也不一样，很难统一。完全分开又发现会有一些
相同逻辑需要复用，因此定了一些规则。
1. test_loader、process_loader、module_loader是三个通用的loader。业务相关的一般统一
用module_loader

2. 如果某个进程、worker差异比较大，可以自定义一个loader

3. 所有业务模块必须都放loader文件，热更会重载此文件

4. 放loader的模块都是可热更的，不能热更的放Bootstrap.xxx_init
]]


local __require = require

local local_type = LOCAL_TYPE -- 当前worker的类型

local function require_by_wtype(path, wtype, ...)
    if not wtype then return end

    if local_type == wtype then __require(path) end

    return require_by_wtype(path, ...)
end

-- 重写require函数，根据类型判断是否在当前worker加载
local function require(path, wtype, ...)
    if not wtype then return __require(path) end

    return require_by_wtype(path, wtype, ...)
end

-- 指定哪些worker不加载，其他worker都加载
local function require_exclude(path, wtype, ...)
    if not wtype then return __require(path) end

    for _, wt in pairs({wtype, ...}) do
        if local_type == wt then return end
    end
    return __require(path)
end

-- 引用一起基础文件。其他逻辑初始化时可能会用到这些库
require "engine.preloader"

print("热更只加载loader文件，rpc等文件虽然已经加载了，在这里也需要重新加载")

require_define "modules.system.define"

table.const(EMPTY)

require "modules.system.hot_fix"
E = require "modules.system.error"

require_exclude("protocol.protocol", W_DATA)
require_exclude("message.pbc", W_DATA)
require_exclude("message.net_msg", W_DATA)

-- P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0
-- 在加载其他业务模块之前优先级p0的逻辑
Pbc.update()
NetMsg.load_forward_msg()
-- P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0 P0 p0

require("network.clt_mgr", W_GATEWAY)
require("account.account_mgr", W_GATEWAY)

Rtti.collect()
