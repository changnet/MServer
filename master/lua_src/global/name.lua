-- name.lua
-- xzc
-- 2019-05-26

-- 根据变量取名字
-- lua目前本身没有提供根据变量取名字的函数，即使debug库也没有
-- 但在一起功能，例如定时器回调中，考虑到函数会热更，使用函数名较为合适

-- 这里提供一个机制取对象(oo.lua)中的成员函数，做了缓存之后，也不会有效率问题

-- 如果是local函数，那没有通用的方法可以取，只能通过手动注册。
-- 这个仅限于定时器、rpc等特殊调用，一般情况下，还是用对象来做吧

--[[
-- 你这样用法，foo做为回调函数就不影响热更
local function foo()
end
__reg_func__(foo,"foo")
g_rpc:proxy(foo):back_to_foo( ... )
]]

-- ！！！local函数调用当前在定时器、rpc中都去掉了，因为手动注册函数这个不优雅，以后有需求再加

local names = {}
local func_names = {}

local function raw_name( mt,method )
    if not mt then return nil end

    for k,v in pairs( mt ) do
        if v == method then return k end
    end

    -- 当前类找不到，再找基类
    return raw_name( oo.classof(mt),method )
end

-- 这个函数目前只对oo中的对象函数有用，对标C的 __func__ 宏
function __method__( this,method )
    local name = names[method]
    if name then return name end

    assert( this:is_oo(),"only object support" )

    name = raw_name( oo.classof(this),method )

    assert( name,"method name not found" )

    names[method] = name
    return name
end

function __reg_func__ (func,name)
    assert( nil == func_names[func] )

    func_names[func] = name
end

-- 取local函数名
function __func__( func )
    return func_names[func]
end
