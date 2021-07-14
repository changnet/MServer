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

3. 功能拆分到不同进程，对应进程只加载自己需要的文件
]]

-- !!! 此文件不能热更

local raw_require = require

local __require_list = {} -- 在全局记录require过的文件路径
local __require_no_update = {} -- 这些文件不需要热更

-- 重写require函数
function require(path)
    --[[
    这个函数要慎重处理。系统库、C++库、系统变量(_G全局变量这些)都存放在package.loaded
    中的。热更时不要把系统库、_G这些销毁掉。因为起服时，上述的库已加载好，这时我们再加载
    脚本，就可以得出哪些是后面加载的脚本了。
    ]]
    if not package.loaded[path] then -- 已加载的可能是系统模块，不能热更，不要记录
        assert(nil == __require_no_update[path])
        __require_list[path] = 1
    end
    return raw_require(path)
end

-- 加载该文件，但执行热更新时不更新该文件
function require_no_update(path)
    __require_no_update[path] = 1

    return raw_require(path)
end

-- 清除加载的脚本
function unrequire()
    -- 先把所有文件都销毁，因为我们不知道文件里的引用关系。避免一个重新加载的文件引用
    -- 了另一个还没重新加载的文件数据
    for path in pairs(__require_list) do
        package.loaded[path] = nil
        assert(nil == __require_no_update[path])
    end

    -- 清空旧文件记录
    -- 不要尝试直接require __require_list中的文件，因为有些文件可能删除了
    __require_list = {}
end

-- 开始一组定义，接下来的全局定义都会插入到def中
function DEFINE_BEG(def)
    -- 得用rawset，因为这个是在require_define中调用，触发了__newindex
    -- _G.def = def
    rawset(_G, "def", def)
end

-- 结束一组定义
function DEFINE_END(def)
    -- 假如def不存在的话，即使设置一个nil值也会触发__newindex
    -- _G.def = nil
    rawset(_G, "def", nil)
end

-- 从文件中加载宏定义(该文件必须未被加载过，且里面的宏定义未在其他地方定义过)
-- @param path 需要加载的文件路径，同require的参数，一般用点号
-- @param no_g 不要把变量设置到全局
-- @return table,包含该文件中的所有全局变量定义
function require_define(path, no_g)
    -- 加载过的不要重复加载，不然会导致把数据删掉了，但require又不生效
    if __require_list[path] then return end

    local _g_defines = _G._g_defines
    if not _g_defines then
        _g_defines = {}
        _G._g_defines = _g_defines
    end
    if _g_defines[path] then
        -- 必须先清除旧的变量，否则__newindex不会触发
        for k in pairs(_g_defines[path]) do _G[k] = nil end
    end

    local defines = {}
    setmetatable(_G, {
        __newindex = function(t, k, v)
            rawset(defines, k, v)
            if not no_g then rawset(t, k, v) end
            if _G.def then rawset(_G.def, k, v) end
        end
    })
    require(path)
    setmetatable(_G, nil)

    _g_defines[path] = defines
    return _g_defines[path]
end
