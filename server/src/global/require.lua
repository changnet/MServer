-- require.lua
-- 2018-03-23
-- xzc

-- Lua脚本热更

--[[
TODO: 当整个系统的文件特别多(达到几百M)时，require会出现比较多的问题
1. 热更过慢
2. 解析lua文件的过程中，产生的零碎内存太多，导致内存占用很高

解决办法：
1. 增量更新
逻辑文件，由于相互依赖，可能没法做到增量更新。但配置文件是可以做增量更新的，增量更新有两
种方式，一种是指定文件名，即更新的时候，从git导出变动的文件名，重新require，不过这种方式
不适用于线下开发。另一种是遍历所有文件，对比文件时间戳，这种方式更常用。

2. 编译lua文件
使用luac编译lua文件为字节码再加载，可大大加快加载速度。但这个可能会对一些部署、调试流
程带来麻烦，折衷的办法是只编译一部分巨大的配置文件

3. 功能拆分到不同进程
]]

local raw_require = require

local __require_list = {} -- 在全局记录require过的文件路径
local __no_update_require = {} -- 这些文件不需要热更

-- 重写require函数
--[[
    这个函数要慎重处理。系统库、C++库、系统变量(_G全局变量这些)都存放在
package.loaded中的。热更时不要把系统库、_G这些销毁掉。
    因为起服时，上述的库已加载好，这时我们再加载脚本，就可以得出哪些是后面加载的脚本了。
]]
function require( path )
    if not package.loaded[path] then -- 已加载的可能是系统模块，不能热更，不要记录
        assert( nil == __no_update_require[path] )
        __require_list[path] = 1
    end
    return raw_require(path)
end

-- 一些不需要热更的文件
function no_update_require( path )
    __no_update_require[path] = 1

    return raw_require( path )
end

-- 清除加载的脚本
function unrequire()
    -- 先把所有文件都销毁，因为我们不知道文件里的引用关系。避免一个重新加载的文件引用
    -- 了另一个还没重新加载的文件数据
    for path in pairs( __require_list ) do
        package.loaded[path] = nil
        assert( nil == __no_update_require[path] )
    end

    -- 清空旧文件记录
    -- 不要尝试直接require __require_list中的文件，因为有些文件可能删除了
    __require_list = {}
end

