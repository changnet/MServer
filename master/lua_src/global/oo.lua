-- a much simple OO implementation base on loop.simple

oo = {}

local class_name = {}       -- 类列表，类为k，name为v
local name_class = {}       -- 类列表，name为k，类为v

local object     = {}       -- 已创建对象列表，对象为k，name为value
local singleton  = {}       -- 单例，类为k，v为实例

local obj_count  = {}        -- 对象创建次数列表
local stat_flag  = true      -- 是否记录数据

setmetatable(object, {["__mode"]='k'})
setmetatable(singleton, {["__mode"]='v'})

--******************************************************************************
local class_base = {}  --默认基类
-- default construnctor
function class_base:__init()
end

-- isa operator(是否从某个类继承而来或者就是该类)
function class_base:isa(clz)
    local c = classof(self)
    while c and c ~= class_base do
        if c == clz then return true end
        c = superclassof(c)
    end
    return false
end
--******************************************************************************
-- 创建lua对象
local function new(clz, ...)
    local obj = {}

    setmetatable(obj, clz)
    obj:__init(...)

    if stat_flag then                  --check
        local name = class_name[clz] or "none"
        object[obj] = name
        obj_count[name] = (obj_count[name] or 0) + 1
    end

    return obj
end

-- 创建单例
local function new_singleton( clz,... )
    if not singleton[clz] then
        singleton[clz] = new( clz,... )
    end

    return singleton[clz]
end

--******************************************************************************
-- 声明lua类
function oo.class(super, name)
    local clz = {}
    if type(name) == "string" then
        if name_class[name] ~= nil then
            clz = name_class[name]
            for k,v in pairs(clz) do  --这是热更的关键
                clz[k] = nil
            end
        else
            name_class[name] = clz
        end

        if stat_flag then                     --check
            class_name[clz] = name
        end
    else
        error( "oo class no name specify" )
        return
    end

    super = super or class_base
    rawset(clz, "__super", super)
    -- 设置metatable的__index,创建实例(调用__call)时让自己成为一个metatable
    rawset(clz, "__index",clz)
    -- 设置自己的metatable为父类，这样才能调用父类函数
    setmetatable(clz, {__index = super, __call = new})
    return clz
end

-- 声明lua单例类
function oo.singleton(super, name)
    local clz = {}
    if type(name) == "string" then
        if name_class[name] ~= nil then
            clz = name_class[name]
            for k,v in pairs(clz) do  --这是热更的关键
                clz[k] = nil
            end
        else
            name_class[name] = clz
        end

        if stat_flag then                     --check
            class_name[clz] = name
        end
    else
        error( "oo singleton no name specify" )
        return
    end

    super = super or class_base
    rawset(clz, "__super", super)
    rawset(clz, "__index",clz)       --让自己成为一个metatable
    rawset(clz, "__singleton",true)
    setmetatable(clz, {__index = super, __call = new_singleton})
    return clz
end

--******************************************************************************

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
    for obj,name in pairs(object) do
        total = total + 1
        cur_count[name] = (cur_count[name] or 0) + 1
    end

    local stat = {}
    stat.total = total
    stat.cur = cur_count

    stat.max = {}
    for k,v in pairs(obj_count) do stat.max[k] = v end

    return stat
end


return oo
