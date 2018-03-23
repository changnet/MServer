-- require.lua
-- 2018-03-23
-- xzc

-- Lua脚本热更

raw_require = require

__require_list__ = {} -- 在全局记录require过的文件路径

-- 重写require函数
--[[
    这个函数要慎重处理。系统库、C++库、系统变量(_G全局变量这些)都存放在
package.loaded中的。热更时不要把系统库、_G这些销毁掉。
    因为起服时，上述的库已加载好，这时我们再加载脚本，就可以得出哪些是后面加载的脚本了。
]]
function require( path )
    if not package.loaded[path] then -- 已加载的可能是系统加，不要记录
        __require_list__[path] = 1
    end
    return raw_require(path)
end

-- 重新加载脚本
function re_require()
    -- 先把所有文件都销毁，因为我们不知道文件里的引用关系。避免一个重新加载的文件引用
    -- 了另一个还没重新加载的文件数据
    for path in pairs( __require_list__ ) do
        package.loaded[path] = nil
    end

    -- 清空旧文件记录
    -- 不要尝试直接require __require_list__中的文件，因为有些文件可能删除了
    __require_list__ = {}

    -- 重新加载入口文件
    require "modules/module_pre_header"
    require "modules/module_header"
end

