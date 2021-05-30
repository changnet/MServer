-- Object Oriented
oo = {}

local class_name = {} -- 类列表，类为k，name为v
local name_class = {} -- 类列表，name为k，类为v

local object = {} -- 已创建对象列表，对象为k，name为value
local singleton = {} -- 单例，类为k，v为实例

local obj_count = {} -- 对象创建次数列表
local stat_flag = true -- 是否记录数据

setmetatable(object, {["__mode"] = 'k'})
setmetatable(singleton, {["__mode"] = 'v'})

-- ******************************************************************************
local class_base = {} -- 默认基类

-- default construnctor
function class_base:__init()
end

-- 是否从oo.class创建的对象
function class_base:is_oo()
    return true
end

-- isa operator(是否从某个类继承而来或者就是该类)
function class_base:isa(clz)
    local c = oo.classof(self)
    while c and c ~= class_base do
        if c == clz then return true end
        c = oo.superclassof(c)
    end
    return false
end
-- ******************************************************************************
-- 创建lua对象
local function new(clz, ...)
    local obj = {}

    setmetatable(obj, clz)
    obj:__init(...)

    if stat_flag then -- check
        local name = class_name[clz] or "none"
        object[obj] = name
        obj_count[name] = (obj_count[name] or 0) + 1
    end

    return obj
end

-- 创建单例
local function new_singleton(clz, ...)
    if not singleton[clz] then singleton[clz] = new(clz, ...) end

    return singleton[clz]
end

-- ******************************************************************************
-- 创建惰性继承类，无法实现多继承
-- 惰性继承用元表实现继承，这样如果单一更新其中一个文件，则所有对象(包括子类都会被更新)
-- luacheck: ignore lazy_class
local function lazy_class(new_method, clz, super)
    super = super or class_base
    rawset(clz, "__super", super)
    -- 设置metatable的__index,创建实例(调用__call)时让自己成为一个metatable
    rawset(clz, "__index", clz)
    -- 设置自己的metatable为父类，这样才能调用父类函数
    setmetatable(clz, {__index = super, __call = new_method})

    return clz
end

-- 创建快速继承类，可实现多继承
-- 直接将基类函数复制到当前类，多重继承不会有损耗
-- 要求热更时，能热更所有文件(基类有更新，则子类也需要更新)
-- 使用这个继承，设计框架时要注意引用先后关系
-- 先热更子类，再热更基类就出现子类用了旧的基类函数
local function fast_class(new_method, clz, super, ...)
    super = super or class_base

    local supers = {super, ...}

    -- 多重继承或多继承时，各个基类必须按 s3,s2,s1... 顺序传进来
    -- 而同名函数则会按 s1覆盖s0,s2覆盖s1,s3覆盖s2 的顺序
    for idx = #supers, 1, -1 do
        -- 复制基类函数到子类
        local clz_base = supers[idx]
        for k, v in pairs(clz_base) do clz[k] = v end
    end

    -- 设置metatable的__index,创建实例(调用__call)时让自己成为一个metatable
    rawset(clz, "__index", clz)
    -- 设置自己的metatable为父类，这样才能调用父类函数
    setmetatable(clz, {__call = new_method})

    return clz
end

--[[
    声明lua类，有3种写法
    oo.class( ... ) -- 无继承
    oo.class( ...,s3,s2,s1,s0 ) -- 多继承
    oo.class( ClassName,s0 ) -- 手动声明类名

    对于lua5.1，require函数传入模块路径，刚好用作类名，避免冲突，即上面的 ...
    但lua5.3 require传入两个参数，因此super的类型需要判断一下
    oo.class( ...,s3,s2,s1,s0 )这种写法，会让 ... 只取第一个值
]]
local function raw_class(new_method, name, super, ...)
    local clz = {}
    if type(name) == "string" then
        -- 如果已经存在，则是热更，先把旧函数都清空
        if name_class[name] ~= nil then
            clz = name_class[name]
            for k in pairs(clz) do clz[k] = nil end
        else
            name_class[name] = clz
        end

        if stat_flag then -- 状态统计
            class_name[clz] = name
        end
    else
        error("oo class no name specify")
        return
    end

    if "table" == type(super) then
        -- lazy_class(new_method,clz,super,...)
        return fast_class(new_method, clz, super, ...)
    else
        return fast_class(new_method, clz, ...)
    end
end

-- 声明普通类
function oo.class(name, ...)
    return raw_class(new, name, ...)
end

-- 声明lua单例类
function oo.singleton(name, ...)
    return raw_class(new_singleton, name, ...)
end

-- ******************************************************************************

-- 获取基类元表(clz是元表而不是对象)
function oo.superclassof(clz)
    return rawget(clz, "__super")
end

-- 获取当前类元表
function oo.classof(ob)
    return getmetatable(ob).__index
end

-- 获取某个类的元表
function oo.metatableof(name)
    return name_class[name]
end

-- 返回各对象的统计(由于lua的gc问题，这些统计并不是实时的)
function oo.stat()
    -- collectgarbage("collect")

    local total = 0 -- 所有对象数量
    local cur_count = {} -- 当前数量
    for _, name in pairs(object) do
        total = total + 1
        cur_count[name] = (cur_count[name] or 0) + 1
    end

    local stat = {}
    stat.total = total
    stat.cur = cur_count

    stat.max = {}
    for k, v in pairs(obj_count) do stat.max[k] = v end

    return stat
end

return oo
