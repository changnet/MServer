-- name.lua
-- xzc
-- 2019-05-26
-- 根据变量取名字
-- lua目前本身没有提供根据变量取名字的函数，即使debug库也没有
-- 但在一些功能，例如定时器回调中，考虑到函数会热更，使用函数名较为合适
-- 这里提供一个机制取对象(oo.lua)中的成员函数，做了缓存之后，也不会有效率问题
-- 如果是local函数，那没有通用的方法可以取，只能通过手动注册。
-- 这个仅限于定时器、rpc等特殊调用，一般情况下，还是用对象来做吧
--[[
-- 你这样用法，foo做为回调函数就不影响热更
local function foo()
end
__reg_func(foo,"foo")
g_rpc:proxy(foo):back_to_foo( ... )
]]

-- ！！！local函数调用当前在定时器、rpc中都去掉了，因为手动注册函数这个不优雅，以后有需求再加

local method_names = {}
local func_names = {}
local names_func = {}

setmetatable(method_names, {["__mode"] = 'k'})
setmetatable(func_names, {["__mode"] = 'k'})

local function raw_name(mt, method)
    if not mt then return nil end

    for k, v in pairs(mt) do if v == method then return k end end

    -- 当前类找不到，再找基类
    return raw_name(oo.classof(mt), method)
end

-- 取对象中的函数名，目前只对oo中的对象函数有用，对标C的 __func__ 宏
function __method(this, method)
    local name = method_names[method]
    if name then
        if -1 == name then return nil end -- 已查找过但找不到名字的，标为-1
        return name
    end

    -- 当前仅仅支持oo里的对象
    if "table" ~= type(this) or not this.is_oo then
        method_names[method] = -1
        return nil
    end

    name = raw_name(oo.classof(this), method)

    method_names[method] = name or -1
    return name
end

-- 成员函数调用及其参数转换为函数调用
-- 需要通过名字调用，因为会热更
-- @p0 ...: paramN,可变参数，因为不能用...，但也没必要用table.pack
-- cannot use '...' outside a vararg function near '...'
function method_thunk(this, method, p0, p1, p2, p3, p4, p5)
    assert(this and method, "method_thunk nil object or method")

    -- 需要通过函数名去调用，因为函数可能被热更
    local name = __method(this, method)
    if not name then return error("no method found") end

    -- method用不着了，这里没必要继承引用旧函数，不然热更的时候不会gc
    -- ps:高版本的lua(5.3)会自动判断这些upvalue是否被引用。而低版本则不会
    -- 传入不同的参数到闭包，如果这个参数没用到，lua 5.3返回的函数指针是一样的，5.1的不一样
    method = nil
    return function()
        return this[name](this, p0, p1, p2, p3, p4, p5)
    end
end

-- 把一个函数调用及其参数转换为一个函数保存起来
function func_thunk(func, p0, p1, p2, p3, p4, p5)
    local name = func_names[func]
    if not name then
        return function()
            return func(p0, p1, p2, p3, p4, p5)
        end
    end

    -- 能查找到名字，说明这个函数需要支持热更
    return function()
        local cb = assert(names_func[name], name)
        return cb(p0, p1, p2, p3, p4, p5)
    end
end

function __reg_func(func, name)
    assert(nil == func_names[func])

    func_names[func] = name
    names_func[name] = func
end

-- 取local函数名，仅对手动注册过的函数有效，对标C的 __func__ 宏
function __func(func)
    return func_names[func]
end
